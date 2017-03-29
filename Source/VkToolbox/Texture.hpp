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
class CommandBuffer;

// ========================================================
// class Sampler:
// ========================================================

class Sampler final
{
public:

    explicit Sampler(const VulkanContext & vkContext);
    Sampler(const VulkanContext & vkContext, const VkSamplerCreateInfo & samplerCreateInfo);
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
    VkSampler getVkSamplerHandle() const;
    const VulkanContext & getVkContext() const;
    const VkSamplerCreateInfo & getVkSamplerDesc() const;

    // Implicit conversion to VkSampler.
    operator VkSampler() const { return m_samplerHandle; }

private:

    VkSampler             m_samplerHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
    VkSamplerCreateInfo   m_samplerDesc;
};

// ========================================================
// class Texture:
// ========================================================

class Texture final
    : public Resource
{
public:

    // Static initialization and shutdown for internal shared state.
    static void initClass();
    static void shutdownClass();

    Texture(const VulkanContext & vkContext, ResourceId id);
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
    VkImage getVkImageHandle() const;
    VkImageView getVkImageViewHandle() const;
    VkDeviceMemory getVkDeviceMemoryHandle() const;

    bool isMipmapped() const;
    int getMipmapCount() const;
    const Size2D getSize() const;
    VkFormat getFormat() const;
    VkImageViewType getViewType() const;

    static VkFormat getVkFormat(Image::Format format);
    static Image::Format getImageFormat(VkFormat format);

    void setGenerateMipmapsOnLoad(bool trueIfShouldGenMipmaps);
    bool generateMipmapsOnLoad() const;

    // This can be safely called after the texture staging buffer of
    // the context has been submitted and waited for completion.
    void releaseStagingImage();

private:

    void clear() override;
    void initVkTextureData(const Image & image);

    VkImage         m_imageHandle;
    VkImageView     m_imageViewHandle;
    VkDeviceMemory  m_imageMemHandle;
    VkImage         m_stagingImageHandle;
    VkDeviceMemory  m_stagingImageMemHandle;
    Size2D          m_imageSize;
    VkFormat        m_imageFormat;
    VkImageViewType m_imageViewType;
    std::uint32_t   m_imageMipmaps   : 31;
    std::uint32_t   m_dontGenMipmaps : 1;
};

// ========================================================

inline bool Sampler::isInitialized() const
{
    return (m_samplerHandle != VK_NULL_HANDLE);
}

inline VkSampler Sampler::getVkSamplerHandle() const
{
    return m_samplerHandle;
}

inline const VulkanContext & Sampler::getVkContext() const
{
    return *m_vkContext;
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

inline VkImage Texture::getVkImageHandle() const
{
    return m_imageHandle;
}

inline VkImageView Texture::getVkImageViewHandle() const
{
    return m_imageViewHandle;
}

inline VkDeviceMemory Texture::getVkDeviceMemoryHandle() const
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

inline const Size2D Texture::getSize() const
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

inline void Texture::setGenerateMipmapsOnLoad(const bool trueIfShouldGenMipmaps)
{
    m_dontGenMipmaps = (trueIfShouldGenMipmaps ? false : true);
}

inline bool Texture::generateMipmapsOnLoad() const
{
    return !m_dontGenMipmaps;
}

// ========================================================

} // namespace VkToolbox
