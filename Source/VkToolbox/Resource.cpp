
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
   
Resource::Resource(VkDevice device, ResourceId id)
    : m_device{ device }
    , m_resId{ id }
{
}

Resource::~Resource()
{
    // Just here to anchor the vtable to this file.
}

void Resource::clear()
{
    m_device = VK_NULL_HANDLE;
    m_resId  = ResourceId::getNull();
}

} // namespace VkToolbox
