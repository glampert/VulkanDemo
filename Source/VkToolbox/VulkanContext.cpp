
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/VulkanContext.cpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: Main Vulkan context and device class.
// ================================================================================================

// To enable vkCreateWin32SurfaceKHR extensions (note: Windows-specific!)
// Defined here before the include so vulkan.h exposes that just in this file.
#if (!defined(VK_USE_PLATFORM_WIN32_KHR) && (defined(WIN32) || defined(WIN64)))
    #define VK_USE_PLATFORM_WIN32_KHR 1
#endif // !VK_USE_PLATFORM_WIN32_KHR && Windows

#include "VulkanContext.hpp"
#include "OSWindow.hpp"

// For Win32SurfaceKH extension stuff.
#if defined(WIN32) || defined(WIN64)
    #define NOMINMAX 1
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
#endif // WIN32 || WIN64

namespace VkToolbox
{

// ========================================================
// VulkanContext statics:
// ========================================================

const char *  VulkanContext::sm_appName          = "VulkanApp";
std::uint32_t VulkanContext::sm_appVersion       = 1;
std::uint32_t VulkanContext::sm_multiSampleCount = VK_SAMPLE_COUNT_1_BIT;
VkFormat      VulkanContext::sm_depthFormat      = VK_FORMAT_D24_UNORM_S8_UINT;

#if DEBUG
VulkanContext::ValidationMode VulkanContext::sm_validationMode = VulkanContext::Debug;
#else // !DEBUG
VulkanContext::ValidationMode VulkanContext::sm_validationMode = VulkanContext::Release;
#endif // DEBUG

// ========================================================
// class VulkanContext:
// ========================================================

VulkanContext::VulkanContext(WeakRef<const OSWindow> window, Size2D fbSize, WeakRef<const VkAllocationCallbacks> allocCbs)
    : m_instance{ VK_NULL_HANDLE }
    , m_renderWindow{ window }
    , m_allocationCallbacks{ allocCbs }
    , m_renderSurface{ VK_NULL_HANDLE }
    , m_renderSurfaceFormat{ VK_FORMAT_UNDEFINED }
    , m_framebufferSize{ fbSize }
    , m_swapChainCount{ 0 }
    , m_swapChain{ VK_NULL_HANDLE }
    , m_renderPass{ VK_NULL_HANDLE }
    , m_cmdPool{ VK_NULL_HANDLE }
    , m_cmdBuffer{ VK_NULL_HANDLE }
    , m_cmdBufferRecordingState{ false }
    , m_cmdBufferExecuteState{ false }
    , m_device{ VK_NULL_HANDLE }
    , m_gpuPhysDevice{ VK_NULL_HANDLE }
    , m_gpuQueueFamilyCount{ 0 }
    , m_gpuPresentQueueFamilyIndex{ -1 }
    , m_gpuGraphicsQueueFamilyIndex{ -1 }
    , m_gpuPresentQueue{ VK_NULL_HANDLE }
    , m_gpuGraphicsQueue{ VK_NULL_HANDLE }
    , m_gpuFeatures{}
    , m_gpuProperties{}
    , m_gpuMemoryProperties{}
{
    Log::debugF("Initializing Vulkan API context...");

    initInstanceLayerProperties();
    initInstance();
    initEnumerateGpus();
    initSwapChainExtensions();
    initDevice();
    initSwapChain();
    initCommandPoolAndBuffer();
    initDepthBuffer();
    initRenderPass();
    initFramebuffers();

    Log::debugF("VulkanContext initialized successfully!");
}

VulkanContext::~VulkanContext()
{
    Log::debugF("Destroying Vulkan API context...");

    // The depth/frame-buffer owns its images and views.
    destroyFramebuffers();
    destroyDepthBuffer();

    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_renderPass, m_allocationCallbacks);
    }
    if (m_cmdBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_cmdBuffer);
    }
    if (m_cmdPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_cmdPool, m_allocationCallbacks);
    }
    if (m_swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapChain, m_allocationCallbacks);
    }
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, m_allocationCallbacks);
    }
    if (m_renderSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_renderSurface, m_allocationCallbacks);
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, m_allocationCallbacks);
    }
}

void VulkanContext::initInstanceLayerProperties()
{
    std::vector<VkLayerProperties> layerProperties;
    std::uint32_t instanceLayerCount;
    VkResult res;

    // 
    // It's possible, though very rare, that the number of
    // instance layers could change. For example, installing something
    // could include new layers that the loader would pick up
    // between the initial query for the count and the
    // request for VkLayerProperties. The loader indicates that
    // by returning a VK_INCOMPLETE status and will update the
    // the count parameter.
    // The count parameter will be updated with the number of
    // entries loaded into the data pointer - in case the number
    // of layers went down or is smaller than the size given.
    //
    do {
        instanceLayerCount = 0;
        res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        if (res != VK_SUCCESS || instanceLayerCount == 0)
        {
            return;
        }

        layerProperties.resize(instanceLayerCount);
        res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProperties.data());
    } while (res == VK_INCOMPLETE);

    // Now gather the extension list for each instance layer:
    for (std::uint32_t l = 0; l < instanceLayerCount; ++l)
    {
        LayerProperties myLayerProps;
        myLayerProps.properties = layerProperties[l];

        initInstanceExtensionProperties(myLayerProps);
        m_instanceLayerProperties.emplace_back(std::move(myLayerProps));
    }
}

void VulkanContext::initInstanceExtensionProperties(LayerProperties & layerProps)
{
    VkResult res;
    do {
        const char * const layerName = layerProps.properties.layerName;
        std::uint32_t instanceExtensionCount = 0;

        res = vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, nullptr);
        if (res != VK_SUCCESS || instanceExtensionCount == 0)
        {
            return;
        }

        layerProps.extensions.resize(instanceExtensionCount);
        auto instanceExtensions = layerProps.extensions.data();

        res = vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, instanceExtensions);
    } while (res == VK_INCOMPLETE);
}

void VulkanContext::initInstance()
{
    std::uint32_t instanceLayerCount;
    const char * const * instanceLayerNames;

    const char * const instanceLayerNamesDebug[] = {
        "VK_LAYER_LUNARG_standard_validation"
    };
    const char * const instanceExtensionNames[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };
    const auto instanceExtensionCount = static_cast<std::uint32_t>(arrayLength(instanceExtensionNames));

    if (isDebug())
    {
        Log::debugF("Creating VK Instance with debug validation...");
        instanceLayerNames = instanceLayerNamesDebug;
        instanceLayerCount = static_cast<std::uint32_t>(arrayLength(instanceLayerNamesDebug));
    }
    else
    {
        Log::debugF("Creating VK Instance without validation (Release)...");
        instanceLayerNames = nullptr;
        instanceLayerCount = 0;
    }

    VkApplicationInfo appInfo        = {};
    appInfo.sType                    = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext                    = nullptr;
    appInfo.pApplicationName         = sm_appName;
    appInfo.applicationVersion       = sm_appVersion;
    appInfo.pEngineName              = sm_appName;
    appInfo.engineVersion            = sm_appVersion;
    appInfo.apiVersion               = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo    = {};
    instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pNext                   = nullptr;
    instInfo.flags                   = 0;
    instInfo.pApplicationInfo        = &appInfo;
    instInfo.enabledLayerCount       = instanceLayerCount;
    instInfo.ppEnabledLayerNames     = instanceLayerNames;
    instInfo.enabledExtensionCount   = instanceExtensionCount;
    instInfo.ppEnabledExtensionNames = instanceExtensionNames;

    VKTB_CHECK(vkCreateInstance(&instInfo, m_allocationCallbacks, &m_instance));
    assert(m_instance != VK_NULL_HANDLE);

    Log::debugF("VK Instance created!");
}

void VulkanContext::initDevice()
{
    const char * const deviceExtensionNames[] = {
        "VK_KHR_swapchain"
    };
    const auto deviceExtensionCount = static_cast<std::uint32_t>(arrayLength(deviceExtensionNames));

    const float             queuePriorities[1] = { 0.0f }; // Dummy priorities - don't care atm
    VkDeviceQueueCreateInfo queueCreateInfo    = {};
    VkDeviceCreateInfo      deviceCreateInfo   = {};

    queueCreateInfo.sType                      = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext                      = nullptr;
    queueCreateInfo.queueCount                 = 1;
    queueCreateInfo.pQueuePriorities           = queuePriorities;
    queueCreateInfo.queueFamilyIndex           = m_gpuGraphicsQueueFamilyIndex;

    deviceCreateInfo.sType                     = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext                     = nullptr;
    deviceCreateInfo.queueCreateInfoCount      = 1;
    deviceCreateInfo.pQueueCreateInfos         = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount     = deviceExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames   = deviceExtensionNames;
    deviceCreateInfo.pEnabledFeatures          = nullptr;

    VKTB_CHECK(vkCreateDevice(m_gpuPhysDevice, &deviceCreateInfo, m_allocationCallbacks, &m_device));
    assert(m_device != VK_NULL_HANDLE);

    Log::debugF("VK Device created for GPU 0!");

    // Get the GPU queue handles:
    vkGetDeviceQueue(m_device, m_gpuGraphicsQueueFamilyIndex, 0, &m_gpuGraphicsQueue);
    assert(m_gpuGraphicsQueue != VK_NULL_HANDLE);

    if (m_gpuGraphicsQueueFamilyIndex == m_gpuPresentQueueFamilyIndex)
    {
        m_gpuPresentQueue = m_gpuGraphicsQueue;
        Log::debugF("Graphics and present queues are the same.");
    }
    else
    {
        vkGetDeviceQueue(m_device, m_gpuPresentQueueFamilyIndex, 0, &m_gpuPresentQueue);
        assert(m_gpuPresentQueue != VK_NULL_HANDLE);
    }
}

void VulkanContext::initEnumerateGpus()
{
    std::uint32_t gpuCount = 0;
    std::uint32_t queueFamilyCount = 0;
    std::vector<VkPhysicalDevice> gpus;

    VKTB_CHECK(vkEnumeratePhysicalDevices(m_instance, &gpuCount, nullptr));
    assert(gpuCount >= 1);

    gpus.resize(gpuCount);

    VKTB_CHECK(vkEnumeratePhysicalDevices(m_instance, &gpuCount, gpus.data()));
    assert(gpuCount >= 1);

    // For now we only care about GPU 0 - no support for multi-GPU systems.

    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queueFamilyCount, nullptr);
    assert(queueFamilyCount >= 1);

    m_gpuQueueProperties.resize(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(gpus[0], &queueFamilyCount, m_gpuQueueProperties.data());
    assert(queueFamilyCount >= 1);

    // This is as good a place as any to do this:
    vkGetPhysicalDeviceFeatures(gpus[0], &m_gpuFeatures);
    vkGetPhysicalDeviceProperties(gpus[0], &m_gpuProperties);
    vkGetPhysicalDeviceMemoryProperties(gpus[0], &m_gpuMemoryProperties);

    // Minimal debug printing:
    Log::debugF("Found %i physical GPUs", gpuCount);
    Log::debugF("GPU 0 has %i queues", queueFamilyCount);
    Log::debugF("GPU 0 name: %s", m_gpuProperties.deviceName);

    m_gpuPhysDevice = gpus[0];
    m_gpuQueueFamilyCount = queueFamilyCount;
}

void VulkanContext::initSwapChainExtensions()
{
    assert(m_renderWindow != nullptr);
    assert(m_gpuQueueFamilyCount >= 1);

    // Construct the surface description:
    #if defined(WIN32) || defined(WIN64)
    VkWin32SurfaceCreateInfoKHR surfCreateInfo = {};
    surfCreateInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfCreateInfo.pNext     = nullptr;
    surfCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(m_renderWindow->getInstanceHandle());
    surfCreateInfo.hwnd      = reinterpret_cast<HWND>(m_renderWindow->getWindowHandle());
    VKTB_CHECK(vkCreateWin32SurfaceKHR(m_instance, &surfCreateInfo, m_allocationCallbacks, &m_renderSurface));
    assert(m_renderSurface != VK_NULL_HANDLE);
    #endif // WIN32 || WIN64

    // Iterate over each queue to learn whether it supports presenting:
    std::vector<VkBool32> queuesSupportingPresent(m_gpuQueueFamilyCount, VK_FALSE);
    for (std::uint32_t q = 0; q < m_gpuQueueFamilyCount; ++q)
    {
        VKTB_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_gpuPhysDevice, q, 
                        m_renderSurface, &queuesSupportingPresent[q]));
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both:
    for (std::uint32_t q = 0; q < m_gpuQueueFamilyCount; ++q)
    {
        if ((m_gpuQueueProperties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (m_gpuGraphicsQueueFamilyIndex == -1)
            {
                m_gpuGraphicsQueueFamilyIndex = static_cast<std::int32_t>(q);
            }

            if (queuesSupportingPresent[q] == VK_TRUE)
            {
                m_gpuPresentQueueFamilyIndex  = static_cast<std::int32_t>(q);
                m_gpuGraphicsQueueFamilyIndex = static_cast<std::int32_t>(q);
                break;
            }
        }
    }

    // If didn't find a queue that supports both graphics and present,
    // then find a separate present queue.
    if (m_gpuPresentQueueFamilyIndex == -1)
    {
        for (std::uint32_t q = 0; q < m_gpuQueueFamilyCount; ++q)
        {
            if (queuesSupportingPresent[q] == VK_TRUE)
            {
                m_gpuPresentQueueFamilyIndex = static_cast<std::int32_t>(q);
                break;
            }
        }
    }

    // Error if could not find queues that support graphics and present.
    if (m_gpuPresentQueueFamilyIndex == -1 || m_gpuGraphicsQueueFamilyIndex == -1)
    {
        Log::fatalF("Could not find a VK queue for both graphics and present!");
    }

    // Get the list of VkFormats that are supported:
    std::uint32_t formatCount = 0;
    VKTB_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpuPhysDevice, m_renderSurface, &formatCount, nullptr));
    assert(formatCount >= 1);

    std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
    VKTB_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpuPhysDevice, m_renderSurface, &formatCount, surfFormats.data()));
    assert(formatCount >= 1);

    if (isDebug())
    {
        Log::debugF("GPU 0 Present Queue family index:  %i", m_gpuPresentQueueFamilyIndex);
        Log::debugF("GPU 0 Graphics Queue family index: %i", m_gpuGraphicsQueueFamilyIndex);

        Log::debugF("------ VK render surface formats supported ------");
        for (std::uint32_t f = 0; f < formatCount; ++f)
        {
            Log::debugF("[%u]: Fmt=%s, ColorSpace=%i", f, vkFormatToString(surfFormats[f].format),
                        static_cast<int>(surfFormats[f].colorSpace));
        }
        Log::debugF("-------------------------------------------------");
    }

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format. Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        m_renderSurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        m_renderSurfaceFormat = surfFormats[0].format;
    }

    Log::debugF("RenderSurfaceFormat=%s", vkFormatToString(m_renderSurfaceFormat));
}

void VulkanContext::initSwapChain()
{
    VkSurfaceCapabilitiesKHR surfCapabilities;
    VKTB_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_gpuPhysDevice, m_renderSurface, &surfCapabilities));

    std::uint32_t presentModeCount = 0; // Get the count:
    VKTB_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_gpuPhysDevice, m_renderSurface, &presentModeCount, nullptr));
    assert(presentModeCount >= 1);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount, VkPresentModeKHR(0));
    VKTB_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_gpuPhysDevice, m_renderSurface, &presentModeCount, presentModes.data()));
    assert(presentModeCount >= 1);

    VkExtent2D swapChainExtent;

    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF)
    {
        // If the surface size is undefined, size is set to the
        // the window size, but clamped to min/max extents supported.
        swapChainExtent.width  = m_framebufferSize.width;
        swapChainExtent.height = m_framebufferSize.height;

        clamp(&swapChainExtent.width,  surfCapabilities.minImageExtent.width,  surfCapabilities.maxImageExtent.width);
        clamp(&swapChainExtent.height, surfCapabilities.minImageExtent.height, surfCapabilities.maxImageExtent.height);
    }
    else
    {
        // If the surface size is defined, the swap chain size must match.
        swapChainExtent = surfCapabilities.currentExtent;
    }

    // If mailbox mode is available, use it, as is the lowest-latency non-
    // tearing mode. If not, try IMMEDIATE which will usually be available,
    // and is the fastest (though it tears). If not, fall back to FIFO which is
    // always available.
    VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (std::uint32_t i = 0; i < presentModeCount; ++i)
    {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if ((swapChainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
            (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
        {
            swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    const char * const presentModeNames[] = {
        "VK_PRESENT_MODE_IMMEDIATE_KHR",
        "VK_PRESENT_MODE_MAILBOX_KHR",
        "VK_PRESENT_MODE_FIFO_KHR",
        "VK_PRESENT_MODE_FIFO_RELAXED_KHR"
    };
    Log::debugF("KHRPresentMode=%s", presentModeNames[std::uint32_t(swapChainPresentMode)]);

    // Determine the number of VkImage's to use in the swap chain.
    // We need to acquire only 1 presentable image at a time.
    // Asking for minImageCount images ensures that we can acquire
    // 1 presentable image as long as we present it before attempting
    // to acquire another.
    const std::uint32_t desiredNumberOfSwapChainImages = surfCapabilities.minImageCount;

    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        preTransform = surfCapabilities.currentTransform;
    }

    const VkFlags imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.pNext                    = nullptr;
    swapChainCreateInfo.surface                  = m_renderSurface;
    swapChainCreateInfo.minImageCount            = desiredNumberOfSwapChainImages;
    swapChainCreateInfo.imageFormat              = m_renderSurfaceFormat;
    swapChainCreateInfo.imageExtent.width        = swapChainExtent.width;
    swapChainCreateInfo.imageExtent.height       = swapChainExtent.height;
    swapChainCreateInfo.preTransform             = preTransform;
    swapChainCreateInfo.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.imageArrayLayers         = 1;
    swapChainCreateInfo.presentMode              = swapChainPresentMode;
    swapChainCreateInfo.oldSwapchain             = VK_NULL_HANDLE;
    swapChainCreateInfo.clipped                  = true;
    swapChainCreateInfo.imageColorSpace          = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapChainCreateInfo.imageUsage               = imageUsage;
    swapChainCreateInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount    = 0;
    swapChainCreateInfo.pQueueFamilyIndices      = nullptr;

    const std::uint32_t queueFamilyIndices[] = {
        static_cast<std::uint32_t>(m_gpuGraphicsQueueFamilyIndex),
        static_cast<std::uint32_t>(m_gpuPresentQueueFamilyIndex)
    };

    if (m_gpuGraphicsQueueFamilyIndex != m_gpuPresentQueueFamilyIndex)
    {
        // If the graphics and present queues are from different queue families,
        // we either have to explicitly transfer ownership of images between the
        // queues, or we have to create the swap-chain with imageSharingMode
        // as VK_SHARING_MODE_CONCURRENT
        swapChainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }

    VKTB_CHECK(vkCreateSwapchainKHR(m_device, &swapChainCreateInfo, m_allocationCallbacks, &m_swapChain));
    assert(m_swapChain != VK_NULL_HANDLE);

    VKTB_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_swapChainCount, nullptr));
    assert(m_swapChainCount >= 1);

    std::vector<VkImage> swapChainImages(m_swapChainCount, VK_NULL_HANDLE);
    VKTB_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_swapChainCount, swapChainImages.data()));
    assert(m_swapChainCount >= 1);

    m_swapChainBuffers.reserve(m_swapChainCount);
    for (std::uint32_t i = 0; i < m_swapChainCount; ++i)
    {
        VkImageViewCreateInfo imgViewCreateInfo           = {};
        imgViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgViewCreateInfo.pNext                           = nullptr;
        imgViewCreateInfo.format                          = m_renderSurfaceFormat;
        imgViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
        imgViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
        imgViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
        imgViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
        imgViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imgViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imgViewCreateInfo.subresourceRange.levelCount     = 1;
        imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imgViewCreateInfo.subresourceRange.layerCount     = 1;
        imgViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imgViewCreateInfo.flags                           = 0;
        imgViewCreateInfo.image                           = swapChainImages[i];

        SwapChainBuffer scBuffer = {};
        scBuffer.image = swapChainImages[i];
        VKTB_CHECK(vkCreateImageView(m_device, &imgViewCreateInfo, m_allocationCallbacks, &scBuffer.view));
        m_swapChainBuffers.push_back(scBuffer);
    }

    Log::debugF("Swap-chain created with %u image buffers.", m_swapChainCount);
}

void VulkanContext::initDepthBuffer()
{
    VkImageCreateInfo imageCreateInfo = {};

    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(m_gpuPhysDevice, sm_depthFormat, &props);

    if (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    }
    else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }
    else
    {
        // Try other depth formats?
        Log::fatalF("Depth format %s not supported!", vkFormatToString(sm_depthFormat));
    }

    Log::debugF("DepthBufferFormat=%s", vkFormatToString(sm_depthFormat));

    imageCreateInfo.sType                    = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                    = nullptr;
    imageCreateInfo.imageType                = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                   = sm_depthFormat;
    imageCreateInfo.extent.width             = m_framebufferSize.width;
    imageCreateInfo.extent.height            = m_framebufferSize.height;
    imageCreateInfo.extent.depth             = 1;
    imageCreateInfo.mipLevels                = 1;
    imageCreateInfo.arrayLayers              = 1;
    imageCreateInfo.samples                  = static_cast<VkSampleCountFlagBits>(sm_multiSampleCount);
    imageCreateInfo.initialLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.queueFamilyIndexCount    = 0;
    imageCreateInfo.pQueueFamilyIndices      = nullptr;
    imageCreateInfo.sharingMode              = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.usage                    = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.flags                    = 0;

    VkMemoryRequirements memReqs             = {};
    VkMemoryAllocateInfo memAllocInfo        = {};
    memAllocInfo.sType                       = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkImageViewCreateInfo viewInfo           = {};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext                           = nullptr;
    viewInfo.image                           = VK_NULL_HANDLE;
    viewInfo.format                          = sm_depthFormat;
    viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.flags                           = 0;

    if (sm_depthFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
        sm_depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
        sm_depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VKTB_CHECK(vkCreateImage(m_device, &imageCreateInfo, m_allocationCallbacks, &m_depthBuffer.image));
    assert(m_depthBuffer.image != VK_NULL_HANDLE);

    vkGetImageMemoryRequirements(m_device, m_depthBuffer.image, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;

    // Use the memory properties to determine the type of memory required:
    memAllocInfo.memoryTypeIndex = memoryTypeFromProperties(memReqs.memoryTypeBits, /* requirementsMask = */ 0);
    assert(memAllocInfo.memoryTypeIndex < UINT32_MAX);

    // Allocate the memory:
    VKTB_CHECK(vkAllocateMemory(m_device, &memAllocInfo, m_allocationCallbacks, &m_depthBuffer.memory));
    assert(m_depthBuffer.memory != VK_NULL_HANDLE);

    // Bind memory:
    VKTB_CHECK(vkBindImageMemory(m_device, m_depthBuffer.image, m_depthBuffer.memory, 0));

    // Set the image layout to depth stencil optimal:
    setImageLayout(m_depthBuffer.image, viewInfo.subresourceRange.aspectMask,
                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // And finally create the image view.
    viewInfo.image = m_depthBuffer.image;
    VKTB_CHECK(vkCreateImageView(m_device, &viewInfo, m_allocationCallbacks, &m_depthBuffer.view));
    assert(m_depthBuffer.view != VK_NULL_HANDLE);

    Log::debugF("Depth buffer initialized.");
}

void VulkanContext::destroyDepthBuffer()
{
    if (m_depthBuffer.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_device, m_depthBuffer.view, m_allocationCallbacks);
        m_depthBuffer.view = VK_NULL_HANDLE;
    }
    if (m_depthBuffer.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(m_device, m_depthBuffer.image, m_allocationCallbacks);
        m_depthBuffer.image = VK_NULL_HANDLE;
    }
    if (m_depthBuffer.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device, m_depthBuffer.memory, m_allocationCallbacks);
        m_depthBuffer.memory = VK_NULL_HANDLE;
    }
}

void VulkanContext::initFramebuffers()
{
    assert(m_depthBuffer.view != VK_NULL_HANDLE); // Depth buffer created first,
    assert(m_renderPass       != VK_NULL_HANDLE); // and render pass also needed

    VkImageView attachments[2];
    attachments[1] = m_depthBuffer.view;

    VkFramebufferCreateInfo fbCreateInfo = {};
    fbCreateInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCreateInfo.pNext                   = nullptr;
    fbCreateInfo.renderPass              = m_renderPass;
    fbCreateInfo.attachmentCount         = static_cast<std::uint32_t>(arrayLength(attachments)); // Include the depth buffer
    fbCreateInfo.pAttachments            = attachments;
    fbCreateInfo.width                   = m_framebufferSize.width;
    fbCreateInfo.height                  = m_framebufferSize.height;
    fbCreateInfo.layers                  = 1;

    for (std::uint32_t i = 0; i < m_swapChainCount; ++i)
    {
        attachments[0] = m_swapChainBuffers[i].view;
        VKTB_CHECK(vkCreateFramebuffer(m_device, &fbCreateInfo, m_allocationCallbacks, &m_swapChainBuffers[i].framebuffer));
        assert(m_swapChainBuffers[i].framebuffer != VK_NULL_HANDLE);
    }

    Log::debugF("Framebuffer initialized.");
}

void VulkanContext::destroyFramebuffers()
{
    // Clean up the swap-chain image views and FBs.
    // The swap-chain images themselves are owned by the swap-chain.
    for (SwapChainBuffer & scBuffer : m_swapChainBuffers)
    {
        if (scBuffer.view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_device, scBuffer.view, m_allocationCallbacks);
        }
        if (scBuffer.framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_device, scBuffer.framebuffer, m_allocationCallbacks);
        }
    }
    m_swapChainBuffers.clear();
}

void VulkanContext::initRenderPass()
{
    // Need attachments for render target (fb) and depth buffer
    VkAttachmentDescription attachments[2];

    VkAttachmentReference colorReference = {};
    colorReference.attachment            = 0;
    colorReference.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment            = 1;
    depthReference.layout                = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // fb
    attachments[0].format                = m_renderSurfaceFormat;
    attachments[0].samples               = VkSampleCountFlagBits(sm_multiSampleCount);
    attachments[0].loadOp                = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp               = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout         = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].flags                 = 0;

    // depth
    attachments[1].format                = sm_depthFormat;
    attachments[1].samples               = VkSampleCountFlagBits(sm_multiSampleCount);
    attachments[1].loadOp                = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp               = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].stencilStoreOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags                 = 0;

    VkSubpassDescription subpass         = {};
    subpass.pipelineBindPoint            = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags                        = 0;
    subpass.inputAttachmentCount         = 0;
    subpass.pInputAttachments            = nullptr;
    subpass.colorAttachmentCount         = 1;
    subpass.pColorAttachments            = &colorReference;
    subpass.pResolveAttachments          = nullptr;
    subpass.pDepthStencilAttachment      = &depthReference;
    subpass.preserveAttachmentCount      = 0;
    subpass.pPreserveAttachments         = nullptr;

    VkRenderPassCreateInfo rpCreateInfo  = {};
    rpCreateInfo.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCreateInfo.pNext                   = nullptr;
    rpCreateInfo.attachmentCount         = static_cast<std::uint32_t>(arrayLength(attachments));
    rpCreateInfo.pAttachments            = attachments;
    rpCreateInfo.subpassCount            = 1;
    rpCreateInfo.pSubpasses              = &subpass;
    rpCreateInfo.dependencyCount         = 0;
    rpCreateInfo.pDependencies           = nullptr;

    VKTB_CHECK(vkCreateRenderPass(m_device, &rpCreateInfo, m_allocationCallbacks, &m_renderPass));
    assert(m_renderPass != VK_NULL_HANDLE);

    Log::debugF("Default render-pass initialized.");
}

void VulkanContext::initCommandPoolAndBuffer()
{
    // Command pool:
    {
        VkCommandPoolCreateInfo cpCreateInfo = {};
        cpCreateInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpCreateInfo.pNext                   = nullptr;
        cpCreateInfo.queueFamilyIndex        = m_gpuGraphicsQueueFamilyIndex;
        cpCreateInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VKTB_CHECK(vkCreateCommandPool(m_device, &cpCreateInfo, m_allocationCallbacks, &m_cmdPool));
        assert(m_cmdPool != VK_NULL_HANDLE);
    }

    // Default command buffer:
    {
        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext                       = nullptr;
        cmdAllocInfo.commandPool                 = m_cmdPool;
        cmdAllocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount          = 1;

        VKTB_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_cmdBuffer));
        assert(m_cmdBuffer != VK_NULL_HANDLE);

        // Put the default buffer in a state ready to receive commands.
        // Must call vkEndCommandBuffer on it later on to be able to submit.
        VkCommandBufferBeginInfo cmdBufBeginInfo = {};
        cmdBufBeginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufBeginInfo.pNext                    = nullptr;
        cmdBufBeginInfo.flags                    = 0;
        cmdBufBeginInfo.pInheritanceInfo         = nullptr;

        VKTB_CHECK(vkBeginCommandBuffer(m_cmdBuffer, &cmdBufBeginInfo));
        m_cmdBufferRecordingState = true;
        m_cmdBufferExecuteState   = false;
    }

    Log::debugF("Default command pool and buffer initialized for queue %i.", m_gpuGraphicsQueueFamilyIndex);
}

std::uint32_t VulkanContext::memoryTypeFromProperties(std::uint32_t typeBits, VkFlags requirementsMask) const
{
    // Search mem types to find first index with those properties
    for (std::uint32_t i = 0; i < m_gpuMemoryProperties.memoryTypeCount; ++i)
    {
        if ((typeBits & 1) == 1)
        {
            // Type is available, does it match user properties?
            if ((m_gpuMemoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
            {
                return i;
            }
        }
        typeBits >>= 1;
    }

    // No memory types matched, return failure
    Log::errorF("Unable to find index for requested memory type %#x, with mask %#x", typeBits, requirementsMask);
    return UINT32_MAX;
}

void VulkanContext::setImageLayout(VkImage image, VkImageAspectFlags aspectMask,
                                   VkImageLayout oldImageLayout, VkImageLayout newImageLayout)
{
    assert(image != VK_NULL_HANDLE);
    assert(m_cmdBuffer != VK_NULL_HANDLE);
    assert(m_gpuGraphicsQueue != VK_NULL_HANDLE);

    VkImageMemoryBarrier imageMemBarrier            = {};
    imageMemBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemBarrier.pNext                           = nullptr;
    imageMemBarrier.srcAccessMask                   = 0;
    imageMemBarrier.dstAccessMask                   = 0;
    imageMemBarrier.oldLayout                       = oldImageLayout;
    imageMemBarrier.newLayout                       = newImageLayout;
    imageMemBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    imageMemBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    imageMemBarrier.image                           = image;
    imageMemBarrier.subresourceRange.aspectMask     = aspectMask;
    imageMemBarrier.subresourceRange.baseMipLevel   = 0;
    imageMemBarrier.subresourceRange.levelCount     = 1;
    imageMemBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemBarrier.subresourceRange.layerCount     = 1;

    if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (oldImageLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
        imageMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        imageMemBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        imageMemBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    vkCmdPipelineBarrier(
        /* commandBuffer            = */ m_cmdBuffer,
        /* srcStageMask             = */ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        /* dstStageMask             = */ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        /* dependencyFlags          = */ 0,
        /* memoryBarrierCount       = */ 0,
        /* pMemoryBarriers          = */ nullptr,
        /* bufferMemoryBarrierCount = */ 0,
        /* pBufferMemoryBarriers    = */ nullptr,
        /* imageMemoryBarrierCount  = */ 1,
        /* pImageBarriers           = */ &imageMemBarrier);
}

void VulkanContext::logInstanceLayerProperties() const
{
    Log::debugF("------ VK Instance Layer properties ------");

    for (const LayerProperties & prop : m_instanceLayerProperties)
    {
        Log::debugF("VK Layer %s - \"%s\"", prop.properties.layerName, prop.properties.description);
        Log::debugF("Extensions for Layer %s:", prop.properties.layerName);

        if (!prop.extensions.empty())
        {
            for (const VkExtensionProperties & ext : prop.extensions)
            {
                Log::debugF("-> %s", ext.extensionName);
            }
        }
        else
        {
            Log::debugF("<no extensions>");
        }
    }

    Log::debugF("------------------------------------------");
}

Size2D VulkanContext::getRenderWindowSize() const
{
    assert(m_renderWindow != nullptr);
    return m_renderWindow->getSize();
}

// ========================================================
// Helpers free functions:
// ========================================================

const char * vkResultToString(const VkResult res)
{
    #define CASE_(x) case x : return #x
    switch (res)
    {
    CASE_(VK_SUCCESS);
    CASE_(VK_NOT_READY);
    CASE_(VK_TIMEOUT);
    CASE_(VK_EVENT_SET);
    CASE_(VK_EVENT_RESET);
    CASE_(VK_INCOMPLETE);
    CASE_(VK_ERROR_OUT_OF_HOST_MEMORY);
    CASE_(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CASE_(VK_ERROR_INITIALIZATION_FAILED);
    CASE_(VK_ERROR_DEVICE_LOST);
    CASE_(VK_ERROR_MEMORY_MAP_FAILED);
    CASE_(VK_ERROR_LAYER_NOT_PRESENT);
    CASE_(VK_ERROR_EXTENSION_NOT_PRESENT);
    CASE_(VK_ERROR_FEATURE_NOT_PRESENT);
    CASE_(VK_ERROR_INCOMPATIBLE_DRIVER);
    CASE_(VK_ERROR_TOO_MANY_OBJECTS);
    CASE_(VK_ERROR_FORMAT_NOT_SUPPORTED);
    CASE_(VK_ERROR_FRAGMENTED_POOL);
    CASE_(VK_ERROR_SURFACE_LOST_KHR);
    CASE_(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    CASE_(VK_SUBOPTIMAL_KHR);
    CASE_(VK_ERROR_OUT_OF_DATE_KHR);
    CASE_(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    CASE_(VK_ERROR_VALIDATION_FAILED_EXT);
    CASE_(VK_ERROR_INVALID_SHADER_NV);
    CASE_(VK_RESULT_RANGE_SIZE);
    default : return "UNKNOWN_RESULT";
    } // switch
    #undef CASE_
}

const char * vkFormatToString(const VkFormat fmt)
{
    #define CASE_(x) case x : return #x
    switch (fmt)
    {
    CASE_(VK_FORMAT_UNDEFINED);
    CASE_(VK_FORMAT_R4G4_UNORM_PACK8);
    CASE_(VK_FORMAT_R4G4B4A4_UNORM_PACK16);
    CASE_(VK_FORMAT_B4G4R4A4_UNORM_PACK16);
    CASE_(VK_FORMAT_R5G6B5_UNORM_PACK16);
    CASE_(VK_FORMAT_B5G6R5_UNORM_PACK16);
    CASE_(VK_FORMAT_R5G5B5A1_UNORM_PACK16);
    CASE_(VK_FORMAT_B5G5R5A1_UNORM_PACK16);
    CASE_(VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    CASE_(VK_FORMAT_R8_UNORM);
    CASE_(VK_FORMAT_R8_SNORM);
    CASE_(VK_FORMAT_R8_USCALED);
    CASE_(VK_FORMAT_R8_SSCALED);
    CASE_(VK_FORMAT_R8_UINT);
    CASE_(VK_FORMAT_R8_SINT);
    CASE_(VK_FORMAT_R8_SRGB);
    CASE_(VK_FORMAT_R8G8_UNORM);
    CASE_(VK_FORMAT_R8G8_SNORM);
    CASE_(VK_FORMAT_R8G8_USCALED);
    CASE_(VK_FORMAT_R8G8_SSCALED);
    CASE_(VK_FORMAT_R8G8_UINT);
    CASE_(VK_FORMAT_R8G8_SINT);
    CASE_(VK_FORMAT_R8G8_SRGB);
    CASE_(VK_FORMAT_R8G8B8_UNORM);
    CASE_(VK_FORMAT_R8G8B8_SNORM);
    CASE_(VK_FORMAT_R8G8B8_USCALED);
    CASE_(VK_FORMAT_R8G8B8_SSCALED);
    CASE_(VK_FORMAT_R8G8B8_UINT);
    CASE_(VK_FORMAT_R8G8B8_SINT);
    CASE_(VK_FORMAT_R8G8B8_SRGB);
    CASE_(VK_FORMAT_B8G8R8_UNORM);
    CASE_(VK_FORMAT_B8G8R8_SNORM);
    CASE_(VK_FORMAT_B8G8R8_USCALED);
    CASE_(VK_FORMAT_B8G8R8_SSCALED);
    CASE_(VK_FORMAT_B8G8R8_UINT);
    CASE_(VK_FORMAT_B8G8R8_SINT);
    CASE_(VK_FORMAT_B8G8R8_SRGB);
    CASE_(VK_FORMAT_R8G8B8A8_UNORM);
    CASE_(VK_FORMAT_R8G8B8A8_SNORM);
    CASE_(VK_FORMAT_R8G8B8A8_USCALED);
    CASE_(VK_FORMAT_R8G8B8A8_SSCALED);
    CASE_(VK_FORMAT_R8G8B8A8_UINT);
    CASE_(VK_FORMAT_R8G8B8A8_SINT);
    CASE_(VK_FORMAT_R8G8B8A8_SRGB);
    CASE_(VK_FORMAT_B8G8R8A8_UNORM);
    CASE_(VK_FORMAT_B8G8R8A8_SNORM);
    CASE_(VK_FORMAT_B8G8R8A8_USCALED);
    CASE_(VK_FORMAT_B8G8R8A8_SSCALED);
    CASE_(VK_FORMAT_B8G8R8A8_UINT);
    CASE_(VK_FORMAT_B8G8R8A8_SINT);
    CASE_(VK_FORMAT_B8G8R8A8_SRGB);
    CASE_(VK_FORMAT_A8B8G8R8_UNORM_PACK32);
    CASE_(VK_FORMAT_A8B8G8R8_SNORM_PACK32);
    CASE_(VK_FORMAT_A8B8G8R8_USCALED_PACK32);
    CASE_(VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
    CASE_(VK_FORMAT_A8B8G8R8_UINT_PACK32);
    CASE_(VK_FORMAT_A8B8G8R8_SINT_PACK32);
    CASE_(VK_FORMAT_A8B8G8R8_SRGB_PACK32);
    CASE_(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    CASE_(VK_FORMAT_A2R10G10B10_SNORM_PACK32);
    CASE_(VK_FORMAT_A2R10G10B10_USCALED_PACK32);
    CASE_(VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
    CASE_(VK_FORMAT_A2R10G10B10_UINT_PACK32);
    CASE_(VK_FORMAT_A2R10G10B10_SINT_PACK32);
    CASE_(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    CASE_(VK_FORMAT_A2B10G10R10_SNORM_PACK32);
    CASE_(VK_FORMAT_A2B10G10R10_USCALED_PACK32);
    CASE_(VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
    CASE_(VK_FORMAT_A2B10G10R10_UINT_PACK32);
    CASE_(VK_FORMAT_A2B10G10R10_SINT_PACK32);
    CASE_(VK_FORMAT_R16_UNORM);
    CASE_(VK_FORMAT_R16_SNORM);
    CASE_(VK_FORMAT_R16_USCALED);
    CASE_(VK_FORMAT_R16_SSCALED);
    CASE_(VK_FORMAT_R16_UINT);
    CASE_(VK_FORMAT_R16_SINT);
    CASE_(VK_FORMAT_R16_SFLOAT);
    CASE_(VK_FORMAT_R16G16_UNORM);
    CASE_(VK_FORMAT_R16G16_SNORM);
    CASE_(VK_FORMAT_R16G16_USCALED);
    CASE_(VK_FORMAT_R16G16_SSCALED);
    CASE_(VK_FORMAT_R16G16_UINT);
    CASE_(VK_FORMAT_R16G16_SINT);
    CASE_(VK_FORMAT_R16G16_SFLOAT);
    CASE_(VK_FORMAT_R16G16B16_UNORM);
    CASE_(VK_FORMAT_R16G16B16_SNORM);
    CASE_(VK_FORMAT_R16G16B16_USCALED);
    CASE_(VK_FORMAT_R16G16B16_SSCALED);
    CASE_(VK_FORMAT_R16G16B16_UINT);
    CASE_(VK_FORMAT_R16G16B16_SINT);
    CASE_(VK_FORMAT_R16G16B16_SFLOAT);
    CASE_(VK_FORMAT_R16G16B16A16_UNORM);
    CASE_(VK_FORMAT_R16G16B16A16_SNORM);
    CASE_(VK_FORMAT_R16G16B16A16_USCALED);
    CASE_(VK_FORMAT_R16G16B16A16_SSCALED);
    CASE_(VK_FORMAT_R16G16B16A16_UINT);
    CASE_(VK_FORMAT_R16G16B16A16_SINT);
    CASE_(VK_FORMAT_R16G16B16A16_SFLOAT);
    CASE_(VK_FORMAT_R32_UINT);
    CASE_(VK_FORMAT_R32_SINT);
    CASE_(VK_FORMAT_R32_SFLOAT);
    CASE_(VK_FORMAT_R32G32_UINT);
    CASE_(VK_FORMAT_R32G32_SINT);
    CASE_(VK_FORMAT_R32G32_SFLOAT);
    CASE_(VK_FORMAT_R32G32B32_UINT);
    CASE_(VK_FORMAT_R32G32B32_SINT);
    CASE_(VK_FORMAT_R32G32B32_SFLOAT);
    CASE_(VK_FORMAT_R32G32B32A32_UINT);
    CASE_(VK_FORMAT_R32G32B32A32_SINT);
    CASE_(VK_FORMAT_R32G32B32A32_SFLOAT);
    CASE_(VK_FORMAT_R64_UINT);
    CASE_(VK_FORMAT_R64_SINT);
    CASE_(VK_FORMAT_R64_SFLOAT);
    CASE_(VK_FORMAT_R64G64_UINT);
    CASE_(VK_FORMAT_R64G64_SINT);
    CASE_(VK_FORMAT_R64G64_SFLOAT);
    CASE_(VK_FORMAT_R64G64B64_UINT);
    CASE_(VK_FORMAT_R64G64B64_SINT);
    CASE_(VK_FORMAT_R64G64B64_SFLOAT);
    CASE_(VK_FORMAT_R64G64B64A64_UINT);
    CASE_(VK_FORMAT_R64G64B64A64_SINT);
    CASE_(VK_FORMAT_R64G64B64A64_SFLOAT);
    CASE_(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    CASE_(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
    CASE_(VK_FORMAT_D16_UNORM);
    CASE_(VK_FORMAT_X8_D24_UNORM_PACK32);
    CASE_(VK_FORMAT_D32_SFLOAT);
    CASE_(VK_FORMAT_S8_UINT);
    CASE_(VK_FORMAT_D16_UNORM_S8_UINT);
    CASE_(VK_FORMAT_D24_UNORM_S8_UINT);
    CASE_(VK_FORMAT_D32_SFLOAT_S8_UINT);
    CASE_(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
    CASE_(VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    CASE_(VK_FORMAT_BC2_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC2_SRGB_BLOCK);
    CASE_(VK_FORMAT_BC3_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC3_SRGB_BLOCK);
    CASE_(VK_FORMAT_BC4_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC4_SNORM_BLOCK);
    CASE_(VK_FORMAT_BC5_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC5_SNORM_BLOCK);
    CASE_(VK_FORMAT_BC6H_UFLOAT_BLOCK);
    CASE_(VK_FORMAT_BC6H_SFLOAT_BLOCK);
    CASE_(VK_FORMAT_BC7_UNORM_BLOCK);
    CASE_(VK_FORMAT_BC7_SRGB_BLOCK);
    CASE_(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
    CASE_(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    CASE_(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
    CASE_(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
    CASE_(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    CASE_(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
    CASE_(VK_FORMAT_EAC_R11_UNORM_BLOCK);
    CASE_(VK_FORMAT_EAC_R11_SNORM_BLOCK);
    CASE_(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
    CASE_(VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
    CASE_(VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
    CASE_(VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
    CASE_(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
    CASE_(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
    default : return "UNKNOWN_FORMAT";
    } // switch
    #undef CASE_
}

} // namespace VkToolbox
