#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/ResourceManager.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Base template for the resource registries/managers.
// ================================================================================================

#include "Resource.hpp"
#include "Texture.hpp"
#include "GlslShader.hpp"

#include <cstdint>
#include <vector>

namespace VkToolbox
{

template<typename T>
class ResourceManager
{
public:

    using ResourceType  = T;
    using ResourceIndex = std::uint32_t;
    static constexpr ResourceIndex InvalidResIndex = ~static_cast<ResourceIndex>(0);

    // Not copyable.
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager & operator = (const ResourceManager &) = delete;

    // Init with the VK device that will own all the resources.
    explicit ResourceManager(WeakHandle<VkDevice> device);

    // Preallocate storage for a number of resources. The parameter is merely a hint.
    void preallocate(int resourceCount);

    // Test if a slot for the resource is registered AND the resource is loaded.
    bool findLoaded(ResourceId inResId, ResourceIndex * outResIndex) const;

    // Find existing resource. If no slot registered, one is created. If not loaded yet, load the resource.
    bool findOrLoad(ResourceId inResId, ResourceIndex * outResIndex);

    // Only create the resource slot but don't load yet. Returns existing slot if already registered.
    bool registerSlot(ResourceId inResId, ResourceIndex * outResIndex);

    // Check if resource slot already registered (may or may not be loaded).
    bool isRegistered(ResourceId inResId) const;

    // Check if resource already loaded.
    bool isLoaded(ResourceIndex resIndex) const;

    // Reload given resource at slot.
    bool reload(ResourceIndex resIndex);

    // Unload given resource at slot.
    bool unload(ResourceIndex resIndex);

    // Attempt to reload all registered resources.
    void reloadAll(int * outNumReloaded = nullptr, int * outNumFailed = nullptr);

    // Unload all resources without removing the slots - can reloaded them later.
    void unloadAll();

    // Clear all resource slots and unloads the resources.
    void unregisterAll();

    // Number of registered resource slots.
    int getResourceCount() const;

    // Deref a slot index to get the resource object. Asserts if the slot index is invalid.
    // NOTE: It is not safe to store references to the returned object
    //       since the underlaying store might be moved or reallocated!
    const T & getResourceRef(ResourceIndex resIndex) const;

private:

    // Expand the resource store by one item and also register the new index into the LUT.
    ResourceIndex createNewSlot(ResourceId id);

    using HashIndex = hash_index<ResourceIndex, std::uint64_t>;

    WeakHandle<VkDevice> m_device;
    std::vector<T>       m_resourcesStore;
    HashIndex            m_resourcesLookupTable;
};

// ========================================================
// Standard Resource Managers:
// ========================================================

using GlslShaderManager = ResourceManager<GlslShader>;
using TextureManager    = ResourceManager<Texture>;

// ========================================================
// ResourceManager inline methods:
// ========================================================

template<typename T>
ResourceManager<T>::ResourceManager(WeakHandle<VkDevice> device)
    : m_device{ device }
{
}

template<typename T>
typename ResourceManager<T>::ResourceIndex ResourceManager<T>::createNewSlot(const ResourceId id)
{
    m_resourcesStore.emplace_back(m_device, id);
    const auto index = narrowCast<ResourceIndex>(m_resourcesStore.size() - 1);
    m_resourcesLookupTable.insert(id.hash.value, index);
    return index;
}

template<typename T>
void ResourceManager<T>::preallocate(const int resourceCount)
{
    if (resourceCount > 0)
    {
        m_resourcesStore.reserve(resourceCount);
    }
}

template<typename T>
bool ResourceManager<T>::findLoaded(const ResourceId inResId, ResourceIndex * outResIndex) const
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    const auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                                   [](const ResourceId & key, const T & item) {
                                                       return key == item.getId();
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
bool ResourceManager<T>::findOrLoad(const ResourceId inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                             [](const ResourceId & key, const T & item) {
                                                 return key == item.getId();
                                             });

    if (index == HashIndex::null_index) // Register the slot if needed
    {
        index = createNewSlot(inResId);
    }

    // Slot stays registered for future load attempts even if we can't load now.
    const bool loaded = m_resourcesStore[index].load();
    (*outResIndex) = index;
    return loaded;
}

template<typename T>
bool ResourceManager<T>::registerSlot(const ResourceId inResId, ResourceIndex * outResIndex)
{
    assert(!inResId.isNull());
    assert(outResIndex != nullptr);

    auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                             [](const ResourceId & key, const T & item) {
                                                 return key == item.getId();
                                             });

    // Register new or just return existing:
    if (index == HashIndex::null_index)
    {
        index = createNewSlot(inResId);
    }

    (*outResIndex) = index;
    return true;
}

template<typename T>
bool ResourceManager<T>::isRegistered(const ResourceId inResId) const
{
    assert(!inResId.isNull());
    const auto index = m_resourcesLookupTable.find(inResId.hash.value, inResId, m_resourcesStore,
                                                   [](const ResourceId & key, const T & item) {
                                                       return key == item.getId();
                                                   });
    return index != HashIndex::null_index;
}

template<typename T>
bool ResourceManager<T>::isLoaded(const ResourceIndex resIndex) const
{
    return getResourceRef(resIndex).isLoaded();
}

template<typename T>
bool ResourceManager<T>::reload(const ResourceIndex resIndex)
{
    return getResourceRef(resIndex).load();
}

template<typename T>
bool ResourceManager<T>::unload(const ResourceIndex resIndex)
{
    return getResourceRef(resIndex).unload();
}

template<typename T>
void ResourceManager<T>::reloadAll(int * outNumReloaded, int * outNumFailed)
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
void ResourceManager<T>::unloadAll()
{
    for (auto & resource : m_resourcesStore)
    {
        resource.unload();
    }
}

template<typename T>
void ResourceManager<T>::unregisterAll()
{
    unloadAll();
    m_resourcesLookupTable.clear();
    m_resourcesStore.clear();
}

template<typename T>
int ResourceManager<T>::getResourceCount() const
{
    return narrowCast<int>(m_resourcesStore.size());
}

template<typename T>
const T & ResourceManager<T>::getResourceRef(const ResourceIndex resIndex) const
{
    assert(resIndex < m_resourcesStore.size());
    return m_resourcesStore[resIndex];
}

} // namespace VkToolbox
