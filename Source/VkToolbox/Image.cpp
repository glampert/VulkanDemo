
// ================================================================================================
// File: VkToolbox/Image.cpp
// Author: Guilherme R. Lampert
// Created on: 21/03/17
// Brief: Image loading and mip mapping helpers.
// ================================================================================================

#include "Image.hpp"
#include "Log.hpp"

// STB image libraries:
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_image_resize.h"

namespace VkToolbox
{

// ========================================================

const enum_array<Image::Format, const char *> Image::FormatStrings = {{
    "None",
    "R8",
    "RG8",
    "RGB8",
    "RGBA8",
    "DXT1",
    "DXT3",
    "DXT5",
    "RGTC1_ATI1N",
    "RGTC2_ATI2N"
}};

Image::LoadOptions Image::sm_loadOptions;

// ========================================================
// Image:
// ========================================================

void Image::shutdown()
{
    m_rawDataBaseSurface    = nullptr;
    m_rawDataMipmapSurfaces = nullptr;
    m_memoryUsageBytes      = 0;
    m_format                = Format::None;
    m_surfaces.clear();
}

void Image::initWithSize(const Size2D wh, const Format fmt)
{
    assert(wh.width  != 0);
    assert(wh.height != 0);
    assert(!isInitialized());

    m_format = fmt;
    m_memoryUsageBytes = (wh.width * wh.height * bytesPerPixel());

    auto * const pixels = static_cast<std::uint8_t *>(std::malloc(m_memoryUsageBytes));
    if (pixels == nullptr)
    {
        Log::fatalF("Failed to malloc %u bytes for Image data!", m_memoryUsageBytes);
    }
    m_rawDataBaseSurface.reset(pixels);

    ImageSurface baseSurface{ pixels, m_memoryUsageBytes, wh };
    m_surfaces.push(baseSurface);
}

void Image::initWithColorFill(const Size2D wh, const Color32 fillColor)
{
    initWithSize(wh, Format::RGBA8); // Color32 is RGBA

    const int pixelCount  = wh.width * wh.height;
    auto * const pixels   = reinterpret_cast<std::uint32_t *>(pixelDataBaseSurface());
    const std::uint32_t c = fillColor.toUInt32();

    for (int p = 0; p < pixelCount; ++p)
    {
        pixels[p] = c;
    }
}

void Image::initWithExternalData(const Size2D wh, const Format fmt, const void * const baseSurface)
{
    assert(baseSurface != nullptr);
    initWithSize(wh, fmt);
    std::memcpy(pixelDataBaseSurface(), baseSurface, m_memoryUsageBytes);
}

void Image::initWithCheckerPattern(const Size2D wh, const int squares)
{
    assert(squares >= 2 && squares <= 64);
    assert((wh.width  % squares) == 0);
    assert((wh.height % squares) == 0);
    assert(!isInitialized());

    initWithSize(wh, Format::RGBA8);

    // One square black, one white.
    const Color32 colors[2] = {
        Color32{ 0,   0,   0,   255 },
        Color32{ 255, 255, 255, 255 }
    };

    auto * const pixels = reinterpret_cast<Color32 *>(pixelDataBaseSurface());
    const int checkerSize = wh.width / squares; // Size of one checker square, in pixels.

    for (int y = 0; y < wh.height; ++y)
    {
        for (int x = 0; x < wh.width; ++x)
        {
            const int colorIndex = ((y / checkerSize) + (x / checkerSize)) % 2;
            const int pixelIndex = x + (y * wh.width);

            assert(colorIndex < 2);
            assert(pixelIndex < (wh.width * wh.height));
            pixels[pixelIndex] = colors[colorIndex];
        }
    }
}

bool Image::initFromFile(const char * const filePath)
{
    assert(filePath != nullptr);
    assert(filePath[0] != '\0');
    assert(!isInitialized());

    int width  = 0;
    int height = 0;
    int comps  = 0;
    const int requiredComps = (sm_loadOptions.forceRGBA ? 4 : 0);

    stbi_set_flip_vertically_on_load(sm_loadOptions.flipVerticallyOnLoad);
    std::uint8_t * const newImageData = stbi_load(filePath, &width, &height, &comps, requiredComps);

    if (newImageData == nullptr)
    {
        Log::errorF("stdi_load() failed with error '%s' for image \"%s\".", stbi_failure_reason(), filePath);
        return false;
    }

    Format fmt;
    if (sm_loadOptions.forceRGBA)
    {
        fmt = Format::RGBA8;
    }
    else
    {
        switch (comps)
        {
        case 1: // Grayscale (equivalent to R8 - 1 channel)
            fmt = Format::R8;
            break;
        case 2: // Gray+alpha (equivalent to RG8 - 2 channels)
            fmt = Format::RG8;
            break;
        case 3: // RGB (3 channels)
            fmt = Format::RGB8;
            break;
        case 4: // RGBA (4 channels)
            fmt = Format::RGBA8;
            break;
        default: // Unsupported
            Log::errorF("\"%s\": Image format not supported! components=%i", filePath, comps);
            std::free(newImageData);
            return false;
        } // switch
    }

    m_format = fmt;
    m_memoryUsageBytes = (width * height * bytesPerPixel());
    m_rawDataBaseSurface.reset(newImageData);

    ImageSurface baseSurface{ newImageData, m_memoryUsageBytes, { width, height } };
    m_surfaces.push(baseSurface);

    if (!VkToolbox::isPowerOfTwo(width) || !VkToolbox::isPowerOfTwo(height))
    {
        if (sm_loadOptions.roundDownToPowerOfTwo)
        {
            roundDownBaseSurfaceToPowerOfTwo();
        }
        else if (sm_loadOptions.roundUpToPowerOfTwo)
        {
            roundUpBaseSurfaceToPowerOfTwo();
        }
    }

    if (sm_loadOptions.generateMipmaps)
    {
        generateMipmapSurfaces();
    }

    return true;
}

bool Image::writeToFile(const char * const filePath, const int surfaceIndex) const
{
    assert(filePath != nullptr);
    assert(filePath[0] != '\0');

    if (!isValid())
    {
        return false;
    }

    const str_ref strFilePathName{ filePath };
    const Size2D surfSize = size(surfaceIndex);

    if (strFilePathName.ends_with(".tga"))
    {
        return stbi_write_tga(filePath, surfSize.width, surfSize.height,
                              channelCount(), surface(surfaceIndex).rawData) != 0;
    }
    else if (strFilePathName.ends_with(".bmp"))
    {
        return stbi_write_bmp(filePath, surfSize.width, surfSize.height,
                              channelCount(), surface(surfaceIndex).rawData) != 0;
    }
    else if (strFilePathName.ends_with(".png"))
    {
        const int strideInBytes = surfSize.width * bytesPerPixel();
        return stbi_write_png(filePath, surfSize.width, surfSize.height,
                              channelCount(), surface(surfaceIndex).rawData, strideInBytes) != 0;
    }
    else
    {
        Log::errorF("Unsupported image format or unknown file extension in path \"%s\"!", filePath);
        return false;
    }
}

bool Image::writeAllSurfacesToFiles(const char * const basePathName, const char * const extension) const
{
    assert(basePathName != nullptr && basePathName[0] != '\0');
    assert(extension    != nullptr && extension[0]    != '\0');

    if (!isValid())
    {
        return false;
    }

    str512 surfaceName;
    int errorCount = 0;

    const int surfaces = surfaceCount();
    for (int s = 0; s < surfaces; ++s)
    {
        surfaceName.setf("%s_%i.%s", basePathName, s, extension);
        if (!writeToFile(surfaceName.c_str(), s))
        {
            ++errorCount;
        }
    }

    return errorCount == 0;
}

// R8 pixel access ========================================

static inline void Image_setPixel_R8(ImageSurface & surface, const int pX, const int pY, const std::uint8_t * pixelIn)
{
    std::uint8_t * const pixels = surface.rawData;
    pixels[pX + (pY * surface.size.width)] = *pixelIn;
}

static inline void Image_pixel_R8(const ImageSurface & surface, const int pX, const int pY, std::uint8_t * pixelOut)
{
    const std::uint8_t * const pixels = surface.rawData;
    *pixelOut = pixels[pX + (pY * surface.size.width)];
}

// RG8 pixel access =======================================

static inline void Image_setPixel_RG8(ImageSurface & surface, const int pX, const int pY, const std::uint8_t * pixelIn)
{
    std::uint8_t * const pixels = surface.rawData;
    const int pixelIndex = pX + (pY * surface.size.width);
    pixels[(pixelIndex * 2) + 0] = pixelIn[0];
    pixels[(pixelIndex * 2) + 1] = pixelIn[1];
}

static inline void Image_pixel_RG8(const ImageSurface & surface, const int pX, const int pY, std::uint8_t * pixelOut)
{
    const std::uint8_t * const pixels = surface.rawData;
    const int pixelIndex = pX + (pY * surface.size.width);
    pixelOut[0] = pixels[(pixelIndex * 2) + 0];
    pixelOut[1] = pixels[(pixelIndex * 2) + 1];
}

// RGB8 pixel access ======================================

static inline void Image_setPixel_RGB8(ImageSurface & surface, const int pX, const int pY, const std::uint8_t * pixelIn)
{
    std::uint8_t * const pixels = surface.rawData;
    const int pixelIndex = pX + (pY * surface.size.width);
    pixels[(pixelIndex * 3) + 0] = pixelIn[0];
    pixels[(pixelIndex * 3) + 1] = pixelIn[1];
    pixels[(pixelIndex * 3) + 2] = pixelIn[2];
}

static inline void Image_pixel_RGB8(const ImageSurface & surface, const int pX, const int pY, std::uint8_t * pixelOut)
{
    const std::uint8_t * const pixels = surface.rawData;
    const int pixelIndex = pX + (pY * surface.size.width);
    pixelOut[0] = pixels[(pixelIndex * 3) + 0];
    pixelOut[1] = pixels[(pixelIndex * 3) + 1];
    pixelOut[2] = pixels[(pixelIndex * 3) + 2];
}

// RGBA8 pixel access =====================================

static inline void Image_setPixel_RGBA8(ImageSurface & surface, const int pX, const int pY, const std::uint8_t * pixelIn)
{
    auto * outPixelU32 = surface.pixelDataAs<std::uint32_t>();
    const auto * inPixelU32 = reinterpret_cast<const std::uint32_t *>(pixelIn);
    outPixelU32[pX + (pY * surface.size.width)] = *inPixelU32;
}

static inline void Image_pixel_RGBA8(const ImageSurface & surface, const int pX, const int pY, std::uint8_t * pixelOut)
{
    const auto * inPixelU32 = surface.pixelDataAs<std::uint32_t>();
    auto * outPixelU32 = reinterpret_cast<std::uint32_t *>(pixelOut);
    *outPixelU32 = inPixelU32[pX + (pY * surface.size.width)];
}

// ========================================================

void Image::setPixel(const int pX, const int pY, const std::uint8_t * const pixelIn, const int surfaceIndex)
{
    assert(pixelIn != nullptr);
    assert(isValid());

    ImageSurface & surf = surface(surfaceIndex);
    assert(pX >= 0 && pX < surf.size.width);
    assert(pY >= 0 && pY < surf.size.height);

    switch (m_format)
    {
    case Format::R8    : { Image_setPixel_R8   (surf, pX, pY, pixelIn); break; }
    case Format::RG8   : { Image_setPixel_RG8  (surf, pX, pY, pixelIn); break; }
    case Format::RGB8  : { Image_setPixel_RGB8 (surf, pX, pY, pixelIn); break; }
    case Format::RGBA8 : { Image_setPixel_RGBA8(surf, pX, pY, pixelIn); break; }
    default            : { assert(false && "Image format does not support pixel access!"); break; }
    } // switch
}

void Image::pixel(const int pX, const int pY, std::uint8_t * pixelOut, const int surfaceIndex) const
{
    assert(pixelOut != nullptr);
    assert(isValid());

    const ImageSurface & surf = surface(surfaceIndex);
    assert(pX >= 0 && pX < surf.size.width);
    assert(pY >= 0 && pY < surf.size.height);

    switch (m_format)
    {
    case Format::R8    : { Image_pixel_R8   (surf, pX, pY, pixelOut); break; }
    case Format::RG8   : { Image_pixel_RG8  (surf, pX, pY, pixelOut); break; }
    case Format::RGB8  : { Image_pixel_RGB8 (surf, pX, pY, pixelOut); break; }
    case Format::RGBA8 : { Image_pixel_RGBA8(surf, pX, pY, pixelOut); break; }
    default            : { assert(false && "Image format does not support pixel access!"); break; }
    } // switch
}

void Image::swapPixel(const int pX0, const int pY0, const int pX1, const int pY1, const int surfaceIndex)
{
    std::uint8_t tempPixel0[MaxPixelBytes];
    std::uint8_t tempPixel1[MaxPixelBytes];

    pixel(pX0, pY0, tempPixel0, surfaceIndex);
    pixel(pX1, pY1, tempPixel1, surfaceIndex);

    setPixel(pX0, pY0, tempPixel1, surfaceIndex);
    setPixel(pX1, pY1, tempPixel0, surfaceIndex);
}

void Image::flipVInPlace(const int surfaceIndex)
{
    assert(isValid());
    const ImageSurface & surf = surface(surfaceIndex);

    const int width      = surf.size.width;
    const int maxY       = surf.size.height - 1;
    const int halfHeight = surf.size.height / 2;

    for (int y = 0; y < halfHeight; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            swapPixel(x, y, x, maxY - y, surfaceIndex);
        }
    }
}

void Image::flipHInPlace(const int surfaceIndex)
{
    assert(isValid());
    const ImageSurface & surf = surface(surfaceIndex);

    const int height    = surf.size.height;
    const int maxX      = surf.size.width - 1;
    const int halfWidth = surf.size.width / 2;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < halfWidth; ++x)
        {
            swapPixel(x, y, maxX - x, y, surfaceIndex);
        }
    }
}

void Image::generateMipmapSurfaces()
{
    assert(isValid());

    if (size().width == 1 && size().height == 1)
    {
        // If the base surface happens to be a 1x1 image then
        // we can't subdivide it any further. A 2x2 image could
        // still generate one 1x1 mipmap level though.
        return;
    }

    // Initial image is the base surface (mipmap level = 0).
    // We always use the initial image to generate all mipmaps
    // to avoid propagating errors from the downsampling.
    const int initialWidth  = size().width;
    const int initialHeight = size().height;
    const std::uint8_t * initialImage = pixelDataBaseSurface();

    int targetWidth  = initialWidth;
    int targetHeight = initialHeight;
    int mipmapCount  = 1; // Surface 0 is the initial mipmap.
    int mipmapBytes  = 0;

    // First, figure out how much memory to allocate.
    // Stop when any of the dimensions reach 1.
    while (mipmapCount != MaxSurfaces)
    {
        targetWidth  = std::max(1, targetWidth  / 2);
        targetHeight = std::max(1, targetHeight / 2);

        mipmapBytes += targetWidth * targetHeight * bytesPerPixel();
        mipmapCount++;

        if (targetWidth == 1 && targetHeight == 1)
        {
            break;
        }
    }
    m_surfaces.resize(mipmapCount);

    // Allocate the needed memory to accommodate all mipmaps:
    m_rawDataMipmapSurfaces.reset(static_cast<std::uint8_t *>(std::malloc(mipmapBytes)));
    if (m_rawDataMipmapSurfaces == nullptr)
    {
        Log::fatalF("Failed to malloc %u bytes for Image mipmap data!", mipmapBytes);
    }

    // Also update the image's memory statistics.
    m_memoryUsageBytes += mipmapBytes;

    // Now build the mipmap surfaces:
    std::uint8_t * mipDataPtr = m_rawDataMipmapSurfaces.get();
    targetWidth  = initialWidth;
    targetHeight = initialHeight;
    mipmapCount  = 1;

    while (mipmapCount != MaxSurfaces)
    {
        targetWidth  = std::max(1, targetWidth  / 2);
        targetHeight = std::max(1, targetHeight / 2);

        // NOTE:
        // Should this be stbir_resize_uint8_srgb() instead?
        // Have a flag in Image telling if it is sRGB, maybe?
        stbir_resize_uint8(
            // Source Image:
            initialImage,
            initialWidth,
            initialHeight,
            0,
            // Destination Image:
            mipDataPtr,
            targetWidth,
            targetHeight,
            0,
            // # Image channels:
            channelCount()
        );

        m_surfaces[mipmapCount].rawData     = mipDataPtr;
        m_surfaces[mipmapCount].sizeBytes   = targetWidth * targetHeight * bytesPerPixel();
        m_surfaces[mipmapCount].size.width  = targetWidth;
        m_surfaces[mipmapCount].size.height = targetHeight;
        mipmapCount++;

        if (targetWidth == 1 && targetHeight == 1)
        {
            break;
        }

        mipDataPtr += targetWidth * targetHeight * bytesPerPixel();
    }
}

void Image::flipVCopyBaseSurface(Image & flippedImage) const
{
    assert(isValid());
    const Size2D wh = size();

    flippedImage.shutdown();
    flippedImage.initWithSize(wh, m_format);

    const int maxX = wh.width  - 1;
    const int maxY = wh.height - 1;
    std::uint8_t tempPixel[MaxPixelBytes];

    for (int y = 0; y < wh.height; ++y)
    {
        for (int x = 0; x < wh.width; ++x)
        {
            pixel(maxX - x, y, tempPixel);
            flippedImage.setPixel(maxX - x, maxY - y, tempPixel);
        }
    }
}

void Image::flipHCopyBaseSurface(Image & flippedImage) const
{
    assert(isValid());
    const Size2D wh = size();

    flippedImage.shutdown();
    flippedImage.initWithSize(wh, m_format);

    std::uint8_t tempPixel[MaxPixelBytes];

    for (int y = 0; y < wh.height; ++y)
    {
        for (int x = 0; x < wh.width;)
        {
            pixel(x++, y, tempPixel);
            flippedImage.setPixel(wh.width - x, y, tempPixel);
        }
    }
}

void Image::resizeBaseSurfaceInPlace(const int targetWidth, const int targetHeight)
{
    assert(targetWidth  > 0);
    assert(targetHeight > 0);
    assert(isValid());

    if (size().width == targetWidth && size().height == targetHeight)
    {
        return;
    }

    // Resulting image may be bigger or smaller. We always need a copy.
    Image resizedImage;
    resizeCopyBaseSurface(resizedImage, targetWidth, targetHeight);

    // Move resized image into this
    (*this) = std::move(resizedImage);
}

void Image::resizeCopyBaseSurface(Image & destImage, const int targetWidth, const int targetHeight) const
{
    assert(targetWidth  > 0);
    assert(targetHeight > 0);
    assert(isValid());

    if (size().width == targetWidth && size().height == targetHeight)
    {
        return;
    }

    if (destImage.isInitialized())
    {
        destImage.shutdown();
    }
    destImage.initWithSize({ targetWidth, targetHeight }, m_format);

    // NOTE:
    // Should this be stbir_resize_uint8_srgb() instead?
    // Have a flag in Image telling if it is sRGB, maybe?
    stbir_resize_uint8(
        // Source Image:
        pixelDataBaseSurface(),
        size().width,
        size().height,
        /* src stride = */ 0,
        // Destination Image:
        destImage.pixelDataBaseSurface(),
        destImage.size().width,
        destImage.size().height,
        /* dest stride = */ 0,
        /* channels = */ channelCount()
    );
}

void Image::roundDownBaseSurfaceToPowerOfTwo()
{
    assert(isValid());
    if (isPowerOfTwo())
    {
        return; // Nothing to do.
    }
    resizeBaseSurfaceInPlace(roundDownToPowerOfTwo(size().width),
                             roundDownToPowerOfTwo(size().height));
}

void Image::roundUpBaseSurfaceToPowerOfTwo()
{
    assert(isValid());
    if (isPowerOfTwo())
    {
        return; // Nothing to do.
    }
    resizeBaseSurfaceInPlace(roundUpToPowerOfTwo(size().width),
                             roundUpToPowerOfTwo(size().height));
}

void Image::discardBaseSurfaceAlphaChannel()
{
    assert(isValid());
    if (m_format != Format::RGBA8)
    {
        return;
    }

    const Size2D wh = size();
    std::uint8_t tempPixel[MaxPixelBytes];

    // Resulting image will be smaller; We need a new one:
    Image rgbImage;
    rgbImage.initWithSize(wh, Format::RGB8);

    // Discard alpha for each pixels of the source image:
    for (int y = 0; y < wh.height; ++y)
    {
        for (int x = 0; x < wh.width; ++x)
        {
            pixel(x, y, tempPixel);

            // Since 'rgbImage' is 3 channels, the alpha
            // component will simply be ignored by setPixel().
            rgbImage.setPixel(x, y, tempPixel);
        }
    }

    (*this) = std::move(rgbImage);
}

void Image::copyBaseSurfaceRect(Image & destImage, const int xOffset, const int yOffset,
                                const int rectWidth, const int rectHeight, const bool topLeft) const
{
    assert(xOffset >= 0);
    assert(yOffset >= 0);

    assert(rectHeight > 0);
    assert(rectWidth  > 0);

    assert(isValid());

    // We might need to resize the destination image or change its pixel format.
    const Size2D destSize = destImage.size();
    if (destImage.m_format != m_format || destSize.width < rectWidth || destSize.height < rectHeight)
    {
        if (destImage.isInitialized())
        {
            destImage.shutdown();
        }
        destImage.initWithSize({ rectWidth, rectHeight }, m_format);
    }

    std::uint8_t tempPixel[MaxPixelBytes];
    if (topLeft)
    {
        // Use the top-left corner of the image as the (0,0) origin; Invert Y.
        int maxY = size().height - 1;
        for (int y = 0; y < rectHeight; ++y)
        {
            for (int x = 0; x < rectWidth; ++x)
            {
                pixel(x + xOffset, maxY - yOffset, tempPixel);
                destImage.setPixel(x, y, tempPixel);
            }
            --maxY;
        }
    }
    else
    {
        // Use the bottom-left corner of the image as the (0,0) origin; Default Y.
        for (int y = 0; y < rectHeight; ++y)
        {
            for (int x = 0; x < rectWidth; ++x)
            {
                pixel(x + xOffset, y + yOffset, tempPixel);
                destImage.setPixel(x, y, tempPixel);
            }
        }
    }
}

void Image::setBaseSurfaceRect(const Image & srcImage, const int xOffset, const int yOffset,
                               const int rectWidth, const int rectHeight, const bool topLeft)
{
    assert(xOffset >= 0);
    assert(yOffset >= 0);

    assert(rectHeight > 0);
    assert(rectWidth  > 0);

    assert(srcImage.m_format == m_format);
    assert(isValid());

    std::uint8_t tempPixel[MaxPixelBytes];
    if (topLeft)
    {
        // Use the top-left corner of the image as the (0,0) origin; Invert Y.
        int maxY = size().height - 1;
        for (int y = 0; y < rectHeight; ++y)
        {
            for (int x = 0; x < rectWidth; ++x)
            {
                srcImage.pixel(x, y, tempPixel);
                setPixel(x + xOffset, maxY - yOffset, tempPixel);
            }
            --maxY;
        }
    }
    else
    {
        // Use the bottom-left corner of the image as the (0,0) origin; Default Y.
        for (int y = 0; y < rectHeight; ++y)
        {
            for (int x = 0; x < rectWidth; ++x)
            {
                srcImage.pixel(x, y, tempPixel);
                setPixel(x + xOffset, y + yOffset, tempPixel);
            }
        }
    }
}

void Image::setBaseSurfaceRect(const std::uint8_t * image, const int xOffset, const int yOffset,
                               const int rectWidth, const int rectHeight, const bool topLeft)
{
    assert(xOffset >= 0);
    assert(yOffset >= 0);

    assert(rectHeight > 0);
    assert(rectWidth  > 0);

    assert(image != nullptr);
    assert(isValid());

    const int bpp = bytesPerPixel();
    std::uint8_t tempPixel[MaxPixelBytes];

    if (topLeft)
    {
        // Use the top-left corner of the image as the (0,0) origin; Invert Y.
        int maxY = size().height - 1;
        for (int y = 0; y < rectHeight; ++y)
        {
            for (int x = 0; x < rectWidth; ++x)
            {
                std::memcpy(tempPixel, (image + (x + y * rectWidth) * bpp), bpp);
                setPixel(x + xOffset, maxY - yOffset, tempPixel);
            }
            --maxY;
        }
    }
    else
    {
        // Use the bottom-left corner of the image as the (0,0) origin; Default Y.
        for (int y = 0; y < rectHeight; ++y)
        {
            for (int x = 0; x < rectWidth; ++x)
            {
                std::memcpy(tempPixel, (image + (x + y * rectWidth) * bpp), bpp);
                setPixel(x + xOffset, y + yOffset, tempPixel);
            }
        }
    }
}

// ========================================================
// DDS file loading helpers:
// ========================================================

namespace DDS
{

#pragma pack(push, 1)

struct PixelFormat
{
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t fourCC;
    std::uint32_t RGBBitCount;
    std::uint32_t RBitMask;
    std::uint32_t GBitMask;
    std::uint32_t BBitMask;
    std::uint32_t ABitMask;
};

struct FileHeader
{
    std::uint32_t fourCC;
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitchOrLinearSize;
    std::uint32_t depth;
    std::uint32_t mipMapCount;
    std::uint32_t reserved1[11];
    PixelFormat   pixelFormat;
    std::uint32_t caps1;
    std::uint32_t caps2;
    std::uint32_t caps3;
    std::uint32_t caps4;
    std::uint32_t reserved2;
};

struct FileHeaderDX10
{
    // DDS DX10 has some extra fields...
    std::uint32_t dxgiFormat;
    std::uint32_t resourceDimension;
    std::uint32_t miscFlag;
    std::uint32_t arraySize;
    std::uint32_t reserved;
};

#pragma pack(pop)

} // namespace DDS

// ========================================================
// DXTCompressedImage:
// ========================================================

bool DXTCompressedImage::initFromFile(const char * const filePath)
{
    ScopedFileHandle fileIn = openFile(filePath, "rb");
    if (fileIn == nullptr)
    {
        Log::errorF("Failed to load DXT image from '%s' - can't open file!", filePath);
        return false;
    }

    DDS::FileHeader header = {};
    readStructFromFile(fileIn, &header);

    if (header.fourCC != ' SDD') // 'DDS '
    {
        Log::errorF("%s: Not a DDS file - invalid 4CC!", filePath);
        return false;
    }
    if (header.mipMapCount == 0)
    {
        Log::errorF("%s: DDS header mipmap count is zero!", filePath);
        return false;
    }

    bool dx10;
    Format pixelFormat;
    std::uint32_t blockSize;

    if (header.pixelFormat.fourCC == '01XD') // 'DX10'
    {
        DDS::FileHeaderDX10 dx10Header;
        readStructFromFile(fileIn, &dx10Header);

        dx10 = true;
        switch (dx10Header.dxgiFormat)
        {
        case 71 : { pixelFormat = Format::DXT1;        blockSize = 8;  break; }
        case 74 : { pixelFormat = Format::DXT3;        blockSize = 16; break; }
        case 77 : { pixelFormat = Format::DXT5;        blockSize = 16; break; }
        case 80 : { pixelFormat = Format::RGTC1_ATI1N; blockSize = 8;  break; }
        case 83 : { pixelFormat = Format::RGTC2_ATI2N; blockSize = 16; break; }
        default :
            Log::errorF("%s: DDS-DX10 Compression format (%u) not supported!", filePath, dx10Header.dxgiFormat);
            return false;
        } // switch
    }
    else
    {
        dx10 = false;
        switch (header.pixelFormat.fourCC)
        {
        case '1TXD' : { pixelFormat = Format::DXT1; blockSize = 8;  break; }
        case '3TXD' : { pixelFormat = Format::DXT3; blockSize = 16; break; }
        case '5TXD' : { pixelFormat = Format::DXT5; blockSize = 16; break; }
        default :
            Log::errorF("%s: DDS Compression format (%#x) not supported!", filePath, header.pixelFormat.fourCC);
            return false;
        } // switch
    }

    //
    // Read in all the precomputed DDS mipmap levels:
    //

    std::uint32_t w = header.width;
    std::uint32_t h = header.height;
    allocateImageStorage(w, h, header.mipMapCount, blockSize, pixelFormat);

    Log::debugF("Load DDS '%s': (w:%u, h:%u, mipmaps:%u, blockSize:%u, format:%s, dx10:%c)",
                filePath, w, h, header.mipMapCount, blockSize, Image::FormatStrings[pixelFormat], (dx10 ? 'y' : 'n'));

    for (std::uint32_t i = 0; (i < header.mipMapCount) && (i < MaxSurfaces); ++i)
    {
        // Deduce # of bytes
        // Close to the higher mipmap levels, w or h may become 0; keep things at 1
        if (w == 0) { w = 1; }
        if (h == 0) { h = 1; }
        const std::size_t numBytes = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;

        if (std::fread(m_surfaces[i].rawData, 1, numBytes, fileIn) != numBytes)
        {
            Log::errorF("%s: Failed to read %zu bytes from DDS file! Stopping...", filePath, numBytes);
            shutdown(); return false;
        }

        // Next level is half the current size.
        w /= 2;
        h /= 2;
    }

    return true;
}

void DXTCompressedImage::shutdown()
{
    m_format            = Format::None;
    m_memoryUsageBytes  = 0;
    m_dxtCompressedData = nullptr;
    m_surfaces.clear();
}

void DXTCompressedImage::allocateImageStorage(const std::uint32_t width, const std::uint32_t height,
                                              const std::uint32_t mipMapCount, const std::uint32_t blockSize,
                                              const Format pixelFormat)
{
    std::uint32_t memoryBytesNeeded = 0;
    std::uint32_t w = width;
    std::uint32_t h = height;

    // Find out the total amount of memory we need to allocate:
    for (std::uint32_t i = 0; (i < mipMapCount) && (i < MaxSurfaces); ++i)
    {
        if (w == 0) { w = 1; }
        if (h == 0) { h = 1; }
        memoryBytesNeeded += ((w + 3) / 4) * ((h + 3) / 4) * blockSize;
        w /= 2;
        h /= 2;
    }

    // Allocate storage:
    m_dxtCompressedData.reset(new std::uint8_t[memoryBytesNeeded]);
    m_memoryUsageBytes = memoryBytesNeeded;
    m_format = pixelFormat;

    // Set the surfaces to point to offsets inside the compressed data buffer:
    w = width;
    h = height;
    std::uint8_t * nextMipLevelPtr = m_dxtCompressedData.get();
    for (std::uint32_t i = 0; (i < mipMapCount) && (i < MaxSurfaces); ++i)
    {
        if (w == 0) { w = 1; }
        if (h == 0) { h = 1; }
        const std::uint32_t numBytes = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;

        Size2D surfaceDims;
        surfaceDims.width  = w;
        surfaceDims.height = h;
        m_surfaces.push({ nextMipLevelPtr, numBytes, surfaceDims });

        w /= 2;
        h /= 2;
        nextMipLevelPtr += numBytes;
    }
}

} // namespace VkToolbox
