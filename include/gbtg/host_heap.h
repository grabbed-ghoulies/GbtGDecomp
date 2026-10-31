#pragma once

// Host heap boundary for retail HeapAlloc / HeapSize / HeapFree.
//
// Retail heap records keep a footer at HeapSize - 4, so callers ask the host heap
// for a block's size. MSVC reports the requested size; glibc reports the usable
// size, which may be larger. Each footer is written and read through the same
// call, so either placement is consistent.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <malloc.h>

namespace gbtg
{
// HeapSize of a block from std::malloc / std::realloc.
inline std::size_t host_heap_block_size(void* block)
{
#if defined(_WIN32)
    return _msize(block);
#else
    return malloc_usable_size(block);
#endif
}

// HeapAlloc with an alignment request; release with host_aligned_free.
inline void* host_aligned_allocate(std::size_t size, std::size_t alignment)
{
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    void* block = nullptr;
    // posix_memalign needs a power of two that is at least a pointer wide.
    if (posix_memalign(&block, (std::max)(alignment, sizeof(void*)), size) != 0)
    {
        return nullptr;
    }
    return block;
#endif
}

// HeapSize of a block from host_aligned_allocate.
inline std::size_t host_aligned_block_size(void* block, std::size_t alignment)
{
#if defined(_WIN32)
    return _aligned_msize(block, alignment, 0);
#else
    (void)alignment;
    return malloc_usable_size(block);
#endif
}

inline void host_aligned_free(void* block)
{
#if defined(_WIN32)
    _aligned_free(block);
#else
    std::free(block);
#endif
}
}
