#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Utils.hpp
// Author: Guilherme R. Lampert
// Created on: 07/01/17
// Brief: Miscellaneous helper code.
// ================================================================================================

#include <cassert>
#include <utility>
#include <type_traits>

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

// Alias to signify that the pointer is a weak reference to some object (e.g. non-owning) and should not be deleted.
template<typename T>
using WeakRef = T*;

// Alias to signify that the VK handle is weak (e.g. non-owning) and should not be deleted.
template<typename H>
using WeakHandle = H;

// Alias to signify that the VK handle is strong (e.g. owning) and should be deleted by the holder.
template<typename H>
using OwnedHandle = H;

// ========================================================

} // namespace VkToolbox
