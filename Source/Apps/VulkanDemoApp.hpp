#pragma once

// ================================================================================================
// File: Apps/VulkanDemoApp.hpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Vulkan demo app base class.
// ================================================================================================

#include "VkToolbox/Utils.hpp"
#include "VkToolbox/OSWindow.hpp"
#include "VkToolbox/VulkanContext.hpp"
#include "VkToolbox/StringRegistry.hpp"

// ========================================================
// class VulkanDemoApp:
// ========================================================

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

    static void initClass();
    static void shutdownClass();

    VulkanDemoApp(const StartupOptions & options);
    virtual ~VulkanDemoApp();

    virtual void onRunMainLoop();
    virtual void onFrameUpdate();
    virtual void onResizeWindow(const VkToolbox::Size2D newSize);

    VulkanDemoApp(const VulkanDemoApp &) = delete;
    VulkanDemoApp & operator = (const VulkanDemoApp &) = delete;

protected:

    VkToolbox::OSWindow          m_window;
    VkToolbox::VulkanContext     m_vkContext;
    VkToolbox::StringRegistry    m_strRegistry;

    static cfg::CVarManager    * sm_cvarManager;
    static cfg::CommandManager * sm_cmdManager;

private:

    struct AppClassFactory
    {
        const char *    appClassName;
        FactoryFunction factoryFunc;
    };

    static const AppClassFactory * findAppFactory(const char * appClassName);
    static std::vector<AppClassFactory> & factoriesList();
};

// ========================================================

#define VULKAN_DEMO_REGISTER_APP(appClassName)                                                          \
    static VulkanDemoApp * appClassName##_createInstance(const VulkanDemoApp::StartupOptions & options) \
    {                                                                                                   \
        return new appClassName{ options };                                                             \
    }                                                                                                   \
    struct AutoRegister_##appClassName                                                                  \
    {                                                                                                   \
        AutoRegister_##appClassName()                                                                   \
        {                                                                                               \
            VulkanDemoApp::registerAppClass(#appClassName, &appClassName##_createInstance);             \
        }                                                                                               \
    } appClassName##_registration

// ========================================================

extern cfg::CVar * g_startupWindowWidth;
extern cfg::CVar * g_startupWindowHeight;
extern cfg::CVar * g_startupMaximizeWindow;
extern cfg::CVar * g_smoketestRunOnly;

// ========================================================
