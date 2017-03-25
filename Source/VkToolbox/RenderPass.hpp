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

    explicit RenderPass(WeakRef<const VulkanContext> vkContext);
    RenderPass(WeakRef<const VulkanContext> vkContext,
               const VkRenderPassCreateInfo & rpCreateInfo);
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
    const VulkanContext & getVkContext() const;
    WeakHandle<VkRenderPass> getVkRenderPassHandle() const;

private:

    // Vulkan handles:
    OwnedHandle<VkRenderPass>    m_renderPassHandle;
    WeakRef<const VulkanContext> m_vkContext;
};

// ========================================================

inline bool RenderPass::isInitialized() const
{
    return (m_renderPassHandle != VK_NULL_HANDLE);
}

inline WeakHandle<VkRenderPass> RenderPass::getVkRenderPassHandle() const
{
    return m_renderPassHandle;
}

} // namespace VkToolbox
