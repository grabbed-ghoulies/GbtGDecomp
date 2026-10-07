#pragma once

#include "gbtg/model.h"

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
// Dialogue cameras use the retail 640x480 projection independently of the host window.
void gfx_set_dialog_camera(const Vec3& position, const Quat& orientation, float fov_y);
void gfx_clear_dialog_background();
// Xbox 0x000629D0 / 0x00062A90: render into the dynamic texture selected by
// 0x001417A0, then restore the story target and camera.
void gfx_begin_dialog_texture(u8 slot, int width, int height);
void gfx_end_dialog_texture();
void gfx_release_dialog_texture();
// Xbox 0x000F9250: UI orthographic mesh, with model Y inverted and Z = -50.
void gfx_draw_screen_mesh(const Mesh& mesh, float x, float y, float scale, bool clear_depth = true);
// Xbox D3DDevice_Clear flags 3: depth 1, stencil 0, in a 640x480 UI rectangle.
void gfx_clear_ui_depth(int left, int top, int right, int bottom);
// Column-major 4x4 joint matrices for the next skinned draws. count 0 clears skinning.
void gfx_set_bones(const float* column_major, int count);
// origin/scale place a cutscene actor. Zero origin and unit scale draw the mesh as stored.
void gfx_draw_mesh(const Mesh& mesh, Vec3 origin = {}, Vec3 scale = {1.f, 1.f, 1.f});
// Drop a mesh the caller is about to destroy. The draw cache is keyed by address.
void gfx_release_mesh(const Mesh& mesh);

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
    std::array<float, 4> color{1.f, 1.f, 1.f, 1.f};
};

void gfx_draw_sprite(const RgbaImage& image, const GfxRect& rect);
void gfx_release_sprite(const RgbaImage& image);

// HUD quads, in pixels of the 640x480 viewport 0x000F6F70 restores.
// 0x0005E380 pushes a quad; present_draw_frame draws the list before the swap.
void gfx_sprite_clear();
void gfx_sprite_submit(const RgbaImage& image, float x, float y, float w, float h, float u0 = 0.f, float v0 = 0.f,
                       float u1 = 1.f, float v1 = 1.f,
                       std::array<float, 4> color = {1.f, 1.f, 1.f, 1.f});

void gfx_size(int& width, int& height);
void gfx_end_frame();

// Last presented target as BGRA. False when the backend has no target.
bool gfx_readback(RgbaImage& out);
// CopyRects source for UI pages: flush queued quads and copy the fitted
// 4:3 game viewport, excluding host letterboxing.
bool gfx_readback_ui(RgbaImage& out);

} // namespace gbtg
