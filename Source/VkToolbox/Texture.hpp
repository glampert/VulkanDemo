#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Texture.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Resource.hpp"

namespace VkToolbox
{

class Texture final
    : public Resource
{
public:

    Texture(VkDevice device, ResourceId id);
    ~Texture();

    // Movable.
    Texture(Texture && other);
    Texture & operator = (Texture && other);

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
