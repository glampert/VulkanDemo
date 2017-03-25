
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/RenderPass.cpp
// Author: Guilherme R. Lampert
// Created on: 24/03/17
// Brief: Vulkan render pass wrapper.
// ================================================================================================

#include "RenderPass.hpp"
#include "VulkanContext.hpp"

namespace VkToolbox
{

RenderPass::RenderPass(WeakRef<const VulkanContext> vkContext)
    : m_renderPassHandle{ VK_NULL_HANDLE }
    , m_vkContext{ vkContext }
{
}

RenderPass::RenderPass(WeakRef<const VulkanContext> vkContext, const VkRenderPassCreateInfo & rpCreateInfo)
    : m_renderPassHandle{ VK_NULL_HANDLE }
    , m_vkContext{ vkContext }
{
    initialize(rpCreateInfo);
}

RenderPass::~RenderPass()
{
    shutdown();
}

void RenderPass::initialize(const VkRenderPassCreateInfo & rpCreateInfo)
{
    assert(m_renderPassHandle == VK_NULL_HANDLE); // Prevent double init

    VKTB_CHECK(vkCreateRenderPass(m_vkContext->getVkDeviceHandle(), &rpCreateInfo,
                                  m_vkContext->getAllocationCallbacks(), &m_renderPassHandle));

    assert(m_renderPassHandle != VK_NULL_HANDLE);
}

void RenderPass::shutdown()
{
    if (m_renderPassHandle != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_vkContext->getVkDeviceHandle(), m_renderPassHandle,
                            m_vkContext->getAllocationCallbacks());

        m_renderPassHandle = VK_NULL_HANDLE;
    }
}

RenderPass::RenderPass(RenderPass && other)
    : m_renderPassHandle{ other.m_renderPassHandle }
    , m_vkContext{ other.m_vkContext }
{
    other.m_renderPassHandle = VK_NULL_HANDLE;
    other.m_vkContext        = nullptr;
}

RenderPass & RenderPass::operator = (RenderPass && other)
{
    shutdown();

    m_renderPassHandle       = other.m_renderPassHandle;
    m_vkContext              = other.m_vkContext;

    other.m_renderPassHandle = VK_NULL_HANDLE;
    other.m_vkContext        = nullptr;

    return *this;
}

const VulkanContext & RenderPass::getVkContext() const
{
    assert(m_vkContext != nullptr);
    return *m_vkContext;
}

} // namespace VkToolbox
