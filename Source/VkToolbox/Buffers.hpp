#pragma once

// ================================================================================================
// File: VkToolbox/Buffers.hpp
// Author: Guilherme R. Lampert
// Created on: 30/03/17
// Brief: Vulkan buffer helpers for UBs, VBs and IBs.
// ================================================================================================

#include "VulkanContext.hpp"

namespace VkToolbox
{

// ========================================================
// template class StagingBuffer:
// ========================================================

template<VkBufferUsageFlags BufferType>
class StagingBuffer
{
public:

    explicit StagingBuffer(const VulkanContext & vkContext);
    StagingBuffer(const VulkanContext & vkContext, int sizeBytes);
    ~StagingBuffer();

    StagingBuffer(const StagingBuffer &) = delete;
    StagingBuffer & operator = (const StagingBuffer &) = delete;

    void initialize(int sizeBytes);
    void shutdown();
    bool isInitialized() const;
    void uploadStagingToGpu(const CommandBuffer & cmdBuff) const;

    int bufferSizeBytes() const;
    VkBuffer bufferHandle() const;
    VkBuffer stagingBufferHandle() const;
    VkDeviceMemory bufferMemoryHandle() const;
    VkDeviceMemory stagingBufferMemoryHandle() const;
    static constexpr VkBufferUsageFlags bufferType();
    const VulkanContext & context() const;

private:

    VkBuffer              m_bufferHandle           = VK_NULL_HANDLE;
    VkBuffer              m_stagingBufferHandle    = VK_NULL_HANDLE;
    VkDeviceMemory        m_bufferMemHandle        = VK_NULL_HANDLE;
    VkDeviceMemory        m_stagingBufferMemHandle = VK_NULL_HANDLE;
    int                   m_bufferSizeBytes        = 0;
    const VulkanContext * m_vkContext;
};

// ========================================================

using StagingBufferUB = StagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>;
using StagingBufferVB = StagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>;
using StagingBufferIB = StagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>;

// ========================================================
// class PersistentlyMappedBuffer:
// ========================================================

template<typename BaseBuffer>
class PersistentlyMappedBuffer
    : public BaseBuffer
{
public:

    explicit PersistentlyMappedBuffer(const VulkanContext & vkContext) // Uninitialized - unmapped
        : BaseBuffer{ vkContext }
    { }

    PersistentlyMappedBuffer(const VulkanContext & vkContext, const int sizeBytes)
        : BaseBuffer{ vkContext, sizeBytes }
    {
        mapMemory();
    }

    ~PersistentlyMappedBuffer()
    {
        unmapMemory();
    }

    void initialize(const int sizeBytes)
    {
        BaseBuffer::initialize(sizeBytes);
        mapMemory();
    }

    void writeBytes(const void * data, const int sizeBytes, const int offsetBytes = 0)
    {
        assert(m_mappedPtr != nullptr && data != nullptr);
        assert(sizeBytes > 0 && sizeBytes <= BaseBuffer::bufferSizeBytes() - offsetBytes);
        std::memcpy(static_cast<std::uint8_t *>(m_mappedPtr) + offsetBytes, data, sizeBytes);
    }

    void mapMemory()
    {
        assert(m_mappedPtr == nullptr);
        VKTB_CHECK(vkMapMemory(BaseBuffer::context().deviceHandle(),
                               BaseBuffer::stagingBufferMemoryHandle(),
                               0, BaseBuffer::bufferSizeBytes(), 0, &m_mappedPtr));
    }

    void unmapMemory()
    {
        vkUnmapMemory(BaseBuffer::context().deviceHandle(),
                      BaseBuffer::stagingBufferMemoryHandle());
        m_mappedPtr = nullptr;
    }

    void * mappedPointer() const { return m_mappedPtr; }
    operator VkBuffer()    const { return BaseBuffer::bufferHandle(); }

private:

    void * m_mappedPtr = nullptr;
};

// ========================================================

// Persistently mapped buffer for shader uniform variables (AKA Constant Buffers in DX lingo).
using ShaderUniformBuffer = PersistentlyMappedBuffer<StagingBufferUB>;

// Persistently mapped buffer types for vertex and index buffers:
using VertexBuffer = PersistentlyMappedBuffer<StagingBufferVB>;
using IndexBuffer  = PersistentlyMappedBuffer<StagingBufferIB>;

// ========================================================
// template class StructuredShaderUniformBuffer:
// ========================================================

template<typename T>
class StructuredShaderUniformBuffer final
    : public ShaderUniformBuffer
{
public:

    explicit StructuredShaderUniformBuffer(const VulkanContext & vkContext) // Uninitialized - unmapped
        : ShaderUniformBuffer{ vkContext }
        , m_paddedElemSize{ 0 }
    { }

    StructuredShaderUniformBuffer(const VulkanContext & vkContext, const int sizeInElems, const bool pad = true)
        : ShaderUniformBuffer{ vkContext, calcPaddedBufferSize(vkContext, sizeInElems, pad) }
    { }

    void initialize(const int sizeInElems, const bool pad = true)
    {
        ShaderUniformBuffer::initialize(calcPaddedBufferSize(ShaderUniformBuffer::context(), sizeInElems, pad));
    }

    void write(const T & payload, const int offsetInElems = 0)
    {
        ShaderUniformBuffer::writeBytes(&payload, sizeof(T), offsetInElems * m_paddedElemSize);
    }

    void writeN(array_view<const T> payload, int offsetInElems = 0)
    {
        if (sizeof(T) == m_paddedElemSize)
        {
            const int totalSizeBytes = narrowCast<int>(payload.size_bytes());
            ShaderUniformBuffer::writeBytes(payload.data(), totalSizeBytes, offsetInElems * m_paddedElemSize);
        }
        else // There's padding between each element
        {
            for (const T & iter : payload)
            {
                write(iter, offsetInElems++);
            }
        }
    }

    template<typename T, std::size_t Size>
    void writeN(const T (&payload)[Size])
    {
        return writeN(make_array_view(payload, Size));
    }

    int elementSizeBytes() const
    {
        return m_paddedElemSize;
    }

private:

    int m_paddedElemSize;
    int calcPaddedBufferSize(const VulkanContext & vkContext, int sizeInElems, bool doPad);
};

// ========================================================
// template class MultiStructuredShaderUniformBuffer:
// ========================================================

template<typename ... Types>
class MultiStructuredShaderUniformBuffer final
    : public ShaderUniformBuffer
{
public:

    static constexpr int TypeCount = sizeof...(Types);

    explicit MultiStructuredShaderUniformBuffer(const VulkanContext & vkContext) // Uninitialized - unmapped
        : ShaderUniformBuffer{ vkContext }
        , m_paddedElemSize{ 0 }
    { }

    MultiStructuredShaderUniformBuffer(const VulkanContext & vkContext, const int elemCounts[TypeCount], const bool pad = true)
        : ShaderUniformBuffer{ vkContext, calcPaddedBufferSize(vkContext, elemCounts, pad) }
    { }

    void initialize(const int elemCounts[TypeCount], const bool pad = true)
    {
        ShaderUniformBuffer::initialize(calcPaddedBufferSize(ShaderUniformBuffer::context(), elemCounts, pad));
    }

    template<typename T>
    void write(const T & payload, const int offsetInElems = 0)
    {
        assert(sizeof(T) <= m_paddedElemSize);
        ShaderUniformBuffer::writeBytes(&payload, sizeof(T), offsetInElems * m_paddedElemSize);
    }

    template<typename T>
    void writeN(array_view<T> payload, int offsetInElems = 0)
    {
        if (sizeof(T) == m_paddedElemSize)
        {
            const int totalSizeBytes = narrowCast<int>(payload.size_bytes());
            ShaderUniformBuffer::writeBytes(payload.data(), totalSizeBytes, offsetInElems * m_paddedElemSize);
        }
        else // There's padding between each element
        {
            for (const T & iter : payload)
            {
                write(iter, offsetInElems++);
            }
        }
    }

    template<typename T, std::size_t Size>
    void writeN(const T (&payload)[Size])
    {
        return writeN(make_array_view(payload, Size));
    }

    int elementSizeBytes() const
    {
        return m_paddedElemSize;
    }

private:

    int m_paddedElemSize;
    int calcPaddedBufferSize(const VulkanContext & vkContext, const int elemCounts[TypeCount], bool doPad);
};

// ========================================================
// template class GenericStructuredBuffer:
// ========================================================

template<typename T, typename BaseBuffer>
class GenericStructuredBuffer final
    : public BaseBuffer
{
public:

    explicit GenericStructuredBuffer(const VulkanContext & vkContext) // Uninitialized - unmapped
        : BaseBuffer{ vkContext }
    { }

    GenericStructuredBuffer(const VulkanContext & vkContext, const int sizeInElems)
        : BaseBuffer{ vkContext, sizeInElems * sizeof(T) }
    { }

    void initialize(const int sizeInElems)
    {
        BaseBuffer::initialize(sizeInElems * sizeof(T));
    }

    void write(const T & payload, const int offsetInElems = 0)
    {
        BaseBuffer::writeBytes(&payload, sizeof(T), offsetInElems * sizeof(T));
    }

    void writeN(array_view<const T> payload, int offsetInElems = 0)
    {
        const int totalSizeBytes = narrowCast<int>(payload.size_bytes());
        BaseBuffer::writeBytes(payload.data(), totalSizeBytes, offsetInElems * sizeof(T));
    }

    template<typename T, std::size_t Size>
    void writeN(const T (&payload)[Size])
    {
        return writeN(make_array_view(payload, Size));
    }

    int elementCount() const
    {
        return BaseBuffer::bufferSizeBytes() / sizeof(T);
    }

    static constexpr int elementSizeBytes()
    {
        return sizeof(T);
    }
};

// ========================================================

template<typename T> using StructuredVertexBuffer = GenericStructuredBuffer<T, VertexBuffer>;
template<typename T> using StructuredIndexBuffer  = GenericStructuredBuffer<T, IndexBuffer>;

// ========================================================

inline VkIndexType vkIndexTypeForBuffer(const StructuredIndexBuffer<std::uint16_t> &) { return VK_INDEX_TYPE_UINT16; }
inline VkIndexType vkIndexTypeForBuffer(const StructuredIndexBuffer<std::uint32_t> &) { return VK_INDEX_TYPE_UINT32; }

// ========================================================
// StagingBuffer inline methods:
// ========================================================

template<VkBufferUsageFlags BufferType>
inline StagingBuffer<BufferType>::StagingBuffer(const VulkanContext & vkContext, const int sizeBytes)
    : m_vkContext{ &vkContext }
{
    initialize(sizeBytes);
}

template<VkBufferUsageFlags BufferType>
inline StagingBuffer<BufferType>::StagingBuffer(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

template<VkBufferUsageFlags BufferType>
inline StagingBuffer<BufferType>::~StagingBuffer()
{
    shutdown();
}

template<VkBufferUsageFlags BufferType>
inline void StagingBuffer<BufferType>::initialize(const int sizeBytes)
{
    assert(sizeBytes > 0);
    assert(!isInitialized());

    constexpr auto stagingUsage    = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    constexpr auto stagingMemProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    constexpr auto bufferUsage     = VK_BUFFER_USAGE_TRANSFER_DST_BIT | BufferType;
    constexpr auto bufferMemProps  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_bufferSizeBytes = sizeBytes;
    m_vkContext->createBuffer(sizeBytes, bufferUsage, bufferMemProps, &m_bufferHandle, &m_bufferMemHandle);
    m_vkContext->createBuffer(sizeBytes, stagingUsage, stagingMemProps, &m_stagingBufferHandle, &m_stagingBufferMemHandle);
}

template<VkBufferUsageFlags BufferType>
inline void StagingBuffer<BufferType>::shutdown()
{
    const auto device   = m_vkContext->deviceHandle();
    const auto allocCBs = m_vkContext->allocationCallbacks();

    // Buffer handles:
    if (m_bufferHandle != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_bufferHandle, allocCBs);
        m_bufferHandle = VK_NULL_HANDLE;
    }
    if (m_stagingBufferHandle != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_stagingBufferHandle, allocCBs);
        m_stagingBufferHandle = VK_NULL_HANDLE;
    }

    // Memory:
    if (m_bufferMemHandle != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_bufferMemHandle, allocCBs);
        m_bufferMemHandle = VK_NULL_HANDLE;
    }
    if (m_stagingBufferMemHandle != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_stagingBufferMemHandle, allocCBs);
        m_stagingBufferMemHandle = VK_NULL_HANDLE;
    }
}

template<VkBufferUsageFlags BufferType>
inline bool StagingBuffer<BufferType>::isInitialized() const
{
    return (m_bufferHandle != VK_NULL_HANDLE);
}

template <VkBufferUsageFlags BufferType>
inline void StagingBuffer<BufferType>::uploadStagingToGpu(const CommandBuffer & cmdBuff) const
{
    m_vkContext->copyBuffer(cmdBuff, m_stagingBufferHandle, m_bufferHandle, m_bufferSizeBytes);
}

template<VkBufferUsageFlags BufferType>
inline int StagingBuffer<BufferType>::bufferSizeBytes() const
{
    return m_bufferSizeBytes;
}

template<VkBufferUsageFlags BufferType>
inline VkBuffer StagingBuffer<BufferType>::bufferHandle() const
{
    return m_bufferHandle;
}

template<VkBufferUsageFlags BufferType>
inline VkBuffer StagingBuffer<BufferType>::stagingBufferHandle() const
{
    return m_stagingBufferHandle;
}

template<VkBufferUsageFlags BufferType>
inline VkDeviceMemory StagingBuffer<BufferType>::bufferMemoryHandle() const
{
    return m_bufferMemHandle;
}

template<VkBufferUsageFlags BufferType>
inline VkDeviceMemory StagingBuffer<BufferType>::stagingBufferMemoryHandle() const
{
    return m_stagingBufferMemHandle;
}

template<VkBufferUsageFlags BufferType>
constexpr VkBufferUsageFlags StagingBuffer<BufferType>::bufferType()
{
    return BufferType;
}

template<VkBufferUsageFlags BufferType>
inline const VulkanContext & StagingBuffer<BufferType>::context() const
{
    return *m_vkContext;
}

// ========================================================
// StructuredShaderUniformBuffer inline methods:
// ========================================================

template<typename T>
inline int StructuredShaderUniformBuffer<T>::calcPaddedBufferSize(const VulkanContext & vkContext,
                                                                  const int sizeInElems,
                                                                  const bool doPad)
{
    if (!doPad)
    {
        m_paddedElemSize = sizeof(T);
    }
    else
    {
        const auto & deviceProps = vkContext.deviceProperties();
        const int minOffsetAlign = static_cast<int>(deviceProps.limits.minUniformBufferOffsetAlignment);

        int elementSize = sizeof(T);
        if (elementSize < minOffsetAlign)
        {
            elementSize = minOffsetAlign;
        }
        else if (elementSize > minOffsetAlign)
        {
            while ((elementSize % minOffsetAlign) != 0)
            {
                elementSize = roundUpToPowerOfTwo(elementSize);
            }
        }

        m_paddedElemSize = elementSize;
    }

    return m_paddedElemSize * sizeInElems;
}

// ========================================================
// MultiStructuredShaderUniformBuffer inline methods:
// ========================================================

template<typename... Types>
inline int MultiStructuredShaderUniformBuffer<Types...>::calcPaddedBufferSize(const VulkanContext & vkContext,
                                                                              const int elemCounts[TypeCount],
                                                                              const bool doPad)
{
    constexpr int maxElementSize = int(CT::maxOfN(sizeof(Types) ...));
    if (!doPad)
    {
        m_paddedElemSize = maxElementSize;
    }
    else
    {
        const auto & deviceProps = vkContext.deviceProperties();
        const int minOffsetAlign = static_cast<int>(deviceProps.limits.minUniformBufferOffsetAlignment);

        int elementSize = maxElementSize;
        if (elementSize < minOffsetAlign)
        {
            elementSize = minOffsetAlign;
        }
        else if (elementSize > minOffsetAlign)
        {
            while ((elementSize % minOffsetAlign) != 0)
            {
                elementSize = roundUpToPowerOfTwo(elementSize);
            }
        }

        m_paddedElemSize = elementSize;
    }

    int totalElementCount = 0;
    for (int i = 0; i < TypeCount; ++i)
    {
        totalElementCount += elemCounts[i];
    }

    return m_paddedElemSize * totalElementCount;
}

// ========================================================

} // namespace VkToolbox
