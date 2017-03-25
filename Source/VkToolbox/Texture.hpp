#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Texture.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Resource.hpp"
#include "Image.hpp"

namespace VkToolbox
{

class Image;

// ========================================================
// class Sampler:
// ========================================================

class Sampler final
{
public:

    explicit Sampler(WeakRef<const VulkanContext> vkContext);
    Sampler(WeakRef<const VulkanContext> vkContext,
            const VkSamplerCreateInfo & samplerCreateInfo);
    ~Sampler();

    void initialize(const VkSamplerCreateInfo & samplerCreateInfo);
    void shutdown();
    bool isInitialized() const;

    // Movable.
    Sampler(Sampler && other);
    Sampler & operator = (Sampler && other);

    // But not copyable.
    Sampler(const Sampler &) = delete;
    Sampler & operator = (const Sampler &) = delete;

    // Accessors:
    const VulkanContext & getVkContext() const;
    WeakHandle<VkSampler> getVkSamplerHandle() const;
    const VkSamplerCreateInfo & getVkSamplerDesc() const;

private:

    WeakRef<const VulkanContext> m_vkContext;
    OwnedHandle<VkSampler>       m_samplerHandle;
    VkSamplerCreateInfo          m_samplerDesc;
};

// ========================================================
// class Texture:
// ========================================================

class Texture final
    : public Resource
{
public:

    Texture(WeakRef<const VulkanContext> vkContext, ResourceId id);
    ~Texture();

    // Movable.
    Texture(Texture && other);
    Texture & operator = (Texture && other);

    // Resource overrides:
    bool load() override;
    void unload() override;
    void shutdown() override;
    bool isLoaded() const override;

    // Accessors:
    WeakHandle<VkImage> getVkImageHandle() const;
    WeakHandle<VkImageView> getVkImageViewHandle() const;
    WeakHandle<VkDeviceMemory> getVkDeviceMemoryHandle() const;

    bool isMipmapped() const;
    int getMipmapCount() const;
    const Size3D getSize() const;
    VkFormat getFormat() const;
    VkImageViewType getViewType() const;

    void setDontGenerateMipmapsOnLoad(bool trueIfShouldntGenMipmaps);
    bool willGenerateMipmapsOnLoad() const;

    static VkFormat getVkFormat(Image::Format format);

private:

    void clear() override;
    bool initGpuData(const Image & img);

    OwnedHandle<VkImage>        m_imageHandle;
    OwnedHandle<VkImageView>    m_imageViewHandle;
    OwnedHandle<VkDeviceMemory> m_imageMemHandle;
    Size3D                      m_imageSize;
    VkFormat                    m_imageFormat;
    VkImageViewType             m_imageViewType;
    std::uint32_t               m_imageMipmaps   : 31;
    std::uint32_t               m_dontGenMipmaps : 1;
};

// ========================================================

inline bool Sampler::isInitialized() const
{
    return (m_samplerHandle != VK_NULL_HANDLE);
}

inline const VulkanContext & Sampler::getVkContext() const
{
    assert(m_vkContext != nullptr);
    return *m_vkContext;
}

inline WeakHandle<VkSampler> Sampler::getVkSamplerHandle() const
{
    return m_samplerHandle;
}

inline const VkSamplerCreateInfo & Sampler::getVkSamplerDesc() const
{
    return m_samplerDesc;
}

// ========================================================

inline bool Texture::isLoaded() const
{
    return (m_imageHandle != nullptr);
}

inline WeakHandle<VkImage> Texture::getVkImageHandle() const
{
    return m_imageHandle;
}

inline WeakHandle<VkImageView> Texture::getVkImageViewHandle() const
{
    return m_imageViewHandle;
}

inline WeakHandle<VkDeviceMemory> Texture::getVkDeviceMemoryHandle() const
{
    return m_imageMemHandle;
}

inline bool Texture::isMipmapped() const
{
    return (m_imageMipmaps > 1);
}

inline int Texture::getMipmapCount() const
{
    return static_cast<int>(m_imageMipmaps);
}

inline const Size3D Texture::getSize() const
{
    return m_imageSize;
}

inline VkFormat Texture::getFormat() const
{
    return m_imageFormat;
}

inline VkImageViewType Texture::getViewType() const
{
    return m_imageViewType;
}

inline void Texture::setDontGenerateMipmapsOnLoad(const bool trueIfShouldntGenMipmaps)
{
    m_dontGenMipmaps = trueIfShouldntGenMipmaps;
}

inline bool Texture::willGenerateMipmapsOnLoad() const
{
    return !m_dontGenMipmaps;
}

// ========================================================

} // namespace VkToolbox
