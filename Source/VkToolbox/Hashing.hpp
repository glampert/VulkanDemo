#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Hashing.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: Hash functions / hashing utilities.
// ================================================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <type_traits>

namespace VkToolbox
{

// ========================================================
// Murmur hash function; See:
// https://en.wikipedia.org/wiki/MurmurHash
// ========================================================

std::uint32_t murmurHash32(const void * key, std::size_t len, std::uint32_t seed = 0);
std::uint64_t murmurHash64(const void * key, std::size_t len, std::uint64_t seed = 0);

// ========================================================
// struct Hash32:
// ========================================================

struct Hash32 final
{
    std::uint32_t value;

    Hash32()
        : value{ 0 }
    { }

    // Using enable_if to disambiguate 0, since it also converts to pointer
    template<typename Integer, typename = std::enable_if_t<std::is_integral<Integer>::value>>
    explicit Hash32(const Integer h)
        : value{ static_cast<std::uint32_t>(h) }
    { }

    explicit Hash32(const char * const s)
        : value{ murmurHash32(s, std::strlen(s)) }
    { }

    Hash32(const char * const s, const std::size_t len)
        : value{ murmurHash32(s, len) }
    { }

    template<std::size_t Size>
    char * toString(char (&buffer)[Size]) const
    {
        const std::size_t n = std::snprintf(buffer, Size, "0x%X", value);
        buffer[n < Size ? n : Size - 1] = '\0';
        return buffer;
    }

    bool isNull() const { return value == 0; }
    bool operator == (const Hash32 other) const { return this->value == other.value; }
    bool operator != (const Hash32 other) const { return this->value != other.value; }
};

// ========================================================
// struct Hash64:
// ========================================================

struct Hash64 final
{
    std::uint64_t value;

    Hash64()
        : value{ 0 }
    { }

    template<typename Integer, typename = std::enable_if_t<std::is_integral<Integer>::value>>
    explicit Hash64(const Integer h)
        : value{ static_cast<std::uint64_t>(h) }
    { }

    explicit Hash64(const char * const s)
        : value{ murmurHash64(s, std::strlen(s)) }
    { }

    Hash64(const char * const s, const std::size_t len)
        : value{ murmurHash64(s, len) }
    { }

    template<std::size_t Size>
    char * toString(char (&buffer)[Size]) const
    {
        const std::size_t n = std::snprintf(buffer, Size, "0x%llX", value);
        buffer[n < Size ? n : Size - 1] = '\0';
        return buffer;
    }

    bool isNull() const { return value == 0; }
    bool operator == (const Hash64 other) const { return this->value == other.value; }
    bool operator != (const Hash64 other) const { return this->value != other.value; }
};

} // namespace VkToolbox
