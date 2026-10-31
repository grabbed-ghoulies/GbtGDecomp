#pragma once

#include "gbtg/model.h"

#include <filesystem>
#include <functional>

namespace gbtg
{

// Calls the reconstructed game makes at present time (frame_tick → 0x00033480 / 0x00032AB0 / 0x00033A90).
// Xbox D3D8 would implement these directly. PC backends are compiled in at build time
// (GBTG_GFX_D3D11 / GBTG_GFX_VULKAN, plus null) and chosen with gfx_select_backend; they must
// not leak backend types through this header.

struct GfxConfig
{
    int width = 960;
    int height = 540;
    int vsync = 1;
    bool visible = true; // false: offscreen target (headless / screenshot)
    bool widescreen = false; // The Xbox frame is shown at 16:9 (anamorphic widescreen), else 4:3.
    const char* title = "Grabbed by the Ghoulies";
};

// "d3d11", "vulkan" or "null". Every later gfx_* call goes to that backend. Without a call, or for a name not
// compiled in (false, logged), the first compiled backend is used. Call before gfx_init.
bool gfx_select_backend(const char* name);
bool gfx_init(const GfxConfig& cfg);
void gfx_shutdown();
const char* gfx_backend_name();
bool gfx_has_window();
void gfx_show_startup_loading_screen(const RgbaImage& text, float progress);
void gfx_warmup_shader_permutations(const std::filesystem::path& path,
                                    const std::function<void(float)>& progress = {});
void gfx_capture_shader_permutations(const std::filesystem::path& path);

// PC window messages. Sets quit on close / Escape. No-op without a window.
void gfx_pump(bool& quit);

void gfx_begin_frame();
// Xbox D3DDevice_Clear flags 3: depth 1, stencil 0, in a 640x480 UI rectangle.
void gfx_clear_ui_depth(int left, int top, int right, int bottom);

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

// Xbox D3D draws a native scene recorded, after its vertex program has run. Positions are clip
// coordinates of the 640x480 back buffer; render states keep their NV2A values.
struct GfxNativeVertex
{
    std::array<float, 4> position;
    std::array<float, 4> diffuse;
    std::array<float, 4> specular;
    std::array<std::array<float, 2>, 4> uv;
    float fog = 1.f; // oFog.x: 1 leaves the colour, 0 is all fog colour.
    // Each stage's oT z and w: the dot-product stages (DOTPRODUCT, DOT_ZW) dot the whole coordinate.
    std::array<std::array<float, 2>, 4> uv_zw{{{0.f, 1.f}, {0.f, 1.f}, {0.f, 1.f}, {0.f, 1.f}}};
};
using GfxNativeInputs = std::array<std::array<float, 4>, 16>;
bool gfx_native_vertex_program_supported(std::span<const u32> program);
void gfx_validate_native_boundary();
struct GfxNativeDraw
{
    std::span<const GfxNativeVertex> vertices; // Triangle list, or unique vertices when indices are supplied.
    std::span<const GfxNativeInputs> program_inputs;
    std::span<const u32> vertex_program;
    const std::array<std::array<float, 4>, 192>* vertex_constants = nullptr;
    std::span<const u32> indices; // Optional triangle-list indices, in original draw order.
    std::array<const RgbaImage*, 4> textures{};
    std::array<u32, 4> address_u{1, 1, 1, 1}; // D3DTADDRESS: 1 wrap, 2 mirror, 3 clamp, 4 border, 5 clamp to edge.
    std::array<u32, 4> address_v{1, 1, 1, 1};
    const u32* pixel_shader = nullptr; // 60-word D3DPIXELSHADERDEF; null modulates texture 0 by diffuse.
    u32 cull = 0; // 0 none, 0x900 clockwise, 0x901 counter-clockwise (screen space).
    bool lines = false; // The vertices are a line list (Xbox line list, loop or strip), drawn one pixel wide.
    bool depth_test = false;
    bool depth_write = false;
    u32 depth_function = 0x203; // 0x200 never .. 0x207 always.
    // D3DRS_ZBias (0x00171660): a nonzero bias offsets depth by -z_bias units and -0.25 z_bias times the slope.
    i32 z_bias = 0;
    bool blend = false;
    u32 source_blend = 1;
    u32 destination_blend = 0;
    u32 blend_equation = 0x8006;
    bool alpha_test = false;
    u32 alpha_function = 0x207;
    u32 alpha_reference = 0;
    u32 color_mask = 0x01010101; // NV097_SET_COLOR_MASK: alpha, red, green, blue bytes.
    // NV097 stencil test: function 0x200 never .. 0x207 always; ops 0x1E00 keep, 0 zero, 0x1E01 replace, 0x1E02
    // incr-sat, 0x1E03 decr-sat, 0x150A invert, 0x8507 incr, 0x8508 decr.
    bool stencil_test = false;
    u32 stencil_function = 0x207;
    u32 stencil_reference = 0;
    u32 stencil_read_mask = 0xFF;
    u32 stencil_write_mask = 0xFF;
    u32 stencil_fail = 0x1E00;
    u32 stencil_depth_fail = 0x1E00;
    u32 stencil_pass = 0x1E00;
    std::array<i32, 4> scissor{0, 0, 640, 480}; // Viewport rectangle, back-buffer pixels.
    // D3DRS_FOGENABLE and D3DRS_FOGCOLOR (D3DCOLOR). The combiners' FOG register is the fog colour with the
    // vertex fog factor in alpha; a definition without a final combiner gets D3D's fog blend.
    bool fog = false;
    u32 fog_color = 0;
};
void gfx_draw_native(const GfxNativeDraw& draw);
void gfx_begin_native_batch();
void gfx_end_native_batch();
// D3DDevice_Clear over the 640x480 back buffer: flags 1 depth, 2 stencil, 0xF0 colour.
void gfx_clear_native(std::span<const std::array<i32, 4>> rectangles, u32 flags, u32 color, float depth);
// D3DDevice_SetRenderTarget onto a texture surface: native draws and clears go to an offscreen width x height
// target, in the same Xbox pixel space as the back buffer. Ending reads the target back as BGRA and restores the
// back buffer. Without `depth` (the Xbox target's depth surface is null) the target has no depth buffer: depth and
// stencil neither test nor write.
void gfx_begin_native_target(int width, int height, bool depth);
void gfx_end_native_target(RgbaImage& output);
// The open texture target's depth-stencil as the Xbox D24S8 words its memory holds (depth << 8 | stencil),
// width x height in rows. Empty without a device.
void gfx_read_native_target_depth(std::vector<u32>& output);

void gfx_size(int& width, int& height);
void gfx_end_frame();
// Presents the last finished frame again with no logic tick, so the host window can refresh faster than the
// retail 30 Hz swap. Nothing happens before the first frame or without a window.
void gfx_present_again();

// Last presented target as BGRA. False when the backend has no target.
bool gfx_readback(RgbaImage& out);
// CopyRects source for UI pages: flush queued quads and copy the fitted
// 4:3 game viewport, excluding host letterboxing.
bool gfx_readback_ui(RgbaImage& out);

} // namespace gbtg
