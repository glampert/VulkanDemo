
// ================================================================================================
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

ResourceId ResourceId::null()
{
    static const str s_nullName;
    return { &s_nullName, Hash64{} };
}

// ========================================================
// class Resource:
// ========================================================

Resource::~Resource()
{
    // Just here to anchor the vtable to this file.
}

void Resource::clear()
{
    m_vkContext = nullptr;
    m_resId     = ResourceId::null();
}

} // namespace VkToolbox
