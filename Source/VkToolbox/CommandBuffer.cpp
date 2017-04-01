
// ================================================================================================
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

void CommandPool::initialize(const VkCommandPoolCreateFlags flags, const int queueFamilyIndex)
{
    assert(!isInitialized()); // Prevent double init

    VkCommandPoolCreateInfo cpCreateInfo;
    cpCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpCreateInfo.pNext            = nullptr;
    cpCreateInfo.queueFamilyIndex = queueFamilyIndex;
    cpCreateInfo.flags            = flags;

    m_queueFamilyIndex = queueFamilyIndex;
    VKTB_CHECK(vkCreateCommandPool(m_vkContext->deviceHandle(), &cpCreateInfo,
                                   m_vkContext->allocationCallbacks(), &m_cmdPoolHandle));

    assert(m_cmdPoolHandle != VK_NULL_HANDLE);
}

void CommandPool::shutdown()
{
    if (m_cmdPoolHandle != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_vkContext->deviceHandle(), m_cmdPoolHandle,
                             m_vkContext->allocationCallbacks());
        m_cmdPoolHandle = VK_NULL_HANDLE;
    }
}

void CommandPool::reset() const
{
    VKTB_CHECK(vkResetCommandPool(m_vkContext->deviceHandle(), m_cmdPoolHandle, 0));
}

// ========================================================
// class CommandBuffer:
// ========================================================

void CommandBuffer::initialize(const VkCommandBufferLevel lvl, VkCommandPool pool)
{
    assert(!isInitialized()); // Prevent double init
    assert(pool != VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo cmdAllocInfo;
    cmdAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext              = nullptr;
    cmdAllocInfo.commandPool        = pool;
    cmdAllocInfo.level              = lvl;
    cmdAllocInfo.commandBufferCount = 1;

    m_cmdPoolHandle = pool;
    VKTB_CHECK(vkAllocateCommandBuffers(m_vkContext->deviceHandle(), &cmdAllocInfo, &m_cmdBufferHandle));
    assert(m_cmdBufferHandle != VK_NULL_HANDLE);
}

void CommandBuffer::shutdown()
{
    if (m_cmdBufferHandle != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_vkContext->deviceHandle(), m_cmdPoolHandle, 1, &m_cmdBufferHandle);
        m_cmdBufferHandle = VK_NULL_HANDLE;
        m_cmdPoolHandle   = VK_NULL_HANDLE;
    }
}

void CommandBuffer::reset() const
{
    VKTB_CHECK(vkResetCommandBuffer(m_cmdBufferHandle, 0));
    m_stateFlags = 0;
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
    AutoFence fence{ m_vkContext->mainFenceCache() };
    submit(queue, fence);
}

} // namespace VkToolbox
