
// ================================================================================================
// File: VkToolbox/ResourceManager.cpp
// Author: Guilherme R. Lampert
// Created on: 18/02/17
// Brief: Explicit template instantiations of the known resource managers.
// ================================================================================================

#include "ResourceManager.hpp"

namespace VkToolbox
{

// ========================================================

template<typename T>
const char * resourceManagerThreadName(); // unimplemented - fails to link

template<> const char * resourceManagerThreadName<Texture>()
{
    return "Texture Loader";
}
template<> const char * resourceManagerThreadName<GlslShader>()
{
    return "GLSL Shader Loader";
}

// ========================================================
// ResourceManager common:
// ========================================================

template<typename T>
const VulkanContext * ResourceManager<T>::sm_vkContext(nullptr);

template<typename T>
std::vector<T> ResourceManager<T>::sm_resourcesStore;

template<typename T>
typename ResourceManager<T>::HashIndex ResourceManager<T>::sm_resourcesLookupTable;

template<typename T>
bool ResourceManager<T>::sm_inResourceLoadState(false);

template<typename T>
JobQueue ResourceManager<T>::sm_asyncLoadRequests(resourceManagerThreadName<T>());

// ========================================================

template<typename T>
void ResourceManager<T>::initialize(const VulkanContext & vkContext)
{
    sm_vkContext = &vkContext;
    sm_asyncLoadRequests.launch();
}

template<typename T>
void ResourceManager<T>::shutdown()
{
    sm_resourcesStore.clear();
    sm_resourcesStore.shrink_to_fit();
    sm_resourcesLookupTable.clear_and_free();

    sm_vkContext = nullptr;
    sm_inResourceLoadState = false;
}

template<typename T>
void ResourceManager<T>::preallocate(const int resourceCount)
{
    if (resourceCount > 0)
    {
        sm_resourcesStore.reserve(resourceCount);
    }
}

template<typename T>
void ResourceManager<T>::beginResourceLoad()
{
    sm_inResourceLoadState = true;
}

template<typename T>
void ResourceManager<T>::endResourceLoad()
{
    sm_inResourceLoadState = false;
}

template<typename T>
void ResourceManager<T>::pushAsyncLoadRequest(const ResourceIndex resIndex)
{
    assert(resIndex < sm_resourcesStore.size());
    sm_asyncLoadRequests.pushJob([resIndex]()
    {
        if (!sm_resourcesStore[resIndex].load())
        {
            Log::warningF("Async load request failed for resource at index %u", resIndex);
        }
    });
}

template<typename T>
void ResourceManager<T>::waitPendingAsyncLoadRequests()
{
    sm_asyncLoadRequests.waitAll();
}

template<typename T>
typename ResourceManager<T>::ResourceIndex ResourceManager<T>::createNewSlot(ResourceId id)
{
    const auto hashKey = id.hash.value;
    sm_resourcesStore.emplace_back(*sm_vkContext, std::move(id));
    const auto index = narrowCast<ResourceIndex>(sm_resourcesStore.size() - 1);
    sm_resourcesLookupTable.insert(hashKey, index);
    return index;
}

template<typename T>
bool ResourceManager<T>::findLoaded(const ResourceId & inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    const auto index = sm_resourcesLookupTable.find(inResId.hash.value, inResId, sm_resourcesStore,
                                                    [](const ResourceId & key, const T & item) {
                                                        return key == item.resourceId();
                                                    });

    if (index == HashIndex::null_index)
    {
        (*outResIndex) = InvalidResIndex;
        return false; // Not registered
    }

    if (!sm_resourcesStore[index].isLoaded())
    {
        (*outResIndex) = InvalidResIndex;
        return false; // Not loaded
    }

    (*outResIndex) = index;
    return true;
}

template<typename T>
bool ResourceManager<T>::findOrLoad(ResourceId inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);
    assert(sm_inResourceLoadState);

    auto index = sm_resourcesLookupTable.find(inResId.hash.value, inResId, sm_resourcesStore,
                                              [](const ResourceId & key, const T & item) {
                                                  return key == item.resourceId();
                                              });

    if (index == HashIndex::null_index) // Register the slot if needed
    {
        index = createNewSlot(std::move(inResId));
    }

    // Slot stays registered for future load attempts even if we can't load now.
    const bool loaded = sm_resourcesStore[index].load();
    (*outResIndex) = index;
    return loaded;
}

template<typename T>
bool ResourceManager<T>::registerSlot(ResourceId inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    auto index = sm_resourcesLookupTable.find(inResId.hash.value, inResId, sm_resourcesStore,
                                              [](const ResourceId & key, const T & item) {
                                                  return key == item.resourceId();
                                              });

    // Register new or just return existing:
    if (index == HashIndex::null_index)
    {
        index = createNewSlot(std::move(inResId));
    }

    (*outResIndex) = index;
    return true;
}

template<typename T>
bool ResourceManager<T>::isRegistered(const ResourceId & inResId)
{
    assert(!inResId.isNull());
    const auto index = sm_resourcesLookupTable.find(inResId.hash.value, inResId, sm_resourcesStore,
                                                    [](const ResourceId & key, const T & item) {
                                                        return key == item.resourceId();
                                                    });
    return index != HashIndex::null_index;
}

template<typename T>
bool ResourceManager<T>::isLoaded(const ResourceIndex resIndex)
{
    return resourceRef(resIndex).isLoaded();
}

template<typename T>
bool ResourceManager<T>::reload(const ResourceIndex resIndex)
{
    assert(sm_inResourceLoadState);
    return mutableResourceRef(resIndex).load();
}

template<typename T>
void ResourceManager<T>::unload(const ResourceIndex resIndex)
{
    mutableResourceRef(resIndex).unload();
}

template<typename T>
void ResourceManager<T>::reloadAll(int * outNumReloaded, int * outNumFailed)
{
    assert(sm_inResourceLoadState);

    int numReloaded = 0;
    int numFailed   = 0;

    for (auto & resource : sm_resourcesStore)
    {
        if (resource.load())
        {
            ++numReloaded;
        }
        else
        {
            ++numFailed;
        }
    }

    if (outNumReloaded != nullptr) { *outNumReloaded = numReloaded; }
    if (outNumFailed   != nullptr) { *outNumFailed   = numFailed;   }
}

template<typename T>
void ResourceManager<T>::unloadAll()
{
    for (auto & resource : sm_resourcesStore)
    {
        resource.unload();
    }
}

template<typename T>
void ResourceManager<T>::unregisterAll()
{
    unloadAll();
    sm_resourcesLookupTable.clear();
    sm_resourcesStore.clear();
}

template<typename T>
int ResourceManager<T>::resourceCount()
{
    return narrowCast<int>(sm_resourcesStore.size());
}

template<typename T>
const T & ResourceManager<T>::resourceRef(const ResourceIndex resIndex)
{
    assert(resIndex < sm_resourcesStore.size());
    return sm_resourcesStore[resIndex];
}

template<typename T>
typename ResourceManager<T>::ResourceIndex ResourceManager<T>::indexOf(const T & ref)
{
    const T * startAddr = sm_resourcesStore.data();
    const T * endAddr   = startAddr + sm_resourcesStore.size();

    if (&ref < startAddr || &ref >= endAddr)
    {
        return InvalidResIndex;
    }

    const std::ptrdiff_t offset = (&ref - startAddr);
    return narrowCast<ResourceIndex>(offset);
}

template<typename T>
T & ResourceManager<T>::mutableResourceRef(const ResourceIndex resIndex)
{
    assert(resIndex < sm_resourcesStore.size());
    return sm_resourcesStore[resIndex];
}

template<typename T>
const VulkanContext & ResourceManager<T>::context()
{
    return *sm_vkContext;
}

// ========================================================
// TextureManager specific:
// ========================================================

template<>
void ResourceManager<Texture>::beginResourceLoad()
{
    assert(!sm_inResourceLoadState);
    sm_inResourceLoadState = true;

    const CommandBuffer & cmdBuff = sm_vkContext->mainTextureStagingCmdBuffer();
    cmdBuff.beginRecording();
}

template<>
void ResourceManager<Texture>::endResourceLoad()
{
    assert(sm_inResourceLoadState);
    sm_inResourceLoadState = false;

    const CommandBuffer & cmdBuff = sm_vkContext->mainTextureStagingCmdBuffer();
    cmdBuff.endRecording();

    cmdBuff.submitAndWaitComplete(sm_vkContext->graphisQueue());
    cmdBuff.reset();

    ResourceIndex texIdx = Texture::sm_stagingChainHead;
    while (texIdx != InvalidResIndex)
    {
        Texture & tex = mutableResourceRef(texIdx);
        ResourceIndex nextIdx = tex.m_stagingLinkNext;
        tex.releaseStagingResources();
        texIdx = nextIdx;
    }
    Texture::sm_stagingChainHead = InvalidResIndex;
}

// ========================================================
// Global instances:
// ========================================================

template class ResourceManager<GlslShader>;
template class ResourceManager<Texture>;

// ========================================================

} // namespace VkToolbox
