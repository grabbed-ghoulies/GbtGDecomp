#include "render_profile.h"
#include <cstdlib>
#if defined(_WIN32)
#include <malloc.h>
#endif
#include <new>

namespace
{
void* allocate_aligned(size_t size, size_t alignment)
{
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, ((size + alignment - 1) / alignment) * alignment);
#endif
}
void release_aligned(void* pointer)
{
#if defined(_WIN32)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
void record_allocation()
{
    if (gbtg::render_profile::count_allocations)
    {
        ++gbtg::render_profile::frame.allocations;
    }
}
void* allocate(size_t size, size_t alignment = 0)
{
    for (;;)
    {
        void* result = alignment == 0 ? std::malloc(size == 0 ? 1 : size) : allocate_aligned(size == 0 ? 1 : size, alignment);
        if (result != nullptr)
        {
            record_allocation();
            return result;
        }
        const auto handler = std::get_new_handler();
        if (handler == nullptr)
        {
            throw std::bad_alloc();
        }
        handler();
    }
}
}
void* operator new(size_t size)
{
    return allocate(size);
}
void* operator new[](size_t size)
{
    return allocate(size);
}
void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, size_t) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer, size_t) noexcept
{
    std::free(pointer);
}
void* operator new(size_t size, std::align_val_t alignment)
{
    return allocate(size, size_t(alignment));
}
void* operator new[](size_t size, std::align_val_t alignment)
{
    return allocate(size, size_t(alignment));
}
void operator delete(void* pointer, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete(void* pointer, size_t, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete[](void* pointer, size_t, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
