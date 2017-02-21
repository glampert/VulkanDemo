#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/FixedSizeArray.hpp
// Author: Guilherme R. Lampert
// Created on: 21/02/17
// Brief: Simple stack-like compile-time sized array template.
// ================================================================================================

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

namespace VkToolbox
{

// Very simple array/stack-like container of fixed size.
// All elements are default constructed on initialization.
// Popping an element doesn't destroy it, it just decrements
// the array used size. Clearing the array just sets size = 0.
template
<
    typename T,
    int Capacity,
    typename SizeType = int
>
class FixedSizeArray final
{
public:

	using value_type      = T;
	using size_type       = SizeType;
	using pointer         = T *;
	using const_pointer   = const T *;
	using reference       = T &;
	using const_reference = const T &;

    FixedSizeArray() = default;
    FixedSizeArray(FixedSizeArray &&) = default;
    FixedSizeArray(const FixedSizeArray &) = default;
    FixedSizeArray & operator = (FixedSizeArray &&) = default;
    FixedSizeArray & operator = (const FixedSizeArray &) = default;

    FixedSizeArray(const_pointer first, const size_type count)
    {
        assert(count <= capacity());
        std::copy_n(first, count, data());
        m_count = count;
    }

    FixedSizeArray(const_pointer first, const_pointer last)
    {
        const auto count = narrowCast<size_type>(std::distance(first, last));
        assert(count <= capacity());
        std::copy(first, last, data());
        m_count = count;
    }

    template<int ArraySize>
    FixedSizeArray(const value_type (&arr)[ArraySize])
        : FixedSizeArray{ arr, ArraySize }
    {
    }

    void fill(const_reference val, const size_type count = Capacity)
    {
        assert(count <= capacity());
        std::fill_n(data(), count, val);
        m_count = count;
    }

    void resize(const size_type count, const_reference val)
    {
        fill(val, count);
    }

    void resize(const size_type count)
    {
        m_count = count;
    }

    void clear()
    {
        m_count = 0;
    }

    void push(const_reference val)
    {
        assert(size() < capacity());
        m_elements[m_count++] = val;
    }

    void push(value_type && val)
    {
        assert(size() < capacity());
        m_elements[m_count++] = std::forward<value_type>(val);
    }

    void pop()
    {
        assert(!empty());
        --m_count;
    }

    bool operator == (const FixedSizeArray & other) const
    {
        return std::equal(begin(), end(), other.begin(), other.end());
    }

    bool operator != (const FixedSizeArray & other) const
    {
        return !(*this == other);
    }

    const_reference operator[](const size_type index) const
    {
        assert(index >= 0 && index < size());
        return m_elements[index];
    }

    reference operator[](const size_type index)
    {
        assert(index >= 0 && index < size());
        return m_elements[index];
    }

    const_reference front() const { assert(!empty()); return m_elements[0]; }
    reference       front()       { assert(!empty()); return m_elements[0]; }

    const_reference back()  const { assert(!empty()); return m_elements[size() - 1]; }
    reference       back()        { assert(!empty()); return m_elements[size() - 1]; }

    size_type       size()  const { return m_count; }
    bool            empty() const { return size() == 0; }

    const_pointer   data()  const { return &m_elements[0]; }
    pointer         data()        { return &m_elements[0]; }

    const_pointer   begin() const { return &m_elements[0]; }
    pointer         begin()       { return &m_elements[0]; }

    const_pointer   end()   const { return &m_elements[size()]; }
    pointer         end()         { return &m_elements[size()]; }

    static constexpr size_type capacity() { return Capacity; }

private:

    static_assert(Capacity > 0, "Cannot allocate FixedSizeArray of zero capacity!");

    size_type  m_count = 0;
    value_type m_elements[Capacity];
};

} // namespace VkToolbox
