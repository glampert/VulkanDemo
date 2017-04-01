#pragma once

// ================================================================================================
// File: VkToolbox/CommandBuffer.hpp
// Author: Guilherme R. Lampert
// Created on: 24/03/17
// Brief: Vulkan command buffer wrapper.
// ================================================================================================

#include <vulkan/vulkan.h>
#include "Utils.hpp"

namespace VkToolbox
{

class VulkanContext;

// ========================================================
// class CommandPool:
// ========================================================

class CommandPool final
{
public:

    explicit CommandPool(const VulkanContext & vkContext);
    CommandPool(const VulkanContext & vkContext, VkCommandPoolCreateFlags flags, int queueFamilyIndex);
    ~CommandPool();

    CommandPool(const CommandPool &) = delete;
    CommandPool & operator = (const CommandPool &) = delete;

    void initialize(VkCommandPoolCreateFlags flags, int queueFamilyIndex);
    void shutdown();
    bool isInitialized() const;
    void reset() const;

    const VulkanContext & context() const { return *m_vkContext; }
    int queueFamilyIndex() const { return m_queueFamilyIndex; }

    VkCommandPool commandPoolHandle() const { return m_cmdPoolHandle; }
    operator VkCommandPool() const { return m_cmdPoolHandle; }

private:

    VkCommandPool         m_cmdPoolHandle    = VK_NULL_HANDLE;
    int                   m_queueFamilyIndex = -1;
    const VulkanContext * m_vkContext;
};

// ========================================================
// class CommandBuffer:
// ========================================================

class CommandBuffer final
{
public:

    explicit CommandBuffer(const VulkanContext & vkContext);
    CommandBuffer(const VulkanContext & vkContext, VkCommandBufferLevel lvl, VkCommandPool pool);
    ~CommandBuffer();

    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer & operator = (const CommandBuffer &) = delete;

    void initialize(VkCommandBufferLevel lvl, VkCommandPool pool);
    void shutdown();
    bool isInitialized() const;
    void reset() const;

    // Command recording.
    void beginRecording() const;
    void endRecording() const;

    bool isInRecordingState()  const;
    bool isInSubmissionState() const;

    // Submit/execute previously recorded buffer.
    void submit(VkQueue queue, VkFence fence = VK_NULL_HANDLE) const;

    // Submit and waits on a fence. Fence sourced from the context's main fence cache.
    void submitAndWaitComplete(VkQueue queue) const;

    const VulkanContext & context() const { return *m_vkContext; }
    VkCommandPool commandPoolHandle() const { return m_cmdPoolHandle; }

    VkCommandBuffer commandBufferHandle() const { return m_cmdBufferHandle; }
    operator VkCommandBuffer() const { return m_cmdBufferHandle; }

private:

    enum
    {
        FlagRecordingState  = (1 << 1), // Between vkBeginCommandBuffer and vkEndCommandBuffer
        FlagSubmissionState = (1 << 2), // After vkEndCommandBuffer
    };

    VkCommandBuffer       m_cmdBufferHandle = VK_NULL_HANDLE;
    VkCommandPool         m_cmdPoolHandle   = VK_NULL_HANDLE;
    mutable std::uint32_t m_stateFlags      = 0;
    const VulkanContext * m_vkContext;
};

// ========================================================
// CommandPool inline methods:
// ========================================================

inline CommandPool::CommandPool(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

inline CommandPool::CommandPool(const VulkanContext & vkContext,
                                const VkCommandPoolCreateFlags flags,
                                const int queueFamilyIndex)
    : m_vkContext{ &vkContext }
{
    initialize(flags, queueFamilyIndex);
}

inline CommandPool::~CommandPool()
{
    shutdown();
}

inline bool CommandPool::isInitialized() const
{
    return (m_cmdPoolHandle != VK_NULL_HANDLE);
}

// ========================================================
// CommandBuffer inline methods:
// ========================================================

inline CommandBuffer::CommandBuffer(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

inline CommandBuffer::CommandBuffer(const VulkanContext & vkContext,
                                    const VkCommandBufferLevel lvl,
                                    VkCommandPool pool)
    : m_vkContext{ &vkContext }
{
    initialize(lvl, pool);
}

inline CommandBuffer::~CommandBuffer()
{
    shutdown();
}

inline bool CommandBuffer::isInitialized() const
{
    return (m_cmdBufferHandle != VK_NULL_HANDLE);
}

inline bool CommandBuffer::isInRecordingState() const
{
    return (m_stateFlags & FlagRecordingState) != 0;
}

inline bool CommandBuffer::isInSubmissionState() const
{
    return (m_stateFlags & FlagSubmissionState) != 0;
}

// ========================================================

} // namespace VkToolbox
