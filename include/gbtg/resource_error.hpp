#pragma once

#include "gbtg/clock.hpp"
#include <functional>
#include <span>

namespace gbtg
{

struct LockedErrorSurface
{
    int pitch;
    u8* pixels;
};

// These callbacks correspond to the original XAPI file/section and D3D calls.
// The caller supplies its host implementations, rather than simulated success.
struct ResourceErrorBoundaries
{
    std::function<bool(const char*)> delete_file;
    std::function<int(const char*)> section_handle;
    std::function<u32()> last_error;
    std::function<std::span<const u8>(int)> load_section;
    std::function<u32(int)> section_size;
    std::function<void*()> back_buffer;
    std::function<LockedErrorSurface(void*)> lock_surface;
    std::function<void(void*)> release_surface;
    std::function<u32()> insert_fence;
    std::function<void()> swap;
    std::function<void(u32)> block_on_fence;
};

struct ResourceErrorState
{
    int suppressed;          // DAT_004DD8F8
    int code;                // DAT_004DD8FC
    int message;             // DAT_0042A710
    bool pal;                // DAT_005096E0
    bool pal_override;       // DAT_005096E4
    u32 background;          // DAT_00314F4C
    int surface_locks;       // DAT_004E5E04
    void* surface;           // DAT_004E5E00
    LockedErrorSurface locked; // DAT_004E5DF8 / DAT_004E5DFC
    PerformanceCounterState counter;
    ResourceErrorBoundaries boundaries;
};

void fill_error_surface(LockedErrorSurface target, int width, int top, int left,
                          int height, u32 color); // Xbox 0x00056D80
void draw_resource_error(ResourceErrorState& state, int message); // Xbox 0x00032770
void resource_error(ResourceErrorState& state, int code); // Xbox 0x00032700

} // namespace gbtg
