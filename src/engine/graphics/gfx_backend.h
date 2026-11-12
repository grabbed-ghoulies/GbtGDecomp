#pragma once

#include "gbtg/gfx.h"

#include <algorithm>

namespace gbtg
{

// Every gfx.h entry point. gfx_dispatch.cpp defines the public gbtg::gfx_* functions and forwards each one to the
// backend gfx_init selected. A backend defines the same names in its own namespace and exports them through
// GBTG_GFX_BACKEND_TABLE.
#define GBTG_GFX_FUNCTIONS(X)                \
    X(gfx_init)                              \
    X(gfx_shutdown)                          \
    X(gfx_backend_name)                      \
    X(gfx_has_window)                        \
    X(gfx_show_startup_loading_screen)       \
    X(gfx_prepare_native_pixel_definition)   \
    X(gfx_compiled_shader_permutations)          X(gfx_overlay_device)                    \
    X(gfx_pump)                              \
    X(gfx_begin_frame)                       \
    X(gfx_clear_ui_depth)                    \
    X(gfx_draw_sprite)                       \
    X(gfx_release_sprite)                    \
    X(gfx_sprite_clear)                      \
    X(gfx_sprite_submit)                     \
    X(gfx_native_vertex_program_supported)   \
    X(gfx_validate_native_boundary)          \
    X(gfx_draw_native)                       \
    X(gfx_begin_native_batch)                \
    X(gfx_end_native_batch)                  \
    X(gfx_clear_native)                      \
    X(gfx_begin_native_target)               \
    X(gfx_end_native_target)                 \
    X(gfx_end_native_target_into)            \
    X(gfx_read_ended_native_target)          \
    X(gfx_begin_native_scene_target)         \
    X(gfx_end_native_scene_target)           \
    X(gfx_read_native_target_depth)          \
    X(gfx_size)                              \
    X(gfx_end_frame)                         \
    X(gfx_present_again)                     \
    X(gfx_readback)                          \
    X(gfx_readback_ui)                       \
    X(gfx_ui_frame_size)                     \
    X(gfx_copy_ui_into)                      \
    X(gfx_scene_target_size)                 \
    X(gfx_end_native_scene_target_into)      \
    X(gfx_read_native_target_depth_into)

// gfx_readback_ui's picture in a frame_width x frame_height back buffer: the 640x480 back buffer's picture, 16:9 in
// widescreen (the Xbox's anamorphic buffer holds the whole 16:9 view, which the book's widescreen redraw 0x0005CE80
// then corrects), else 4:3, or the wide_aspect plugin's frame when frame_aspect is set; centred.
struct GfxUiFrame
{
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

inline GfxUiFrame gfx_ui_frame(int frame_width, int frame_height, bool widescreen, float frame_aspect)
{
    const int across = widescreen ? 16 : 4;
    const int down = widescreen ? 9 : 3;
    GfxUiFrame frame;
    frame.width = std::min(frame_width, frame_height * across / down);
    frame.height = std::min(frame_height, frame_width * down / across);
    if (frame_aspect > 0.f)
    {
        frame.width = std::min(frame_width, int(float(frame_height) * frame_aspect + 0.5f));
        frame.height = std::min(frame_height, int(float(frame_width) / frame_aspect + 0.5f));
    }
    frame.left = (frame_width - frame.width) / 2;
    frame.top = (frame_height - frame.height) / 2;
    return frame;
}

// gfx_end_native_scene_target's picture: the fitted area present_hud_viewport draws the 640x480 frame into, for a
// window_width x window_height window, in a surface_width x surface_height target, rounded and clamped as that
// readback does.
inline GfxUiFrame gfx_scene_target_area(int window_width, int window_height, int surface_width, int surface_height,
    bool widescreen, float frame_aspect)
{
    float frame = widescreen ? 16.f / 9.f : 640.f / 480.f;
    if (frame_aspect > 0.f)
    {
        frame = frame_aspect;
    }
    const float window = float(window_width) / float(window_height);
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    if (window > frame)
    {
        height = float(window_height);
        width = height * frame;
        x = (float(window_width) - width) * 0.5f;
    }
    else
    {
        width = float(window_width);
        height = width / frame;
        y = (float(window_height) - height) * 0.5f;
    }
    GfxUiFrame area;
    area.left = std::clamp(int(x + 0.5f), 0, surface_width - 1);
    area.top = std::clamp(int(y + 0.5f), 0, surface_height - 1);
    area.width = std::clamp(int(width + 0.5f), 1, surface_width - area.left);
    area.height = std::clamp(int(height + 0.5f), 1, surface_height - area.top);
    return area;
}

struct GfxBackend
{
    const char* name; // The --gfx / gbtg.ini Backend value.
#define GBTG_GFX_BACKEND_MEMBER(fn) decltype(&::gbtg::fn) fn;
    GBTG_GFX_FUNCTIONS(GBTG_GFX_BACKEND_MEMBER)
#undef GBTG_GFX_BACKEND_MEMBER
};

// Fills a GfxBackend from the gfx_* definitions in namespace ns.
#define GBTG_GFX_BACKEND_ENTRY(fn) &fn,
#define GBTG_GFX_BACKEND_TABLE(backend_name) GfxBackend{backend_name, GBTG_GFX_FUNCTIONS(GBTG_GFX_BACKEND_ENTRY)}

// The backends compiled into this executable. Each is defined beside its implementation.
#if defined(GBTG_GFX_D3D11)
const GfxBackend& gfx_d3d11_backend();
#endif
#if defined(GBTG_GFX_VULKAN)
const GfxBackend& gfx_vulkan_backend();
#endif
const GfxBackend& gfx_null_backend();

} // namespace gbtg
