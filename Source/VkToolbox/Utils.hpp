#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/Utils.hpp
// Author: Guilherme R. Lampert
// Created on: 07/01/17
// Brief: Miscellaneous helper code.
// ================================================================================================

#include <cassert>
#include <type_traits>

namespace VkToolbox
{

// ========================================================

// Test if types T and U are both signed.
template<typename T, typename U>
struct IsSameSignedness
    : public std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value>
{
};

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
    if (!IsSameSignedness<T, U>::value && ((t < T{}) != (u < U{})))
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

// ========================================================

// Alias to signify that the VK handle is weak (e.g. non-owning) and should not be deleted.
template<typename H>
using WeakHandle = H;

// Alias to signify that the VK handle is strong (e.g. owning) and should be deleted by the holder.
template<typename H>
using OwnedHandle = H;

// ========================================================

} // namespace VkToolbox
