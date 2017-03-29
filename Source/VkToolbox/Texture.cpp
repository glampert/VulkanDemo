
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Texture.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Texture.hpp"
#include "CommandBuffer.hpp"

namespace VkToolbox
{

// ========================================================
// class Sampler:
// ========================================================

Sampler::Sampler(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
    , m_samplerDesc{}
{
}

Sampler::Sampler(const VulkanContext & vkContext, const VkSamplerCreateInfo & samplerCreateInfo)
    : m_vkContext{ &vkContext }
{
    initialize(samplerCreateInfo);
}

Sampler::~Sampler()
{
    shutdown();
}

void Sampler::initialize(const VkSamplerCreateInfo & samplerCreateInfo)
{
    assert(m_samplerHandle == VK_NULL_HANDLE); // Prevent double init

    m_samplerDesc = samplerCreateInfo;
    VKTB_CHECK(vkCreateSampler(m_vkContext->getVkDeviceHandle(), &m_samplerDesc,
                               m_vkContext->getAllocationCallbacks(), &m_samplerHandle));

    assert(m_samplerHandle != VK_NULL_HANDLE);
}

void Sampler::shutdown()
{
    if (m_samplerHandle != VK_NULL_HANDLE) // Could have been moved
    {
        vkDestroySampler(m_vkContext->getVkDeviceHandle(), m_samplerHandle,
                         m_vkContext->getAllocationCallbacks());

        m_samplerHandle = VK_NULL_HANDLE;
    }
}

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

// ========================================================
// class Texture:
// ========================================================

Texture::Texture(const VulkanContext & vkContext, ResourceId id)
    : Resource{ &vkContext, id }
    , m_imageHandle{ VK_NULL_HANDLE }
    , m_imageViewHandle{ VK_NULL_HANDLE }
    , m_imageMemHandle{ VK_NULL_HANDLE }
    , m_stagingImageHandle{ VK_NULL_HANDLE }
    , m_stagingImageMemHandle{ VK_NULL_HANDLE }
    , m_imageSize{ 0,0 }
    , m_imageFormat{ VK_FORMAT_UNDEFINED }
    , m_imageViewType{ VK_IMAGE_VIEW_TYPE_2D }
    , m_imageMipmaps{ 0 }
    , m_dontGenMipmaps{ false }
{
}

Texture::~Texture()
{
    Texture::shutdown();
}

Texture::Texture(Texture && other)
    : Resource{ other.m_vkContext, other.m_resId }
    , m_imageHandle{ other.m_imageHandle }
    , m_imageViewHandle{ other.m_imageViewHandle }
    , m_imageMemHandle{ other.m_imageMemHandle }
    , m_stagingImageHandle{ other.m_stagingImageHandle }
    , m_stagingImageMemHandle{ other.m_stagingImageMemHandle }
    , m_imageSize{ other.m_imageSize }
    , m_imageFormat{ other.m_imageFormat }
    , m_imageViewType{ other.m_imageViewType }
    , m_imageMipmaps{ other.m_imageMipmaps }
    , m_dontGenMipmaps{ other.m_dontGenMipmaps }
{
    other.clear();
}

Texture & Texture::operator = (Texture && other)
{
    Texture::shutdown();

    m_vkContext             = other.m_vkContext;
    m_resId                 = other.m_resId;
    m_imageHandle           = other.m_imageHandle;
    m_imageViewHandle       = other.m_imageViewHandle;
    m_imageMemHandle        = other.m_imageMemHandle;
    m_stagingImageHandle    = other.m_stagingImageHandle;
    m_stagingImageMemHandle = other.m_stagingImageMemHandle;
    m_imageSize             = other.m_imageSize;
    m_imageFormat           = other.m_imageFormat;
    m_imageViewType         = other.m_imageViewType;
    m_imageMipmaps          = other.m_imageMipmaps;
    m_dontGenMipmaps        = other.m_dontGenMipmaps;

    other.clear();
    return *this;
}

bool Texture::load()
{
    if (isShutdown())
    {
        Log::warningF("Texture is already shutdown and cannot be loaded!");
        return false;
    }

    const char * const name = getId().getName();
    if (!probeFile(name))
    {
        Log::warningF("Image file '%s' does not exist! Can't load texture from it.", name);
        return false;
    }

    // We load the image in-place to avoid keeping two copies of the memory
    // at the same time in case of a reload, so need to check for that before
    // anything else.
    if (isLoaded())
    {
        unload();
    }

    str256 errorInfo;
    Image image;

    if (!image.initFromFile(name, &errorInfo))
    {
        Log::warningF("Failed to load texture: %s", errorInfo.c_str());
        return false;
    }

    // If we didn't already do so in Image::initFromFile...
    if (generateMipmapsOnLoad() && !image.isMipmapped())
    {
        image.generateMipmapSurfaces();
    }

    initVkTextureData(image);
    return true;
}

void Texture::unload()
{
    const auto device   = m_vkContext->getVkDeviceHandle();
    const auto allocCBs = m_vkContext->getAllocationCallbacks();

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

    releaseStagingImage();
}

void Texture::releaseStagingImage()
{
    const auto device   = m_vkContext->getVkDeviceHandle();
    const auto allocCBs = m_vkContext->getAllocationCallbacks();

    if (m_stagingImageHandle != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, m_stagingImageHandle, allocCBs);
        m_stagingImageHandle = VK_NULL_HANDLE;
    }
    if (m_stagingImageMemHandle != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_stagingImageMemHandle, allocCBs);
        m_stagingImageMemHandle = VK_NULL_HANDLE;
    }
}

void Texture::shutdown()
{
    Texture::unload();
    Resource::clear();
}

void Texture::clear()
{
    Resource::clear();

    // Used by the move ops, so don't delete, just drop ownership.
    m_imageHandle           = VK_NULL_HANDLE;
    m_imageViewHandle       = VK_NULL_HANDLE;
    m_imageMemHandle        = VK_NULL_HANDLE;
    m_stagingImageHandle    = VK_NULL_HANDLE;
    m_stagingImageMemHandle = VK_NULL_HANDLE;
}

void Texture::initVkTextureData(const Image & image)
{
    VkImageCreateInfo imageInfo;

    const auto device                 = m_vkContext->getVkDeviceHandle();
    const CommandBuffer & cmdBuff     = m_vkContext->getMainTextureStagingCmdBuffer();
    const Size2D imageSize            = image.getSize();
    const int bytesPerPixel           = image.getBytesPerPixel();
    const int mipmapCount             = image.getSurfaceCount();
    const VkFormat vkImgFormat        = getVkFormat(image.getFormat());
    const VkDeviceSize imageSizeBytes = (imageSize.width * imageSize.height * bytesPerPixel);

    // Temporary staging image:
    imageInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext                   = nullptr;
    imageInfo.flags                   = 0;
    imageInfo.imageType               = VK_IMAGE_TYPE_2D;
    imageInfo.format                  = vkImgFormat;
    imageInfo.extent.width            = imageSize.width;
    imageInfo.extent.height           = imageSize.height;
    imageInfo.extent.depth            = 1;
    imageInfo.mipLevels               = mipmapCount;
    imageInfo.arrayLayers             = 1;
    imageInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling                  = VK_IMAGE_TILING_LINEAR;
    imageInfo.usage                   = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode             = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount   = 0;
    imageInfo.pQueueFamilyIndices     = nullptr;
    imageInfo.initialLayout           = VK_IMAGE_LAYOUT_PREINITIALIZED;

    m_vkContext->createImage(imageInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             &m_stagingImageHandle, &m_stagingImageMemHandle);

    // Copy into staging image:
    {
        VkImageSubresource subResource;
        subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subResource.mipLevel   = 0;
        subResource.arrayLayer = 0;

        VkSubresourceLayout stagingImageLayout = {};
        vkGetImageSubresourceLayout(device, m_stagingImageHandle, &subResource, &stagingImageLayout);

        void * pData = nullptr;
        ScopedMapMemory memMap{ *m_vkContext, m_stagingImageMemHandle, 0, imageSizeBytes, 0, &pData };

        if (stagingImageLayout.rowPitch == (imageSize.width * bytesPerPixel))
        {
            std::memcpy(pData, image.getPixelDataBaseSurface(), imageSizeBytes);
        }
        else
        {
            auto * pixels = image.getPixelDataBaseSurface();
            auto * dataBytes = static_cast<std::uint8_t *>(pData);

            for (int y = 0; y < imageSize.height; ++y)
            {
                std::memcpy(&dataBytes[y * stagingImageLayout.rowPitch],
                            &pixels[y * imageSize.width * bytesPerPixel],
                            imageSize.width * bytesPerPixel);
            }
        }
    }

    // The final texture image:
    imageInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext                 = nullptr;
    imageInfo.flags                 = 0;
    imageInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageInfo.format                = vkImgFormat;
    imageInfo.extent.width          = imageSize.width;
    imageInfo.extent.height         = imageSize.height;
    imageInfo.extent.depth          = 1;
    imageInfo.mipLevels             = mipmapCount;
    imageInfo.arrayLayers           = 1;
    imageInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.pQueueFamilyIndices   = nullptr;
    imageInfo.initialLayout         = VK_IMAGE_LAYOUT_PREINITIALIZED;

    m_vkContext->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             &m_imageHandle, &m_imageMemHandle);

    // Staging image => source of an image copy op
    m_vkContext->changeImageLayout(&cmdBuff, m_stagingImageHandle,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_PREINITIALIZED,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Final color texture image => destination of an image copy op
    m_vkContext->changeImageLayout(&cmdBuff, m_imageHandle,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_PREINITIALIZED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy from the staging to the final color texture image:
    m_vkContext->copyImage(&cmdBuff, m_stagingImageHandle, m_imageHandle, imageSize);

    // Final color texture image => shader visible image (we can now sample from it)
    m_vkContext->changeImageLayout(&cmdBuff, m_imageHandle,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Create the image view for our final color texture:
    VkImageViewCreateInfo viewCreateInfo;
    viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext                           = nullptr;
    viewCreateInfo.flags                           = 0;
    viewCreateInfo.image                           = m_imageHandle;
    viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                          = vkImgFormat;
    viewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel   = 0;
    viewCreateInfo.subresourceRange.levelCount     = mipmapCount;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = 1;
    VKTB_CHECK(vkCreateImageView(device, &viewCreateInfo, m_vkContext->getAllocationCallbacks(), &m_imageViewHandle));

    // Set the new member states and we are done.
    m_imageSize     = imageSize;
    m_imageFormat   = vkImgFormat;
    m_imageMipmaps  = mipmapCount;
    m_imageViewType = VK_IMAGE_VIEW_TYPE_2D;
}

VkFormat Texture::getVkFormat(const Image::Format format)
{
    switch (format)
    {
    case Image::Format::R8    : return VK_FORMAT_R8_UNORM;
    case Image::Format::RG8   : return VK_FORMAT_R8G8_UNORM;
    case Image::Format::RGB8  : return VK_FORMAT_R8G8B8_UNORM;
    case Image::Format::RGBA8 : return VK_FORMAT_R8G8B8A8_UNORM;
    default : Log::fatalF("Invalid Image::Format enum!");
    } // switch
}

Image::Format Texture::getImageFormat(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_UNORM       : return Image::Format::R8;
    case VK_FORMAT_R8G8_UNORM     : return Image::Format::RG8;
    case VK_FORMAT_R8G8B8_UNORM   : return Image::Format::RGB8;
    case VK_FORMAT_R8G8B8A8_UNORM : return Image::Format::RGBA8;
    default : Log::fatalF("Invalid VkFormat enum!");
    } // switch
}

void Texture::initClass()
{
    Log::debugF("---- Texture::initClass ----");

    Image::sm_loadOptions.forceRGBA = true;
    Image::sm_loadOptions.roundUpToPowerOfTwo = true;
}

void Texture::shutdownClass()
{
    Log::debugF("---- Texture::shutdownClass ----");
    // Nothing at the moment...
}

} // namespace VkToolbox
