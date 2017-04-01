#pragma once

// ================================================================================================
// File: VkToolbox/RenderPass.hpp
// Author: Guilherme R. Lampert
// Created on: 24/03/17
// Brief: Vulkan Render Pass wrapper.
// ================================================================================================

#include <vulkan/vulkan.h>
#include "Utils.hpp"

namespace VkToolbox
{

class VulkanContext;

// ========================================================
// class RenderPass:
// ========================================================

class RenderPass final
{
public:

    explicit RenderPass(const VulkanContext & vkContext);
    RenderPass(const VulkanContext & vkContext, const VkRenderPassCreateInfo & rpCreateInfo);
    ~RenderPass();

    RenderPass(const RenderPass &) = delete;
    RenderPass & operator = (const RenderPass &) = delete;

    void initialize(const VkRenderPassCreateInfo & rpCreateInfo);
    void shutdown();
    bool isInitialized() const;

    const VulkanContext & context() const { return *m_vkContext; }
    VkRenderPass renderPassHandle() const { return m_renderPassHandle; }
    operator VkRenderPass() const { return m_renderPassHandle; }

private:

    VkRenderPass m_renderPassHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
};

// ========================================================
// RenderPass inline methods:
// ========================================================

inline RenderPass::RenderPass(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

inline RenderPass::RenderPass(const VulkanContext & vkContext, const VkRenderPassCreateInfo & rpCreateInfo)
    : m_vkContext{ &vkContext }
{
    initialize(rpCreateInfo);
}

inline RenderPass::~RenderPass()
{
    shutdown();
}

inline bool RenderPass::isInitialized() const
{
    return (m_renderPassHandle != VK_NULL_HANDLE);
}

// ========================================================

} // namespace VkToolbox
