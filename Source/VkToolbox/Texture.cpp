
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Texture.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Texture.hpp"

namespace VkToolbox
{

Texture::Texture(WeakRef<const VulkanContext> vkContext, ResourceId id)
    : Resource{ vkContext, id }
{
}

Texture::~Texture()
{
    Texture::shutdown();
}

Texture::Texture(Texture && other)
    : Resource{ other.m_vkContext, other.m_resId }
{
    other.clear();
}

Texture & Texture::operator = (Texture && other)
{
    Texture::shutdown();

    m_vkContext = other.m_vkContext;
    m_resId     = other.m_resId;

    other.clear();
    return *this;
}

bool Texture::load()
{
    //TODO
    return false;
}

void Texture::unload()
{
    //TODO
}

void Texture::shutdown()
{
    //TODO
    Texture::clear();
}

void Texture::clear()
{
    //TODO
    Resource::clear();
}

bool Texture::isLoaded() const
{
    //TODO
    return false;
}

} // namespace VkToolbox
