#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/StringRegistry.hpp
// Author: Guilherme R. Lampert
// Created on: 05/01/17
// Brief: Simple registry for string objects. Avoids duplicating the same str instance.
// ================================================================================================

#include <unordered_set>
#include "External.hpp"
#include "Hashing.hpp"
#include "Pool.hpp"

namespace VkToolbox
{

// ========================================================

struct HashStr final
{
    std::size_t operator()(const str * const s) const
    {
        return murmurHash64(s->c_str(), s->length());
    }
};

struct CmpStrEq final
{
    bool operator()(const str * const a, const str * const b) const
    {
        return (*a) == (*b);
    }
};

// ========================================================
// class StringRegistry:
// ========================================================

class StringRegistry final
{
public:

    StringRegistry() = default;
    ~StringRegistry() { shutdown(); }

    const str * get(const char * const key)
    {
        assert(key != nullptr);
        const str_ref keyRefStr{ key };
        return get(&keyRefStr);
    }

    const str * get(const str * const key)
    {
        assert(key != nullptr);

        const auto iter = m_lookupTable.find(key);
        if (iter != std::end(m_lookupTable))
        {
            return *iter;
        }

        // Register new string:
        str * value = construct(m_pool.allocate());
        value->set(*key);

        m_lookupTable.insert(value);
        return value;
    }

    void shutdown()
    {
        // The pool will not destroy the objects for us
        for (const str * iter : m_lookupTable)
        {
            // Cast since str doesn't have a virtual dtor
            auto value = static_cast<const PooledStrType *>(iter);
            destroy(value);
        }

        m_lookupTable.clear();
        m_pool.drain();
    }

private:

    static constexpr int PoolGranularity = 512;
    using PooledStrType = str32;

    // String pool backing-store.
    Pool<PooledStrType, PoolGranularity> m_pool;

    // Set of strings for fast lookup. Each pointer is a PooledStrType object sourced from the m_pool.
    std::unordered_set<const str *, HashStr, CmpStrEq> m_lookupTable;
};

} // namespace VkToolbox
