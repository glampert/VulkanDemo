#pragma once

// ================================================================================================
// File: VkToolbox/Texture.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "VulkanContext.hpp"
#include "Hashing.hpp"

namespace VkToolbox
{

#define VKTB_TEXTURES_PATH "Assets/Textures/"

// ========================================================
// class Sampler:
// ========================================================

class Sampler final
{
public:

    Sampler() = default;
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
    const VulkanContext * m_vkContext     = nullptr;
    VkSamplerCreateInfo   m_samplerDesc;
};

// ========================================================
// class Texture:
// ========================================================

class Texture final
{
public:

    enum LayerIndex
    {
        DiffuseLayer,
        NormalLayer,
        SpecularLayer,

        // Number of entries in the enum - internal use.
        MaxLayers
    };
    static const char * const LayerSuffixes[];

    // Static initialization and shutdown for internal shared state.
    static void initClass();
    static void shutdownClass();

    Texture(const VulkanContext & vkContext, const char * id);
    Texture(const VulkanContext & vkContext, StrId<str> && id);
    ~Texture();

    // Movable.
    Texture(Texture && other);
    Texture & operator = (Texture && other);

    // Not copyable.
    Texture(const Texture &) = delete;
    Texture & operator = (const Texture &) = delete;

    // Resourcing methods:
    bool load();
    void unload();
    void shutdown();
    bool isLoaded() const;
    bool isShutdown() const;
    const VulkanContext & context() const;
    const StrId<str> & resourceId() const;

    // Accessors:
    VkImage imageHandle() const;
    VkImageView imageViewHandle() const;
    VkDeviceMemory imageMemoryHandle() const;

    bool isMipmapped() const;
    int mipmapCount() const;

    bool isArrayTexture() const;
    int layerCount() const;

    const Size2D size() const;
    VkFormat format() const;
    VkImageViewType imageViewType() const;

    static VkFormat toVkImageFormat(Image::Format format);
    static Image::Format toInternalImageFormat(VkFormat format);

    void setGenerateMipmapsOnLoad(bool trueIfShouldGenMipmaps);
    bool generateMipmapsOnLoad() const;

    // Explicitly load as an array texture.
    // m_resId has only the texture base name + extension, e.g.:
    //   'background.dds'
    // Then we will also load into the array:
    //   'background_diff.dds' -> diffuse texture
    //   'background_ddn.dds'  -> normal map
    //   'background_spec.dds' -> specular map
    // And they all get added to the same texture array.
    // Same mipmapping behavior of Texture applies.
    bool loadAsArrayTexture();

    // Load the texture from already in-memory Image or DXTCompressedImage instance.
    // Layer count is implicitly = 1. Might generate mipmaps.
    template<typename ImageType>
    bool loadFromImageInMemory(const ImageType & image);

    // Same as loadAsArrayTexture(), but takes an array of already in-memory images instead.
    // All images must have same dimensions, mipmap count and format.
    template<typename ImageType>
    bool loadAsArrayTextureFromImagesInMemory(const ImageType * images, int layerCount);

    // This can be safely called after the texture staging buffer of
    // the context has been submitted and waited for completion.
    void releaseStagingResources();

private:

    template<typename T>
    friend class ResourceManager;

    void clear();
    static bool isArrayTextureName(const str_ref & name);
    void initVkTextureData(const ImageSurface * const * surfaces, int surfaceCount,
                           int layerCount, VkFormat imageFormat, Size2D imageSize,
                           std::size_t memorySizeBytes);

    const VulkanContext * m_vkContext;
    StrId<str> m_resId;

    VkImage m_imageHandle;
    VkImageView m_imageViewHandle;
    VkDeviceMemory m_imageMemHandle;

    VkBuffer m_stagingBufferHandle;
    VkDeviceMemory m_stagingBufferMemHandle;

    std::uint32_t m_stagingLinkNext;
    static std::uint32_t sm_stagingChainHead;

    Size2D m_imageSize;
    VkFormat m_imageFormat;
    VkImageViewType m_imageViewType;

    std::uint16_t m_imageMipmapCount;
    std::uint16_t m_imageLayerCount;
    bool m_dontGenMipmaps;
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

inline Texture::Texture(const VulkanContext & vkContext, const char * const id)
    : Texture{ vkContext, StrId<str>{ id } }
{
}

inline Texture::~Texture()
{
    shutdown();
}

inline bool Texture::isLoaded() const
{
    return (m_imageHandle != nullptr);
}

inline bool Texture::isShutdown() const
{
    return (m_vkContext == nullptr);
}

inline const VulkanContext & Texture::context() const
{
    return *m_vkContext;
}

inline const StrId<str> & Texture::resourceId() const
{
    return m_resId;
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
    return (m_imageMipmapCount > 1);
}

inline int Texture::mipmapCount() const
{
    return static_cast<int>(m_imageMipmapCount);
}

inline bool Texture::isArrayTexture() const
{
    return (m_imageLayerCount > 1);
}

inline int Texture::layerCount() const
{
    return static_cast<int>(m_imageLayerCount);
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

template<typename ImageType>
inline bool Texture::loadFromImageInMemory(const ImageType & image)
{
    if (isShutdown() || !image.isValid())
    {
        Log::warningF("Texture/Image already shutdown and cannot be loaded!");
        return false;
    }

    const ImageSurface * surfaces = image.surfaces();
    initVkTextureData(&surfaces, image.surfaceCount(), /* layerCount = */ 1,
                      toVkImageFormat(image.format()), image.size(),
                      image.memoryUsageBytes());
    return true;
}

template <typename ImageType>
inline bool Texture::loadAsArrayTextureFromImagesInMemory(const ImageType * const images, const int layerCount)
{
    assert(layerCount <= MaxLayers);
    if (isShutdown())
    {
        Log::warningF("Texture already shutdown and cannot be loaded!");
        return false;
    }

    const Size2D sizeLayer0   = images[0].size();
    const auto formatLayer0   = images[0].format();
    const int surfCountLayer0 = images[0].surfaceCount();

    std::size_t memorySizeBytes = 0;
    FixedSizeArray<const ImageSurface *, MaxLayers> layerSurfaces;

    for (int l = 0; l < layerCount; ++l)
    {
        if (images[l].size()         != sizeLayer0   ||
            images[l].format()       != formatLayer0 ||
            images[l].surfaceCount() != surfCountLayer0)
        {
            Log::errorF("Array texture requires all image layers to have the same size, format and mip count!");
            return false;
        }

        memorySizeBytes += images[l].memoryUsageBytes();
        layerSurfaces.push(images[l].surfaces());
    }

    const ImageSurface * const * surfaces = layerSurfaces.data();
    initVkTextureData(surfaces, surfCountLayer0, layerSurfaces.size(),
                      toVkImageFormat(formatLayer0), sizeLayer0,
                      memorySizeBytes);
    return true;
}

// ========================================================

} // namespace VkToolbox
