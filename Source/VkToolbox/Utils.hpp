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

// Test if types T and U are both signed.
template<typename T, typename U>
struct is_same_signedness : public std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value>
{
};

// narrow_cast() : A checked version of static_cast() that asserts if the cast changed the value.
// This is based on the "Guidelines Support Library (GSL)".
template<typename T, typename U>
T narrow_cast(const U u)
{
    const T t = static_cast<T>(u);
    #if DEBUG
    if (static_cast<U>(t) != u)
    {
        assert(false && "Integer value was truncated!");
    }
    if (!is_same_signedness<T, U>::value && ((t < T{}) != (u < U{})))
    {
        assert(false && "Integer value sign overflow!");
    }
    #endif // DEBUG
    return t;
}

} // namespace VkToolbox
