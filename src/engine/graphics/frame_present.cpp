#include "frame_present.h"

#include "../camera/runtime_display_setup.h"
#include "../entities/actor_shape_bounds.h"
#include "../entities/ghouly_spawn_record.h"
#include "../video/native_movies.h"

#include <bit>

namespace gbtg
{
GhoulySpawnPool* g_depth_probe_pool = nullptr;
void (*g_swap_notify)(i32 phase) = nullptr;

void boot_depth_probe_pool()
{
    g_depth_probe_pool = allocate_runtime_shape_pool(0x18, 4);
}

// Xbox 0x00154A20
void clear_present_target(u32 flags, FramePresentServices& services)
{
    // D3DDevice_Clear(0, NULL, flags, DAT_00434264, 1.0f, 0). DAT_00434264 is BSS and nothing writes it.
    services.clear(0, nullptr, flags, 0, 1.f, 0);
}

// Xbox 0x00066080
void sample_depth_probes()
{
    const GhoulySpawnPool& pool = *g_depth_probe_pool;
    if (pool.begin != pool.cursor)
    {
        throw Error("0x00066080 samples a depth probe: the depth surface lock and 0x00065F90 are untranslated");
    }
}

// Xbox 0x00154A40
void swap_present_buffers()
{
    if (g_swap_notify != nullptr)
    {
        g_swap_notify(0);
    }
    // D3DDevice_Swap(0): the host presents the finished back buffer in gfx_end_frame, once frame_tick returns.
    if (g_swap_notify != nullptr)
    {
        g_swap_notify(1);
    }
}

// Xbox 0x00033A90
void present_frame(FramePresentServices& services)
{
    // Z and stencil (3); the colour target too (0xF3) unless a post model or a background keeps it, and always
    // while a transition runs.
    u32 flags = 3;
    if ((!services.post_model && services.disable_framebuffer_clear == 0) || services.transition_phase != 0)
    {
        flags = 0xF3;
    }
    // DAT_0033A5B8: the whole 640x480 back buffer, z 0..1.
    const std::array<u32, 6> full{0, 0, 640, 480, std::bit_cast<u32>(0.f), std::bit_cast<u32>(1.f)};
    services.viewport(full);
    clear_present_target(flags, services);
    services.reset_render_state(); // 0x0005E110
    services.present_recorded();
    services.collect_model_releases(); // 0x000F9780
    services.flush_resource_unloads(); // 0x00036180
    tick_native_movie_list(services.movies); // 0x00107420 over DAT_0050E0B8 unless DAT_004E5CD8 / DAT_004E5CDC
    services.copy_movie_frames(); // 0x00106CD0
    services.present_views(); // 0x00033A20
    present_runtime_hud_viewport(services.screen, services.viewport, services.clear); // 0x000F6F70
    services.present_recorded();
    sample_depth_probes(); // 0x00066080
    swap_present_buffers(); // 0x00154A40
    tick_native_movie_list(services.movies);
}
}
