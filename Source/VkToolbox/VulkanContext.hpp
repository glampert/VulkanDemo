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
    static bool           sm_useTripleBufferHint;
    static ValidationMode sm_validationMode;

    // Static initialization and shutdown for internal shared state.
    // Call at the beginning/end of main().
    static void initClass();
    static void shutdownClass();

    // Instance initialization.
    VulkanContext(const OSWindow & window, Size2D fbSize, const VkAllocationCallbacks * allocCBs = nullptr);

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
    VkDevice getVkDeviceHandle() const;
    const VkPhysicalDeviceFeatures & getDeviceFeatures() const;
    const VkPhysicalDeviceProperties & getDeviceProperties() const;
    const VkPhysicalDeviceMemoryProperties & getDeviceMemoryProperties() const;
    const VkFormatProperties & getVkFormatPropertiesForImageFormat(Image::Format format) const;
    const VkImageFormatProperties & getVkImageFormatPropertiesForImageFormat(Image::Format format) const;

    // Accessors:
    const OSWindow & getRenderWindow() const;
    const VkAllocationCallbacks * getAllocationCallbacks() const;
    VkFramebuffer getVkFramebufferHandle(int index) const;
    Size2D getRenderWindowSize()   const;
    Size2D getFramebufferSize()    const;
    std::uint32_t getFrameNumber() const;
    int getSwapChainBufferIndex()  const;
    int getSwapChainBufferCount()  const;

    // Shareable objects:
    FenceCache * getMainFenceCache() const;
    const RenderPass & getMainRenderPass() const;
    const CommandPool & getMainTextureStagingCmdBufferPool() const;
    const CommandBuffer & getMainTextureStagingCmdBuffer() const;

    //
    // Frame rendering:
    //

    void beginFrame();
    void endFrame(array_view<const VkCommandBuffer> submitBuffers, VkFence fence = VK_NULL_HANDLE);
    void waitGpuIdle() const;

    //
    // GPU queues info:
    //

    struct GpuQueue
    {
        VkQueue      queue       = VK_NULL_HANDLE;
        std::int32_t familyIndex = -1;

        operator VkQueue() const { return queue; }
    };

    const GpuQueue & getPresentQueue() const;
    const GpuQueue & getGraphisQueue() const;

    //
    // Miscellaneous helpers:
    //

    static constexpr auto ColorMaskRGBA = (VK_COLOR_COMPONENT_R_BIT |
                                           VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT |
                                           VK_COLOR_COMPONENT_A_BIT);

    std::uint32_t getMemoryTypeFromProperties(std::uint32_t typeBits, VkFlags requirementsMask) const;

    void changeImageLayout(const CommandBuffer * cmdBuff, VkImage image,
                           VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout,
                           VkImageLayout newImageLayout, int baseMipLevel = 0, int mipLevelCount = 1) const;

    void copyImage(const CommandBuffer * cmdBuff, VkImage srcImage,
                   VkImage dstImage, Size2D size) const;

    void createImage(const VkImageCreateInfo & imageInfo, VkMemoryPropertyFlags memoryProperties,
                     VkImage * outImage, VkDeviceMemory * outImageMemory) const;

    void copyBuffer(const CommandBuffer * cmdBuff, VkBuffer srcBuffer,
                    VkBuffer dstBuffer, VkDeviceSize sizeToCopy,
                    VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) const;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties,
                      VkBuffer * outBuffer, VkDeviceMemory * outBufferMemory) const;

private:

    struct LayerProperties
    {
        VkLayerProperties                  properties = {};
        std::vector<VkExtensionProperties> extensions = {};
    };

    struct DepthBuffer
    {
        VkImage        image  = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    struct SwapChainBuffer
    {
        VkImage       image       = VK_NULL_HANDLE; // Owned by the swap-chain
        VkImageView   view        = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };

    struct SwapChainState
    {
        Size2D                         framebufferSize = {0,0};
        std::uint32_t                  bufferIndex     = 0;
        std::uint32_t                  bufferCount     = 0;
        VkSwapchainKHR                 handle          = VK_NULL_HANDLE;
        std::array<SwapChainBuffer, 3> buffers         = {};
    };

    struct GpuInfo
    {
        VkPhysicalDeviceFeatures         features         = {};
        VkPhysicalDeviceProperties       properties       = {};
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

    // Framebuffer/depth-buffer/misc initialization/shutdown:
    void initDepthBuffer();
    void destroyDepthBuffer();
    void initFramebuffers();
    void destroyFramebuffers();
    void initRenderPass();
    void initCommandPoolAndBuffers();
    void initSemaphores();
    void cacheFormatProperties();

private:

    // API global context.
    VkInstance m_instance = VK_NULL_HANDLE;

    // The rendering device we are going to use (always GPU 0 for now).
    VkDevice m_device = VK_NULL_HANDLE;

    // User provided allocations callbacks or nullptr.
    const VkAllocationCallbacks * m_allocationCallbacks = nullptr;

    // Ref to the OS Window we are rendering to (non-null).
    const OSWindow * m_renderWindow = nullptr;

    // Render target surface for the rendering window (our screen framebuffer).
    VkSurfaceKHR m_renderSurface = VK_NULL_HANDLE;

    // Texture format of the rendering surface (the framebuffer).
    VkFormat m_renderSurfaceFormat = VK_FORMAT_UNDEFINED;

    // Incrementing value with the current frame number. Bumped each beginFrame().
    std::uint32_t m_frameNumber = 0;

    // Image swap-chain (framebuffers).
    SwapChainState m_swapChain;

    // Depth/stencil buffer images & views.
    DepthBuffer m_depthBuffer;

    // Semaphores to synchronize the swap-chain framebuffer swaps.
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;

    // Main/default render pass and shareable fence cache.
    RenderPass m_mainRenderPass;
    std::unique_ptr<FenceCache> m_mainFenceCache;

    // Command buffers used exclusively for texture uploads (staging resources).
    CommandPool m_mainTextureStagingCmdBufferPool;
    CommandBuffer m_mainTextureStagingCmdBuffer;

    // Handle to the "Physical Device", AKA the GPU. This handle is owned by the VK instance.
    // Vulkan allows explicitly selecting the device you want to use, on systems with more than one
    // GPU (e.g. SLI, Crossfire, etc). We will be sticking to a single GPU for now.
    VkPhysicalDevice m_gpuPhysDevice = VK_NULL_HANDLE;

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
    enum_array<Image::Format, VkFormatProperties> m_formatPropsCache;
    enum_array<Image::Format, VkImageFormatProperties> m_imageFormatPropsCache;

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;
};

// ========================================================
// class FenceCache:
// ========================================================

class FenceCache final
{
public:

    explicit FenceCache(const VulkanContext & vkContext)
        : m_vkContext{ &vkContext }
        , m_allocCount{ 0 }
    { }

    const VulkanContext & getVkContext() const { return *m_vkContext; }

    // Note: Will free all fences currently in the cache!
    // Make sure no external refs remain.
    void shutdown();
    ~FenceCache();

    // Not copyable.
    FenceCache(const FenceCache &) = delete;
    FenceCache & operator = (const FenceCache &) = delete;

    // Fill the cache with a preallocated number of fences, so next
    // allocRecyclableFence() call won't have to fill up the cache.
    // numFences must be <= 128.
    void preallocate(int numFences);

    // Get a temporary VkFence from the cache. Once you've waited on the fence,
    // you must call recycleFence() to return it to the cache. The cache cleans
    // up all remaining fences on shutdown.
    VkFence allocRecyclableFence();
    void recycleFence(VkFence fence);

private:

    VkFence newFence();

    const VulkanContext * m_vkContext;
    int m_allocCount;

    // Fixed-size cache of recyclable fence handles for AutoFences.
    FixedSizeArray<VkFence, 128> m_cache;
};

// ========================================================
// class AutoFence:
// ========================================================

// Amount of time, in nanoseconds, to wait for a fence/semaphore to be signaled.
constexpr auto DefaultFenceWaitTimeout  = UINT64_C(100000000);
constexpr auto InfiniteFenceWaitTimeout = UINT64_MAX;

// Automatically waits and recycles the underlaying VkFence object.
class AutoFence final
{
public:

    explicit AutoFence(FenceCache * cache);

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
    FenceCache * getFenceCache() const;
    VkFence getVkFenceHandle() const;

    // Implicit conversion to VkFence.
    operator VkFence() const { return m_fenceHandle; }

private:

    // Owned my the context and recycled once waited on.
    FenceCache * m_cache;
    VkFence      m_fenceHandle;
};

// ========================================================
// struct ScopedMapMemory:
// ========================================================

struct ScopedMapMemory final
{
    ScopedMapMemory(const VulkanContext & context, VkDeviceMemory memory, VkDeviceSize offset,
                    VkDeviceSize size, VkMemoryMapFlags flags, void ** ppData)
        : m_deviceHandle{ context.getVkDeviceHandle() }
        , m_memoryHandle{ memory }
    {
        const VkResult res = vkMapMemory(m_deviceHandle, memory, offset, size, flags, ppData);
        assert(res == VK_SUCCESS); (void)res;
    }

    ~ScopedMapMemory()
    {
        vkUnmapMemory(m_deviceHandle, m_memoryHandle);
    }

    ScopedMapMemory(const ScopedMapMemory &) = delete;
    ScopedMapMemory & operator = (const ScopedMapMemory &) = delete;

private:

    VkDevice       m_deviceHandle;
    VkDeviceMemory m_memoryHandle;
};

// ========================================================
// template class VDeleter:
// ========================================================

// Base on as class with same name used in Alexander Overvoorde's Vulkan Tutorials.
// https://github.com/Overv/VulkanTutorial
//
template<typename T>
class VDeleter final
{
public:

    using DeleteFunc = void (VKAPI_CALL*)(VkDevice, T, const VkAllocationCallbacks *);

    VDeleter(const VDeleter &) = delete;
    VDeleter & operator = (const VDeleter &) = delete;

    VDeleter(const VulkanContext * context, DeleteFunc deleter, T handle = VK_NULL_HANDLE)
        : m_handle{ handle }
        , m_deleter{ deleter }
        , m_vkContext{ context }
    { }

    ~VDeleter()
    {
        cleanup();
    }

    const T * operator & () const
    {
        return &m_handle;
    }

    operator T() const
    {
        return m_handle;
    }

    T * replace()
    {
        cleanup();
        return &m_handle;
    }

    VDeleter & operator = (T rhs)
    {
        if (rhs != m_handle)
        {
            cleanup();
            m_handle = rhs;
        }
        return *this;
    }

    template<typename V>
    bool operator == (V rhs) const
    {
        return m_handle == T(rhs);
    }

private:

    T                     m_handle;
    DeleteFunc            m_deleter;
    const VulkanContext * m_vkContex;

    void cleanup()
    {
        if (m_handle != VK_NULL_HANDLE)
        {
            m_deleter(m_vkContex->getVkDeviceHandle(), m_handle, m_vkContex->getAllocationCallbacks());
            m_handle = VK_NULL_HANDLE;
        }
    }
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

inline VkDevice VulkanContext::getVkDeviceHandle() const
{
    return m_device;
}

inline const VkPhysicalDeviceMemoryProperties & VulkanContext::getDeviceMemoryProperties() const
{
    return m_gpuInfo.memoryProperties;
}

inline const VkFormatProperties & VulkanContext::getVkFormatPropertiesForImageFormat(const Image::Format format) const
{
    return m_formatPropsCache[format];
}

inline const VkImageFormatProperties & VulkanContext::getVkImageFormatPropertiesForImageFormat(const Image::Format format) const
{
    return m_imageFormatPropsCache[format];
}

inline const VkPhysicalDeviceProperties & VulkanContext::getDeviceProperties() const
{
    return m_gpuInfo.properties;
}

inline const VkPhysicalDeviceFeatures & VulkanContext::getDeviceFeatures() const
{
    return m_gpuInfo.features;
}

inline const OSWindow & VulkanContext::getRenderWindow() const
{
    return *m_renderWindow;
}

inline const VkAllocationCallbacks * VulkanContext::getAllocationCallbacks() const
{
    return m_allocationCallbacks;
}

inline Size2D VulkanContext::getFramebufferSize() const
{
    return m_swapChain.framebufferSize;
}

inline std::uint32_t VulkanContext::getFrameNumber() const
{
    return m_frameNumber;
}

inline VkFramebuffer VulkanContext::getVkFramebufferHandle(const int index) const
{
    return m_swapChain.buffers[index].framebuffer;
}

inline int VulkanContext::getSwapChainBufferIndex() const
{
    return static_cast<int>(m_swapChain.bufferIndex);
}

inline int VulkanContext::getSwapChainBufferCount() const
{
    return static_cast<int>(m_swapChain.bufferCount);
}

inline FenceCache * VulkanContext::getMainFenceCache() const
{
    return m_mainFenceCache.get();
}

inline const RenderPass & VulkanContext::getMainRenderPass() const
{
    return m_mainRenderPass;
}

inline const CommandPool & VulkanContext::getMainTextureStagingCmdBufferPool() const
{
    return m_mainTextureStagingCmdBufferPool;
}

inline const CommandBuffer & VulkanContext::getMainTextureStagingCmdBuffer() const
{
    return m_mainTextureStagingCmdBuffer;
}

inline const VulkanContext::GpuQueue & VulkanContext::getPresentQueue() const
{
    return m_gpuPresentQueue;
}

inline const VulkanContext::GpuQueue & VulkanContext::getGraphisQueue() const
{
    return m_gpuGraphicsQueue;
}

inline void VulkanContext::waitGpuIdle() const
{
    vkDeviceWaitIdle(m_device);
}

// ========================================================
// AutoFence inline methods:
// ========================================================

inline AutoFence::AutoFence(FenceCache * cache)
    : m_cache{ cache }
    , m_fenceHandle{ m_cache->allocRecyclableFence() }
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

inline FenceCache * AutoFence::getFenceCache() const
{
    return m_cache;
}

inline VkFence AutoFence::getVkFenceHandle() const
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
