
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Resource.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Base graphics resource type for shaders, textures, render-buffers, etc.
// ================================================================================================

#include "Resource.hpp"

namespace VkToolbox
{

// ========================================================
// struct ResourceId:
// ========================================================

ResourceId ResourceId::getNull()
{
    static const str    s_nullName;
    static const Hash64 s_nullHash;
    return { &s_nullName, s_nullHash };
}

// ========================================================
// class Resource:
// ========================================================

Resource::Resource(WeakRef<const VulkanContext> vkContext, ResourceId id)
    : m_vkContext{ vkContext }
    , m_resId{ id }
{
    assert(m_vkContext != nullptr);
}

Resource::~Resource()
{
    // Just here to anchor the vtable to this file.
}

void Resource::clear()
{
    m_vkContext = nullptr;
    m_resId = ResourceId::getNull();
}

} // namespace VkToolbox
