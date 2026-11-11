#include "gfx_present_logic.h"

namespace gbtg
{
namespace
{

// Movie node on the DAT_0050E0B8 list. 0x00107420 reads it from ESI.
// +4 is the next link, +8 the clip, +0x28 the open-failed latch, +0x34 the enable.
struct PresentMovie
{
    PresentMovie* next = nullptr;
    void* clip = nullptr;
    int failed = 0;
    int enabled = 0;
};

// One of the four camera slots at DAT_004E16A0. 0x000489E0 returns 0 when the slot is null.
struct PresentCamera
{
    int marker = 0;
};

// Overlay record. 0x00106CD0 walks 0x0050DD90 through 0x0050DFB0, stride 0x88 (four records).
struct PresentOverlay
{
    void* object = nullptr; // +0
    u8 kind = 0;             // +4, indexes the callback table at 0x00545DC0
};

// Clear and movie gates. BSS, so a fresh boot leaves every pointer clear.
struct PresentFrame
{
    int post_pass = 0;    // DAT_00512AF0, first dword of the 0x2C post table
    int cover = 0;        // DAT_004DEA50, mirrors scene+0x93C
    int clear_mode = 0;   // DAT_0054632C, nonzero forces the target clear
    int movie_lock_a = 0; // DAT_004E5CD8
    int movie_lock_b = 0; // DAT_004E5CDC
    PresentMovie* movies = nullptr;
    PresentCamera* cameras[4] = {};
    PresentOverlay overlays[4] = {};
    void (*swap_notify)(int phase) = nullptr; // DAT_0043427C
};

PresentFrame present_frame;

// Xbox 0x00033A90
// Xbox clear masks: ZBUFFER is 1, STENCIL is 2, TARGET is 0xF0.
// Boot (both retain gates clear) uses 0xF3. A post pass or cover keeps the color target
// unless clear_mode is set.
u32 present_clear_flags()
{
    u32 flags = 3;
    if ((present_frame.post_pass == 0 && present_frame.cover == 0) || present_frame.clear_mode != 0)
    {
        flags = 0xf3;
    }
    return flags;
}

// Xbox 0x00033A90
// Xbox 0x00033A90
// 0x00106CD0. A null object skips the record.
void present_overlays()
{
    for (const PresentOverlay& overlay : present_frame.overlays)
    {
        if (overlay.object == nullptr)
        {
            continue;
        }
    }
}

// Xbox 0x00033A90
// 0x00033A20. Each non-null slot is bound by 0x000489E0 and drawn by 0x00011C240.
void present_cameras()
{
    for (PresentCamera* camera : present_frame.cameras)
    {
        if (camera == nullptr)
        {
            continue;
        }
    }
}

} // namespace

// Backends call these from present_draw_frame (0x00033A90).
u32 gfx_present_clear_flags()
{
    return present_clear_flags();
}

void gfx_present_overlays()
{
    present_overlays();
}

void gfx_present_cameras()
{
    present_cameras();
}

void gfx_present_swap_notify(int phase)
{
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(phase);
    }
}

} // namespace gbtg
