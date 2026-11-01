#include "gfx_backend.h"
#include "gfx_present_logic.h"
#include "gbtg/gfx.h"
#include "gbtg/movie.h"

#include <vector>

namespace gbtg
{
// The gfx_* definitions below are this backend's; gfx_dispatch.cpp owns the public names.
namespace null_gfx
{

bool gfx_init(const GfxConfig&)
{
    return true;
}

void gfx_shutdown()
{
}

const char* gfx_backend_name()
{
    return "null";
}

bool gfx_has_window()
{
    return false;
}

void gfx_show_startup_loading_screen(const RgbaImage&, float)
{
}

void gfx_warmup_shader_permutations(const std::filesystem::path&, const std::function<void(float)>&)
{
}

void gfx_capture_shader_permutations(const std::filesystem::path&)
{
}

void gfx_pump(bool&)
{
}

void gfx_begin_frame()
{
}

void gfx_clear_ui_depth(int, int, int, int)
{
}

void gfx_validate_native_boundary()
{
    throw Error("native rendering validation requires D3D11");
}

bool gfx_native_vertex_program_supported(std::span<const u32>)
{
    return false;
}

void gfx_begin_native_batch()
{
}
void gfx_end_native_batch()
{
}

void gfx_draw_native(const GfxNativeDraw&)
{
}

void gfx_clear_native(std::span<const std::array<i32, 4>>, u32, u32, float)
{
}

void gfx_begin_native_target(int, int, bool)
{
}

void gfx_end_native_target(RgbaImage&)
{
}

void gfx_begin_native_scene_target(u32, int, int)
{
}

void gfx_end_native_scene_target(RgbaImage&)
{
}

void gfx_release_sprite(const RgbaImage&)
{
}

void gfx_draw_sprite(const RgbaImage&, const GfxRect&)
{
}

void gfx_size(int& width, int& height)
{
    width = 0;
    height = 0;
}

namespace
{

// Xbox 0x00033A90
// DAT_0033A5B8 is the full backbuffer: origin 0, retail 640x480, z 0..1.
void present_set_viewport()
{
}

// Xbox 0x00033A90
// 0x00154A20: D3DDevice_Clear(0, nullptr, flags, DAT_00434264, 1.0f, 0).
void present_clear(u32 flags)
{
    (void)flags;
}

// Xbox 0x00033A90
// 0x00154A40. Notify 0, D3DDevice_Swap(0), notify 1.
void present_swap()
{
    gfx_present_swap_notify(0);
    gfx_present_swap_notify(1);
}

// Quad 0x0005E380 / 0x0005E770 push. x,y is the top-left in the 640x480 HUD.
struct HudQuad
{
    const RgbaImage* image = nullptr;
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
    float u0 = 0.f;
    float v0 = 0.f;
    float u1 = 1.f;
    float v1 = 1.f;
};

u16 hud_extra = 16;
std::vector<HudQuad> hud_quads;

// DAT_00504960. Stride 0x18. 0x00065F90 writes the flag at +0x14.
struct DepthProbe
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float depth = 0.f;
    float reach = 0.f;
    float flag = 0.f;
};
static_assert(sizeof(DepthProbe) == 0x18, "DAT_00504960 stride");

std::vector<DepthProbe> depth_probes;

// Xbox 0x000F6F70
void present_hud_viewport()
{
}

// Quads submitted through 0x0005CA60, drawn in the HUD viewport.
void present_hud_sprites()
{
    present_hud_viewport();
    for (const HudQuad& quad : hud_quads)
    {
        if (quad.image == nullptr || quad.w == 0.f || quad.h == 0.f)
        {
            continue;
        }
        GfxRect rect;
        rect.x = quad.x / 640.f;
        rect.y = quad.y / 480.f;
        rect.w = quad.w / 640.f;
        rect.h = quad.h / 480.f;
        rect.u0 = quad.u0;
        rect.v0 = quad.v0;
        rect.u1 = quad.u1;
        rect.v1 = quad.v1;
        null_gfx::gfx_draw_sprite(*quad.image, rect);
    }
}

// Xbox 0x00066080
void present_sprite_span()
{
    if (depth_probes.empty())
    {
        return;
    }
    for (DepthProbe& probe : depth_probes)
    {
        probe.flag = 0.f;
    }
}

// Xbox 0x00033A90
// Xbox 0x00033A90
void present_draw_frame()
{
    const u32 flags = gfx_present_clear_flags();
    present_set_viewport();
    present_clear(flags);
    // 0x0005E110 installs the frame render states.
    // 0x000F9780 retires the deferred span at DAT_005079E0.
    // 0x00036180 submits the draw span at DAT_004DF240 and resets it.
    gfx_present_overlays();
    gfx_present_cameras();
    // 0x000F6F70 restores the HUD viewport from DAT_005096E8 / DAT_005096EA.
    present_hud_sprites();
    // 0x00066080 walks the 0x18 depth list at DAT_00504960.
    present_sprite_span();
    present_swap();
}

} // namespace

void gfx_sprite_clear()
{
    hud_quads.clear();
}

void gfx_sprite_submit(const RgbaImage& image, float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                       std::array<float, 4>)
{
    if (image.bgra.empty() || image.width <= 0 || image.height <= 0 || w == 0.f || h == 0.f)
    {
        return;
    }
    if (hud_quads.size() == hud_quads.capacity())
    {
        hud_quads.reserve(hud_quads.size() + hud_extra);
    }
    HudQuad quad;
    quad.image = &image;
    quad.x = x;
    quad.y = y;
    quad.w = w;
    quad.h = h;
    quad.u0 = u0;
    quad.v0 = v0;
    quad.u1 = u1;
    quad.v1 = v1;
    hud_quads.push_back(quad);
}

void gfx_end_frame()
{
    // Present 0x00033480, then 0x00033A90 (clear and swap). frame_tick runs 0x00032AB0, the app error screen.
    gfx_present_tick_insert();
    present_draw_frame();
}

void gfx_present_again()
{
}

bool gfx_readback(RgbaImage&)
{
    return false;
}

bool gfx_readback_ui(RgbaImage&)
{
    return false;
}

const GfxBackend& backend_table()
{
    static const GfxBackend table = GBTG_GFX_BACKEND_TABLE("null");
    return table;
}

} // namespace null_gfx

const GfxBackend& gfx_null_backend()
{
    return null_gfx::backend_table();
}

} // namespace gbtg
