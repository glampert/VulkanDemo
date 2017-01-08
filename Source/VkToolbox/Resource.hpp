#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Resource.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Base graphics resource type for shaders, textures, render-buffers, etc.
// ================================================================================================

#include <vulkan/vulkan.h>
#include "External.hpp"
#include "Hashing.hpp"
#include "Utils.hpp"

namespace VkToolbox
{

// ========================================================
// struct ResourceId:
// ========================================================

struct ResourceId final
{
    const str * name; // Reference to string pool.
    Hash64      hash; // Hash of name string.

    bool operator == (const ResourceId & other) const { return this->hash == other.hash; }
    bool operator != (const ResourceId & other) const { return this->hash != other.hash; }

    bool isNull() const { return hash.value == 0; }
    static ResourceId getNull();
};

// ========================================================
// class Resource:
// ========================================================

class Resource
{
public:

    // Not copyable.
    Resource(const Resource &) = delete;
    Resource & operator = (const Resource &) = delete;

    Resource(WeakHandle<VkDevice> device, ResourceId id);
    virtual ~Resource();

    // Load or reload the resource from file.
    virtual bool load() = 0;

    // Unload the resource data but keep the object in a valid and reloadable state.
    virtual void unload() = 0;

    // Shutdown the object - cannot be reloaded after.
    virtual void shutdown() = 0;

    // Check if loaded/shutdown:
    virtual bool isLoaded() const = 0;
    virtual bool isShutdown() const = 0;

    // Common accessors:
    WeakHandle<VkDevice> getDevice() const { return m_device; }
    ResourceId getId() const { return m_resId; }

protected:

    // Reset the device handle and id to null/invalid states.
    virtual void clear();

    WeakHandle<VkDevice> m_device;
    ResourceId           m_resId;
};

} // namespace VkToolbox
