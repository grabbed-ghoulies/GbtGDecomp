#pragma once

#include "gbtg/types.h"

#include <array>
#include <functional>
#include <span>

namespace gbtg
{
struct GhoulySpawnPool;
struct NativeMovieRuntime;

// DAT_00504960: the 0x18-byte depth probes 0x00066080 samples against the depth surface. Boot 0x00033030 makes it
// (four records, grown by four); nothing in retail adds a record, so its cursor stays at its start.
extern GhoulySpawnPool* g_depth_probe_pool;
// DAT_0043427C: called with 0 before D3DDevice_Swap and 1 after (0x00154A40). BSS, and nothing in retail sets it.
extern void (*g_swap_notify)(i32 phase);

// Boot 0x00033030's inlined 0x00066260 after the boot shaders: the depth probe pool, stride 0x18, four records.
void boot_depth_probe_pool();

// What the frame's present reaches outside itself; the scene's actor services bind it.
struct FramePresentServices
{
    bool post_model; // DAT_00512AF0, the first post model (a sky) is loaded.
    const i32& disable_framebuffer_clear; // DAT_004DEA50, a background asked to keep the colour target.
    const i32& transition_phase; // DAT_0054632C
    const std::array<u16, 2>& screen; // DAT_005096E8 / DAT_005096EA
    NativeMovieRuntime& movies; // DAT_0050E0B8, DAT_004E5CD8, DAT_004E5CDC and the movie event slots.
    std::function<void(std::span<const u32, 6>)> viewport; // D3DDevice_SetViewport
    std::function<void(u32, const void*, u32, u32, float, u32)> clear; // D3DDevice_Clear
    std::function<void()> reset_render_state; // 0x0005E110
    std::function<void()> collect_model_releases; // 0x000F9780
    std::function<void()> flush_resource_unloads; // 0x00036180
    std::function<void()> copy_movie_frames; // 0x00106CD0
    std::function<void()> present_views; // 0x00033A20
    // Host presentation: the recording device's commands since the last hand-over go to the presenter.
    std::function<void()> present_recorded;
};

// The frame's present at the end of 0x00033B50: the clear, the camera views, the HUD viewport and the swap.
void present_frame(FramePresentServices& services); // Xbox 0x00033A90
void clear_present_target(u32 flags, FramePresentServices& services); // Xbox 0x00154A20
void sample_depth_probes(); // Xbox 0x00066080
void swap_present_buffers(); // Xbox 0x00154A40
}
