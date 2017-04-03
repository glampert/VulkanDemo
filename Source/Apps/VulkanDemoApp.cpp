
// ================================================================================================
// File: Apps/VulkanDemoApp.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Vulkan demo app base class.
// ================================================================================================

#include "VulkanDemoApp.hpp"

// ========================================================
// Global CVars:

cfg::CVar * g_startupWindowWidth    = nullptr;
cfg::CVar * g_startupWindowHeight   = nullptr;
cfg::CVar * g_startupMaximizeWindow = nullptr;
cfg::CVar * g_smoketestRunOnly      = nullptr;

// ========================================================
// Static vars/methods:

cfg::CVarManager    * VulkanDemoApp::sm_cvarManager  = nullptr;
cfg::CommandManager * VulkanDemoApp::sm_cmdManager   = nullptr;
float                 VulkanDemoApp::sm_deltaTimeSec = 0.0f;
std::int64_t          VulkanDemoApp::sm_deltaTimeMs  = 0;
TimePoint             VulkanDemoApp::sm_startTime    = {};

std::unique_ptr<VulkanDemoApp> VulkanDemoApp::create(const int argc, const char * argv[])
{
    #if DEBUG
    for (int i = 0; i < argc; ++i)
    {
        VkToolbox::Log::debugF("argv[%i] = \"%s\"", i, argv[i]);
    }
    #endif // DEBUG

    const char * appClassName;
    if (argc < 2)
    {
        VkToolbox::Log::warningF("Not enough arguments! Use: VulkanDemo.exe -AppClassName [options]");
        VkToolbox::Log::warningF("Running default demo application...");
        appClassName = "VkAppHelloTriangle";
    }
    else
    {
        appClassName = argv[1] + 1;
        sm_cmdManager->execStartupCommandLine(argc - 1, argv + 1); // Skip the first one
    }

    StartupOptions options           = {};
    options.appTitle                 = appClassName;
    options.initialWindowSize.width  = static_cast<int>(g_startupWindowWidth->getIntValue());
    options.initialWindowSize.height = static_cast<int>(g_startupWindowHeight->getIntValue());
    options.openMaximizedWindow      = g_startupMaximizeWindow->getBoolValue();

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
    factoriesList().push_back({ appClassName, factoryFunc });
}

const VulkanDemoApp::AppClassFactory * VulkanDemoApp::findAppFactory(const char * const appClassName)
{
    assert(appClassName != nullptr);

    const auto & factories = factoriesList();
    for (const auto & f : factories)
    {
        if (std::strcmp(f.appClassName, appClassName) == 0)
        {
            return &f;
        }
    }
    return nullptr;
}

std::vector<VulkanDemoApp::AppClassFactory> & VulkanDemoApp::factoriesList()
{
    static std::vector<AppClassFactory> s_factories;
    return s_factories;
}

void VulkanDemoApp::initClass()
{
    sm_startTime   = std::chrono::high_resolution_clock::now();
    sm_cvarManager = cfg::CVarManager::createInstance();
    sm_cmdManager  = cfg::CommandManager::createInstance(0, sm_cvarManager);

    cfg::setErrorCallback(
        [](const char * const message, void *)
        {
            VkToolbox::Log::errorF("%s", message);
        }, 
    nullptr);

    constexpr auto cvarFlags = (cfg::CVar::Flags::InitOnly | cfg::CVar::Flags::Persistent);
    g_startupWindowWidth     = sm_cvarManager->registerCVarInt("windowWidth", "Startup window width", cvarFlags, 1024, 0, 0);
    g_startupWindowHeight    = sm_cvarManager->registerCVarInt("windowHeight", "Startup window height", cvarFlags, 768, 0, 0);
    g_startupMaximizeWindow  = sm_cvarManager->registerCVarBool("maximizeWindow", "Open window maximized", cvarFlags, false);
    g_smoketestRunOnly       = sm_cvarManager->registerCVarBool("smoketest", "Run a smoke test frame and quit", cvarFlags, false);

    VkToolbox::VulkanContext::initClass();
}

void VulkanDemoApp::shutdownClass()
{
    VkToolbox::VulkanContext::shutdownClass();

    g_startupWindowWidth    = nullptr;
    g_startupWindowHeight   = nullptr;
    g_startupMaximizeWindow = nullptr;
    g_smoketestRunOnly      = nullptr;

    if (sm_cmdManager != nullptr)
    {
        cfg::CommandManager::destroyInstance(sm_cmdManager);
        sm_cmdManager = nullptr;
    }
    if (sm_cvarManager != nullptr)
    {
        cfg::CVarManager::destroyInstance(sm_cvarManager);
        sm_cvarManager = nullptr;
    }
}

// ========================================================
// Instance methods:

VulkanDemoApp::VulkanDemoApp(const StartupOptions & options)
    : m_window{ { options.initialWindowSize.width,
                  options.initialWindowSize.height,
                  options.openMaximizedWindow,
                  options.appTitle } }
    , m_vkContext{ m_window, options.initialWindowSize }
{
    m_window.onResize = [this](const VkToolbox::Size2D newSize)
    {
        onResizeWindow(newSize);
    };

    static bool s_smoketestRun = g_smoketestRunOnly->getBoolValue();
    m_window.onRedraw = [this]()
    {
        const auto t0 = timeMilliseconds();

        onFrameUpdate();
        if (s_smoketestRun)
        {
            m_window.setStopEventLoop(true);
            VkToolbox::Log::debugF("Smoke test frame completed - quitting.");
        }

        const auto t1 = timeMilliseconds();

        sm_deltaTimeMs  = t1 - t0;
        sm_deltaTimeSec = sm_deltaTimeMs * 0.001f;
    };
}

VulkanDemoApp::~VulkanDemoApp()
{
    // Nothing atm, just anchors the vtable to this file.
}

void VulkanDemoApp::onRunMainLoop()
{
    m_window.runEventLoop();
    m_vkContext.waitGpuIdle();
}

void VulkanDemoApp::onFrameUpdate()
{
    // Implemented by the derived class.
}

void VulkanDemoApp::onResizeWindow(const VkToolbox::Size2D newSize)
{
    VkToolbox::Log::debugF("Resizing window to {w=%i, h=%i}", newSize.width, newSize.height);
    // Not handled here.
}

// ========================================================
