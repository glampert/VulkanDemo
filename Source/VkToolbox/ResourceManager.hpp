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

#include <cassert>
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

    // Not copyable.
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager & operator = (const ResourceManager &) = delete;

    // No special initialization required.
    ResourceManager() = default;

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

    // Remove the given slot and also unload the associated resource.
    bool unregisterSlot(ResourceIndex resIndex);

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

    std::vector<T> m_resourcesStore;
    //TODO
    //hash_index<> m_resourcesLookupTable;
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
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::findOrLoad(const ResourceId inResId, ResourceIndex * outResIndex)
{
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::registerSlot(const ResourceId inResId, ResourceIndex * outResIndex)
{
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::isRegistered(const ResourceId inResId) const
{
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::isLoaded(const ResourceIndex resIndex) const
{
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::reload(const ResourceIndex resIndex)
{
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::unload(const ResourceIndex resIndex)
{
    return false; //TODO
}

template<typename T>
bool ResourceManager<T>::unregisterSlot(const ResourceIndex resIndex)
{
    return false; //TODO
}

template<typename T>
void ResourceManager<T>::reloadAll(int * outNumReloaded, int * outNumFailed)
{
     //TODO
}

template<typename T>
void ResourceManager<T>::unloadAll()
{
     //TODO
}

template<typename T>
void ResourceManager<T>::unregisterAll()
{
     //TODO
}

template<typename T>
int ResourceManager<T>::getResourceCount() const
{
    //TODO
    //assert(m_resourcesStore.size() == m_resourcesLookupTable.size());
    return static_cast<int>(m_resourcesStore.size());
}

template<typename T>
const T & ResourceManager<T>::getResourceRef(const ResourceIndex resIndex) const
{
    assert(resIndex < m_resourcesStore.size());
    return m_resourcesStore[resIndex];
}

} // namespace VkToolbox
