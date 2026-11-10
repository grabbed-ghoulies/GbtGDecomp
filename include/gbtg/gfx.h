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
    // The frame's shape when the wide_aspect plugin makes it wider than 16:9 (camera/wide_aspect.h); 0 keeps
    // widescreen's. Retail builds leave it 0.
    float frame_aspect = 0.f;
    bool fullscreen = false; // The window covers its display: borderless on Windows, SDL fullscreen elsewhere.
    // GBTG_OPTIMIZE builds: run the backend on a render thread (gfx_render_thread.h). Retail builds ignore it.
    bool render_thread = true;
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
// Compiles the specialized pixel shader for a 60-word D3DPIXELSHADERDEF ahead of its first draw. False when the
// definition runs on the generic combiner shader instead, or without a device.
bool gfx_prepare_native_pixel_definition(const u32* definition);
// The vertex programs and pixel definitions compiled so far, each once.
void gfx_compiled_shader_permutations(std::vector<std::vector<u32>>& vertex_programs,
                                      std::vector<std::array<u32, 60>>& pixel_definitions);

// The device and window a host overlay renders with (plugin overlays, docs/PLUGINS.md), as opaque handles. False for
// the null backend and runs without a window.
struct GfxOverlayDevice
{
    const char* backend = nullptr; // "d3d11" or "vulkan".
    void* window = nullptr; // HWND, or SDL_Window* off Windows.
    void* d3d11_device = nullptr;
    void* d3d11_context = nullptr;
    u32 vk_api_version = 0;
    void* vk_instance = nullptr;
    void* vk_physical_device = nullptr;
    void* vk_device = nullptr;
    u32 vk_queue_family = 0;
    void* vk_queue = nullptr;
    i32 vk_color_format = 0;
    u32 vk_image_count = 0;
};
bool gfx_overlay_device(GfxOverlayDevice& device);

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
    // Non-zero: names the contents of *vertex_constants. Draws with the same id carry the same constants, so a backend
    // may skip comparing them (GBTG_OPTIMIZE step 21). Zero says nothing.
    u64 vertex_constants_id = 0;
    std::span<const u32> indices; // Optional triangle-list indices, in original draw order.
    // Non-zero: program_inputs and indices are the presenter's retained geometry with this id, unchanged for as
    // long as the id is, so a backend may keep their upload. Each rebuild gets a new id (GBTG_OPTIMIZE step 7).
    u64 retained_geometry = 0;
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
// GBTG_OPTIMIZE step 12: ends the native target by copying it on the GPU into the texture native draws sample for
// `image`, which then stands at image.serial, and starts reading it back without waiting. False, with the target
// still bound, when the backend has no such path or the sizes differ: gfx_end_native_target ends it instead.
bool gfx_end_native_target_into(const RgbaImage& image);
// The BGRA pixels of the target gfx_end_native_target_into last ended. Waits only for the GPU work not yet done.
void gfx_read_ended_native_target(RgbaImage& output);
// D3DDevice_SetRenderTarget onto a texture surface with the device's own depth surface: native draws and clears go
// to a colour surface shaped like the back buffer, depth-testing and stencilling against the back buffer's
// depth-stencil under the same fitted viewport. Ending reads the frame's 640x480 area back as BGRA at the window's
// resolution; the caller scales it to the texture's width x height for the texture's memory.
// Each `key` (the texture's address) keeps its own surface, which holds what was last drawn into it: the texture's
// memory is not loaded back, so a user clears it before drawing (0x00119230 does, before its first region).
// With `own_depth` the surface has a depth-stencil of its own, the back buffer's size, instead of the back buffer's:
// for a target the Xbox pairs with a frame-sized depth texture nobody reads (0x00062B00's mode 1 light pass).
void gfx_begin_native_scene_target(u32 key, int width, int height, bool own_depth = false);
void gfx_end_native_scene_target(RgbaImage& output);
// The open texture target's depth-stencil as the Xbox D24S8 words its memory holds (depth << 8 | stencil),
// width x height in rows. For a scene target opened with own_depth, its depth over the fitted 640x480 area at the
// window's resolution, as gfx_end_native_scene_target reads the colour. Empty (0 x 0) without a device.
void gfx_read_native_target_depth(std::vector<u32>& output, int& width, int& height);

void gfx_size(int& width, int& height);
// D3DDevice_SetGammaRamp: the ramp the presented frames go through, 256 bytes per channel. The presenters draw
// without one, so a frame presented under any ramp but the identity raises.
void gfx_set_gamma_ramp(const u8* red, const u8* green, const u8* blue);
void gfx_end_frame();
// Presents the last finished frame again with no logic tick, so the host window can refresh faster than the
// retail 30 Hz swap. Nothing happens before the first frame or without a window.
void gfx_present_again();

// Last presented target as BGRA. False when the backend has no target.
bool gfx_readback(RgbaImage& out);
// CopyRects source for UI pages: flush queued quads and copy the fitted
// 4:3 game viewport, excluding host letterboxing.
bool gfx_readback_ui(RgbaImage& out);
// GBTG_OPTIMIZE step 25: gfx_readback_ui's picture kept on the GPU. gfx_ui_frame_size is its size (0 x 0 without a
// device). gfx_copy_ui_into flushes the queued quads as gfx_readback_ui does and copies the picture into the texture
// native draws sample for `image`, which then stands at image.serial; nothing is read back. False when the backend
// has no such path: gfx_readback_ui reads the picture instead.
void gfx_ui_frame_size(int& width, int& height);
bool gfx_copy_ui_into(const RgbaImage& image);
// GBTG_OPTIMIZE step 27: gfx_end_native_scene_target's picture kept on the GPU. gfx_scene_target_size is its size (0 x
// 0 without a device). gfx_end_native_scene_target_into ends the scene target by copying the picture into the texture
// native draws sample for `image`, which then stands at image.serial; nothing is read back. False, with the target
// still bound, when the backend has no such path: gfx_end_native_scene_target ends it instead.
void gfx_scene_target_size(int& width, int& height);
bool gfx_end_native_scene_target_into(const RgbaImage& image);

} // namespace gbtg
