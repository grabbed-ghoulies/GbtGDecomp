#pragma once
#if defined(GBTG_INSPECTOR_APP)

#include "gbtg/texture.h"

namespace gbtg
{
// D3D11 host presentation helpers; only the inspector executable links these.
void* inspector_capture_viewport(bool read_depth); // Borrowed SRV; valid until resize or shutdown.
bool inspector_depth_visible(float retail_x, float retail_y, float depth);
void inspector_finish_frame(); // Host UI + swap only, without retail present timers/movies.
void* inspector_texture_preview(const RgbaImage& image);
bool inspector_viewport_readback(RgbaImage& image);
void inspector_release_graphics();
void* inspector_window();
bool inspector_window_minimized();
void inspector_resize_surface(int width, int height);
}
#endif
