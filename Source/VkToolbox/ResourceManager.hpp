#pragma once

// ================================================================================================
// File: VkToolbox/ResourceManager.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Base template for the resource registries/managers.
// ================================================================================================

#include "Texture.hpp"
#include "GlslShader.hpp"
#include "JobQueue.hpp"

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
    static constexpr ResourceIndex InvalidResIndex = ~0u;

    // Init with the VK device that will own all the resources.
    static void initialize(const VulkanContext & vkContext);
    static void shutdown();

    // Call before loading/reloading any resource (even indirectly with findOrLoad).
    // Some resources, like textures, need to create command buffers and cleanup staging
    // buffers after being loaded. Not all resource types require this and might leave unimplemented.
    static void beginResourceLoad();
    static void endResourceLoad();

    // Push an async job to load (or reload) an already registered resource.
    // It is not safe to attempt using the resource before the request has completed.
    // Call waitPendingAsyncLoadRequests() to finish all pending jobs. This must also
    // happen between beginResourceLoad/endResourceLoad.
    static void pushAsyncLoadRequest(ResourceIndex resIndex);

    // Wait all pending requests to finish loading, so you can safely call endResourceLoad().
    static void waitPendingAsyncLoadRequests();

    // Check if the given resource was already queued for async load and hasn't finished yet.
    static bool hasPendingAsyncLoadRequest(ResourceIndex resIndex);

    // Preallocate storage for a number of resources. The parameter is merely a hint.
    static void preallocate(int resourceCount);

    // Test if a slot for the resource is registered AND the resource is loaded.
    static bool findLoaded(const ResourceId & inResId, ResourceIndex * outResIndex);

    // Find existing resource. If no slot registered, one is created. If not loaded yet, load the resource.
    static bool findOrLoad(ResourceId inResId, ResourceIndex * outResIndex);

    // Only create the resource slot but don't load yet. Returns existing slot if already registered.
    static void registerSlot(ResourceId inResId, ResourceIndex * outResIndex);

    // Check if resource slot already registered (may or may not be loaded).
    static bool isRegistered(const ResourceId & inResId);

    // Check if resource already loaded (slot must be registered).
    static bool isLoaded(ResourceIndex resIndex);

    // Reload given resource at slot (slot must be registered).
    static bool reload(ResourceIndex resIndex);

    // Unload given resource at slot (slot must be registered).
    static void unload(ResourceIndex resIndex);

    // Attempt to reload all registered resources.
    static void reloadAll(int * outNumReloaded = nullptr, int * outNumFailed = nullptr);

    // Unload all resources without removing the slots - can reloaded them later.
    static void unloadAll();

    // Clear all resource slots and unloads the resources.
    static void unregisterAll();

    // Number of registered resource slots.
    static int resourceCount();

    // Deref a slot index to get the resource object. Asserts if the slot index is invalid.
    // NOTE: It is not safe to store references to the returned object
    //       since the underlaying store might be moved or reallocated!
    static const T & resourceRef(ResourceIndex resIndex);

    // Convert a resource reference back into an index.
    static ResourceIndex indexOf(const T & ref);

    // Access to the Vulkan instance that owns all the graphics resources.
    static const VulkanContext & context();

private:

    // Expand the resource store by one item and also register the new index into the LUT.
    static ResourceIndex createNewSlot(ResourceId id);

    // Internal variant of resourceRef() for load/unload mutable calls.
    static T & mutableResourceRef(ResourceIndex resIndex);

private:

    using HashIndex = hash_index<ResourceIndex, std::uint64_t>;

    static const VulkanContext *     sm_vkContext;
    static std::vector<T>            sm_resourcesStore;
    static HashIndex                 sm_resourcesLookupTable;
    static bool                      sm_inResourceLoadState;
    static JobQueue                  sm_asyncLoadRequests;
    static std::vector<std::uint8_t> sm_pendingAsyncRequestFlags;
};

// ========================================================
// Standard Resource Managers:
// ========================================================

extern template class ResourceManager<GlslShader>;
using GlslShaderManager = ResourceManager<GlslShader>;

extern template class ResourceManager<Texture>;
using TextureManager = ResourceManager<Texture>;

// ========================================================

} // namespace VkToolbox
