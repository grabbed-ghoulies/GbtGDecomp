#pragma once
#if defined(GBTG_INSPECTOR_APP)

#include "gbtg/texture.h"

namespace gbtg
{
// D3D11 host presentation helpers; only the inspector executable links these.
// Slot 0 is the scene view; slot 1 the asset viewer's model preview.
void* inspector_capture_viewport(bool read_depth, int slot = 0); // Borrowed SRV; valid until resize or shutdown.
bool inspector_depth_visible(float retail_x, float retail_y, float depth);
// The scene view's depth (D3D 0..1) under a 640x480 retail pixel, from the last slot-0 capture that read depth.
bool inspector_depth_at(float retail_x, float retail_y, float& depth);
// Clears the target and depth before the model preview records its draws.
void inspector_begin_preview(const float color[4]);
void inspector_finish_frame(); // Host UI + swap only, without retail present timers/movies.
void* inspector_texture_preview(const RgbaImage& image);
bool inspector_viewport_readback(RgbaImage& image, int slot = 0);
void inspector_release_graphics();
void* inspector_window();
bool inspector_window_minimized();
void inspector_resize_surface(int width, int height);
}
#endif
