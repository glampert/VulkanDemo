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

namespace VkToolbox
{

// ========================================================
// Murmur hash function; See:
// https://en.wikipedia.org/wiki/MurmurHash
// ========================================================

std::uint32_t MurmurHash32(const void * key, std::size_t len, std::uint32_t seed = 0);
std::uint64_t MurmurHash64(const void * key, std::size_t len, std::uint64_t seed = 0);

// ========================================================
// struct Hash32:
// ========================================================

struct Hash32 final
{
    std::uint32_t value;

    Hash32()
        : value{ 0 }
    { }
    explicit Hash32(const std::uint32_t h)
        : value{ h }
    { }
    explicit Hash32(const char * const s)
        : value{ MurmurHash32(s, std::strlen(s)) }
    { }
    Hash32(const char * const s, const std::size_t len)
        : value{ MurmurHash32(s, len) }
    { }

    template<std::size_t Size>
    char * toString(char (&buffer)[Size]) const
    {
        std::snprintf(buffer, Size, "0x%X", value);
        buffer[Size - 1] = '\0';
        return buffer;
    }

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
    explicit Hash64(const std::uint64_t h)
        : value{ h }
    { }
    explicit Hash64(const char * const s)
        : value{ MurmurHash64(s, std::strlen(s)) }
    { }
    Hash64(const char * const s, const std::size_t len)
        : value{ MurmurHash64(s, len) }
    { }

    template<std::size_t Size>
    char * toString(char (&buffer)[Size]) const
    {
        std::snprintf(buffer, Size, "0x%llX", value);
        buffer[Size - 1] = '\0';
        return buffer;
    }

    bool operator == (const Hash64 other) const { return this->value == other.value; }
    bool operator != (const Hash64 other) const { return this->value != other.value; }
};

} // namespace VkToolbox
