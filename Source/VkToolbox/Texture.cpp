
// ================================================================================================
// File: VkToolbox/Texture.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Texture.hpp"
#include "ResourceManager.hpp"

namespace VkToolbox
{

// ========================================================
// class Sampler:
// ========================================================

Sampler::Sampler(Sampler && other)
    : m_vkContext{ other.m_vkContext }
    , m_samplerHandle{ other.m_samplerHandle }
    , m_samplerDesc{ other.m_samplerDesc }
{
    other.m_samplerHandle = VK_NULL_HANDLE;
}

Sampler & Sampler::operator = (Sampler && other)
{
    shutdown();

    m_vkContext     = other.m_vkContext;
    m_samplerHandle = other.m_samplerHandle;
    m_samplerDesc   = other.m_samplerDesc;

    other.m_samplerHandle = VK_NULL_HANDLE;
    return *this;
}

void Sampler::initialize(const VkSamplerCreateInfo & samplerCreateInfo)
{
    assert(!isInitialized()); // Prevent double init

    m_samplerDesc = samplerCreateInfo;
    VKTB_CHECK(vkCreateSampler(m_vkContext->deviceHandle(), &m_samplerDesc,
                               m_vkContext->allocationCallbacks(), &m_samplerHandle));

    assert(m_samplerHandle != VK_NULL_HANDLE);
}

void Sampler::shutdown()
{
    if (m_samplerHandle != VK_NULL_HANDLE) // Could have been moved
    {
        vkDestroySampler(m_vkContext->deviceHandle(), m_samplerHandle,
                         m_vkContext->allocationCallbacks());
        m_samplerHandle = VK_NULL_HANDLE;
    }
}

const VkSamplerCreateInfo & Sampler::defaults()
{
    static const VkSamplerCreateInfo s_defaultSamplerDesc = {
        /* sType                   = */ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        /* pNext                   = */ nullptr,
        /* flags                   = */ 0,
        /* magFilter               = */ VK_FILTER_LINEAR,
        /* minFilter               = */ VK_FILTER_LINEAR,
        /* mipmapMode              = */ VK_SAMPLER_MIPMAP_MODE_LINEAR,
        /* addressModeU            = */ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        /* addressModeV            = */ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        /* addressModeW            = */ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        /* mipLodBias              = */ 0.0f,
        /* anisotropyEnable        = */ VK_FALSE,
        /* maxAnisotropy           = */ 0.0f,
        /* compareEnable           = */ VK_FALSE,
        /* compareOp               = */ VK_COMPARE_OP_NEVER,
        /* minLod                  = */ 0.0f,
        /* maxLod                  = */ 0.0f,
        /* borderColor             = */ VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        /* unnormalizedCoordinates = */ VK_FALSE,
    };
    return s_defaultSamplerDesc;
}

// ========================================================
// class Texture:
// ========================================================

std::uint32_t Texture::sm_stagingChainHead = TextureManager::InvalidResIndex;

const char * const Texture::LayerSuffixes[] = {
    "diff",
    "ddn",
    "spec",
};
static_assert(arrayLength(Texture::LayerSuffixes) == Texture::MaxLayers, "Keep in sync with the enum!");

// ========================================================

Texture::Texture(const VulkanContext & vkContext, StrId<str> && id)
    : m_vkContext{ &vkContext }
    , m_resId{ std::move(id) }
    , m_imageHandle{ VK_NULL_HANDLE }
    , m_imageViewHandle{ VK_NULL_HANDLE }
    , m_imageMemHandle{ VK_NULL_HANDLE }
    , m_stagingBufferHandle{ VK_NULL_HANDLE }
    , m_stagingBufferMemHandle{ VK_NULL_HANDLE }
    , m_stagingLinkNext{ TextureManager::InvalidResIndex }
    , m_imageSize{ 0,0 }
    , m_imageFormat{ VK_FORMAT_UNDEFINED }
    , m_imageViewType{ VK_IMAGE_VIEW_TYPE_2D }
    , m_imageMipmapCount{ 0 }
    , m_imageLayerCount{ 0 }
    , m_dontGenMipmaps{ false }
{
}

Texture::Texture(Texture && other)
    : m_vkContext{ other.m_vkContext }
    , m_resId{ std::move(other.m_resId) }
    , m_imageHandle{ other.m_imageHandle }
    , m_imageViewHandle{ other.m_imageViewHandle }
    , m_imageMemHandle{ other.m_imageMemHandle }
    , m_stagingBufferHandle{ other.m_stagingBufferHandle }
    , m_stagingBufferMemHandle{ other.m_stagingBufferMemHandle }
    , m_stagingLinkNext{ other.m_stagingLinkNext }
    , m_imageSize{ other.m_imageSize }
    , m_imageFormat{ other.m_imageFormat }
    , m_imageViewType{ other.m_imageViewType }
    , m_imageMipmapCount{ other.m_imageMipmapCount }
    , m_imageLayerCount{ other.m_imageLayerCount }
    , m_dontGenMipmaps{ other.m_dontGenMipmaps }
{
    other.clear();
}

Texture & Texture::operator = (Texture && other)
{
    shutdown();

    m_vkContext              = other.m_vkContext;
    m_resId                  = std::move(other.m_resId);
    m_imageHandle            = other.m_imageHandle;
    m_imageViewHandle        = other.m_imageViewHandle;
    m_imageMemHandle         = other.m_imageMemHandle;
    m_stagingBufferHandle    = other.m_stagingBufferHandle;
    m_stagingBufferMemHandle = other.m_stagingBufferMemHandle;
    m_stagingLinkNext        = other.m_stagingLinkNext;
    m_imageSize              = other.m_imageSize;
    m_imageFormat            = other.m_imageFormat;
    m_imageViewType          = other.m_imageViewType;
    m_imageMipmapCount       = other.m_imageMipmapCount;
    m_imageLayerCount        = other.m_imageLayerCount;
    m_dontGenMipmaps         = other.m_dontGenMipmaps;

    other.clear();
    return *this;
}

bool Texture::isArrayTextureName(const str_ref & name)
{
    static const char * const ArrayTexPrefix = "a_";

    int lastSlash = name.last_index_of('/');
    if (lastSlash == -1)
    {
        lastSlash = name.last_index_of('\\'); // Windows paths
    }

    if (lastSlash != -1) // With path
    {
        const str_ref fileOnly{ name, lastSlash + 1 };
        return fileOnly.starts_with(ArrayTexPrefix);
    }
    else // Filename only
    {
        return name.starts_with(ArrayTexPrefix);
    }
}

bool Texture::load()
{
    if (isShutdown())
    {
        Log::warningF("Texture is already shutdown and cannot be loaded!");
        return false;
    }

    const str_ref name{ resourceId().c_str() };
    if (isArrayTextureName(name))
    {
        // Multiple textures combined into an array texture
        return loadAsArrayTexture();
    }

    // Single texture:
    if (!probeFile(name.c_str()))
    {
        Log::warningF("Image file '%s' does not exist! Can't load texture from it.", name.c_str());
        return false;
    }

    if (name.ends_with(".dds"))
    {
        DXTCompressedImage compressedImage;
        if (!compressedImage.initFromFile(name.c_str()))
        {
            Log::warningF("Failed to load DDS texture: %s", name.c_str());
            return false;
        }

        return loadFromImageInMemory(compressedImage);
    }
    else // Default loader from PNG/JPEG/etc:
    {
        Image uncompressedImage;
        if (!uncompressedImage.initFromFile(name.c_str()))
        {
            Log::warningF("Failed to load texture: %s", name.c_str());
            return false;
        }

        // If we didn't already do so in Image::initFromFile...
        if (generateMipmapsOnLoad() && !uncompressedImage.isMipmapped())
        {
            uncompressedImage.generateMipmapSurfaces();
        }

        return loadFromImageInMemory(uncompressedImage);
    }
}

bool Texture::loadAsArrayTexture()
{
    str256 texName{ resourceId().name };
    str256 layerName;

    if (texName.ends_with(".dds"))
    {
        texName.truncate(texName.last_index_of('.')); // Get rid of the extension

        DXTCompressedImage compressedImages[MaxLayers];
        for (int l = 0; l < MaxLayers; ++l)
        {
            layerName.setf("%s_%s.dds", texName.c_str(), LayerSuffixes[l]);
            if (!compressedImages[l].initFromFile(layerName.c_str()))
            {
                Log::warningF("Failed to load DDS texture: %s", layerName.c_str());
                return false;
            }
        }

        return loadAsArrayTextureFromImagesInMemory(compressedImages, MaxLayers);
    }
    else // Decompressed PNG/JPEG/etc path:
    {
        const int extIdx = texName.last_index_of('.');
        const str32 ext{ texName, extIdx, texName.length() - extIdx };
        texName.truncate(extIdx); // Get rid of the extension

        Image uncompressedImages[MaxLayers];
        for (int l = 0; l < MaxLayers; ++l)
        {
            layerName.setf("%s_%s%s", texName.c_str(), LayerSuffixes[l], ext.c_str());
            if (!uncompressedImages[l].initFromFile(layerName.c_str()))
            {
                Log::warningF("Failed to load texture: %s", layerName.c_str());
                return false;
            }

            // If we didn't already do so in Image::initFromFile...
            if (generateMipmapsOnLoad() && !uncompressedImages[l].isMipmapped())
            {
                uncompressedImages[l].generateMipmapSurfaces();
            }
        }

        return loadAsArrayTextureFromImagesInMemory(uncompressedImages, MaxLayers);
    }
}

void Texture::unload()
{
    if (isShutdown()) { return; }

    const auto device   = m_vkContext->deviceHandle();
    const auto allocCBs = m_vkContext->allocationCallbacks();

    if (m_imageViewHandle != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, m_imageViewHandle, allocCBs);
        m_imageViewHandle = VK_NULL_HANDLE;
    }
    if (m_imageHandle != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, m_imageHandle, allocCBs);
        m_imageHandle = VK_NULL_HANDLE;
    }
    if (m_imageMemHandle != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_imageMemHandle, allocCBs);
        m_imageMemHandle = VK_NULL_HANDLE;
    }

    releaseStagingResources();
}

void Texture::releaseStagingResources()
{
    const auto device   = m_vkContext->deviceHandle();
    const auto allocCBs = m_vkContext->allocationCallbacks();

    if (m_stagingBufferHandle != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_stagingBufferHandle, allocCBs);
        m_stagingBufferHandle = VK_NULL_HANDLE;
    }
    if (m_stagingBufferMemHandle != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_stagingBufferMemHandle, allocCBs);
        m_stagingBufferMemHandle = VK_NULL_HANDLE;
    }

    m_stagingLinkNext = TextureManager::InvalidResIndex;
}

void Texture::shutdown()
{
    unload();

    m_vkContext = nullptr;
    m_resId     = {};
}

void Texture::clear()
{
    m_vkContext = nullptr;
    m_resId     = {};

    // Used by the move ops, so don't delete, just drop ownership.
    m_imageHandle            = VK_NULL_HANDLE;
    m_imageViewHandle        = VK_NULL_HANDLE;
    m_imageMemHandle         = VK_NULL_HANDLE;
    m_stagingBufferHandle    = VK_NULL_HANDLE;
    m_stagingBufferMemHandle = VK_NULL_HANDLE;
    m_stagingLinkNext        = TextureManager::InvalidResIndex;
}

void Texture::initVkTextureData(const ImageSurface * const * surfaces, const int surfaceCount,
                                const int layerCount, const VkFormat imageFormat, const Size2D imageSize,
                                const std::size_t memorySizeBytes)
{
    // We load the image in-place to avoid keeping two copies of the memory
    // at the same time in case of a reload, so need to check for that before
    // proceeding.
    if (isLoaded())
    {
        unload();
    }

    const VkDevice device = m_vkContext->deviceHandle();
    const CommandBuffer & cmdBuff = m_vkContext->mainTextureStagingCmdBuffer();

    //
    // Create a host-visible staging buffer that will contain the raw image data:
    //
    VkMemoryRequirements memReqs;
    constexpr auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    m_vkContext->createBuffer(memorySizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memoryType,
                              &m_stagingBufferHandle, &m_stagingBufferMemHandle, &memReqs);

    if (memReqs.size < memorySizeBytes)
    {
        Log::fatalF("Buffer memReqs.size (%zu) < Image memorySizeBytes (%zu)!",
                    std::size_t(memReqs.size), memorySizeBytes);
    }

    //
    // Copy texture data into the staging buffer:
    //
    void * pMappedMem;
    VKTB_CHECK(vkMapMemory(device, m_stagingBufferMemHandle, 0, memReqs.size, 0, &pMappedMem));
    {
        auto * destPixels = static_cast<std::uint8_t *>(pMappedMem);
        for (int l = 0; l < layerCount; ++l)
        {
            for (int s = 0; s < surfaceCount; ++s)
            {
                const ImageSurface & surface = surfaces[l][s];
                std::memcpy(destPixels, surface.rawData, surface.sizeBytes);
                destPixels += surface.sizeBytes;
            }
        }
    }
    vkUnmapMemory(device, m_stagingBufferMemHandle);

    //
    // Setup buffer copy regions for each mip level:
    //
    VkDeviceSize bufferOffset = 0;
    FixedSizeArray<VkBufferImageCopy, MaxLayers * Image::MaxSurfaces> bufferCopyRegions;
    for (int l = 0; l < layerCount; ++l)
    {
        for (int s = 0; s < surfaceCount; ++s)
        {
            VkBufferImageCopy bufferCopyRegion               = {};
            bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel       = s;
            bufferCopyRegion.imageSubresource.baseArrayLayer = l;
            bufferCopyRegion.imageSubresource.layerCount     = 1;
            bufferCopyRegion.imageExtent.width               = surfaces[l][s].size.width;
            bufferCopyRegion.imageExtent.height              = surfaces[l][s].size.height;
            bufferCopyRegion.imageExtent.depth               = 1;
            bufferCopyRegion.bufferOffset                    = bufferOffset;

            bufferCopyRegions.push(bufferCopyRegion);
            bufferOffset += surfaces[l][s].sizeBytes;
        }
    }

    //
    // Create optimal tiled target image:
    //
    VkImageCreateInfo imageInfo;
    imageInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext                 = nullptr;
    imageInfo.flags                 = 0;
    imageInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageInfo.format                = imageFormat;
    imageInfo.extent.width          = imageSize.width;
    imageInfo.extent.height         = imageSize.height;
    imageInfo.extent.depth          = 1;
    imageInfo.mipLevels             = surfaceCount;
    imageInfo.arrayLayers           = layerCount;
    imageInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.pQueueFamilyIndices   = nullptr;
    imageInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    m_vkContext->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_imageHandle, &m_imageMemHandle);

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy.
    m_vkContext->changeImageLayout(cmdBuff, m_imageHandle,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   0, surfaceCount, 0, layerCount);

    // Copy mip levels from staging buffer:
    vkCmdCopyBufferToImage(cmdBuff, m_stagingBufferHandle,
                           m_imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           bufferCopyRegions.size(), bufferCopyRegions.data());

    // Change texture image layout to shader read after all mip levels have been copied:
    m_vkContext->changeImageLayout(cmdBuff, m_imageHandle,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   0, surfaceCount, 0, layerCount);

    //
    // Create the image view for our final color texture:
    //
    VkImageViewCreateInfo viewCreateInfo;
    viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext                           = nullptr;
    viewCreateInfo.flags                           = 0;
    viewCreateInfo.image                           = m_imageHandle;
    viewCreateInfo.viewType                        = (layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    viewCreateInfo.format                          = imageFormat;
    viewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel   = 0;
    viewCreateInfo.subresourceRange.levelCount     = surfaceCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = layerCount;
    VKTB_CHECK(vkCreateImageView(device, &viewCreateInfo, m_vkContext->allocationCallbacks(), &m_imageViewHandle));

    //
    // Set the new member states and we are done:
    //
    m_imageSize        = imageSize;
    m_imageFormat      = imageFormat;
    m_imageMipmapCount = narrowCast<std::uint16_t>(surfaceCount);
    m_imageLayerCount  = narrowCast<std::uint16_t>(layerCount);
    m_imageViewType    = (layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);

    // Link itself to the staging chain, so that the staging resources
    // get freed later on by the TextureManager.
    m_stagingLinkNext   = sm_stagingChainHead;
    sm_stagingChainHead = TextureManager::indexOf(*this);
}

VkFormat Texture::toVkImageFormat(const Image::Format format)
{
    switch (format)
    {
    case Image::Format::R8          : return VK_FORMAT_R8_UNORM;
    case Image::Format::RG8         : return VK_FORMAT_R8G8_UNORM;
    case Image::Format::RGB8        : return VK_FORMAT_R8G8B8_UNORM;
    case Image::Format::RGBA8       : return VK_FORMAT_R8G8B8A8_UNORM;
    case Image::Format::DXT1        : return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case Image::Format::DXT3        : return VK_FORMAT_BC2_UNORM_BLOCK;
    case Image::Format::DXT5        : return VK_FORMAT_BC3_UNORM_BLOCK;
    case Image::Format::RGTC1_ATI1N : return VK_FORMAT_BC4_UNORM_BLOCK;
    case Image::Format::RGTC2_ATI2N : return VK_FORMAT_BC5_UNORM_BLOCK;
    default : Log::fatalF("Invalid Image::Format enum!");
    } // switch
}

Image::Format Texture::toInternalImageFormat(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_UNORM            : return Image::Format::R8;
    case VK_FORMAT_R8G8_UNORM          : return Image::Format::RG8;
    case VK_FORMAT_R8G8B8_UNORM        : return Image::Format::RGB8;
    case VK_FORMAT_R8G8B8A8_UNORM      : return Image::Format::RGBA8;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK : return Image::Format::DXT1;
    case VK_FORMAT_BC2_UNORM_BLOCK     : return Image::Format::DXT3;
    case VK_FORMAT_BC3_UNORM_BLOCK     : return Image::Format::DXT5;
    case VK_FORMAT_BC4_UNORM_BLOCK     : return Image::Format::RGTC1_ATI1N;
    case VK_FORMAT_BC5_UNORM_BLOCK     : return Image::Format::RGTC2_ATI2N;
    default : Log::fatalF("Invalid VkFormat enum!");
    } // switch
}

void Texture::initClass()
{
    Log::debugF("---- Texture::initClass ----");

    // Vulkan works best with these settings.
    Image::sm_loadOptions.forceRGBA = true;
    Image::sm_loadOptions.roundUpToPowerOfTwo = true;
}

void Texture::shutdownClass()
{
    Log::debugF("---- Texture::shutdownClass ----");
    // Nothing at the moment...
}

} // namespace VkToolbox
