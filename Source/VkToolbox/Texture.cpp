
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Texture.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Vulkan texture wrapper.
// ================================================================================================

#include "Texture.hpp"

namespace VkToolbox
{

// ========================================================
// class Sampler:
// ========================================================

Sampler::Sampler(WeakRef<const VulkanContext> vkContext)
    : m_vkContext{ vkContext }
    , m_samplerHandle{ VK_NULL_HANDLE }
    , m_samplerDesc{}
{
}

Sampler::Sampler(WeakRef<const VulkanContext> vkContext,
                 const VkSamplerCreateInfo & samplerCreateInfo)
    : m_vkContext{ vkContext }
    , m_samplerHandle{ VK_NULL_HANDLE }
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
    other.m_vkContext     = VK_NULL_HANDLE;
    other.m_samplerHandle = VK_NULL_HANDLE;
}

Sampler & Sampler::operator = (Sampler && other)
{
    shutdown();

    m_vkContext           = other.m_vkContext;
    m_samplerHandle       = other.m_samplerHandle;
    m_samplerDesc         = other.m_samplerDesc;

    other.m_vkContext     = VK_NULL_HANDLE;
    other.m_samplerHandle = VK_NULL_HANDLE;

    return *this;
}

// ========================================================
// class Texture:
// ========================================================

Texture::Texture(WeakRef<const VulkanContext> vkContext, ResourceId id)
    : Resource{ vkContext, id }
    , m_imageHandle{ VK_NULL_HANDLE }
    , m_imageViewHandle{ VK_NULL_HANDLE }
    , m_imageMemHandle{ VK_NULL_HANDLE }
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

    m_vkContext       = other.m_vkContext;
    m_resId           = other.m_resId;
    m_imageHandle     = other.m_imageHandle;
    m_imageViewHandle = other.m_imageViewHandle;
    m_imageMemHandle  = other.m_imageMemHandle;
    m_imageSize       = other.m_imageSize;
    m_imageFormat     = other.m_imageFormat;
    m_imageViewType   = other.m_imageViewType;
    m_imageMipmaps    = other.m_imageMipmaps;
    m_dontGenMipmaps  = other.m_dontGenMipmaps;

    other.clear();
    return *this;
}

bool Texture::load()
{
    const char * const name = getId().getName();
    if (isShutdown())
    {
        Log::warningF("Resource %s is already shutdown and cannot be loaded!", name);
        return false;
    }

    Image img;
    str256 errorInfo;
    if (!img.initFromFile(name, &errorInfo))
    {
        Log::errorF("Failed to load texture: %s", errorInfo.c_str());
        return false;
    }

    // If we didn't already do so in Image::initFromFile
    if (willGenerateMipmapsOnLoad() && !Image::sm_loadOptions.generateMipmaps)
    {
        img.generateMipmapSurfaces();
    }

    return initGpuData(img);
}

void Texture::unload()
{
    if (m_imageViewHandle != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_vkContext->getVkDeviceHandle(), m_imageViewHandle, m_vkContext->getAllocationCallbacks());
        m_imageViewHandle = VK_NULL_HANDLE;
    }
    if (m_imageHandle != VK_NULL_HANDLE)
    {
        vkDestroyImage(m_vkContext->getVkDeviceHandle(), m_imageHandle, m_vkContext->getAllocationCallbacks());
        m_imageHandle = VK_NULL_HANDLE;
    }
    if (m_imageMemHandle != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_vkContext->getVkDeviceHandle(), m_imageMemHandle, m_vkContext->getAllocationCallbacks());
        m_imageMemHandle = VK_NULL_HANDLE;
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
    m_imageHandle     = VK_NULL_HANDLE;
    m_imageViewHandle = VK_NULL_HANDLE;
    m_imageMemHandle  = VK_NULL_HANDLE;
}

bool Texture::initGpuData(const Image & img)
{
    const VkFormat vkImgFormat = getVkFormat(img.getFormat());
    const VkFormatProperties & vkFormatProps = m_vkContext->getVkFormatPropertiesForImageFormat(img.getFormat());

    // See if we can use a linear tiled image for a texture,
    // if not, we will need a staging image for the texture data.
    const bool needStaging = (!(vkFormatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) ? true : false;

    VkImageCreateInfo imageCreateInfo     = {};
    imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                 = nullptr;
    imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                = vkImgFormat;
    imageCreateInfo.extent.width          = img.getSize().width;
    imageCreateInfo.extent.height         = img.getSize().height;
    imageCreateInfo.extent.depth          = 1;
    imageCreateInfo.mipLevels             = img.getSurfaceCount();
    imageCreateInfo.arrayLayers           = 1;
    imageCreateInfo.samples               = VkSampleCountFlagBits(VulkanContext::sm_multiSampleCount);
    imageCreateInfo.tiling                = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageCreateInfo.usage                 = (needStaging ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT);
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices   = nullptr;
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.flags                 = 0;

    VkMemoryAllocateInfo memAllocInfo     = {};
    memAllocInfo.sType                    = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext                    = nullptr;

    VkImage        newVkImage;
    VkImageView    newVkImageView;
    VkDeviceMemory newVkImageMemory;

    auto device   = m_vkContext->getVkDeviceHandle();
    auto allocCBs = m_vkContext->getAllocationCallbacks();

    // Create a mappable image. It will be the texture if linear images are OK
    // to be textures or it will be the staging image if they are not.
    VKTB_CHECK(vkCreateImage(device, &imageCreateInfo, allocCBs, &newVkImage));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, newVkImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;

    // Find the memory type that is host mappable:
    constexpr VkFlags mask = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    memAllocInfo.memoryTypeIndex = m_vkContext->getMemoryTypeFromProperties(memRequirements.memoryTypeBits, mask);
    assert(memAllocInfo.memoryTypeIndex < UINT32_MAX && "No mappable, coherent memory!");

    // Allocate the GPU memory:
    VKTB_CHECK(vkAllocateMemory(device, &memAllocInfo, allocCBs, &newVkImageMemory));

    // Bind memory to the texture image object:
    VKTB_CHECK(vkBindImageMemory(device, newVkImage, newVkImageMemory, 0));

    // Create the image view:
    VkImageViewCreateInfo viewCreateInfo           = {};
    viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext                           = nullptr;
    viewCreateInfo.image                           = newVkImage;
    viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    viewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel   = 0;
    viewCreateInfo.subresourceRange.levelCount     = img.getSurfaceCount();
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount     = 1;
    VKTB_CHECK(vkCreateImageView(device, &viewCreateInfo, allocCBs, &newVkImageView));

    //TODO
    // all the rest
    // update all members, etc...

    return true;
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

} // namespace VkToolbox
