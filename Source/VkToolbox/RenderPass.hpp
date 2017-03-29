#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/RenderPass.hpp
// Author: Guilherme R. Lampert
// Created on: 24/03/17
// Brief: Vulkan render pass wrapper.
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

    void initialize(const VkRenderPassCreateInfo & rpCreateInfo);
    void shutdown();
    bool isInitialized() const;

    // Movable.
    RenderPass(RenderPass && other);
    RenderPass & operator = (RenderPass && other);

    // But not copyable.
    RenderPass(const RenderPass &) = delete;
    RenderPass & operator = (const RenderPass &) = delete;

    // Accessors:
    VkRenderPass getVkRenderPassHandle() const;
    const VulkanContext & getVkContext() const;

    // Implicit conversion to VkRenderPass.
    operator VkRenderPass() const { return m_renderPassHandle; }

private:

    VkRenderPass m_renderPassHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
};

// ========================================================

inline bool RenderPass::isInitialized() const
{
    return (m_renderPassHandle != VK_NULL_HANDLE);
}

inline VkRenderPass RenderPass::getVkRenderPassHandle() const
{
    return m_renderPassHandle;
}

inline const VulkanContext & RenderPass::getVkContext() const
{
    return *m_vkContext;
}

// ========================================================

} // namespace VkToolbox
