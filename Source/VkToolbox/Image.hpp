#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Image.hpp
// Author: Guilherme R. Lampert
// Created on: 21/03/17
// Brief: Image loading and mip mapping helpers.
// ================================================================================================

#include "Utils.hpp"
#include "External.hpp"
#include "FixedSizeArray.hpp"

#include <cstdint>
#include <cstdlib>

namespace VkToolbox
{

// ========================================================
// class Color32:
// ========================================================

class Color32 final
{
public:

    // Creates a white color (all 255s).
    Color32();

    // Create a color with each individual component specified.
    Color32(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Create with single fill value for R, G, B, and A.
    explicit Color32(std::uint8_t rgba);

    // Create from array of at least 4 bytes
    explicit Color32(const std::uint8_t rgba[4]);

    // Create from float vectors in the [0,1] range. No clamping performed!
    explicit Color32(const Vector4 & rgba);
    explicit Color32(const Vector3 & rgb, float a = 1.0f);

    // To/from conversion helpers:

    std::uint32_t toUInt32() const;
    void fromUInt32(std::uint32_t rgba);

    Vector4 toVector4() const;
    void fromVector4(const Vector4 & rgba);

    Vector3 toVector3() const;
    void fromVector3(const Vector3 & rgb, float a = 1.0f);

    // Element accessors:

    std::uint8_t byteR() const;
    std::uint8_t byteG() const;
    std::uint8_t byteB() const;
    std::uint8_t byteA() const;

    float floatR() const;
    float floatG() const;
    float floatB() const;
    float floatA() const;

    void setByteR(std::uint8_t r);
    void setByteG(std::uint8_t g);
    void setByteB(std::uint8_t b);
    void setByteA(std::uint8_t a);
    void setByteRGBA(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

    void setFloatR(float r);
    void setFloatG(float g);
    void setFloatB(float b);
    void setFloatA(float a);
    void setFloatRGBA(float r, float g, float b, float a);

    // Color format conversion helpers:

    // Byte in [0,255] range to float in [0,1] range.
    static float byteToFloat(std::uint8_t b);

    // Float in [0,1] range to byte in [0,255] range. 'f' not clamped!
    static std::uint8_t floatToByte(float f);

    // Pack each byte into an integer:
    // 0x00-00-00-00
    //   aa-bb-gg-rr
    static std::uint32_t packRGBA(std::uint8_t r, std::uint8_t g,
                                  std::uint8_t b, std::uint8_t a);

    // Undo the work of packRGBA.
    static void unpackRGBA(std::uint32_t rgba, std::uint8_t & r,
                           std::uint8_t & g, std::uint8_t & b, std::uint8_t & a);

private:

    // Stored as bytes to save space.
    std::uint8_t m_r;
    std::uint8_t m_g;
    std::uint8_t m_b;
    std::uint8_t m_a;
};

static_assert(sizeof(Color32) == 4, "Unexpected padding in Color32 structure!");

// ========================================================
// struct ImageSurface:
// ========================================================

struct ImageSurface final
{
    // Opaque pixel data. Data layout depends on the pixel format of
    // the parent Image. This points to the Image that owns the surface.
    std::uint8_t * rawData = nullptr;

    // Width(x) and height(y) of this surface.
    Size2D size = {0,0};

    ImageSurface() = default;
    ImageSurface(std::uint8_t * data, Size2D s)
        : rawData{ data }, size{ s }
    { }

    bool isValid() const
    {
        return (rawData != nullptr && size.width != 0 && size.height != 0);
    }

    bool isPowerOfTwo() const
    {
        return ((size.width  & (size.width  - 1)) == 0) &&
               ((size.height & (size.height - 1)) == 0);
    }

    int getPixelCount() const 
    {
        return size.width * size.height;
    }

    template<typename T> const T * getPixelDataAs() const { return reinterpret_cast<T *>(rawData); }
    template<typename T>       T * getPixelDataAs()       { return reinterpret_cast<T *>(rawData); }
};

// ========================================================
// class Image:
// ========================================================

// Image loaded from a file (with the help of STB image).
// Not directly related to a VkImage - The Texture class
// handles the GPU/VK glue instead.
class Image final
{
public:

    // The maximum number of mipmap surfaces for a single Image.
    // This is enough for current-generation graphics cards.
    // A 65536 x 65536 (2^16) image has a maximum of 16 levels.
    static constexpr int MaxSurfaces = 16;

    enum class Format
    {
        None,  // Set when Image is uninitialized. Not a valid format
        R8,    // 1 byte  per pixel, 1 channel
        RG8,   // 2 bytes per pixel, 2 channels
        RGB8,  // 3 bytes per pixel, 3 channels
        RGBA8, // 4 bytes per pixel, 4 channels

        // Number of entries in this enum - internal use.
        Count
    };

    // Printable strings for the above enum.
    static const enum_array<Format, const char *> FormatStrings;

    struct LoadOptions
    {
        bool forceRGBA             = false; // Image is converted to RGBA if not already.
        bool flipVerticallyOnLoad  = false; // Image is flipped vertically if loading succeeds.
        bool roundDownToPowerOfTwo = false; // Image is rounded DOWN to its nearest PoT size if not already.
        bool roundUpToPowerOfTwo   = false; // Image is rounded UP to its nearest PoT size if not already.
        bool generateMipmaps       = false; // Trigger mipmap surface generation after loading images from file.
    };

    // Global options for when loading Images with initFromFile().
    static LoadOptions sm_loadOptions;

    // Constructs an empty (and invalid) image.
    // All dynamic resources automatically freed by the implicit destructor.
    Image() = default;

    // Parameterized constructors simply forward to the equivalent init*() method, see below.
    Image(Size2D size, Format format);
    Image(Size2D size, Color32 fillColor);
    Image(Size2D size, Format format, const void * baseSurface);
    Image(const char * filePath, str * outOptErrorInfo = nullptr);

    // Movable.
    Image(Image && other);
    Image & operator = (Image && other);

    // Not copyable.
    Image(const Image & other) = delete;
    Image & operator = (const Image & other) = delete;

    // Miscellaneous info queries:
    bool isInitialized() const;
    bool isValidAllSurfaces() const;
    bool isValid() const;
    bool isMipmapped() const;

    int getBytesPerPixel() const;
    int getChannelCount() const;
    Format getFormat() const;
    std::size_t getMemoryUsageBytes() const;

    int getSurfaceCount() const;
    bool isPowerOfTwo(int surface = 0) const;
    const Size2D & getSize(int surface = 0) const;

    // Access the pixel data as an opaque array
    // of bytes (fist surface/level only):
    const std::uint8_t * getPixelDataBaseSurface() const;
          std::uint8_t * getPixelDataBaseSurface();

    // Surface access:
    const ImageSurface & getSurface(int surface) const;
          ImageSurface & getSurface(int surface);

    // Frees all memory and resets the object to initial states (an invalid image).
    void shutdown();

    // Allocate uninitialized memory for the image. Single surface.
    void initWithSize(Size2D size, Format format);

    // Allocate and fill with given color value (format is implicit RGBA8). Single surface.
    void initWithColorFill(Size2D size, Color32 fillColor);

    // Initialize from an externally provided memory block.
    // The Image will copy the data. Creates a single base surface.
    void initWithExternalData(Size2D size, Format format, const void * baseSurface);

    // Creates a RGBA checkerboard pattern image, size must be PoT. The number of checker
    // squares may range from 2 to 64, as long as it is a power-of-two. Single surface image.
    void initWithCheckerPattern(Size2D size, int squares);

    // Attempt to initialize the image from a file on disk.
    // Will create at least a base surface if the file succeeds to load.
    // Dimensions and pixel format are inferred from the image file.
    // Supports: TGA, JPEG, BMP, PNG and GIF (without animation).
    bool initFromFile(const char * filePath, str * outOptErrorInfo = nullptr);

    // Writes the given image surface to a file on disk. Overwrites any existing
    // file with the same name. The format of the output image file will be inferred
    // from the extension. Supports: TGA, BMP and PNG.
    bool writeToFile(const char * filePath, int surface = 0, str * outOptErrorInfo = nullptr) const;

    // Dumps all surfaces in this image to files. Used mostly for debugging the mipmapper.
    // Path/directories must already exit. Output file names will be in the format:
    // <basePathName>_<surfaceIndex>.<extension>
    bool writeAllSurfacesToFiles(const char * basePathName, const char * extension) const;

    // Get/set individual pixels for a given image surface.
    void setPixel(int pX, int pY, const std::uint8_t * pixelIn, int surface = 0);
    void getPixel(int pX, int pY, std::uint8_t * pixelOut, int surface = 0) const;

    // Swap the pixel at pX0/pY0 with the one at pX1/pY1.
    void swapPixel(int pX0, int pY0, int pX1, int pY1, int surface = 0);

    // Flips the image surface vertically in-place without copying.
    void flipVInPlace(int surface = 0);

    // Flips the image surface horizontally in-place without copying.
    void flipHInPlace(int surface = 0);

    // Set every byte of the image's surface to the given value.
    // Equivalent to C's memset() function.
    void byteFill(std::uint8_t fillWith, int surface = 0);

    // Applies the user supplied function to every pixel of the image surface.
    template<typename Func> void forEveryPixel(Func && fn, int surface = 0) const;
    template<typename Func> void forEveryPixel(Func && fn, int surface = 0);

    // Generates a set of mipmap surfaces from the base surface (surface=0).
    // This will fail if the image is invalid (e.g. has no surfaces).
    // Uses an implementation defined filter. Might allocate some memory.
    void generateMipmapSurfaces();

    // Creates a vertically flipped copy of the base surface of this image.
    void flipVCopyBaseSurface(Image & flippedImage) const;

    // Creates a horizontally flipped copy of the base surface of this image.
    void flipHCopyBaseSurface(Image & flippedImage) const;

    // Resizes this image's base surface in-place.
    void resizeBaseSurfaceInPlace(int targetWidth, int targetHeight);

    // Creates a resized copy of this image's base surface.
    void resizeCopyBaseSurface(Image & destImage, int targetWidth, int targetHeight) const;

    // Rounds this image's base surface DOWN to its nearest power-of-2 dimensions. No-op if already PoT.
    void roundDownBaseSurfaceToPowerOfTwo();

    // Rounds this image's base surface UP to its closest power-of-2 dimensions. No-op if already PoT.
    void roundUpBaseSurfaceToPowerOfTwo();

    // Discard the alpha component of an RGBA image's base surface,
    // if it has one. Format changes from RGBA to RGB.
    void discardBaseSurfaceAlphaChannel();

    // Copy a portion of this image's base surface to the destination image.
    // If 'topLeft' is true, the (0,0) origin of the image is assumed to be the top-left corner. Otherwise,
    // the origin point is the bottom-left corner. This applies to both the source and destination images.
    void copyBaseSurfaceRect(Image & destImage, int xOffset, int yOffset,
                             int rectWidth, int rectHeight, bool topLeft) const;

    // Set a portion of this image's base surface from a source image.
    // If 'topLeft' is true, the (0,0) origin of the image is assumed to be the top-left corner. Otherwise,
    // the origin point is the bottom-left corner. This applies to both the source and destination images.
    void setBaseSurfaceRect(const Image & srcImage, int xOffset, int yOffset,
                            int rectWidth, int rectHeight, bool topLeft);

    // setBaseSurfaceRect() overload that takes a raw pointer as data source.
    // Data is assumed to be of the same pixel format as this image's base surface.
    void setBaseSurfaceRect(const std::uint8_t * image, int xOffset, int yOffset,
                            int rectWidth, int rectHeight, bool topLeft);

private:

    // Arbitrary big value, enough to hold the largest pixel size (RGBA8) or more.
    // Used internally for image resizing/transforming to avoid allocating heap memory.
    static constexpr int MaxPixelBytes = 16;

    struct DeleterStdFree
    {
        // By default, stb image uses malloc/free.
        void operator()(std::uint8_t * ptr) const
        {
            std::free(ptr);
        }
    };
    using RawBuffer = std::unique_ptr<std::uint8_t[], DeleterStdFree>;

    // Pointer to the pixel data of the first surface. Usually the one loaded from file.
    RawBuffer m_rawDataBaseSurface = nullptr;

    // If this image has other surfaces (mipmaps), then they are all allocated from this chunk.
    RawBuffer m_rawDataMipmapSurfaces = nullptr;

    // Estimate bytes used to store this image (for all surfaces).
    std::uint32_t m_memoryUsageBytes = 0;

    // How to interpret the pixel data for all surfaces.
    Format m_format = Format::None;

    // Array of surfaces. Used to store image mipmap levels.
    FixedSizeArray<ImageSurface, MaxSurfaces> m_surfaces;
};

// ========================================================

inline Image::Image(const Size2D size, const Format format)
{
    initWithSize(size, format);
}

inline Image::Image(const Size2D size, const Color32 fillColor)
{
    initWithColorFill(size, fillColor);
}

inline Image::Image(const Size2D size, const Format format, const void * const baseSurface)
{
    initWithExternalData(size, format, baseSurface);
}

inline Image::Image(const char * const filePath, str * outOptErrorInfo)
{
    initFromFile(filePath, outOptErrorInfo);
}

inline Image::Image(Image && other)
    : m_rawDataBaseSurface{ std::move(other.m_rawDataBaseSurface) }
    , m_rawDataMipmapSurfaces{ std::move(other.m_rawDataMipmapSurfaces) }
    , m_memoryUsageBytes{ other.m_memoryUsageBytes }
    , m_format{ other.m_format }
    , m_surfaces{ std::move(other.m_surfaces) }
{
    other.shutdown();
}

inline Image & Image::operator = (Image && other)
{
    m_rawDataBaseSurface    = std::move(other.m_rawDataBaseSurface);
    m_rawDataMipmapSurfaces = std::move(other.m_rawDataMipmapSurfaces);
    m_memoryUsageBytes      = other.m_memoryUsageBytes;
    m_format                = other.m_format;
    m_surfaces              = std::move(other.m_surfaces);

    other.shutdown();
    return *this;
}

inline bool Image::isInitialized() const
{
    return (m_rawDataBaseSurface != nullptr);
}

inline bool Image::isValidAllSurfaces() const
{
    if (!isValid())
    {
        return false;
    }
    for (const ImageSurface & surface : m_surfaces)
    {
        if (!surface.isValid())
        {
            return false;
        }
    }
    return true;
}

inline bool Image::isValid() const
{
    return (m_rawDataBaseSurface != nullptr &&
            m_format != Format::None &&
            m_surfaces.size() != 0);
}

inline bool Image::isMipmapped() const
{
    return (m_rawDataMipmapSurfaces != nullptr);
}

inline int Image::getBytesPerPixel() const
{
    return static_cast<int>(m_format);
}

inline int Image::getChannelCount() const
{
    return static_cast<int>(m_format);
}

inline Image::Format Image::getFormat() const
{
    return m_format;
}

inline std::size_t Image::getMemoryUsageBytes() const
{
    return m_memoryUsageBytes;
}

inline int Image::getSurfaceCount() const
{
    return m_surfaces.size();
}

inline bool Image::isPowerOfTwo(const int surface) const
{
    return m_surfaces[surface].isPowerOfTwo();
}

inline const Size2D & Image::getSize(const int surface) const
{
    return m_surfaces[surface].size;
}

inline const std::uint8_t * Image::getPixelDataBaseSurface() const
{
    return m_rawDataBaseSurface.get();
}

inline std::uint8_t * Image::getPixelDataBaseSurface()
{
    return m_rawDataBaseSurface.get();
}

inline const ImageSurface & Image::getSurface(const int surface) const
{
    return m_surfaces[surface];
}

inline ImageSurface & Image::getSurface(const int surface)
{
    return m_surfaces[surface];
}

inline void Image::byteFill(const std::uint8_t fillWith, const int surface)
{
    assert(isValid());

    ImageSurface & imgSurf = getSurface(surface);
    const int pixelCount = imgSurf.getPixelCount();

    std::memset(imgSurf.rawData, fillWith, pixelCount * getBytesPerPixel());
}

template<typename Func>
inline void Image::forEveryPixel(Func && fn, const int surface) const
{
    assert(isValid());

    const ImageSurface & imgSurf       = getSurface(surface);
    const std::uint8_t * surfDataPtr   = imgSurf.rawData;
    const int            pixelCount    = imgSurf.getPixelCount();
    const int            bytesPerPixel = getBytesPerPixel();

    for (int p = 0; p < pixelCount; ++p, surfDataPtr += bytesPerPixel)
    {
        if (!fn(surfDataPtr)) { break; }
    }
}

template<typename Func>
inline void Image::forEveryPixel(Func && fn, const int surface)
{
    assert(isValid());

    ImageSurface & imgSurf       = getSurface(surface);
    std::uint8_t * surfDataPtr   = imgSurf.rawData;
    const int      pixelCount    = imgSurf.getPixelCount();
    const int      bytesPerPixel = getBytesPerPixel();

    for (int p = 0; p < pixelCount; ++p, surfDataPtr += bytesPerPixel)
    {
        if (!fn(surfDataPtr)) { break; }
    }
}

// ========================================================

inline Color32::Color32()
{
    setByteRGBA(255, 255, 255, 255);
}

inline Color32::Color32(const std::uint8_t r, const std::uint8_t g,
                        const std::uint8_t b, const std::uint8_t a)
{
    setByteRGBA(r, g, b, a);
}

inline Color32::Color32(const std::uint8_t rgba)
{
    setByteRGBA(rgba, rgba, rgba, rgba);
}

inline Color32::Color32(const std::uint8_t rgba[4])
{
    setByteRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
}

inline Color32::Color32(const Vector4 & rgba)
{
    fromVector4(rgba);
}

inline Color32::Color32(const Vector3 & rgb, const float a)
{
    fromVector3(rgb, a);
}

inline std::uint32_t Color32::toUInt32() const
{
    return packRGBA(m_r, m_g, m_b, m_a);
}

inline void Color32::fromUInt32(const std::uint32_t rgba)
{
    unpackRGBA(rgba, m_r, m_g, m_b, m_a);
}

inline Vector4 Color32::toVector4() const
{
    return Vector4{ floatR(), floatG(), floatB(), floatA() };
}

inline void Color32::fromVector4(const Vector4 & rgba)
{
    setFloatRGBA(rgba.getX(), rgba.getY(), rgba.getZ(), rgba.getW());
}

inline Vector3 Color32::toVector3() const
{
    return Vector3{ floatR(), floatG(), floatB() };
}

inline void Color32::fromVector3(const Vector3 & rgb, const float a)
{
    setFloatRGBA(rgb.getX(), rgb.getY(), rgb.getZ(), a);
}

inline std::uint8_t Color32::byteR() const
{
    return m_r;
}

inline std::uint8_t Color32::byteG() const
{
    return m_g;
}

inline std::uint8_t Color32::byteB() const
{
    return m_b;
}

inline std::uint8_t Color32::byteA() const
{
    return m_a;
}

inline float Color32::floatR() const
{
    return byteToFloat(m_r);
}

inline float Color32::floatG() const
{
    return byteToFloat(m_g);
}

inline float Color32::floatB() const
{
    return byteToFloat(m_b);
}

inline float Color32::floatA() const
{
    return byteToFloat(m_a);
}

inline void Color32::setByteR(const std::uint8_t r)
{
    m_r = r;
}

inline void Color32::setByteG(const std::uint8_t g)
{
    m_g = g;
}

inline void Color32::setByteB(const std::uint8_t b)
{
    m_b = b;
}

inline void Color32::setByteA(const std::uint8_t a)
{
    m_a = a;
}

inline void Color32::setByteRGBA(const std::uint8_t r, const std::uint8_t g,
                                 const std::uint8_t b, const std::uint8_t a)
{
    m_r = r;
    m_g = g;
    m_b = b;
    m_a = a;
}

inline void Color32::setFloatR(const float r)
{
    m_r = floatToByte(r);
}

inline void Color32::setFloatG(const float g)
{
    m_g = floatToByte(g);
}

inline void Color32::setFloatB(const float b)
{
    m_b = floatToByte(b);
}

inline void Color32::setFloatA(const float a)
{
    m_a = floatToByte(a);
}

inline void Color32::setFloatRGBA(const float r, const float g,
                                  const float b, const float a)
{
    m_r = floatToByte(r);
    m_g = floatToByte(g);
    m_b = floatToByte(b);
    m_a = floatToByte(a);
}

inline float Color32::byteToFloat(const std::uint8_t b)
{
    return static_cast<float>(b) * (1.0f / 255.0f);
}

inline std::uint8_t Color32::floatToByte(const float f)
{
    return static_cast<std::uint8_t>(f * 255.0f);
}

inline std::uint32_t Color32::packRGBA(const std::uint8_t r, const std::uint8_t g,
                                       const std::uint8_t b, const std::uint8_t a)
{
    return static_cast<std::uint32_t>((a << 24) | (b << 16) | (g << 8) | r);
}

inline void Color32::unpackRGBA(const std::uint32_t rgba, std::uint8_t & r,
                                std::uint8_t & g, std::uint8_t & b, std::uint8_t & a)
{
    r = static_cast<std::uint8_t>((rgba & 0x000000FF) >> 0);
    g = static_cast<std::uint8_t>((rgba & 0x0000FF00) >> 8);
    b = static_cast<std::uint8_t>((rgba & 0x00FF0000) >> 16);
    a = static_cast<std::uint8_t>((rgba & 0xFF000000) >> 24);
}

// ========================================================

} // namespace VkToolbox
