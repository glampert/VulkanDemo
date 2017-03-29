
// ================================================================================================
// -*- C++ -*-
// File: Apps/ColoredCube/VkAppColoredCube.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Draws a colored rotating cube using vertex+index buffers.
// ================================================================================================

#include "Apps/VulkanDemoApp.hpp"

class VkAppColoredCube final
    : public VulkanDemoApp
{
public:
    static VulkanDemoApp * createInstance(const StartupOptions & options)
    {
        return new VkAppColoredCube{ options };
    }

    // TODO
    using VulkanDemoApp::VulkanDemoApp;
};

VULKAN_DEMO_REGISTER_APP(VkAppColoredCube);
