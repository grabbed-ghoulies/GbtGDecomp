#include "boot_render_targets.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x000620C0
void create_runtime_boot_render_targets(RuntimeBootRenderTargets& targets, const RuntimeBootRenderTargetD3D& d3d)
{
    targets.light_volume_texture = d3d.create_texture(0x100, 0x100, 1, 1, 1, 6, 3);
    targets.light_volume_surface = d3d.surface_level(targets.light_volume_texture, 0);
    targets.targets.back_buffer = d3d.back_buffer(0);
    const RuntimeSurfaceDescription back = d3d.describe(targets.targets.back_buffer);
    targets.pages[0] = create_runtime_screen_texture(u16(back.width), u16(back.height), 1, back.format, 0,
        d3d.textures); // 0x000533A0
    std::span<u8> page = targets.pages[0];
    u32 frames;
    std::memcpy(&frames, page.data() + 4, 4);
    targets.glow.page_texture = page.data() + frames + u32(page[0x12]) * 0x14;
    targets.glow.page_surface = d3d.surface_level(d3d.textures.address_of(targets.glow.page_texture), 0);
    if (targets.glow.page_surface == 0)
    {
        return;
    }
    d3d.release(targets.targets.back_buffer);
    targets.targets.depth_buffer = d3d.depth_stencil();
    if (targets.targets.depth_buffer == 0)
    {
        return;
    }
    const RuntimeSurfaceDescription depth = d3d.describe(targets.targets.depth_buffer);
    targets.targets.depth_capture_surface = d3d.create_surface(depth.width, depth.height, 0, depth.format);
    if (targets.targets.depth_capture_surface == 0)
    {
        return;
    }
    d3d.texture_header(depth.width, depth.height, 1, 0, 0x12, 0, d3d.textures.address_of(targets.glow.depth_texture),
        d3d.surface_data(targets.targets.depth_capture_surface), 0);
    const u32 bits = d3d.lock_surface(targets.targets.depth_capture_surface, 0x80);
    targets.pages[1] = create_runtime_screen_texture(u16(back.width), u16(back.height), 1, 0x12, bits,
        d3d.textures); // 0x000533A0
    d3d.release(targets.targets.depth_buffer);
    targets.targets.active = 0;
    targets.transition_guard = 0;
}
}
