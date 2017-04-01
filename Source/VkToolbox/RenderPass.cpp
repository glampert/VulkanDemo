
// ================================================================================================
// File: VkToolbox/RenderPass.cpp
// Author: Guilherme R. Lampert
// Created on: 24/03/17
// Brief: Vulkan render pass wrapper.
// ================================================================================================

#include "RenderPass.hpp"
#include "VulkanContext.hpp"

namespace VkToolbox
{

RenderPass::RenderPass(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

RenderPass::RenderPass(const VulkanContext & vkContext, const VkRenderPassCreateInfo & rpCreateInfo)
    : m_vkContext{ &vkContext }
{
    initialize(rpCreateInfo);
}

RenderPass::~RenderPass()
{
    shutdown();
}

void RenderPass::initialize(const VkRenderPassCreateInfo & rpCreateInfo)
{
    assert(!isInitialized()); // Prevent double init

    VKTB_CHECK(vkCreateRenderPass(m_vkContext->deviceHandle(), &rpCreateInfo,
                                  m_vkContext->allocationCallbacks(), &m_renderPassHandle));

    assert(m_renderPassHandle != VK_NULL_HANDLE);
}

void RenderPass::shutdown()
{
    if (m_renderPassHandle != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_vkContext->deviceHandle(), m_renderPassHandle,
                            m_vkContext->allocationCallbacks());
        m_renderPassHandle = VK_NULL_HANDLE;
    }
}

} // namespace VkToolbox
