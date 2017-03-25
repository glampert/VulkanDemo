#pragma once

// ================================================================================================
// -*- C++ -*-
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

    explicit CommandPool(WeakRef<const VulkanContext> vkContext);
    CommandPool(WeakRef<const VulkanContext> vkContext,
                VkCommandPoolCreateFlags flags, int queueFamilyIndex);
    ~CommandPool();

    void initialize(VkCommandPoolCreateFlags flags, int queueFamilyIndex);
    void shutdown();
    bool isInitialized() const;

    // Movable.
    CommandPool(CommandPool && other);
    CommandPool & operator = (CommandPool && other);

    // But not copyable.
    CommandPool(const CommandPool &) = delete;
    CommandPool & operator = (const CommandPool &) = delete;

    // Accessors:
    const VulkanContext & getVkContext() const;
    WeakHandle<VkCommandPool> getVkCmdPoolHandle() const;
    int getQueueFamilyIndex() const;

private:

    // Vulkan handles:
    OwnedHandle<VkCommandPool>   m_cmdPoolHandle;
    WeakRef<const VulkanContext> m_vkContext;
    int                          m_queueFamilyIndex;
};

// ========================================================
// class CommandBuffer:
// ========================================================

class CommandBuffer final
{
public:

    explicit CommandBuffer(WeakRef<const VulkanContext> vkContext);
    CommandBuffer(WeakRef<const VulkanContext> vkContext,
                  VkCommandBufferLevel lvl, WeakHandle<VkCommandPool> pool);
    ~CommandBuffer();

    void initialize(VkCommandBufferLevel lvl, WeakHandle<VkCommandPool> pool);
    void shutdown();
    bool isInitialized() const;

    // Movable.
    CommandBuffer(CommandBuffer && other);
    CommandBuffer & operator = (CommandBuffer && other);

    // But not copyable.
    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer & operator = (const CommandBuffer &) = delete;

    // Command recording:
    void beginRecording() const;
    void endRecording() const;

    // Submit/execute previously recorded buffer.
    void submit(WeakHandle<VkQueue> queue, WeakHandle<VkFence> fence = VK_NULL_HANDLE) const;

    // Accessors:
    const VulkanContext & getVkContext() const;
    WeakHandle<VkCommandPool> getVkCmdPoolHandle() const;
    WeakHandle<VkCommandBuffer> getVkCmdBufferHandle() const;

    bool isInRecordingState() const;
    bool isInExecutionState() const;

private:

    // Vulkan handles:
    OwnedHandle<VkCommandBuffer> m_cmdBufferHandle = VK_NULL_HANDLE;
    WeakHandle<VkCommandPool>    m_cmdPoolHandle   = VK_NULL_HANDLE;
    WeakRef<const VulkanContext> m_vkContext       = VK_NULL_HANDLE;

    enum
    {
        FlagRecordingState = (1 << 1), // Between vkBeginCommandBuffer and vkEndCommandBuffer
        FlagExecutionState = (1 << 2), // After vkEndCommandBuffer
    };
    mutable std::uint32_t m_stateFlags = 0;
};

// ========================================================

inline bool CommandPool::isInitialized() const
{
    return (m_cmdPoolHandle != VK_NULL_HANDLE);
}

inline WeakHandle<VkCommandPool> CommandPool::getVkCmdPoolHandle() const
{
    return m_cmdPoolHandle;
}

inline int CommandPool::getQueueFamilyIndex() const
{
    return m_queueFamilyIndex;
}

// ========================================================

inline bool CommandBuffer::isInitialized() const
{
    return (m_cmdBufferHandle != VK_NULL_HANDLE);
}

inline WeakHandle<VkCommandPool> CommandBuffer::getVkCmdPoolHandle() const
{
    return m_cmdPoolHandle;
}

inline WeakHandle<VkCommandBuffer> CommandBuffer::getVkCmdBufferHandle() const
{
    return m_cmdBufferHandle;
}

inline bool CommandBuffer::isInRecordingState() const
{
    return (m_stateFlags & FlagRecordingState) != 0;
}

inline bool CommandBuffer::isInExecutionState() const
{
    return (m_stateFlags & FlagExecutionState) != 0;
}

// ========================================================

} // namespace VkToolbox
