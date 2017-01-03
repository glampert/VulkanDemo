
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/GlslShader.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: GLSL-syntax Vulkan shader program.
// ================================================================================================

#include "GlslShader.hpp"

namespace VkToolbox
{

GlslShader::GlslShader(VkDevice device, ResourceId id)
    : Resource{ device, id }
{
}

GlslShader::~GlslShader()
{
    GlslShader::shutdown();
}

GlslShader::GlslShader(GlslShader && other)
    : Resource{ other.m_device, other.m_resId }
{
    other.clear();
}

GlslShader & GlslShader::operator = (GlslShader && other)
{
    m_device = other.m_device;
    m_resId  = other.m_resId;
    other.clear();

    return *this;
}

bool GlslShader::load()
{
    //TODO
    return false;
}

void GlslShader::unload()
{
    //TODO
}

void GlslShader::shutdown()
{
    //TODO
    GlslShader::clear();
}

void GlslShader::clear()
{
    //TODO
    Resource::clear();
}

bool GlslShader::isLoaded() const
{
    //TODO
    return false;
}

bool GlslShader::isShutdown() const
{
    //TODO
    return false;
}

} // namespace VkToolbox
