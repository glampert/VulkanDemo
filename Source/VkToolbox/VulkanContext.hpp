#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/VulkanContext.hpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: Main Vulkan context and device class.
// ================================================================================================

// To enable vkCreateWin32SurfaceKHR extensions (note: Windows-specific!)
#if (!defined(VK_USE_PLATFORM_WIN32_KHR) && (defined(WIN32) || defined(WIN64)))
    #define VK_USE_PLATFORM_WIN32_KHR 1
#endif // !VK_USE_PLATFORM_WIN32_KHR && Windows

#include <vulkan/vulkan.h>
#include <vector>

#include "Utils.hpp"
#include "Log.hpp"

// ========================================================

namespace VkToolbox
{

class OSWindow;

//
// Main instance and device holder for a Vulkan rendering context.
// Has the common functionality needed to perform some rendering
// work using the Vulkan API.
//
class VulkanContext final
{
public:

    enum ValidationMode
    {
        Debug, Release
    };

    // Shared configurations for all contexts - these must be set before construction.
    static const char *   appName;
    static std::uint32_t  appVersion;
    static std::uint32_t  multiSampleCount;
    static VkFormat       depthFormat;
    static ValidationMode validationMode;

    // Initialization.
    VulkanContext(WeakRef<const OSWindow> window, WeakRef<const VkAllocationCallbacks> allocCbs = nullptr);

    // Not copyable.
    VulkanContext(const VulkanContext &) = delete;
    VulkanContext & operator = (const VulkanContext &) = delete;

    // Shutdown.
    ~VulkanContext();

    // Prints layer and extension information to the log.
    void logInstanceLayerProperties() const;

    // Mode queries:
    static bool isDebug();
    static bool isRelease();

    // Device (AKA the GPU) queries:
    WeakHandle<VkDevice> getDevice() const;
    const VkPhysicalDeviceFeatures & getDeviceFeatures() const;
    const VkPhysicalDeviceProperties & getDeviceProperties() const;
    const VkPhysicalDeviceMemoryProperties & getDeviceMemoryProperties() const;

    // Misc getters:
    WeakRef<const OSWindow> getRenderWindow() const;
    WeakRef<const VkAllocationCallbacks> getAllocationCallbacks() const;

private:

    struct LayerProperties
    {
        VkLayerProperties properties;
        std::vector<VkExtensionProperties> extensions;
    };

    struct SwapChainImage
    {
        WeakHandle<VkImage> image;     // Owned by the swap-chain
        OwnedHandle<VkImageView> view; // Owned by the VulkanContex
    };

    struct DepthBuffer
    {
        OwnedHandle<VkImage> image = VK_NULL_HANDLE;
        OwnedHandle<VkImageView> view = VK_NULL_HANDLE;
        OwnedHandle<VkDeviceMemory> memory = VK_NULL_HANDLE;

        void destroy();
    };

    // Instance creation helpers:
    void initInstanceLayerProperties();
    void initInstanceExtensionProperties(LayerProperties & layerProps);
    void initInstance(); 

    // Device and swap-chain creation helpers:
    void initDevice();
    void initEnumerateGpus();
    void initSwapChainExtensions();
    void initSwapChain();

    // Framebuffer/depth-buffer initialization:
    void initDepthBuffer();
    void initFramebuffers();

    // Misc helpers:
    static int memoryTypeFromProperties(std::uint32_t typeBits,
                                        VkFlags requirementsMask);
    static void setImageLayout(VkImage image, VkImageAspectFlags aspectMask,
                               VkImageLayout oldImageLayout, VkImageLayout newImageLayout);

private:

    // API global context.
    OwnedHandle<VkInstance> m_instance;

    // Ref to the OS Window we are rendeing to.
    WeakRef<const OSWindow> m_renderWindow;

    // User provided allocations callbacks or nullptr.
    WeakRef<const VkAllocationCallbacks> m_allocationCallbacks;

    // Render target surface for the rendering window (our screen framebuffer).
    OwnedHandle<VkSurfaceKHR> m_renderSurface;
    VkFormat m_renderSurfaceFormat;

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;

    // Image swap-chain:
    std::uint32_t m_swapChainImageCount;
    OwnedHandle<VkSwapchainKHR> m_swapChain;
    std::vector<SwapChainImage> m_swapChainBuffers;

    // Frame/depth buffers:
    DepthBuffer m_depthBuffer;

    // The rendering device we are going to use (always GPU 0 for now).
    OwnedHandle<VkDevice> m_device;

    // Handles to the "Physical Devices", AKA the GPUs. These handles are owned by the VK instance.
    // Vulkan allows explicitly selecting the device you want to use, on systems with more than one
    // GPU (e.g. SLI, Crossfire, etc). We will be sticking to a single GPU in this demo for simplicity.
    std::vector<WeakHandle<VkPhysicalDevice>> m_gpus;

    // Information about the rendering, compute and present queues available in the GPU.
    std::vector<VkQueueFamilyProperties> m_gpuQueueProperties;
    std::uint32_t m_gpuQueueFamilyCount;
    std::int32_t  m_gpuPresentQueueFamilyIndex;
    std::int32_t  m_gpuGraphicsQueueFamilyIndex;
    WeakHandle<VkQueue> m_gpuPresentQueue;
    WeakHandle<VkQueue> m_gpuGraphicsQueue;

    // Information bits about the graphics hardware physical limits. We only care about the first GPU for now.
    VkPhysicalDeviceFeatures m_gpuFeatures;
    VkPhysicalDeviceProperties m_gpuProperties;
    VkPhysicalDeviceMemoryProperties m_gpuMemoryProperties;
};

// ========================================================
// VulkanContext inline methods:
// ========================================================

inline bool VulkanContext::isDebug()
{
    return validationMode == Debug;
}

inline bool VulkanContext::isRelease()
{
    return validationMode == Release;
}

inline WeakHandle<VkDevice> VulkanContext::getDevice() const
{
    return m_device;
}

inline const VkPhysicalDeviceMemoryProperties & VulkanContext::getDeviceMemoryProperties() const
{
    return m_gpuMemoryProperties;
}

inline const VkPhysicalDeviceProperties & VulkanContext::getDeviceProperties() const
{
    return m_gpuProperties;
}

inline const VkPhysicalDeviceFeatures & VulkanContext::getDeviceFeatures() const
{
    return m_gpuFeatures;
}

inline WeakRef<const OSWindow> VulkanContext::getRenderWindow() const
{
    return m_renderWindow;
}

inline WeakRef<const VkAllocationCallbacks> VulkanContext::getAllocationCallbacks() const
{
    return m_allocationCallbacks;
}

// ========================================================
// Helpers free functions / macros:
// ========================================================

// Checks a VkResult returned from the expression and logs a fatal error if it is not VK_SUCCESS.
#define VKTB_CHECK(expr) \
    do { \
        const VkResult errorCode = expr; \
        if (errorCode != VK_SUCCESS) \
        { \
            VkToolbox::Log::fatalF("Vulkan error %s in: %s - %s(%i)", \
                                   VkToolbox::vkResultToString(errorCode), \
                                   #expr, __FILE__, __LINE__); \
        } \
    } while (0,0)

// Printable null-terminated C-string from the VkResult error code.
// Note: The returned string is statically allocated and must not be freed.
const char * vkResultToString(const VkResult res);

// Printable null-terminated C-string for the VK image/buffer format.
// Note: The returned string is statically allocated and must not be freed.
const char * vkFormatToString(const VkFormat fmt);

} // namespace VkToolbox
