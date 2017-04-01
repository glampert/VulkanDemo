
// ================================================================================================
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

#include "GlslShader.hpp"
#include "Texture.hpp"

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

const char *  VulkanContext::sm_appName             = "VulkanApp";
std::uint32_t VulkanContext::sm_appVersion          = VK_MAKE_VERSION(1, 0, 0);
std::uint32_t VulkanContext::sm_multiSampleCount    = VK_SAMPLE_COUNT_1_BIT;
VkFormat      VulkanContext::sm_depthBufferFormat   = VK_FORMAT_D24_UNORM_S8_UINT;
bool          VulkanContext::sm_useTripleBufferHint = true;

#if DEBUG
VulkanContext::ValidationMode VulkanContext::sm_validationMode = VulkanContext::Debug;
#else // !DEBUG
VulkanContext::ValidationMode VulkanContext::sm_validationMode = VulkanContext::Release;
#endif // DEBUG

// ========================================================
// class VulkanContext:
// ========================================================

VulkanContext::VulkanContext(const OSWindow & window, const Size2D fbSize, const VkAllocationCallbacks * allocCBs)
    : m_allocationCallbacks{ allocCBs }
    , m_renderWindow{ &window }
    , m_mainRenderPass{ *this }
    , m_mainFenceCache{ new FenceCache{ *this } }
    , m_mainTextureStagingCmdBufferPool{ *this }
    , m_mainTextureStagingCmdBuffer{ *this }
{
    Log::debugF("Initializing Vulkan API context...");

    m_swapChain.framebufferSize = fbSize;

    for (std::size_t i = 0; i < m_clearValues.size(); ++i)
    {
        m_clearValues[i].color.float32[0] = 0.0f;
        m_clearValues[i].color.float32[1] = 0.0f;
        m_clearValues[i].color.float32[2] = 0.0f;
        m_clearValues[i].color.float32[3] = 1.0f;

        m_clearValues[i].depthStencil.depth   = 0.0f;
        m_clearValues[i].depthStencil.stencil = 0;
    }

    initInstanceLayerProperties();
    initInstance();
    initEnumerateGpus();
    initSwapChainExtensions();
    initDevice();
    initSwapChain();
    initCommandPoolAndBuffers();
    initDepthBuffer();
    initRenderPass();
    initFramebuffers();
    initSemaphores();
    cacheFormatProperties();

    // Finish the frame/depth buffer layout transitions
    m_mainTextureStagingCmdBuffer.endRecording();
    m_mainTextureStagingCmdBuffer.submitAndWaitComplete(m_gpuGraphicsQueue);
    m_mainTextureStagingCmdBuffer.reset();

    Log::debugF("VulkanContext initialized successfully!");

    if (isDebug())
    {
        logInstanceLayerProperties();
    }
}

VulkanContext::~VulkanContext()
{
    Log::debugF("Destroying Vulkan API context...");

    // The depth/frame-buffer owns its images and views.
    destroyFramebuffers();
    destroyDepthBuffer();

    // Have to explicitly shutdown because they depend on the device, destroyed below.
    m_mainRenderPass.shutdown();
    m_mainFenceCache->shutdown();

    m_mainTextureStagingCmdBuffer.shutdown();
    m_mainTextureStagingCmdBufferPool.shutdown();

    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, m_allocationCallbacks);
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, m_allocationCallbacks);
    }
    if (m_swapChain.handle != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapChain.handle, m_allocationCallbacks);
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
    appInfo.pApplicationName         = sm_appName;
    appInfo.applicationVersion       = sm_appVersion;
    appInfo.pEngineName              = sm_appName;
    appInfo.engineVersion            = sm_appVersion;
    appInfo.apiVersion               = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo    = {};
    instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
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

    // Dummy priorities - don't care atm
    const float queuePriorities[] = { 0.0f, 0.0f };

    VkDeviceQueueCreateInfo queueCreateInfo  = {};
    queueCreateInfo.sType                    = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount               = 1;
    queueCreateInfo.pQueuePriorities         = queuePriorities;
    queueCreateInfo.queueFamilyIndex         = m_gpuGraphicsQueue.familyIndex;

    VkDeviceCreateInfo deviceCreateInfo      = {};
    deviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount   = deviceExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames;
    deviceCreateInfo.pEnabledFeatures        = nullptr;

    VKTB_CHECK(vkCreateDevice(m_gpuPhysDevice, &deviceCreateInfo, m_allocationCallbacks, &m_device));
    assert(m_device != VK_NULL_HANDLE);

    Log::debugF("VK Device created for GPU 0!");

    // Get the GPU queue handles:
    vkGetDeviceQueue(m_device, m_gpuGraphicsQueue.familyIndex, 0, &m_gpuGraphicsQueue.queue);
    assert(m_gpuGraphicsQueue.queue != VK_NULL_HANDLE);

    if (m_gpuGraphicsQueue.familyIndex == m_gpuPresentQueue.familyIndex)
    {
        m_gpuPresentQueue.queue = m_gpuGraphicsQueue.queue;
        Log::debugF("Graphics and present queues are the same.");
    }
    else
    {
        vkGetDeviceQueue(m_device, m_gpuPresentQueue.familyIndex, 0, &m_gpuPresentQueue.queue);
        assert(m_gpuPresentQueue.queue != VK_NULL_HANDLE);
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
    vkGetPhysicalDeviceFeatures(gpus[0], &m_gpuInfo.features);
    vkGetPhysicalDeviceProperties(gpus[0], &m_gpuInfo.properties);
    vkGetPhysicalDeviceMemoryProperties(gpus[0], &m_gpuInfo.memoryProperties);

    // Minimal debug printing:
    Log::debugF("Found %i physical GPUs", gpuCount);
    Log::debugF("GPU 0 has %i queues", queueFamilyCount);
    Log::debugF("GPU 0 name: %s", m_gpuInfo.properties.deviceName);

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
    surfCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(m_renderWindow->instanceHandle());
    surfCreateInfo.hwnd      = reinterpret_cast<HWND>(m_renderWindow->windowHandle());
    VKTB_CHECK(vkCreateWin32SurfaceKHR(m_instance, &surfCreateInfo, m_allocationCallbacks, &m_renderSurface));
    assert(m_renderSurface != VK_NULL_HANDLE);
    #endif // WIN32 || WIN64

    // Iterate over each queue to learn whether it supports presenting:
    std::vector<VkBool32> queuesSupportingPresent(m_gpuQueueFamilyCount, VK_FALSE);
    for (std::uint32_t q = 0; q < m_gpuQueueFamilyCount; ++q)
    {
        VKTB_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_gpuPhysDevice, q,
                                                        m_renderSurface,
                                                        &queuesSupportingPresent[q]));
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both:
    for (std::uint32_t q = 0; q < m_gpuQueueFamilyCount; ++q)
    {
        Log::debugF("Queue %u flags: %#x", q, m_gpuQueueProperties[q].queueFlags);

        if ((m_gpuQueueProperties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (m_gpuGraphicsQueue.familyIndex == -1)
            {
                m_gpuGraphicsQueue.familyIndex = static_cast<std::int32_t>(q);
            }

            if (queuesSupportingPresent[q] == VK_TRUE)
            {
                m_gpuPresentQueue.familyIndex  = static_cast<std::int32_t>(q);
                m_gpuGraphicsQueue.familyIndex = static_cast<std::int32_t>(q);
                break;
            }
        }
    }

    // If didn't find a queue that supports both graphics and present,
    // then find a separate present queue.
    if (m_gpuPresentQueue.familyIndex == -1)
    {
        for (std::uint32_t q = 0; q < m_gpuQueueFamilyCount; ++q)
        {
            if (queuesSupportingPresent[q] == VK_TRUE)
            {
                m_gpuPresentQueue.familyIndex = static_cast<std::int32_t>(q);
                break;
            }
        }
    }

    // Error if could not find queues that support graphics and present.
    if (m_gpuPresentQueue.familyIndex == -1 || m_gpuGraphicsQueue.familyIndex == -1)
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
        Log::debugF("GPU 0 Present Queue family index:  %i", m_gpuPresentQueue.familyIndex);
        Log::debugF("GPU 0 Graphics Queue family index: %i", m_gpuGraphicsQueue.familyIndex);

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
        swapChainExtent.width  = m_swapChain.framebufferSize.width;
        swapChainExtent.height = m_swapChain.framebufferSize.height;

        clamp(&swapChainExtent.width,  surfCapabilities.minImageExtent.width,  surfCapabilities.maxImageExtent.width);
        clamp(&swapChainExtent.height, surfCapabilities.minImageExtent.height, surfCapabilities.maxImageExtent.height);

        m_swapChain.framebufferSize.width  = swapChainExtent.width;
        m_swapChain.framebufferSize.height = swapChainExtent.height;
    }
    else
    {
        // If the surface size is defined, the swap chain size must match.
        swapChainExtent = surfCapabilities.currentExtent;
    }
    Log::debugF("SwapChainExtent={%u,%u}", swapChainExtent.width, swapChainExtent.height);

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

    std::uint32_t desiredNumberOfSwapChainImages = (sm_useTripleBufferHint ? 3 : surfCapabilities.minImageCount);
    if (surfCapabilities.maxImageCount > 0 && desiredNumberOfSwapChainImages > surfCapabilities.maxImageCount)
    {
        desiredNumberOfSwapChainImages = surfCapabilities.maxImageCount;
    }
    Log::debugF("DesiredNumberOfSwapChainImages=%u", desiredNumberOfSwapChainImages);
    assert(desiredNumberOfSwapChainImages <= m_swapChain.buffers.size());

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
    swapChainCreateInfo.clipped                  = VK_TRUE;
    swapChainCreateInfo.imageColorSpace          = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapChainCreateInfo.imageUsage               = imageUsage;
    swapChainCreateInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount    = 0;
    swapChainCreateInfo.pQueueFamilyIndices      = nullptr;

    const std::uint32_t queueFamilyIndices[] = {
        static_cast<std::uint32_t>(m_gpuGraphicsQueue.familyIndex),
        static_cast<std::uint32_t>(m_gpuPresentQueue.familyIndex)
    };

    if (m_gpuGraphicsQueue.familyIndex != m_gpuPresentQueue.familyIndex)
    {
        // If the graphics and present queues are from different queue families,
        // we either have to explicitly transfer ownership of images between the
        // queues, or we have to create the swap-chain with imageSharingMode
        // as VK_SHARING_MODE_CONCURRENT
        swapChainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }

    VKTB_CHECK(vkCreateSwapchainKHR(m_device, &swapChainCreateInfo, m_allocationCallbacks, &m_swapChain.handle));
    assert(m_swapChain.handle != VK_NULL_HANDLE);

    VKTB_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapChain.handle, &m_swapChain.bufferCount, nullptr));
    assert(m_swapChain.bufferCount >= 1);

    std::vector<VkImage> swapChainImages(m_swapChain.bufferCount, VK_NULL_HANDLE);
    VKTB_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapChain.handle, &m_swapChain.bufferCount, swapChainImages.data()));
    assert(m_swapChain.bufferCount >= 1);

    for (std::uint32_t i = 0; i < m_swapChain.bufferCount; ++i)
    {
        VkImageViewCreateInfo imgViewCreateInfo           = {};
        imgViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
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
        m_swapChain.buffers[i] = scBuffer;
    }

    Log::debugF("Swap-chain created with %u image buffers.", m_swapChain.bufferCount);
}

void VulkanContext::initDepthBuffer()
{
    VkImageCreateInfo imageCreateInfo = {};

    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(m_gpuPhysDevice, sm_depthBufferFormat, &props);

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
        Log::fatalF("Depth format %s not supported!", vkFormatToString(sm_depthBufferFormat));
    }

    Log::debugF("DepthBufferFormat=%s", vkFormatToString(sm_depthBufferFormat));

    imageCreateInfo.sType                    = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType                = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                   = sm_depthBufferFormat;
    imageCreateInfo.extent.width             = m_swapChain.framebufferSize.width;
    imageCreateInfo.extent.height            = m_swapChain.framebufferSize.height;
    imageCreateInfo.extent.depth             = 1;
    imageCreateInfo.mipLevels                = 1;
    imageCreateInfo.arrayLayers              = 1;
    imageCreateInfo.samples                  = VkSampleCountFlagBits(sm_multiSampleCount);
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
    viewInfo.image                           = VK_NULL_HANDLE;
    viewInfo.format                          = sm_depthBufferFormat;
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

    if (sm_depthBufferFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
        sm_depthBufferFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
        sm_depthBufferFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
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
    changeImageLayout(m_mainTextureStagingCmdBuffer, m_depthBuffer.image, viewInfo.subresourceRange.aspectMask,
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
    assert(m_mainRenderPass.isInitialized());     // and render pass also needed

    VkImageView attachments[2];
    attachments[1] = m_depthBuffer.view;

    VkFramebufferCreateInfo fbCreateInfo = {};
    fbCreateInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCreateInfo.renderPass              = m_mainRenderPass;
    fbCreateInfo.attachmentCount         = static_cast<std::uint32_t>(arrayLength(attachments)); // Include the depth buffer
    fbCreateInfo.pAttachments            = attachments;
    fbCreateInfo.width                   = m_swapChain.framebufferSize.width;
    fbCreateInfo.height                  = m_swapChain.framebufferSize.height;
    fbCreateInfo.layers                  = 1;

    for (std::uint32_t i = 0; i < m_swapChain.bufferCount; ++i)
    {
        attachments[0] = m_swapChain.buffers[i].view;
        VKTB_CHECK(vkCreateFramebuffer(m_device, &fbCreateInfo, m_allocationCallbacks,
                                       &m_swapChain.buffers[i].framebuffer));
        assert(m_swapChain.buffers[i].framebuffer != VK_NULL_HANDLE);
    }

    Log::debugF("Swap-chain Framebuffers initialized.");
}

void VulkanContext::destroyFramebuffers()
{
    // Clean up the swap-chain image views and FBs.
    // The swap-chain images themselves are owned by the swap-chain.
    for (SwapChainBuffer & scBuffer : m_swapChain.buffers)
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
    attachments[0].initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].flags                 = 0;

    // depth
    attachments[1].format                = sm_depthBufferFormat;
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

    VkSubpassDependency dependency       = {};
    dependency.srcSubpass                = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass                = 0;
    dependency.srcStageMask              = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask             = 0;
    dependency.dstStageMask              = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask             = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpCreateInfo  = {};
    rpCreateInfo.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCreateInfo.attachmentCount         = static_cast<std::uint32_t>(arrayLength(attachments));
    rpCreateInfo.pAttachments            = attachments;
    rpCreateInfo.subpassCount            = 1;
    rpCreateInfo.pSubpasses              = &subpass;
    rpCreateInfo.dependencyCount         = 1;
    rpCreateInfo.pDependencies           = &dependency;

    m_mainRenderPass.initialize(rpCreateInfo);

    Log::debugF("Main render-pass initialized.");
}

void VulkanContext::initCommandPoolAndBuffers()
{
    // Shared Command Pool:
    m_mainTextureStagingCmdBufferPool.initialize(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_gpuGraphicsQueue.familyIndex);

    // Default command buffer for texture staging (used to initialized the Depth Buffer as well):
    m_mainTextureStagingCmdBuffer.initialize(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_mainTextureStagingCmdBufferPool);
    m_mainTextureStagingCmdBuffer.beginRecording();

    Log::debugF("Main command pool and texture staging buffer initialized for queue %i.", m_gpuGraphicsQueue.familyIndex);
}

void VulkanContext::initSemaphores()
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VKTB_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, m_allocationCallbacks, &m_imageAvailableSemaphore));
    VKTB_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, m_allocationCallbacks, &m_renderFinishedSemaphore));

    Log::debugF("Frame semaphores initialized.");
}

void VulkanContext::beginFrame()
{
    ++m_frameNumber;

    const VkResult res = vkAcquireNextImageKHR(m_device, m_swapChain.handle,
                                               InfiniteFenceWaitTimeout, m_imageAvailableSemaphore,
                                               VK_NULL_HANDLE, &m_swapChain.bufferIndex);

    #if DEBUG
    if (res != VK_SUCCESS)
    {
        Log::warningF("vkAcquireNextImageKHR failed: %s", vkResultToString(res));
    }
    #else // !DEBUG
    (void)res;
    #endif // DEBUG
}

void VulkanContext::endFrame(array_view<const VkCommandBuffer> submitBuffers, VkFence fence)
{
    const VkSemaphore waitSemaphores[]      = { m_imageAvailableSemaphore };
    const VkSemaphore signalSemaphores[]    = { m_renderFinishedSemaphore };
    const VkSwapchainKHR swapChains[]       = { m_swapChain.handle };
    const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo;
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext                = nullptr;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = static_cast<std::uint32_t>(submitBuffers.size());
    submitInfo.pCommandBuffers      = submitBuffers.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    VKTB_CHECK(vkQueueSubmit(m_gpuGraphicsQueue, 1, &submitInfo, fence));

    VkPresentInfoKHR presentInfo;
    presentInfo.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext               = nullptr;
    presentInfo.waitSemaphoreCount  = 1;
    presentInfo.pWaitSemaphores     = signalSemaphores;
    presentInfo.swapchainCount      = 1;
    presentInfo.pSwapchains         = swapChains;
    presentInfo.pImageIndices       = &m_swapChain.bufferIndex;
    presentInfo.pResults            = nullptr;

    const VkResult res = vkQueuePresentKHR(m_gpuPresentQueue, &presentInfo);

    #if DEBUG
    if (res != VK_SUCCESS)
    {
        Log::warningF("vkQueuePresentKHR failed: %s", vkResultToString(res));
    }
    #else // !DEBUG
    (void)res;
    #endif // DEBUG
}

std::uint32_t VulkanContext::memoryTypeFromProperties(const std::uint32_t typeBits, const VkFlags requirementsMask) const
{
    // Search mem types to find first index with those properties
    auto bits = typeBits;
    for (std::uint32_t i = 0; i < m_gpuInfo.memoryProperties.memoryTypeCount; ++i)
    {
        if ((bits & 1) == 1)
        {
            // Type is available, does it match user properties?
            if ((m_gpuInfo.memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
            {
                return i;
            }
        }
        bits >>= 1;
    }

    // No memory types matched, return failure
    Log::errorF("Unable to find index for requested memory type %#x, with mask %#x", typeBits, requirementsMask);
    return UINT32_MAX;
}

void VulkanContext::changeImageLayout(const CommandBuffer & cmdBuff, VkImage image, const VkImageAspectFlags aspectMask,
                                      const VkImageLayout oldImageLayout, const VkImageLayout newImageLayout,
                                      const int baseMipLevel, const int mipLevelCount) const
{
    assert(image != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());

    VkImageMemoryBarrier imageMemBarrier;
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
    imageMemBarrier.subresourceRange.baseMipLevel   = baseMipLevel;
    imageMemBarrier.subresourceRange.levelCount     = mipLevelCount;
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
        /* commandBuffer            = */ cmdBuff.commandBufferHandle(),
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

void VulkanContext::copyImage(const CommandBuffer & cmdBuff, VkImage srcImage,
                              VkImage dstImage, const Size2D size) const
{
    assert(srcImage != VK_NULL_HANDLE && dstImage != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());

    VkImageSubresourceLayers subResource;
    subResource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel       = 0;
    subResource.baseArrayLayer = 0;
    subResource.layerCount     = 1;

    VkImageCopy region;
    region.srcSubresource = subResource;
    region.dstSubresource = subResource;
    region.srcOffset      = { 0, 0, 0 };
    region.dstOffset      = { 0, 0, 0 };
    region.extent.width   = size.width;
    region.extent.height  = size.height;
    region.extent.depth   = 1;

    vkCmdCopyImage(cmdBuff.commandBufferHandle(),
                   srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &region);
}

void VulkanContext::createImage(const VkImageCreateInfo & imageInfo, const VkMemoryPropertyFlags memoryProperties,
                                VkImage * outImage, VkDeviceMemory * outImageMemory) const
{
    assert(outImage != nullptr && outImageMemory != nullptr);
    VKTB_CHECK(vkCreateImage(m_device, &imageInfo, m_allocationCallbacks, outImage));

    VkMemoryRequirements memRequirements = {};
    vkGetImageMemoryRequirements(m_device, *outImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo;
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext           = nullptr;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeFromProperties(memRequirements.memoryTypeBits, memoryProperties);
    assert(allocInfo.memoryTypeIndex < UINT32_MAX && "Requested memory type not supported!");

    VKTB_CHECK(vkAllocateMemory(m_device, &allocInfo, m_allocationCallbacks, outImageMemory));
    VKTB_CHECK(vkBindImageMemory(m_device, *outImage, *outImageMemory, 0));
}

void VulkanContext::copyBuffer(const CommandBuffer & cmdBuff, VkBuffer srcBuffer,
                               VkBuffer dstBuffer, const VkDeviceSize sizeToCopy,
                               const VkDeviceSize srcOffset, const VkDeviceSize dstOffset) const
{
    assert(sizeToCopy != 0);
    assert(srcBuffer != VK_NULL_HANDLE && dstBuffer != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size      = sizeToCopy;
    vkCmdCopyBuffer(cmdBuff.commandBufferHandle(), srcBuffer, dstBuffer, 1, &copyRegion);
}

void VulkanContext::createBuffer(const VkDeviceSize sizeBytes, const VkBufferUsageFlags usage,
                                 const VkMemoryPropertyFlags memoryProperties, VkBuffer * outBuffer,
                                 VkDeviceMemory * outBufferMemory) const
{
    assert(sizeBytes != 0);
    assert(outBuffer != nullptr && outBufferMemory != nullptr);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size               = sizeBytes;
    bufferInfo.usage              = usage;
    bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    VKTB_CHECK(vkCreateBuffer(m_device, &bufferInfo, m_allocationCallbacks, outBuffer));

    VkMemoryRequirements memRequirements = {};
    vkGetBufferMemoryRequirements(m_device, *outBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo;
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext           = nullptr;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeFromProperties(memRequirements.memoryTypeBits, memoryProperties);
    assert(allocInfo.memoryTypeIndex < UINT32_MAX && "Requested memory type not supported!");

    VKTB_CHECK(vkAllocateMemory(m_device, &allocInfo, m_allocationCallbacks, outBufferMemory));
    VKTB_CHECK(vkBindBufferMemory(m_device, *outBuffer, *outBufferMemory, 0));
}

void VulkanContext::cacheFormatProperties()
{
    for (Image::Format k : m_formatPropsCache.keys())
    {
        if (k == Image::Format::None)
        {
            m_formatPropsCache[k]      = {};
            m_imageFormatPropsCache[k] = {};
            continue;
        }

        const VkFormat vkImageFormat = Texture::toVkImageFormat(k);

        vkGetPhysicalDeviceFormatProperties(m_gpuPhysDevice, vkImageFormat,
                                            &m_formatPropsCache[k]);

        // VK_IMAGE_TILING_LINEAR:  Texels are laid out in row-major order (mipmaps are not supported).
        // VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access.
        vkGetPhysicalDeviceImageFormatProperties(m_gpuPhysDevice, vkImageFormat,
                                                 VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                 VK_IMAGE_USAGE_SAMPLED_BIT, 0,
                                                 &m_imageFormatPropsCache[k]);
    }
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

Size2D VulkanContext::renderWindowSize() const
{
    assert(m_renderWindow != nullptr);
    return m_renderWindow->size();
}

void VulkanContext::initClass()
{
    Log::debugF("---- VulkanContext::initClass ----");

    str512 cwd;
    Log::debugF("CWD is '%s'", currentPath(&cwd));

    GlslShader::initClass();
    Texture::initClass();
}

void VulkanContext::shutdownClass()
{
    Log::debugF("---- VulkanContext::shutdownClass ----");

    Texture::shutdownClass();
    GlslShader::shutdownClass();
}

// ========================================================
// class FenceCache:
// ========================================================

FenceCache::~FenceCache()
{
    shutdown();
}

void FenceCache::shutdown()
{
    assert(m_allocCount == 0);
    if (!m_cache.empty())
    {
        const auto device   = m_vkContext->deviceHandle();
        const auto allocCBs = m_vkContext->allocationCallbacks();
        for (VkFence fence : m_cache)
        {
            vkDestroyFence(device, fence, allocCBs);
        }
        m_cache.clear();
    }
}

void FenceCache::preallocate(const int numFences)
{
    assert(numFences <= m_cache.capacity());

    if (m_cache.size() >= numFences)
    {
        return;
    }

    const int count = numFences - m_cache.size();
    for (int i = 0; i < count; ++i)
    {
        m_cache.push(newFence());
    }
}

VkFence FenceCache::newFence()
{
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = 0;

    VkFence newFence = VK_NULL_HANDLE;
    VKTB_CHECK(vkCreateFence(m_vkContext->deviceHandle(), &fenceCreateInfo,
                             m_vkContext->allocationCallbacks(), &newFence));
    return newFence;
}

VkFence FenceCache::allocRecyclableFence()
{
    assert(m_allocCount < m_cache.capacity());
    ++m_allocCount;

    if (m_cache.empty())
    {
        return newFence();
    }
    else
    {
        auto fence = m_cache.back();
        m_cache.pop();

        // Reset to unsignaled
        VKTB_CHECK(vkResetFences(m_vkContext->deviceHandle(), 1, &fence));
        return fence;
    }
}

void FenceCache::recycleFence(VkFence fence)
{
    assert(fence != VK_NULL_HANDLE);
    m_cache.push(fence);
    --m_allocCount;
}

// ========================================================
// class AutoFence:
// ========================================================

bool AutoFence::wait(const std::uint64_t timeout)
{
    assert(isWaitable());
    assert(m_cache != nullptr);

    const VkResult res = vkWaitForFences(m_cache->context().deviceHandle(),
                                         1, &m_fenceHandle, VK_TRUE, timeout);

    if (res == VK_TIMEOUT)
    {
        return false; // Caller can try again with bigger timeout.
    }
    else if (res == VK_SUCCESS)
    {
        // Done, recycle the fence.
        m_cache->recycleFence(m_fenceHandle);
        m_fenceHandle = VK_NULL_HANDLE;
        return true;
    }
    else // Some other internal failure...
    {
        Log::fatalF("vkWaitForFences() failed with error (%#x): %s", res, vkResultToString(res));
    }
}

bool AutoFence::isSignaled() const
{
    if (!isWaitable())
    {
        return true;
    }

    assert(m_cache != nullptr);
    const VkResult res = vkGetFenceStatus(m_cache->context().deviceHandle(), m_fenceHandle);

    if (res == VK_SUCCESS)
    {
        return true;
    }
    else if (res == VK_NOT_READY)
    {
        return false;
    }
    else
    {
        Log::fatalF("vkGetFenceStatus() failed with error (%#x): %s", res, vkResultToString(res));
    }
}

// ========================================================
// class RenderPass:
// ========================================================

void RenderPass::initialize(const VkRenderPassCreateInfo & rpCreateInfo)
{
    assert(!isInitialized());
    VKTB_CHECK(vkCreateRenderPass(m_vkContext->deviceHandle(), &rpCreateInfo,
                                  m_vkContext->allocationCallbacks(), &m_renderPassHandle));
}

void RenderPass::shutdown()
{
    if (m_renderPassHandle != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_vkContext->deviceHandle(), m_renderPassHandle,
                            m_vkContext->allocationCallbacks());
        m_renderPassHandle = VK_NULL_HANDLE;
    }
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
