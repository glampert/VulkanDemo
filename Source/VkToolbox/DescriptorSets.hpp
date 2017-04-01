#pragma once

// ================================================================================================
// File: VkToolbox/DescriptorSets.hpp
// Author: Guilherme R. Lampert
// Created on: 31/03/17
// Brief: Vulkan Shader Descriptor Pool and Set helpers.
// ================================================================================================

#include "VulkanContext.hpp"

namespace VkToolbox
{

// ----------------
// In essence:
//  + Each uniform variable, buffer UAV or sampler in a shader is a Descriptor.
//  + Each shader can have a set of one or more descriptors (a Descriptor Set).
//  + A shader can reference multiple Descriptor Sets, but in general it is a one-to-one mapping.
// ----------------
//  + DescriptorSetLayout is the data layout of all uniforms (Descriptors) in a shader.
//    A layout can be shared by shaders that use the same set of uniform variables/buffers/etc.
//  + DescriptorSet is the combined set of all descriptors in a shader. They have to be
//    tied to a data source (buffer), so each shader will generally have its own unique set.
//  + DescriptorSetPool is an optimized way of allocating DescriptorSets. Sets cannot be
//    individually freed, they are instead reclaimed when the owning pool is destroyed.
// ----------------

// ========================================================
// class DescriptorSetPool:
// ========================================================

class DescriptorSetPool final
{
public:

    explicit DescriptorSetPool(const VulkanContext & vkContext);
    DescriptorSetPool(const VulkanContext & vkContext, int maxSets,
                      array_view<const VkDescriptorPoolSize> sizesAndTypes);
    ~DescriptorSetPool();

    DescriptorSetPool(const DescriptorSetPool &) = delete;
    DescriptorSetPool & operator = (const DescriptorSetPool &) = delete;

    void initialize(int maxSets, array_view<const VkDescriptorPoolSize> sizesAndTypes);
    void shutdown();
    bool isInitialized() const;
    void reset() const;

    const VulkanContext & context() const { return *m_vkContext; }
    VkDescriptorPool descriptorPoolHandle() const { return m_descriptorPoolHandle; }
    operator VkDescriptorPool() const { return m_descriptorPoolHandle; }

private:

    VkDescriptorPool      m_descriptorPoolHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
};

// ========================================================
// class DescriptorSetLayout:
// ========================================================

class DescriptorSetLayout final
{
public:

    explicit DescriptorSetLayout(const VulkanContext & vkContext);
    DescriptorSetLayout(const VulkanContext & vkContext,
                        array_view<const VkDescriptorSetLayoutBinding> layoutBindings);
    ~DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout &) = delete;
    DescriptorSetLayout & operator = (const DescriptorSetLayout &) = delete;

    void initialize(array_view<const VkDescriptorSetLayoutBinding> layoutBindings);
    void shutdown();
    bool isInitialized() const;

    const VulkanContext & context() const { return *m_vkContext; }
    VkDescriptorSetLayout descriptorSetLayoutHandle() const { return m_descriptorSetLayoutHandle; }
    operator VkDescriptorSetLayout() const { return m_descriptorSetLayoutHandle; }

private:

    VkDescriptorSetLayout m_descriptorSetLayoutHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
};

// ========================================================
// template class DescriptorSets:
// ========================================================

template<int Size>
struct DescriptorSets final
{
    const DescriptorSetPool *         owningPool = nullptr;
    std::array<VkDescriptorSet, Size> descriptorSetHandles{};

    void initialize(const DescriptorSetPool * pool)
    {
        owningPool = pool;
    }

    void allocate(array_view<const VkDescriptorSetLayout> layouts)
    {
        assert(owningPool != nullptr);
        assert(layouts.size() == Size);

        VkDescriptorSetAllocateInfo allocInfo;
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext              = nullptr;
        allocInfo.descriptorPool     = owningPool->descriptorPoolHandle();
        allocInfo.descriptorSetCount = Size;
        allocInfo.pSetLayouts        = layouts.data();

        VKTB_CHECK(vkAllocateDescriptorSets(context().deviceHandle(), &allocInfo, descriptorSetHandles.data()));
    }

    void update(array_view<const VkWriteDescriptorSet> descriptorWrites) const
    {
        assert(!descriptorWrites.empty());
        const auto writeCount = static_cast<std::uint32_t>(descriptorWrites.size());
        vkUpdateDescriptorSets(context().deviceHandle(), writeCount, descriptorWrites.data(), 0, nullptr);
    }

    const VulkanContext & context() const
    {
        return owningPool->context();
    }

    static constexpr int size()
    {
        return Size;
    }
};

// ========================================================
// DescriptorSetPool inline methods:
// ========================================================

inline DescriptorSetPool::DescriptorSetPool(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
    // Uninitialized.
}

inline DescriptorSetPool::DescriptorSetPool(const VulkanContext & vkContext, const int maxSets,
                                            array_view<const VkDescriptorPoolSize> sizesAndTypes)
    : m_vkContext{ &vkContext }
{
    initialize(maxSets, sizesAndTypes);
}

inline DescriptorSetPool::~DescriptorSetPool()
{
    shutdown();
}

inline void DescriptorSetPool::initialize(const int maxSets, array_view<const VkDescriptorPoolSize> sizesAndTypes)
{
    assert(!isInitialized());
    assert(sizesAndTypes != nullptr);

    VkDescriptorPoolCreateInfo poolCreateInfo;
    poolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext         = nullptr;
    poolCreateInfo.flags         = 0;
    poolCreateInfo.maxSets       = maxSets;
    poolCreateInfo.poolSizeCount = static_cast<std::uint32_t>(sizesAndTypes.size());
    poolCreateInfo.pPoolSizes    = sizesAndTypes.data();

    VKTB_CHECK(vkCreateDescriptorPool(m_vkContext->deviceHandle(), &poolCreateInfo,
                                      m_vkContext->allocationCallbacks(), &m_descriptorPoolHandle));
}

inline void DescriptorSetPool::shutdown()
{
    if (m_descriptorPoolHandle != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_vkContext->deviceHandle(), m_descriptorPoolHandle,
                                m_vkContext->allocationCallbacks());
        m_descriptorPoolHandle = VK_NULL_HANDLE;
    }
}

inline bool DescriptorSetPool::isInitialized() const
{
    return (m_descriptorPoolHandle != VK_NULL_HANDLE);
}

inline void DescriptorSetPool::reset() const
{
    VKTB_CHECK(vkResetDescriptorPool(m_vkContext->deviceHandle(), m_descriptorPoolHandle, 0));
}

// ========================================================
// DescriptorSetLayout inline methods:
// =======================================================

inline DescriptorSetLayout::DescriptorSetLayout(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
    // Uninitialized.
}

inline DescriptorSetLayout::DescriptorSetLayout(const VulkanContext & vkContext,
                                                array_view<const VkDescriptorSetLayoutBinding> layoutBindings)
    : m_vkContext{ &vkContext }
{
    initialize(layoutBindings);
}

inline DescriptorSetLayout::~DescriptorSetLayout()
{
    shutdown();
}

inline void DescriptorSetLayout::initialize(array_view<const VkDescriptorSetLayoutBinding> layoutBindings)
{
    assert(!isInitialized());

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext        = nullptr;
    layoutCreateInfo.flags        = 0;
    layoutCreateInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
    layoutCreateInfo.pBindings    = layoutBindings.data();

    VKTB_CHECK(vkCreateDescriptorSetLayout(m_vkContext->deviceHandle(), &layoutCreateInfo,
                                           m_vkContext->allocationCallbacks(), &m_descriptorSetLayoutHandle));
}

inline void DescriptorSetLayout::shutdown()
{
    if (m_descriptorSetLayoutHandle != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_vkContext->deviceHandle(), m_descriptorSetLayoutHandle,
                                     m_vkContext->allocationCallbacks());
        m_descriptorSetLayoutHandle = VK_NULL_HANDLE;
    }
}

inline bool DescriptorSetLayout::isInitialized() const
{
    return (m_descriptorSetLayoutHandle != VK_NULL_HANDLE);
}

// ========================================================

} // namespace VkToolbox
