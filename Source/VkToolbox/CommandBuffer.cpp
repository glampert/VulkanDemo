
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

CommandPool::CommandPool(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

CommandPool::CommandPool(const VulkanContext & vkContext, const VkCommandPoolCreateFlags flags, const int queueFamilyIndex)
    : m_vkContext{ &vkContext }
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

    VkCommandPoolCreateInfo cpCreateInfo;
    cpCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpCreateInfo.pNext            = nullptr;
    cpCreateInfo.queueFamilyIndex = queueFamilyIndex;
    cpCreateInfo.flags            = flags;

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

void CommandPool::reset() const
{
    VKTB_CHECK(vkResetCommandPool(m_vkContext->getVkDeviceHandle(), m_cmdPoolHandle, 0));
}

CommandPool::CommandPool(CommandPool && other)
    : m_cmdPoolHandle{ other.m_cmdPoolHandle }
    , m_vkContext{ other.m_vkContext }
    , m_queueFamilyIndex{ other.m_queueFamilyIndex }
{
    other.m_cmdPoolHandle = VK_NULL_HANDLE;
}

CommandPool & CommandPool::operator = (CommandPool && other)
{
    shutdown();

    m_cmdPoolHandle    = other.m_cmdPoolHandle;
    m_vkContext        = other.m_vkContext;
    m_queueFamilyIndex = other.m_queueFamilyIndex;

    other.m_cmdPoolHandle = VK_NULL_HANDLE;
    return *this;
}

// ========================================================
// class CommandBuffer:
// ========================================================

CommandBuffer::CommandBuffer(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

CommandBuffer::CommandBuffer(const VulkanContext & vkContext, const VkCommandBufferLevel lvl, VkCommandPool pool)
    : m_vkContext{ &vkContext }
{
    initialize(lvl, pool);
}

CommandBuffer::~CommandBuffer()
{
    shutdown();
}

void CommandBuffer::initialize(const VkCommandBufferLevel lvl, VkCommandPool pool)
{
    assert(m_cmdBufferHandle == VK_NULL_HANDLE); // Prevent double init

    assert(pool != VK_NULL_HANDLE);
    m_cmdPoolHandle = pool;

    VkCommandBufferAllocateInfo cmdAllocInfo;
    cmdAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext              = nullptr;
    cmdAllocInfo.commandPool        = m_cmdPoolHandle;
    cmdAllocInfo.level              = lvl;
    cmdAllocInfo.commandBufferCount = 1;

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

void CommandBuffer::reset() const
{
    VKTB_CHECK(vkResetCommandBuffer(m_cmdBufferHandle, 0));
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

    VkCommandBufferBeginInfo cmdBufBeginInfo;
    cmdBufBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufBeginInfo.pNext            = nullptr;
    cmdBufBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    cmdBufBeginInfo.pInheritanceInfo = nullptr;

    VKTB_CHECK(vkBeginCommandBuffer(m_cmdBufferHandle, &cmdBufBeginInfo));
    m_stateFlags = FlagRecordingState;
}

void CommandBuffer::endRecording() const
{
    assert(!isInSubmissionState() && isInRecordingState());
    VKTB_CHECK(vkEndCommandBuffer(m_cmdBufferHandle));
    m_stateFlags = FlagSubmissionState;
}

void CommandBuffer::submit(VkQueue queue, VkFence fence) const
{
    assert(queue != VK_NULL_HANDLE);
    assert(isInSubmissionState());

    VkSubmitInfo submitInfo;
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext                = nullptr;
    submitInfo.waitSemaphoreCount   = 0;
    submitInfo.pWaitSemaphores      = nullptr;
    submitInfo.pWaitDstStageMask    = nullptr;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &m_cmdBufferHandle;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores    = nullptr;

    VKTB_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
}

void CommandBuffer::submitAndWaitComplete(VkQueue queue) const
{
    AutoFence fence{ m_vkContext->getMainFenceCache() };
    submit(queue, fence);
}

} // namespace VkToolbox
