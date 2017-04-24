#pragma once

// ================================================================================================
// File: VkToolbox/ResourceManager.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Base template for the resource registries/managers.
// ================================================================================================

#include "Texture.hpp"
#include "GlslShader.hpp"

#include <cstdint>
#include <vector>

namespace VkToolbox
{

// ========================================================
// class ResourceManager:
// ========================================================

template<typename T>
class ResourceManager final
{
public:

    using ResourceType  = T;
    using ResourceId    = StrId<str>;
    using ResourceIndex = std::uint32_t;
    static constexpr ResourceIndex InvalidResIndex = ~static_cast<ResourceIndex>(0);

    // Not copyable.
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager & operator = (const ResourceManager &) = delete;

    // Call before loading/reloading any resource (even indirectly with findOrLoad).
    // Some resources, like textures, need to create command buffers and cleanup staging
    // images after being loaded. Not all resource types require this and might leave unimplemented.
    void beginResourceLoad() { }
    void endResourceLoad()   { }

    // Init with the VK device that will own all the resources.
    explicit ResourceManager(const VulkanContext & vkContext);

    // Preallocate storage for a number of resources. The parameter is merely a hint.
    void preallocate(int resourceCount);

    // Test if a slot for the resource is registered AND the resource is loaded.
    bool findLoaded(const ResourceId & inResId, ResourceIndex * outResIndex) const;

    // Find existing resource. If no slot registered, one is created. If not loaded yet, load the resource.
    bool findOrLoad(ResourceId inResId, ResourceIndex * outResIndex);

    // Only create the resource slot but don't load yet. Returns existing slot if already registered.
    bool registerSlot(ResourceId inResId, ResourceIndex * outResIndex);

    // Check if resource slot already registered (may or may not be loaded).
    bool isRegistered(const ResourceId & inResId) const;

    // Check if resource already loaded (slot must be registered).
    bool isLoaded(ResourceIndex resIndex) const;

    // Reload given resource at slot (slot must be registered).
    bool reload(ResourceIndex resIndex);

    // Unload given resource at slot (slot must be registered).
    void unload(ResourceIndex resIndex);

    // Attempt to reload all registered resources.
    void reloadAll(int * outNumReloaded = nullptr, int * outNumFailed = nullptr);

    // Unload all resources without removing the slots - can reloaded them later.
    void unloadAll();

    // Clear all resource slots and unloads the resources.
    void unregisterAll();

    // Number of registered resource slots.
    int resourceCount() const;

    // Deref a slot index to get the resource object. Asserts if the slot index is invalid.
    // NOTE: It is not safe to store references to the returned object
    //       since the underlaying store might be moved or reallocated!
    const T & resourceRef(ResourceIndex resIndex) const;

    // Access to the Vulkan instance that owns all the graphics resources.
    const VulkanContext & context() const;

private:

    // Expand the resource store by one item and also register the new index into the LUT.
    ResourceIndex createNewSlot(ResourceId id);

    // Internal variant of resourceRef() for load/unload mutable calls.
    T & mutableResourceRef(ResourceIndex resIndex);

private:

    using HashIndex = hash_index<ResourceIndex, std::uint64_t>;

    const VulkanContext * m_vkContext;
    std::vector<T> m_resourcesStore;
    HashIndex m_resourcesLookupTable;
};

// ========================================================
// Standard Resource Manager types:
// ========================================================

extern template class ResourceManager<GlslShader>;
using GlslShaderManager = ResourceManager<GlslShader>;

extern template class ResourceManager<Texture>;
using TextureManager = ResourceManager<Texture>;

// ========================================================
// ResourceManager inline methods:
// ========================================================

template<typename T>
inline ResourceManager<T>::ResourceManager(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

template<typename T>
inline typename ResourceManager<T>::ResourceIndex ResourceManager<T>::createNewSlot(ResourceId id)
{
    const auto hashKey = id.hash.value;
    m_resourcesStore.emplace_back(*m_vkContext, std::move(id));
    const auto index = narrowCast<ResourceIndex>(m_resourcesStore.size() - 1);
    m_resourcesLookupTable.insert(hashKey, index);
    return index;
}

template<typename T>
inline void ResourceManager<T>::preallocate(const int resourceCount)
{
    if (resourceCount > 0)
    {
        m_resourcesStore.reserve(resourceCount);
    }
}

template<typename T>
inline bool ResourceManager<T>::findLoaded(const ResourceId & inResId, ResourceIndex * outResIndex) const
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    const auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                                   [](const ResourceId & key, const T & item) {
                                                       return key == item.resourceId();
                                                   });

    if (index == HashIndex::null_index)
    {
        (*outResIndex) = InvalidResIndex;
        return false; // Not registered
    }

    if (!m_resourcesStore[index].isLoaded())
    {
        (*outResIndex) = InvalidResIndex;
        return false; // Not loaded
    }

    (*outResIndex) = index;
    return true;
}

template<typename T>
inline bool ResourceManager<T>::findOrLoad(ResourceId inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                             [](const ResourceId & key, const T & item) {
                                                 return key == item.resourceId();
                                             });

    if (index == HashIndex::null_index) // Register the slot if needed
    {
        index = createNewSlot(std::move(inResId));
    }

    // Slot stays registered for future load attempts even if we can't load now.
    const bool loaded = m_resourcesStore[index].load();
    (*outResIndex) = index;
    return loaded;
}

template<typename T>
inline bool ResourceManager<T>::registerSlot(ResourceId inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
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
inline bool ResourceManager<T>::isRegistered(const ResourceId & inResId) const
{
    assert(!inResId.isNull());
    const auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                                   [](const ResourceId & key, const T & item) {
                                                       return key == item.resourceId();
                                                   });
    return index != HashIndex::null_index;
}

template<typename T>
inline bool ResourceManager<T>::isLoaded(const ResourceIndex resIndex) const
{
    return resourceRef(resIndex).isLoaded();
}

template<typename T>
inline bool ResourceManager<T>::reload(const ResourceIndex resIndex)
{
    return mutableResourceRef(resIndex).load();
}

template<typename T>
inline void ResourceManager<T>::unload(const ResourceIndex resIndex)
{
    mutableResourceRef(resIndex).unload();
}

template<typename T>
inline void ResourceManager<T>::reloadAll(int * outNumReloaded, int * outNumFailed)
{
    int numReloaded = 0;
    int numFailed   = 0;

    for (auto & resource : m_resourcesStore)
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

    if (outNumReloaded != nullptr) { (*outNumReloaded) = numReloaded; }
    if (outNumFailed   != nullptr) { (*outNumFailed)   = numFailed;   }
}

template<typename T>
inline void ResourceManager<T>::unloadAll()
{
    for (auto & resource : m_resourcesStore)
    {
        resource.unload();
    }
}

template<typename T>
inline void ResourceManager<T>::unregisterAll()
{
    unloadAll();
    m_resourcesLookupTable.clear();
    m_resourcesStore.clear();
}

template<typename T>
inline int ResourceManager<T>::resourceCount() const
{
    return narrowCast<int>(m_resourcesStore.size());
}

template<typename T>
inline const T & ResourceManager<T>::resourceRef(const ResourceIndex resIndex) const
{
    assert(resIndex < m_resourcesStore.size());
    return m_resourcesStore[resIndex];
}

template<typename T>
inline T & ResourceManager<T>::mutableResourceRef(const ResourceIndex resIndex)
{
    assert(resIndex < m_resourcesStore.size());
    return m_resourcesStore[resIndex];
}

template<typename T>
inline const VulkanContext & ResourceManager<T>::context() const
{
    return *m_vkContext;
}

// ========================================================

} // namespace VkToolbox
