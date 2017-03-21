#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/VulkanContext.hpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: Main Vulkan context and device class.
// ================================================================================================

#include <vulkan/vulkan.h>
#include <vector>

#include "Utils.hpp"
#include "Log.hpp"

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
    static const char *   sm_appName;
    static std::uint32_t  sm_appVersion;
    static std::uint32_t  sm_multiSampleCount;
    static VkFormat       sm_depthBufferFormat;
    static ValidationMode sm_validationMode;

    // Initialization.
    VulkanContext(WeakRef<const OSWindow> window, Size2D fbSize, WeakRef<const VkAllocationCallbacks> allocCbs = nullptr);

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
    Size2D getRenderWindowSize() const;
    Size2D getFramebufferSize() const;

private:

    struct LayerProperties
    {
        VkLayerProperties properties;
        std::vector<VkExtensionProperties> extensions;
    };

    struct SwapChainBuffer
    {
        WeakHandle<VkImage> image = VK_NULL_HANDLE; // Owned by the swap-chain
        OwnedHandle<VkImageView> view = VK_NULL_HANDLE;
        OwnedHandle<VkFramebuffer> framebuffer = VK_NULL_HANDLE;
    };

    struct DepthBuffer
    {
        OwnedHandle<VkImage> image = VK_NULL_HANDLE;
        OwnedHandle<VkImageView> view = VK_NULL_HANDLE;
        OwnedHandle<VkDeviceMemory> memory = VK_NULL_HANDLE;
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

    // Framebuffer/depth-buffer initialization/shutdown:
    void initDepthBuffer();
    void destroyDepthBuffer();
    void initFramebuffers();
    void destroyFramebuffers();
    void initRenderPass();
    void initCommandPoolAndBuffer();

    // Misc helpers:
    std::uint32_t memoryTypeFromProperties(std::uint32_t typeBits, VkFlags requirementsMask) const;
    void setImageLayout(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout);

private:

    // API global context.
    OwnedHandle<VkInstance> m_instance;

    // Ref to the OS Window we are rendering to (non-null).
    WeakRef<const OSWindow> m_renderWindow;

    // User provided allocations callbacks or nullptr.
    WeakRef<const VkAllocationCallbacks> m_allocationCallbacks;

    // Render target surface for the rendering window (our screen framebuffer).
    OwnedHandle<VkSurfaceKHR> m_renderSurface;
    VkFormat m_renderSurfaceFormat;

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;

    // Image swap-chain (framebuffers):
    Size2D m_framebufferSize;
    std::uint32_t m_swapChainCount;
    OwnedHandle<VkSwapchainKHR> m_swapChain;
    std::vector<SwapChainBuffer> m_swapChainBuffers;

    // Depth/stencil buffer images/views:
    DepthBuffer m_depthBuffer;

    // Common render pass and main commands pool/buffer:
    OwnedHandle<VkRenderPass> m_renderPass;
    OwnedHandle<VkCommandPool> m_cmdPool;
    OwnedHandle<VkCommandBuffer> m_cmdBuffer;
    bool m_cmdBufferRecordingState; // Between vkBeginCommandBuffer and vkEndCommandBuffer
    bool m_cmdBufferExecuteState;   // After vkEndCommandBuffer

    // The rendering device we are going to use (always GPU 0 for now).
    OwnedHandle<VkDevice> m_device;

    // Handle to the "Physical Device", AKA the GPU. This handle is owned by the VK instance.
    // Vulkan allows explicitly selecting the device you want to use, on systems with more than one
    // GPU (e.g. SLI, Crossfire, etc). I will be sticking to a single GPU in this demo for simplicity.
    WeakHandle<VkPhysicalDevice> m_gpuPhysDevice;

    // Information about the rendering, compute and present queues available in the first GPU.
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
    return sm_validationMode == Debug;
}

inline bool VulkanContext::isRelease()
{
    return sm_validationMode == Release;
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

inline Size2D VulkanContext::getFramebufferSize() const
{
    return m_framebufferSize;
}

// ========================================================
// Helpers free functions / macros:
// ========================================================

// Checks a VkResult returned from the expression and logs a fatal error if it is not VK_SUCCESS.
#define VKTB_CHECK(expr)                                                   \
    do {                                                                   \
        const VkResult errorCode = expr;                                   \
        if (errorCode != VK_SUCCESS)                                       \
        {                                                                  \
            VkToolbox::Log::fatalF("Vulkan error %s in: %s - %s(%i)",      \
                                   VkToolbox::vkResultToString(errorCode), \
                                   #expr, __FILE__, __LINE__);             \
        }                                                                  \
    } while (0,0)

// Log a presumably non-fatal Vulkan error code with source location info added to it.
#define VKTB_LOG_ERROR(errorCode, message)                         \
    VkToolbox::Log::errorF("Vulkan error %s in: %s(%i): %s",       \
                           VkToolbox::vkResultToString(errorCode), \
                           __FILE__, __LINE__, message)

// ========================================================

// Printable null-terminated C-string from the VkResult error code.
// Note: The returned string is statically allocated and must not be freed.
const char * vkResultToString(const VkResult res);

// Printable null-terminated C-string for the VK image/buffer format.
// Note: The returned string is statically allocated and must not be freed.
const char * vkFormatToString(const VkFormat fmt);

// ========================================================

} // namespace VkToolbox
