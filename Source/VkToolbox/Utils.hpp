#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Utils.hpp
// Author: Guilherme R. Lampert
// Created on: 07/01/17
// Brief: Miscellaneous helper code.
// ================================================================================================

#include <cassert>
#include <cstring>
#include <utility>
#include <memory>
#include <type_traits>

class str;

namespace VkToolbox
{

// ========================================================

// Test if types T and U are both signed.
template<typename T, typename U>
using IsSameSignedness = typename std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value>::type;

// narrowCast(): A checked version of static_cast() that asserts if the cast changed the value.
// This is based on the "Guidelines Support Library (GSL)".
template<typename T, typename U>
inline T narrowCast(const U u)
{
    const T t = static_cast<T>(u);
    #if DEBUG
    if (static_cast<U>(t) != u)
    {
        assert(false && "Integer value was truncated!");
    }
    if (!IsSameSignedness<T, U>{} && ((t < T{}) != (u < U{})))
    {
        assert(false && "Integer value sign overflow!");
    }
    #endif // DEBUG
    return t;
}

// ========================================================

// Statically infer the length in T elements of the C-style array.
template<typename T, std::size_t Size>
constexpr std::size_t arrayLength(const T (&)[Size])
{
    return Size;
}

// Clamp the input value between min and max (inclusive range).
template<typename T>
inline void clamp(T * inOutVal, const T minVal, const T maxVal)
{
    if      ((*inOutVal) < minVal) { (*inOutVal) = minVal; }
    else if ((*inOutVal) > maxVal) { (*inOutVal) = maxVal; }
}

// Test if an integer is a power of two. Always false if the number is negative.
template<typename T>
inline bool isPowerOfTwo(const T x)
{
    return (x > 0) && ((x & (x - 1)) == 0);
}

// Round down an integer to a power of two. The output is never greater than the input.
template<typename T>
inline T roundDownToPowerOfTwo(const T x)
{
    T p2;
    for (p2 = 1; (p2 * 2) <= x; p2 <<= 1) { }
    return p2;
}

// Rounds an integer to its next power of two. Example: 37 => 64.
template<typename T>
inline T roundUpToPowerOfTwo(T x)
{
    if (x == 0)
    {
        return 1;
    }
    --x;
    for (T i = 1; i < sizeof(T) * 8; i <<= 1)
    {
        x = x | x >> i;
    }
    return ++x;
}

// ========================================================

// Adds the minimum extra needed to the size for pointer alignment.
// This size can then be used to malloc some memory and then have
// the pointer aligned with alignPtr().
template<typename T>
inline T alignSize(const T size, const T alignment)
{
    assert(isPowerOfTwo(alignment));
    return size + (alignment - 1);
}

// Aligned if the pointer is evenly divisible by the alignment value.
// Same as '(ptr % align) == 0' (The '&' trick works with PoT alignments only!).
inline bool isAlignedPtr(const void * ptr, const std::size_t alignment)
{
    assert(isPowerOfTwo(alignment));
    return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1)) == 0;
}

// Align the pointer address to a given byte boundary.
// This should normally be used in conjunction with alignSize().
template<typename T>
inline T * alignPtr(const T * ptr, const std::size_t alignment)
{
    assert(isPowerOfTwo(alignment));

    // Cast to integer and align:
    const std::uintptr_t uintPtr = reinterpret_cast<std::uintptr_t>(ptr);
    const std::uintptr_t alignedPtr = (uintPtr + (alignment - 1)) & ~(alignment - 1);

    // Re-cast to pointer, validate and return:
    T * userPtr = reinterpret_cast<T *>(alignedPtr);
    assert(isAlignedPtr(userPtr, alignment));
    return userPtr;
}

// ========================================================

// Width and height pair.
struct Size2D final
{
    int width;
    int height;
};

// Width, height and depth triplet.
struct Size3D final
{
    int width;
    int height;
    int depth;
};

// ========================================================

// Test of a C string is a prefix of another. Strings must not be null!
inline bool strStartsWith(const char * const str, const char * const prefix)
{
    assert(str != nullptr && prefix != nullptr);
    return std::strncmp(str, prefix, std::strlen(prefix)) == 0;
}

// Open and load a file into memory, appending a null byte at the end to make it a valid C-style string.
// outFileSize is mandatory and must not be null.
std::unique_ptr<char[]> loadTextFile(const char * const inFilename, std::size_t * outFileSize);

// Test if the file exists.
bool probeFile(const char * filename);

// Get the CWD.
const char * currentPath(str * inOutPathStr);

// ========================================================

} // namespace VkToolbox
