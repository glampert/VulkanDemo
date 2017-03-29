
// ================================================================================================
// -*- C++ -*-
// File: Apps/VulkanDemoApp.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Vulkan demo app base class.
// ================================================================================================

#include "VulkanDemoApp.hpp"

// ========================================================

std::unique_ptr<VulkanDemoApp> VulkanDemoApp::create(const int argc, const char * argv[])
{
    if (argc < 2)
    {
        VkToolbox::Log::fatalF("Not enough arguments! Use: VulkanDemo.exe -AppClassName [options]");
    }

    #if DEBUG
    for (int i = 0; i < argc; ++i)
    {
        VkToolbox::Log::debugF("argv[%i] = \"%s\"", i, argv[i]);
    }
    #endif // DEBUG

    const char * appClassName = argv[1] + 1;
    const StartupOptions options = { appClassName, { 1024, 768 }, false };

    return create(appClassName, options);
}

std::unique_ptr<VulkanDemoApp> VulkanDemoApp::create(const char * const appClassName, const StartupOptions & options)
{
    const auto * f = findAppFactory(appClassName);
    if (f == nullptr)
    {
        VkToolbox::Log::fatalF("VulkanDemoApp '%s' not found!", appClassName);
    }

    return std::unique_ptr<VulkanDemoApp>{ f->factoryFunc(options) };
}

void VulkanDemoApp::registerAppClass(const char * appClassName, FactoryFunction factoryFunc)
{
    assert(!findAppFactory(appClassName));
    getFactoriesList().push_back({ appClassName, factoryFunc });
}

const VulkanDemoApp::AppClassFactory * VulkanDemoApp::findAppFactory(const char * const appClassName)
{
    assert(appClassName != nullptr);
    for (const auto & f : getFactoriesList())
    {
        if (std::strcmp(f.appClassName, appClassName) == 0)
        {
            return &f;
        }
    }
    return nullptr;
}

std::vector<VulkanDemoApp::AppClassFactory> & VulkanDemoApp::getFactoriesList()
{
    static std::vector<AppClassFactory> s_factories;
    return s_factories;
}

VulkanDemoApp::VulkanDemoApp(const StartupOptions & options)
    : m_window{ { options.initialWindowSize.width,
                  options.initialWindowSize.height,
                  options.openMaximizedWindow,
                  options.appTitle } }
    , m_vkContext{ m_window, options.initialWindowSize }
{
}

void VulkanDemoApp::runLoop()
{
    m_window.runEventLoop();
    m_vkContext.waitGpuIdle();
}

// ========================================================
