
// ================================================================================================
// -*- C++ -*-
// File: Apps/Main.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Common main entry point for all Vulkan demos.
// ================================================================================================

#include "VulkanDemoApp.hpp"

int main(int argc, const char * argv[])
{
    VulkanDemoApp::initClass();

    auto appInstance = VulkanDemoApp::create(argc, argv);
    appInstance->runLoop();

    VulkanDemoApp::shutdownClass();
}
