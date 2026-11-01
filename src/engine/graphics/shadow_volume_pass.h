#pragma once
#include "background_cloth_draw.h"
#include "../scene/background_geometry.h"

#include <functional>
#include <span>

namespace gbtg
{
namespace entity_detail
{
struct DisplayView;
}
// The shadow volume pass's resources: the depth ramp 0x00119160 creates at boot, and the target and depth surface
// 0x00119230 holds while it draws (DAT_0050EC04..DAT_0050EC0C).
struct RuntimeShadowVolumeTargets
{
    u32 ramp = 0; // DAT_0050EC04, a 4096 x 1 A8R8G8B8 texture: texel n has B n >> 8, G n >> 4 & 0xF and R n & 0xF.
    DialogueSurfaceHandle target = 0; // DAT_0050EC08, the render target on entry.
    DialogueSurfaceHandle depth = 0; // DAT_0050EC0C, the depth surface on entry.
};
// The scene's background regions (DAT_0050EAA0, eight rows of 0x2C) and their count (DAT_0050EC00).
struct RuntimeShadowVolumes
{
    std::span<BackgroundRegion> regions;
    i32& count;
    RuntimeShadowVolumeTargets& targets;
    const i32& transition_guard; // DAT_0050484C.
};

// Boot: the depth ramp, then every region's source and the count cleared.
void create_runtime_shadow_ramp(RuntimeShadowVolumeTargets& targets, std::span<BackgroundRegion> regions, i32& count,
    const std::function<u32(u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)>& create_texture,
    const std::function<std::span<u8>(u32 texture)>& texels); // Xbox 0x00119160
// The region's eight box corners against the view's depth row (display +0x88..+0x94): the nearest and farthest
// depth behind it into +0x1C / +0x20 (99999.9 / -99999.9 at the start, the nearest back to 0 after each corner in
// front). True when a corner is behind.
u32 measure_runtime_shadow_region(BackgroundRegion& region,
    const entity_detail::DisplayView& view); // Xbox 0x0013C760, ECX +0x10, EDX +4, ESI +0x1C, EDI +0x20
// Every region's +0x28 from 0x0013C760; the result is 1 once `any` (ECX) or a previous pass set it.
u32 measure_runtime_shadow_regions(u32 any, RuntimeShadowVolumes& volumes,
    const entity_detail::DisplayView& view); // Xbox 0x00119110
// The region's volume: c0x66 {+0x1C, 1 / +0x24}, then its source's +0x14 vertex pairs (+0x30 on) pushed as
// primitive 5 in runs of at most 0x153 pairs.
void draw_runtime_shadow_region(const BackgroundRegion& region, RuntimeBackgroundClothDraw& context); // Xbox 0x00118FD0
// The regions' shadow volumes: the depth ramp through the volume shader into book pages 0 and 1 (back and front
// faces) with each region's stencil, then the pages compared over the screen where the stencil is set, in the first
// region's colour.
void draw_runtime_shadow_volumes(RuntimeShadowVolumes& volumes, RuntimeBackgroundClothDraw& context); // Xbox 0x00119230
}
