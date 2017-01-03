
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

Texture::Texture(VkDevice device, ResourceId id)
    : Resource{ device, id }
{
}

Texture::~Texture()
{
    Texture::shutdown();
}

Texture::Texture(Texture && other)
    : Resource{ other.m_device, other.m_resId }
{
    other.clear();
}

Texture & Texture::operator = (Texture && other)
{
    m_device = other.m_device;
    m_resId  = other.m_resId;
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

bool Texture::isShutdown() const
{
    //TODO
    return false;
}

} // namespace VkToolbox
