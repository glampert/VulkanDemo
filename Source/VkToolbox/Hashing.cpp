
// ================================================================================================
// File: VkToolbox/Hashing.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Hash functions / hashing utilities.
// ================================================================================================

#include "Hashing.hpp"
#include <cassert>

namespace VkToolbox
{

//---------------------------------------------------------------------------------
// MurmurHash2 was written by Austin Appleby, and is placed in the public domain.
// The author hereby disclaims copyright to this source code. Based on code found
// on GitHub: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
//---------------------------------------------------------------------------------

std::uint32_t murmurHash32(const void * key, std::size_t len, std::uint32_t seed)
{
    assert(key != nullptr);
    assert(len <= UINT32_MAX);

    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    constexpr std::uint32_t m = 0x5BD1E995;
    constexpr std::uint32_t r = 24;

    // Initialize the hash to a 'random' value:
    std::uint32_t h = seed ^ std::uint32_t(len);

    // Mix 4 bytes at a time into the hash:
    const std::uint8_t * data = static_cast<const std::uint8_t *>(key);
    while (len >= 4)
    {
        std::uint32_t k = *reinterpret_cast<const std::uint32_t *>(data);

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len  -= 4;
    }

    // Handle the last few bytes of the input array:
    switch (len)
    {
    case 3 :
        h ^= data[2] << 16;
    case 2 :
        h ^= data[1] << 8;
    case 1 :
        h ^= data[0];
        h *= m;
    } // switch

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

std::uint64_t murmurHash64(const void * key, std::size_t len, std::uint64_t seed)
{
    assert(key != nullptr);

    constexpr std::uint64_t m = UINT64_C(0xC6A4A7935BD1E995);
    constexpr std::uint64_t r = 47;

    std::uint64_t h = seed ^ (len * m);

    const std::uint64_t * data = static_cast<const std::uint64_t *>(key);
    const std::uint64_t * end = data + (len / 8);

    while (data != end)
    {
        std::uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const std::uint8_t * data2 = reinterpret_cast<const std::uint8_t *>(data);

    switch (len & 7)
    {
    case 7:
        h ^= std::uint64_t(data2[6]) << 48;
    case 6:
        h ^= std::uint64_t(data2[5]) << 40;
    case 5:
        h ^= std::uint64_t(data2[4]) << 32;
    case 4:
        h ^= std::uint64_t(data2[3]) << 24;
    case 3:
        h ^= std::uint64_t(data2[2]) << 16;
    case 2:
        h ^= std::uint64_t(data2[1]) << 8;
    case 1:
        h ^= std::uint64_t(data2[0]);
        h *= m;
    } // switch

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

} // namespace VkToolbox
