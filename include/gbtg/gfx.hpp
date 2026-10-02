#pragma once

#include "gbtg/model.hpp"

namespace gbtg
{

// Calls the reconstructed game makes at present time (frame_tick → 0x00033480 / 0x00032AB0 / 0x00033A90).
// Xbox D3D8 would implement these directly. PC backends are selected at build time
// (GBTG_GFX_D3D11 / GBTG_GFX_NULL) and must not leak backend types through this header.

struct GfxConfig
{
    int width = 960;
    int height = 540;
    bool visible = true; // false: offscreen target (headless / screenshot)
    const char* title = "GbtGDecomp";
};

bool gfx_init(const GfxConfig& cfg);
void gfx_shutdown();
const char* gfx_backend_name();
bool gfx_has_window();

// PC window messages. Sets quit on close / Escape. No-op without a window.
void gfx_pump(bool& quit);

void gfx_begin_frame();
// `orientation` rotates local -Z (forward) and +Y (up) into the world. fov_y is radians.
void gfx_set_camera(const Vec3& position, const Quat& orientation, float fov_y);
// Column-major 4x4 joint matrices for the next skinned draws. count 0 clears skinning.
void gfx_set_bones(const float* column_major, int count);
// origin/scale place a cutscene actor. Zero origin and unit scale draw the mesh as stored.
void gfx_draw_mesh(const Mesh& mesh, Vec3 origin = {}, Vec3 scale = {1.f, 1.f, 1.f});

// Screen-space UI. x,y,w,h are 0..1 of the framebuffer, origin at the top left.
// Drawn after world meshes. Xbox D3D8 would be another implementation of this call.
struct GfxRect
{
    float x = 0.f;
    float y = 0.f;
    float w = 1.f;
    float h = 1.f;
    float u0 = 0.f;
    float v0 = 0.f;
    float u1 = 1.f;
    float v1 = 1.f;
};

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect);
void gfx_size(int& width, int& height);
void gfx_end_frame();

// Last presented target as BGRA. False when the backend has no target.
bool gfx_readback(RgbaImage& out);

} // namespace gbtg
