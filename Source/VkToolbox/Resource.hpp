#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Resource.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Base graphics resource type for shaders, textures, render-buffers, etc.
// ================================================================================================

#include "VulkanContext.hpp"
#include "External.hpp"
#include "Hashing.hpp"

namespace VkToolbox
{

// ========================================================
// struct ResourceId:
// ========================================================

struct ResourceId final
{
    const str * name; // Pointer to string pool.
    Hash64      hash; // Hash of name string.

    bool operator == (const ResourceId & other) const { return this->hash == other.hash; }
    bool operator != (const ResourceId & other) const { return this->hash != other.hash; }

    const char * getName() const { return name->c_str(); }
    bool isNull() const { return hash.isNull(); }

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

    Resource(WeakRef<const VulkanContext> vkContext, ResourceId id);
    virtual ~Resource();

    // Load or reload the resource from file.
    virtual bool load() = 0;

    // Unload the resource data but keep the object in a valid and reloadable state.
    virtual void unload() = 0;

    // Shutdown the object - cannot be reloaded after.
    virtual void shutdown() = 0;

    // Check if loaded/shutdown:
    virtual bool isLoaded() const = 0;
    virtual bool isShutdown() const;

    // Common accessors:
    const VulkanContext & getVkContext() const;
    const ResourceId & getId() const;

protected:

    // Resets the context and resource id to null.
    virtual void clear();

    // Common resource data:
    WeakRef<const VulkanContext> m_vkContext;
    ResourceId m_resId;
};

// ========================================================

inline bool Resource::isShutdown() const
{
    return m_vkContext == nullptr;
}

inline const VulkanContext & Resource::getVkContext() const
{
    assert(m_vkContext != nullptr);
    return *m_vkContext;
}

inline const ResourceId & Resource::getId() const
{
    return m_resId;
}

// ========================================================

} // namespace VkToolbox
