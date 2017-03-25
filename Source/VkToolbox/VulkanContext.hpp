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

#include "Log.hpp"
#include "Utils.hpp"
#include "External.hpp"
#include "FixedSizeArray.hpp"

#include "Image.hpp"
#include "RenderPass.hpp"
#include "CommandBuffer.hpp"

namespace VkToolbox
{

class OSWindow;
class FenceCache;

// ========================================================
// class VulkanContext:
// ========================================================

// Main instance and device holder for a Vulkan rendering context.
// Has the common functionality needed to perform some rendering
// work using the Vulkan API.
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
    VulkanContext(WeakRef<const OSWindow> window, Size2D fbSize,
                  WeakRef<const VkAllocationCallbacks> allocCBs = nullptr);

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

    // Device (AKA GPU) info queries:
    WeakHandle<VkDevice> getVkDeviceHandle() const;
    const VkPhysicalDeviceFeatures & getDeviceFeatures() const;
    const VkPhysicalDeviceProperties & getDeviceProperties() const;
    const VkPhysicalDeviceMemoryProperties & getDeviceMemoryProperties() const;
    const VkFormatProperties & getVkFormatPropertiesForImageFormat(Image::Format format) const;

    // Accessors:
    WeakRef<const OSWindow> getRenderWindow() const;
    WeakRef<const VkAllocationCallbacks> getAllocationCallbacks() const;
    Size2D getRenderWindowSize() const;
    Size2D getFramebufferSize() const;

    // Shareable objects:
    const RenderPass & getMainRenderPass() const;
    const CommandPool & getMainCmdPool() const;
    const CommandBuffer & getMainCmdBuffer() const;
    WeakRef<FenceCache> getMainFenceCache();

    // GPU queues info:
    struct GpuQueue
    {
        WeakHandle<VkQueue> queue = VK_NULL_HANDLE;
        std::int32_t familyIndex  = -1;
    };
    const GpuQueue & getPresentQueue() const;
    const GpuQueue & getGraphisQueue() const;

    // Misc helpers:
    std::uint32_t getMemoryTypeFromProperties(std::uint32_t typeBits, VkFlags requirementsMask) const;
    void setImageLayout(VkCommandBuffer cmdBuff, VkImage image, VkImageAspectFlags aspectMask,
                        VkImageLayout oldImageLayout, VkImageLayout newImageLayout) const;

private:

    struct LayerProperties
    {
        VkLayerProperties properties = {};
        std::vector<VkExtensionProperties> extensions;
    };

    struct DepthBuffer
    {
        OwnedHandle<VkImage> image = VK_NULL_HANDLE;
        OwnedHandle<VkImageView> view = VK_NULL_HANDLE;
        OwnedHandle<VkDeviceMemory> memory = VK_NULL_HANDLE;
    };

    struct SwapChainBuffer
    {
        WeakHandle<VkImage> image = VK_NULL_HANDLE; // Owned by the swap-chain
        OwnedHandle<VkImageView> view = VK_NULL_HANDLE;
        OwnedHandle<VkFramebuffer> framebuffer = VK_NULL_HANDLE;
    };

    struct SwapChainState
    {
        Size2D framebufferSize = {0,0};
        std::uint32_t swapChainBufferCount = 0;
        OwnedHandle<VkSwapchainKHR> swapChain = VK_NULL_HANDLE;
        std::vector<SwapChainBuffer> swapChainBuffers;
    };

    struct GpuInfo
    {
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceMemoryProperties memoryProperties = {};
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

private:

    // API global context.
    OwnedHandle<VkInstance> m_instance = VK_NULL_HANDLE;

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;

    // User provided allocations callbacks or nullptr.
    WeakRef<const VkAllocationCallbacks> m_allocationCallbacks = nullptr;

    // Ref to the OS Window we are rendering to (non-null).
    WeakRef<const OSWindow> m_renderWindow = nullptr;

    // Render target surface for the rendering window (our screen framebuffer).
    OwnedHandle<VkSurfaceKHR> m_renderSurface = VK_NULL_HANDLE;

    // Texture format of the rendering surface (the framebuffer).
    VkFormat m_renderSurfaceFormat = VK_FORMAT_UNDEFINED;

    // Image swap-chain (framebuffers):
    SwapChainState m_swapChainState;

    // Depth/stencil buffer images & views:
    DepthBuffer m_depthBuffer;

    // Main/default render pass, command pool/buffer and fence cache.
    // These are shared for this context, so need explicit synchronization
    // between threads using the same VulkanContext.
    RenderPass m_mainRenderPass;
    CommandPool m_mainCmdPool;
    CommandBuffer m_mainCmdBuffer;
    std::unique_ptr<FenceCache> m_mainFenceCache;

    // The rendering device we are going to use (always GPU 0 for now).
    OwnedHandle<VkDevice> m_device = VK_NULL_HANDLE;

    // Handle to the "Physical Device", AKA the GPU. This handle is owned by the VK instance.
    // Vulkan allows explicitly selecting the device you want to use, on systems with more than one
    // GPU (e.g. SLI, Crossfire, etc). We will be sticking to a single GPU in this demo for simplicity.
    WeakHandle<VkPhysicalDevice> m_gpuPhysDevice = VK_NULL_HANDLE;

    // Information about the rendering, compute and present queues available in the first GPU.
    std::uint32_t m_gpuQueueFamilyCount = 0;
    std::vector<VkQueueFamilyProperties> m_gpuQueueProperties;

    // Queue index/handle helpers:
    GpuQueue m_gpuPresentQueue;
    GpuQueue m_gpuGraphicsQueue;

    // Information bits about the graphics hardware physical limitations.
    // We only care about the first GPU for now.
    GpuInfo m_gpuInfo;

    // These are queried all the time by Textures, so we cache one for each supported image format.
    enum_array<Image::Format, VkFormatProperties> m_imgFormatProps;
};

// ========================================================
// class FenceCache:
// ========================================================

class FenceCache final
{
public:

    explicit FenceCache(WeakRef<const VulkanContext> vkContext)
        : m_vkContext{ vkContext }
        , m_allocCount{ 0 }
    { }

    const VulkanContext & getVkContext() const
    {
        assert(m_vkContext != nullptr);
        return *m_vkContext;
    }

    // Note: Will free all fences currently in the cache!
    // Make sure no external refs remain.
    void shutdown();
    ~FenceCache();

    // Not copyable.
    FenceCache(const FenceCache &) = delete;
    FenceCache & operator = (const FenceCache &) = delete;

    // Get a temporary VkFence from the cache. Once you've waited on the fence,
    // you must call recycleFence() to return it to the cache. The cache cleans
    // up all remaining fences on shutdown.
    WeakHandle<VkFence> AllocRecyclableFence();
    void recycleFence(WeakHandle<VkFence> fence);

private:

    WeakRef<const VulkanContext> m_vkContext;
    int m_allocCount;

    // Fixed-size cache of recyclable fence handles for AutoFences.
    FixedSizeArray<OwnedHandle<VkFence>, 128> m_cache;
};

// ========================================================
// class AutoFence:
// ========================================================

// Amount of time, in nanoseconds, to wait for a fence to be signaled.
constexpr auto DefaultFenceWaitTimeout = UINT64_C(100000000);

// Automatically waits and recycles the underlaying VkFence object.
class AutoFence final
{
public:

    explicit AutoFence(WeakRef<FenceCache> cache);

    // Implicitly waits on the fence if not already waited/signaled.
    ~AutoFence();

    // Movable.
    AutoFence(AutoFence && other);
    AutoFence & operator = (AutoFence && other);

    // But not copyable.
    AutoFence(const AutoFence &) = delete;
    AutoFence & operator = (const AutoFence &) = delete;

    // Wait for the fence to be signaled (blocks). Once signaled, the fence is recycled.
    // Returns true if the fence was signaled, false if timeouted, which allows you to wait again.
    bool wait(std::uint64_t timeout = DefaultFenceWaitTimeout);

    // Test if signaled, without blocking.
    bool isSignaled() const;

    // Test if valid to call wait() on the fence. Once a wait() is,
    // successful the fence is recycled and can no longer be waited on.
    bool isWaitable() const;

    // Accessors:
    WeakRef<FenceCache> getFenceCache() const;
    WeakHandle<VkFence> getVkFenceHandle() const;

private:

    // Owned my the context and recycled once waited on.
    WeakRef<FenceCache> m_cache;
    WeakHandle<VkFence> m_fenceHandle;
};

// ========================================================
// VulkanContext inline methods:
// ========================================================

inline bool VulkanContext::isDebug()
{
    return (sm_validationMode == Debug);
}

inline bool VulkanContext::isRelease()
{
    return (sm_validationMode == Release);
}

inline WeakHandle<VkDevice> VulkanContext::getVkDeviceHandle() const
{
    return m_device;
}

inline const VkPhysicalDeviceMemoryProperties & VulkanContext::getDeviceMemoryProperties() const
{
    return m_gpuInfo.memoryProperties;
}

inline const VkFormatProperties & VulkanContext::getVkFormatPropertiesForImageFormat(const Image::Format format) const
{
    return m_imgFormatProps[format];
}

inline const VkPhysicalDeviceProperties & VulkanContext::getDeviceProperties() const
{
    return m_gpuInfo.properties;
}

inline const VkPhysicalDeviceFeatures & VulkanContext::getDeviceFeatures() const
{
    return m_gpuInfo.features;
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
    return m_swapChainState.framebufferSize;
}

inline const RenderPass & VulkanContext::getMainRenderPass() const
{
    return m_mainRenderPass;
}

inline const CommandPool & VulkanContext::getMainCmdPool() const
{
    return m_mainCmdPool;
}

inline const CommandBuffer & VulkanContext::getMainCmdBuffer() const
{
    return m_mainCmdBuffer;
}

inline WeakRef<FenceCache> VulkanContext::getMainFenceCache()
{
    return m_mainFenceCache.get();
}

inline const VulkanContext::GpuQueue & VulkanContext::getPresentQueue() const
{
    return m_gpuPresentQueue;
}

inline const VulkanContext::GpuQueue & VulkanContext::getGraphisQueue() const
{
    return m_gpuGraphicsQueue;
}

// ========================================================
// AutoFence inline methods:
// ========================================================

inline AutoFence::AutoFence(WeakRef<FenceCache> cache)
    : m_cache{ cache }
    , m_fenceHandle{ m_cache->AllocRecyclableFence() }
{
}

inline AutoFence::~AutoFence()
{
    if (isWaitable())
    {
        while (wait() == false) { }
    }
}

inline AutoFence::AutoFence(AutoFence && other)
    : m_cache{ other.m_cache }
    , m_fenceHandle{ other.m_fenceHandle }
{
    other.m_cache       = nullptr;
    other.m_fenceHandle = VK_NULL_HANDLE;
}

inline AutoFence & AutoFence::operator = (AutoFence && other)
{
    if (isWaitable())
    {
        // Best strategy here?
        while (wait() == false) { }
    }

    m_cache             = other.m_cache;
    m_fenceHandle       = other.m_fenceHandle;
    other.m_cache       = nullptr;
    other.m_fenceHandle = VK_NULL_HANDLE;

    return *this;
}

inline bool AutoFence::isWaitable() const
{
    return (m_fenceHandle != VK_NULL_HANDLE);
}

inline WeakRef<FenceCache> AutoFence::getFenceCache() const
{
    return m_cache;
}

inline WeakHandle<VkFence> AutoFence::getVkFenceHandle() const
{
    return m_fenceHandle;
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
