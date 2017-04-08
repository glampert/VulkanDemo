#pragma once

// ================================================================================================
// File: VkToolbox/Texture.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Resource.hpp"
#include "Image.hpp"

namespace VkToolbox
{

#define VKTB_TEXTURES_PATH "Assets/Textures/"

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

    // Movable.
    Sampler(Sampler && other);
    Sampler & operator = (Sampler && other);

    // Not copyable.
    Sampler(const Sampler &) = delete;
    Sampler & operator = (const Sampler &) = delete;

    void initialize(const VkSamplerCreateInfo & samplerCreateInfo);
    void shutdown();
    bool isInitialized() const;

    const VulkanContext & context() const { return *m_vkContext; }
    const VkSamplerCreateInfo & samplerDesc() const { return m_samplerDesc; }

    VkSampler samplerHandle() const { return m_samplerHandle; }
    operator VkSampler() const { return m_samplerHandle; }

    static const VkSamplerCreateInfo & defaults();

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
    VkImage imageHandle() const;
    VkImageView imageViewHandle() const;
    VkDeviceMemory imageMemoryHandle() const;

    bool isMipmapped() const;
    int mipmapCount() const;
    const Size2D size() const;
    VkFormat format() const;
    VkImageViewType imageViewType() const;

    static VkFormat toVkImageFormat(Image::Format format);
    static Image::Format toInternalImageFormat(VkFormat format);

    void setGenerateMipmapsOnLoad(bool trueIfShouldGenMipmaps);
    bool generateMipmapsOnLoad() const;

    // Load the texture from already in-memory Image instance.
    bool loadFromImageInMemory(const Image & image);

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
// Sampler inline methods:
// ========================================================

inline Sampler::Sampler(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
    , m_samplerDesc{}
{
}

inline Sampler::Sampler(const VulkanContext & vkContext, const VkSamplerCreateInfo & samplerCreateInfo)
    : m_vkContext{ &vkContext }
{
    initialize(samplerCreateInfo);
}

inline Sampler::~Sampler()
{
    shutdown();
}

inline bool Sampler::isInitialized() const
{
    return (m_samplerHandle != VK_NULL_HANDLE);
}

// ========================================================
// Texture inline methods:
// ========================================================

inline Texture::~Texture()
{
    Texture::shutdown();
}

inline bool Texture::isLoaded() const
{
    return (m_imageHandle != nullptr);
}

inline VkImage Texture::imageHandle() const
{
    return m_imageHandle;
}

inline VkImageView Texture::imageViewHandle() const
{
    return m_imageViewHandle;
}

inline VkDeviceMemory Texture::imageMemoryHandle() const
{
    return m_imageMemHandle;
}

inline bool Texture::isMipmapped() const
{
    return (m_imageMipmaps > 1);
}

inline int Texture::mipmapCount() const
{
    return static_cast<int>(m_imageMipmaps);
}

inline const Size2D Texture::size() const
{
    return m_imageSize;
}

inline VkFormat Texture::format() const
{
    return m_imageFormat;
}

inline VkImageViewType Texture::imageViewType() const
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
