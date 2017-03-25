
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/CommandBuffer.cpp
// Author: Guilherme R. Lampert
// Created on: 24/03/17
// Brief: Vulkan command buffer wrapper.
// ================================================================================================

#include "CommandBuffer.hpp"
#include "VulkanContext.hpp"

namespace VkToolbox
{

// ========================================================
// class CommandPool:
// ========================================================

CommandPool::CommandPool(WeakRef<const VulkanContext> vkContext)
    : m_cmdPoolHandle{ VK_NULL_HANDLE }
    , m_vkContext{ vkContext }
    , m_queueFamilyIndex{ -1 }
{
}

CommandPool::CommandPool(WeakRef<const VulkanContext> vkContext,
                         const VkCommandPoolCreateFlags flags,
                         const int queueFamilyIndex)
    : m_cmdPoolHandle{ VK_NULL_HANDLE }
    , m_vkContext{ vkContext }
{
    initialize(flags, queueFamilyIndex);
}

CommandPool::~CommandPool()
{
    shutdown();
}

void CommandPool::initialize(const VkCommandPoolCreateFlags flags, const int queueFamilyIndex)
{
    assert(m_cmdPoolHandle == VK_NULL_HANDLE); // Prevent double init

    VkCommandPoolCreateInfo cpCreateInfo = {};
    cpCreateInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpCreateInfo.pNext                   = nullptr;
    cpCreateInfo.queueFamilyIndex        = queueFamilyIndex;
    cpCreateInfo.flags                   = flags;

    m_queueFamilyIndex = queueFamilyIndex;
    VKTB_CHECK(vkCreateCommandPool(m_vkContext->getVkDeviceHandle(), &cpCreateInfo,
                                   m_vkContext->getAllocationCallbacks(), &m_cmdPoolHandle));

    assert(m_cmdPoolHandle != VK_NULL_HANDLE);
}

void CommandPool::shutdown()
{
    if (m_cmdPoolHandle != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_vkContext->getVkDeviceHandle(), m_cmdPoolHandle,
                             m_vkContext->getAllocationCallbacks());

        m_cmdPoolHandle    = VK_NULL_HANDLE;
        m_queueFamilyIndex = -1;
    }
}

CommandPool::CommandPool(CommandPool && other)
    : m_cmdPoolHandle{ other.m_cmdPoolHandle }
    , m_vkContext{ other.m_vkContext }
    , m_queueFamilyIndex{ other.m_queueFamilyIndex }
{
    other.m_cmdPoolHandle = VK_NULL_HANDLE;
    other.m_vkContext     = nullptr;
}

CommandPool & CommandPool::operator = (CommandPool && other)
{
    shutdown();

    m_cmdPoolHandle       = other.m_cmdPoolHandle;
    m_vkContext           = other.m_vkContext;
    m_queueFamilyIndex    = other.m_queueFamilyIndex;

    other.m_cmdPoolHandle = VK_NULL_HANDLE;
    other.m_vkContext     = nullptr;

    return *this;
}

const VulkanContext & CommandPool::getVkContext() const
{
    assert(m_vkContext != nullptr);
    return *m_vkContext;
}

// ========================================================
// class CommandBuffer:
// ========================================================

CommandBuffer::CommandBuffer(WeakRef<const VulkanContext> vkContext)
    : m_vkContext{ vkContext }
{
}

CommandBuffer::CommandBuffer(WeakRef<const VulkanContext> vkContext,
                             const VkCommandBufferLevel lvl,
                             WeakHandle<VkCommandPool> pool)
    : m_vkContext{ vkContext }
{
    initialize(lvl, pool);
}

CommandBuffer::~CommandBuffer()
{
    shutdown();
}

void CommandBuffer::initialize(const VkCommandBufferLevel lvl, WeakHandle<VkCommandPool> pool)
{
    assert(m_cmdBufferHandle == VK_NULL_HANDLE); // Prevent double init

    assert(pool != VK_NULL_HANDLE);
    m_cmdPoolHandle = pool;

    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext                       = nullptr;
    cmdAllocInfo.commandPool                 = m_cmdPoolHandle;
    cmdAllocInfo.level                       = lvl;
    cmdAllocInfo.commandBufferCount          = 1;

    VKTB_CHECK(vkAllocateCommandBuffers(m_vkContext->getVkDeviceHandle(),
                                        &cmdAllocInfo, &m_cmdBufferHandle));

    assert(m_cmdBufferHandle != VK_NULL_HANDLE);
}

void CommandBuffer::shutdown()
{
    if (m_cmdBufferHandle != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_vkContext->getVkDeviceHandle(),
                             m_cmdPoolHandle, 1, &m_cmdBufferHandle);

        m_cmdBufferHandle = VK_NULL_HANDLE;
        m_cmdPoolHandle   = VK_NULL_HANDLE;
    }
}

CommandBuffer::CommandBuffer(CommandBuffer && other)
    : m_cmdBufferHandle{ other.m_cmdBufferHandle }
    , m_cmdPoolHandle{ other.m_cmdPoolHandle }
    , m_vkContext{ other.m_vkContext }
    , m_stateFlags{ other.m_stateFlags }
{
    other.m_cmdBufferHandle = VK_NULL_HANDLE;
    other.m_cmdPoolHandle   = VK_NULL_HANDLE;
}

CommandBuffer & CommandBuffer::operator = (CommandBuffer && other)
{
    shutdown();

    m_cmdBufferHandle = other.m_cmdBufferHandle;
    m_cmdPoolHandle   = other.m_cmdPoolHandle;
    m_vkContext       = other.m_vkContext;
    m_stateFlags      = other.m_stateFlags;

    other.m_cmdBufferHandle = VK_NULL_HANDLE;
    other.m_cmdPoolHandle   = VK_NULL_HANDLE;

    return *this;
}

void CommandBuffer::beginRecording() const
{
    assert(!isInRecordingState());

    VkCommandBufferBeginInfo cmdBufBeginInfo = {};
    cmdBufBeginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufBeginInfo.pNext                    = nullptr;
    cmdBufBeginInfo.flags                    = 0;
    cmdBufBeginInfo.pInheritanceInfo         = nullptr;

    VKTB_CHECK(vkBeginCommandBuffer(m_cmdBufferHandle, &cmdBufBeginInfo));
    m_stateFlags = FlagRecordingState;
}

void CommandBuffer::endRecording() const
{
    assert(!isInExecutionState() && isInRecordingState());
    VKTB_CHECK(vkEndCommandBuffer(m_cmdBufferHandle));
    m_stateFlags = FlagExecutionState;
}

void CommandBuffer::submit(WeakHandle<VkQueue> queue, WeakHandle<VkFence> fence) const
{
    assert(queue != VK_NULL_HANDLE);
    const VkCommandBuffer cmdBuffers[] = { m_cmdBufferHandle };

    VkSubmitInfo submitInfo         = {};
    submitInfo.pNext                = nullptr;
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 0;
    submitInfo.pWaitSemaphores      = nullptr;
    submitInfo.pWaitDstStageMask    = nullptr;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = cmdBuffers;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores    = nullptr;

    VKTB_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
}

const VulkanContext & CommandBuffer::getVkContext() const
{
    assert(m_vkContext != nullptr);
    return *m_vkContext;
}

} // namespace VkToolbox
