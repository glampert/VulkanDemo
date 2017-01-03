#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/GlslShader.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: GLSL-syntax Vulkan shader program.
// ================================================================================================

#include "Resource.hpp"

namespace VkToolbox
{

class GlslShader final
    : public Resource
{
public:

    GlslShader(VkDevice device, ResourceId id);
    ~GlslShader();

    // Movable.
    GlslShader(GlslShader && other);
    GlslShader & operator = (GlslShader && other);

    // Resource overrides:
    bool load() override;
    void unload() override;
    void shutdown() override;
    bool isLoaded() const override;
    bool isShutdown() const override;

private:
    
    void clear() override;
};

} // namespace VkToolbox
