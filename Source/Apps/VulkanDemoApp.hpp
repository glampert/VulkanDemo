#pragma once

// ================================================================================================
// -*- C++ -*-
// File: Apps/VulkanDemoApp.hpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Vulkan demo app base class.
// ================================================================================================

#include "VkToolbox/Utils.hpp"
#include "VkToolbox/OSWindow.hpp"
#include "VkToolbox/VulkanContext.hpp"

class VulkanDemoApp
{
public:

    struct StartupOptions
    {
        const char *      appTitle;
        VkToolbox::Size2D initialWindowSize;
        bool              openMaximizedWindow;
    };

    static std::unique_ptr<VulkanDemoApp> create(int argc, const char * argv[]);
    static std::unique_ptr<VulkanDemoApp> create(const char * appClassName, const StartupOptions & options);

    using FactoryFunction = VulkanDemoApp * (*)(const StartupOptions &);
    static void registerAppClass(const char * appClassName, FactoryFunction factoryFunc);

    VulkanDemoApp(const StartupOptions & options);
    virtual ~VulkanDemoApp() = default;
    virtual void runLoop();

    // Not copyable.
    VulkanDemoApp(const VulkanDemoApp &) = delete;
    VulkanDemoApp & operator = (const VulkanDemoApp &) = delete;

protected:

    VkToolbox::OSWindow      m_window;
    VkToolbox::VulkanContext m_vkContext;

private:

    struct AppClassFactory
    {
        const char *    appClassName;
        FactoryFunction factoryFunc;
    };

    static const AppClassFactory * findAppFactory(const char * appClassName);
    static std::vector<AppClassFactory> & getFactoriesList();
};

// ========================================================

#define VULKAN_DEMO_REGISTER_APP(appClassName)                                         \
struct AutoRegister_##appClassName                                                     \
{                                                                                      \
    AutoRegister_##appClassName()                                                      \
    {                                                                                  \
        VulkanDemoApp::registerAppClass(#appClassName, &appClassName::createInstance); \
    }                                                                                  \
} appClassName##_registration

// ========================================================
