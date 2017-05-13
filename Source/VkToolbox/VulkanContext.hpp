#pragma once

// ================================================================================================
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
    VkDevice deviceHandle() const;
    const VkPhysicalDeviceFeatures & deviceFeatures() const;
    const VkPhysicalDeviceProperties & deviceProperties() const;
    const VkPhysicalDeviceMemoryProperties & deviceMemoryProperties() const;

    // Accessors:
    const OSWindow & renderWindow() const;
    const VkAllocationCallbacks * allocationCallbacks() const;
    VkFramebuffer framebufferHandle(int index) const;
    Size2D renderWindowSize()   const;
    Size2D framebufferSize()    const;
    float framebufferAspect()   const;
    std::uint32_t frameNumber() const;
    int swapChainBufferIndex()  const;
    int swapChainBufferCount()  const;

    // Shareable objects:
    FenceCache * mainFenceCache() const;
    const RenderPass & mainRenderPass() const;
    const CommandPool & mainTextureStagingCmdBufferPool() const;
    const CommandBuffer & mainTextureStagingCmdBuffer() const;

    //
    // Frame rendering / draw calls:
    //

    void beginFrame();
    void endFrame(array_view<const VkCommandBuffer> submitBuffers, VkFence fence = VK_NULL_HANDLE);

    void beginRenderPass(const CommandBuffer & cmdBuff) const;
    void endRenderPass(const CommandBuffer & cmdBuff) const;

    void bindGraphicsPipelineState(const CommandBuffer & cmdBuff, VkPipeline pipeline) const;
    void bindGraphicsDescriptorSets(const CommandBuffer & cmdBuff, VkPipelineLayout layout,
                                    array_view<const VkDescriptorSet> descriptorSets) const;

    void bindVertexBuffer(const CommandBuffer & cmdBuff, VkBuffer vb, std::uint32_t offsetInBytes = 0) const;
    void bindIndexBuffer(const CommandBuffer & cmdBuff, VkBuffer ib, VkIndexType type, std::uint32_t offsetInBytes = 0) const;

    void drawUnindexed(const CommandBuffer & cmdBuff,
                       std::uint32_t vertexCount, std::uint32_t instanceCount,
                       std::uint32_t firstVertex, std::uint32_t firstInstance) const;

    void drawIndexed(const CommandBuffer & cmdBuff,
                     std::uint32_t indexCount, std::uint32_t instanceCount,
                     std::uint32_t firstIndex, std::int32_t  vertexOffset,
                     std::uint32_t firstInstance) const;

    // Blocks until all pending GPU work is completed.
    void waitGpuIdle() const;

    // Default screen framebuffer clear-color and depth/stencil values.
    void setDefaultClearColor(const Vector4 & color);
    const Vector4 & defaultClearColor() const;
    void setDefaultClearDepth(float d);
    float defaultClearDepth() const;
    void setDefaultClearStencil(std::uint32_t s);
    std::uint32_t defaultClearStencil() const;

    //
    // GPU queues info:
    //

    struct GpuQueue
    {
        VkQueue      queue       = VK_NULL_HANDLE;
        std::int32_t familyIndex = -1;

        operator VkQueue() const { return queue; }
    };

    const GpuQueue & presentQueue() const;
    const GpuQueue & graphisQueue() const;

    //
    // Miscellaneous helpers:
    //

    std::uint32_t memoryTypeFromProperties(std::uint32_t typeBits, VkFlags requirementsMask) const;

    void changeImageLayout(const CommandBuffer & cmdBuff, VkImage image, VkImageAspectFlags aspectMask,
                           VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                           int baseMipLevel = 0, int mipLevelCount = 1,
                           int baseLayer = 0, int layerCount = 1) const;

    void copyImage(const CommandBuffer & cmdBuff, VkImage srcImage,
                   VkImage dstImage, Size2D size) const;

    void createImage(const VkImageCreateInfo & imageInfo, VkMemoryPropertyFlags memoryProperties,
                     VkImage * outImage, VkDeviceMemory * outImageMemory) const;

    void copyBuffer(const CommandBuffer & cmdBuff, VkBuffer srcBuffer,
                    VkBuffer dstBuffer, VkDeviceSize sizeToCopy,
                    VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) const;

    void createBuffer(VkDeviceSize sizeBytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties,
                      VkBuffer * outBuffer, VkDeviceMemory * outBufferMemory,
                      VkMemoryRequirements * outOptMemReqs = nullptr) const;

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
    Vector4 m_colorClearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
    float m_depthClearValue = 1.0f; // RH, GL-style, depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL
    std::uint32_t m_stencilClearValue = 0;
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

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;
};

// ========================================================

constexpr auto ColorMaskRGBA = (VK_COLOR_COMPONENT_R_BIT |
                                VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT |
                                VK_COLOR_COMPONENT_A_BIT);

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

    const VulkanContext & context() const { return *m_vkContext; }

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

    // Not copyable.
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
    FenceCache * fenceCache() const { return m_cache; }
    VkFence fenceHandle() const { return m_fenceHandle; }
    operator VkFence() const { return m_fenceHandle; }

private:

    // Owned by the context and recycled once waited on.
    FenceCache * m_cache;
    VkFence      m_fenceHandle;
};

// ========================================================
// class Fence:
// ========================================================

class Fence final
{
public:

    explicit Fence(const VulkanContext & vkContext) // Uninitialized fence - call initialize() before use
        : m_vkContext{ &vkContext }
    { }

    Fence(const VulkanContext & vkContext, const VkFenceCreateFlags initFlags)
        : m_vkContext{ &vkContext }
    {
        initialize(initFlags);
    }

    ~Fence() { shutdown(); }

    // Not copyable.
    Fence(const Fence &) = delete;
    Fence & operator = (const Fence &) = delete;

    void initialize(VkFenceCreateFlags flags);
    void shutdown();

    bool wait(std::uint64_t timeout = DefaultFenceWaitTimeout) const;
    bool isSignaled() const;
    void reset();

     // Accessors:
    const VulkanContext & context() const { return *m_vkContext; }
    VkFence fenceHandle() const { return m_fenceHandle; }
    operator VkFence() const { return m_fenceHandle; }

private:

    VkFence m_fenceHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
};

// ========================================================
// struct ScopedMapMemory:
// ========================================================

struct ScopedMapMemory final
{
    ScopedMapMemory(const VulkanContext & context, VkDeviceMemory memory, VkDeviceSize offset,
                    VkDeviceSize size, VkMemoryMapFlags flags, void ** ppData)
        : m_deviceHandle{ context.deviceHandle() }
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
            m_deleter(m_vkContex->deviceHandle(), m_handle, m_vkContex->allocationCallbacks());
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

inline VkDevice VulkanContext::deviceHandle() const
{
    return m_device;
}

inline const VkPhysicalDeviceMemoryProperties & VulkanContext::deviceMemoryProperties() const
{
    return m_gpuInfo.memoryProperties;
}

inline const VkPhysicalDeviceProperties & VulkanContext::deviceProperties() const
{
    return m_gpuInfo.properties;
}

inline const VkPhysicalDeviceFeatures & VulkanContext::deviceFeatures() const
{
    return m_gpuInfo.features;
}

inline const OSWindow & VulkanContext::renderWindow() const
{
    return *m_renderWindow;
}

inline const VkAllocationCallbacks * VulkanContext::allocationCallbacks() const
{
    return m_allocationCallbacks;
}

inline Size2D VulkanContext::framebufferSize() const
{
    return m_swapChain.framebufferSize;
}

inline float VulkanContext::framebufferAspect() const
{
    return static_cast<float>(framebufferSize().width) / static_cast<float>(framebufferSize().height);
}

inline std::uint32_t VulkanContext::frameNumber() const
{
    return m_frameNumber;
}

inline VkFramebuffer VulkanContext::framebufferHandle(const int index) const
{
    return m_swapChain.buffers[index].framebuffer;
}

inline int VulkanContext::swapChainBufferIndex() const
{
    return static_cast<int>(m_swapChain.bufferIndex);
}

inline int VulkanContext::swapChainBufferCount() const
{
    return static_cast<int>(m_swapChain.bufferCount);
}

inline FenceCache * VulkanContext::mainFenceCache() const
{
    return m_mainFenceCache.get();
}

inline const RenderPass & VulkanContext::mainRenderPass() const
{
    return m_mainRenderPass;
}

inline const CommandPool & VulkanContext::mainTextureStagingCmdBufferPool() const
{
    return m_mainTextureStagingCmdBufferPool;
}

inline const CommandBuffer & VulkanContext::mainTextureStagingCmdBuffer() const
{
    return m_mainTextureStagingCmdBuffer;
}

inline const VulkanContext::GpuQueue & VulkanContext::presentQueue() const
{
    return m_gpuPresentQueue;
}

inline const VulkanContext::GpuQueue & VulkanContext::graphisQueue() const
{
    return m_gpuGraphicsQueue;
}

inline void VulkanContext::setDefaultClearColor(const Vector4 & color)
{
    m_colorClearValue = color;
}

inline const Vector4 & VulkanContext::defaultClearColor() const
{
    return m_colorClearValue;
}

inline void VulkanContext::setDefaultClearDepth(const float d)
{
    m_depthClearValue = d;
}

inline float VulkanContext::defaultClearDepth() const
{
    return m_depthClearValue;
}

inline void VulkanContext::setDefaultClearStencil(const std::uint32_t s)
{
    m_stencilClearValue = s;
}

inline std::uint32_t VulkanContext::defaultClearStencil() const
{
    return m_stencilClearValue;
}

inline void VulkanContext::beginRenderPass(const CommandBuffer & cmdBuff) const
{
    assert(cmdBuff.isInRecordingState());

    VkClearValue clearValues[2] = {}; // color+depth
    clearValues[0].color.float32[0]     = m_colorClearValue[0];
    clearValues[0].color.float32[1]     = m_colorClearValue[1];
    clearValues[0].color.float32[2]     = m_colorClearValue[2];
    clearValues[0].color.float32[3]     = m_colorClearValue[3];
    clearValues[1].depthStencil.depth   = m_depthClearValue;
    clearValues[1].depthStencil.stencil = m_stencilClearValue;

    VkRenderPassBeginInfo renderPassInfo;
    renderPassInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.pNext                    = nullptr;
    renderPassInfo.renderPass               = mainRenderPass();
    renderPassInfo.framebuffer              = framebufferHandle(swapChainBufferIndex());
    renderPassInfo.renderArea.offset.x      = 0;
    renderPassInfo.renderArea.offset.y      = 0;
    renderPassInfo.renderArea.extent.width  = framebufferSize().width;
    renderPassInfo.renderArea.extent.height = framebufferSize().height;
    renderPassInfo.clearValueCount          = 2; // Number of attachments in the pass
    renderPassInfo.pClearValues             = clearValues;

    vkCmdBeginRenderPass(cmdBuff.commandBufferHandle(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

inline void VulkanContext::endRenderPass(const CommandBuffer & cmdBuff) const
{
    assert(cmdBuff.isInRecordingState());
    vkCmdEndRenderPass(cmdBuff.commandBufferHandle());
}

inline void VulkanContext::bindGraphicsPipelineState(const CommandBuffer & cmdBuff, VkPipeline pipeline) const
{
    assert(pipeline != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());
    vkCmdBindPipeline(cmdBuff.commandBufferHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

inline void VulkanContext::bindGraphicsDescriptorSets(const CommandBuffer & cmdBuff, VkPipelineLayout layout,
                                                      array_view<const VkDescriptorSet> descriptorSets) const
{
    assert(layout != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());
    vkCmdBindDescriptorSets(cmdBuff.commandBufferHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                            static_cast<std::uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

inline void VulkanContext::bindVertexBuffer(const CommandBuffer & cmdBuff, VkBuffer vb, const std::uint32_t offsetInBytes) const
{
    assert(vb != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());
    const VkBuffer vertexBuffers[] = { vb };
    const VkDeviceSize offsets[]   = { offsetInBytes };
    vkCmdBindVertexBuffers(cmdBuff.commandBufferHandle(), 0, 1, vertexBuffers, offsets);
}

inline void VulkanContext::bindIndexBuffer(const CommandBuffer & cmdBuff, VkBuffer ib,
                                           const VkIndexType type, const std::uint32_t offsetInBytes) const
{
    assert(ib != VK_NULL_HANDLE);
    assert(cmdBuff.isInRecordingState());
    vkCmdBindIndexBuffer(cmdBuff.commandBufferHandle(), ib, offsetInBytes, type);
}

inline void VulkanContext::drawUnindexed(const CommandBuffer & cmdBuff,
                                         std::uint32_t vertexCount, std::uint32_t instanceCount,
                                         std::uint32_t firstVertex, std::uint32_t firstInstance) const
{
    assert(cmdBuff.isInRecordingState());
    vkCmdDraw(cmdBuff.commandBufferHandle(), vertexCount, instanceCount, firstVertex, firstInstance);
}

inline void VulkanContext::drawIndexed(const CommandBuffer & cmdBuff,
                                       std::uint32_t indexCount, std::uint32_t instanceCount,
                                       std::uint32_t firstIndex, std::int32_t  vertexOffset,
                                       std::uint32_t firstInstance) const
{
    assert(cmdBuff.isInRecordingState());
    vkCmdDrawIndexed(cmdBuff.commandBufferHandle(), indexCount, instanceCount,
                     firstIndex, vertexOffset, firstInstance);
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

inline bool AutoFence::isWaitable() const
{
    return (m_fenceHandle != VK_NULL_HANDLE);
}

// ========================================================
// Fence inline methods:
// ========================================================

inline void Fence::initialize(const VkFenceCreateFlags flags)
{
    assert(m_fenceHandle == VK_NULL_HANDLE);

    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = flags;

    const VkResult res = vkCreateFence(m_vkContext->deviceHandle(), &fenceCreateInfo,
                                       m_vkContext->allocationCallbacks(), &m_fenceHandle);
    assert(res == VK_SUCCESS); (void)res;
}

inline void Fence::shutdown()
{
    if (m_fenceHandle != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_vkContext->deviceHandle(), m_fenceHandle, m_vkContext->allocationCallbacks());
        m_fenceHandle = VK_NULL_HANDLE;
    }
}

inline bool Fence::wait(const std::uint64_t timeout) const
{
    assert(m_fenceHandle != VK_NULL_HANDLE);
    const VkResult res = vkWaitForFences(m_vkContext->deviceHandle(), 1, &m_fenceHandle, VK_TRUE, timeout);
    if (res == VK_TIMEOUT)
    {
        return false;
    }
    else if (res == VK_SUCCESS)
    {
        return true;
    }
    else // Some other internal failure...
    {
        Log::fatalF("vkWaitForFences() failed with error %#x", res);
    }
}

inline bool Fence::isSignaled() const
{
    assert(m_fenceHandle != VK_NULL_HANDLE);
    const VkResult res = vkGetFenceStatus(m_vkContext->deviceHandle(), m_fenceHandle);
    return (res == VK_SUCCESS);
}

inline void Fence::reset()
{
    vkResetFences(m_vkContext->deviceHandle(), 1, &m_fenceHandle);
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
