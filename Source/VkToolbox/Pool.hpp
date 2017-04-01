#pragma once

// ================================================================================================
// File: VkToolbox/Pool.hpp
// Author: Guilherme R. Lampert
// Created on: 05/01/17
// Brief: Simple block-based memory pool.
// ================================================================================================

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace VkToolbox
{

// ========================================================
// template class Pool<T, Granularity>:
// ========================================================

//
// Pool of fixed-size memory blocks (similar to a list of arrays).
//
// This pool allocator operates as a linked list of small arrays.
// Each array is a pool of blocks with the size of 'T' template parameter.
// Template parameter 'Granularity' defines the size in objects of type 'T'
// of such arrays.
//
// allocate() will return an uninitialized memory block.
// The user is responsible for calling construct() on it to run class
// constructors if necessary, and destroy() to call class destructor
// before deallocating the block.
//
template<typename T, int Granularity>
class Pool final
{
public:

    // Empty pool; no allocation until first use.
    Pool() = default;

    // Drains the pool.
    ~Pool();

    // Not copyable.
    Pool(const Pool &) = delete;
    Pool & operator = (const Pool &) = delete;

    // Allocates a single memory block of size 'T' and
    // returns an uninitialized pointer to it.
    T * allocate();

    // Deallocates a memory block previously allocated by 'allocate()'.
    // Pointer may be null, in which case this is a no-op. NOTE: Class destructor NOT called!
    void deallocate(void * ptr);

    // Frees all blocks, reseting the pool allocator to its initial state.
    // WARNING: Calling this method will invalidate any memory block still
    // alive that was previously allocated from this pool.
    void drain();

    // Miscellaneous stats queries:
    int totalAllocs()  const;
    int totalFrees()   const;
    int objectsAlive() const;
    int blockCount()   const;

    static constexpr std::size_t poolGranularity();
    static constexpr std::size_t pooledObjectSize();

private:

    // Fill patterns for debug allocations.
    #if DEBUG
    static constexpr std::uint8_t AllocFillVal = 0xCD; // 'Clean memory' -> New allocation
    static constexpr std::uint8_t FreeFillVal  = 0xDD; // 'Dead memory'  -> Freed/deleted
    #endif // DEBUG

    union PoolObj
    {
        alignas(T) std::uint8_t userData[sizeof(T)];
        PoolObj * next;
    };

    struct PoolBlock
    {
        PoolObj objects[Granularity];
        PoolBlock * next;
    };

    PoolBlock * m_blockList      = nullptr; // List of all blocks/pools.
    PoolObj   * m_freeList       = nullptr; // List of free objects that can be recycled.
    int         m_allocCount     = 0;       // Total calls to 'allocate()'.
    int         m_objectCount    = 0;       // User objects ('T' instances) currently alive.
    int         m_poolBlockCount = 0;       // Size in blocks of the 'm_blockList'.
};

// ========================================================
// Pool inline implementation:
// ========================================================

template<typename T, int Granularity>
inline Pool<T, Granularity>::~Pool()
{
    drain();
}

template<typename T, int Granularity>
inline T * Pool<T, Granularity>::allocate()
{
    if (m_freeList == nullptr)
    {
        auto newBlock  = new PoolBlock{};
        newBlock->next = m_blockList;
        m_blockList    = newBlock;

        ++m_poolBlockCount;

        // All objects in the new pool block are appended
        // to the free list, since they are ready to be used.
        for (int i = 0; i < Granularity; ++i)
        {
            newBlock->objects[i].next = m_freeList;
            m_freeList = &newBlock->objects[i];
        }
    }

    ++m_allocCount;
    ++m_objectCount;

    // Fetch one from the free list's head:
    PoolObj * object = m_freeList;
    m_freeList = m_freeList->next;

    // Initializing the object with a known pattern
    // to help detecting memory errors.
    #if DEBUG
    std::memset(object, AllocFillVal, sizeof(PoolObj));
    #endif // DEBUG

    return reinterpret_cast<T *>(object);
}

template<typename T, int Granularity>
inline void Pool<T, Granularity>::deallocate(void * ptr)
{
    assert(m_objectCount != 0); // Can't deallocate from an empty pool!
    if (ptr == nullptr)
    {
        return;
    }

    // Fill user portion with a known pattern to help
    // detecting post-deallocation usage attempts.
    #if DEBUG
    std::memset(ptr, FreeFillVal, sizeof(PoolObj));
    #endif // DEBUG

    // Add back to free list's head. Memory not actually freed now.
    auto object  = static_cast<PoolObj *>(ptr);
    object->next = m_freeList;
    m_freeList   = object;

    --m_objectCount;
}

template<typename T, int Granularity>
inline void Pool<T, Granularity>::drain()
{
    while (m_blockList != nullptr)
    {
        PoolBlock * block = m_blockList;
        m_blockList = m_blockList->next;
        delete block;
    }

    m_blockList      = nullptr;
    m_freeList       = nullptr;
    m_allocCount     = 0;
    m_objectCount    = 0;
    m_poolBlockCount = 0;
}

template<typename T, int Granularity>
inline int Pool<T, Granularity>::totalAllocs() const
{
    return m_allocCount;
}

template<typename T, int Granularity>
inline int Pool<T, Granularity>::totalFrees() const
{
    return m_allocCount - m_objectCount;
}

template<typename T, int Granularity>
inline int Pool<T, Granularity>::objectsAlive() const
{
    return m_objectCount;
}

template<typename T, int Granularity>
inline int Pool<T, Granularity>::blockCount() const
{
    return m_poolBlockCount;
}

template<typename T, int Granularity>
constexpr std::size_t Pool<T, Granularity>::poolGranularity()
{
    return Granularity;
}

template<typename T, int Granularity>
constexpr std::size_t Pool<T, Granularity>::pooledObjectSize()
{
    return sizeof(T);
}

// ========================================================
// construct() / destroy() helpers:
// ========================================================

template<typename T>
inline T * construct(T * obj, const T & other) // Copy constructor
{
    return ::new(obj) T(other);
}

template<typename T, typename... Args>
inline T * construct(T * obj, Args&&... args) // Parameterized or default constructor
{
    return ::new(obj) T(std::forward<Args>(args)...);
}

template<typename T>
inline void destroy(T * obj)
{
    if (obj != nullptr)
    {
        obj->~T();
    }
}

} // namespace VkToolbox
