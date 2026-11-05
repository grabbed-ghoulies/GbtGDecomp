#pragma once
#include "background_cloth_draw.h"
#include "gbtg/entity.h"
#include "gbtg/retail_field.h"
#include <array>
#include <functional>
#include <span>

namespace gbtg
{
// The scene's light cones (DAT_0050EF20, 16 records of 0x160 bytes; DAT_00510560 counts the ones in use): a beam
// from a marker of an actor, drawn as a cone of twelve rim points with its spot projected on the background
// (0x0011AF90). The Medusa's gaze is two of them.
struct LightConeRim
{
    Vec3 position{}; // +0
    u32 color = 0; // +0xC
    u32 word = 0; // +0x10, the constant the cone's vertices end with
};
static_assert(sizeof(LightConeRim) == 0x14);
struct RuntimeLightCone
{
    u32 used = 0; // +0
    u32 visible = 0; // +4
    u32 actor_address = 0; // +8, the actor the beam leaves
    Actor* actor = nullptr; // +8's native view
    Vec3 from{}; // +0xC, the marker the beam leaves
    Vec3 spot{}; // +0x18, `at` moved `spot_offset` radii on along the beam
    Vec3 at{}; // +0x24, where the beam ends
    Vec3 axis{}; // +0x30, from -> at, unit
    std::array<LightConeRim, 12> rim{}; // +0x3C, around the spot, 30 degrees apart
    float radius = 0.f; // +0x12C, the rim's radius at the spot
    float distance = 0.f; // +0x130, squared, from the view to the spot (0x00119BE0)
    std::array<float, 4> plane{}; // +0x134, the axis and its distance through `from`
    float pitch = 0.f; // +0x144, radians added to the marker's euler
    float yaw = 0.f; // +0x148, radians added to the marker's euler
    float spread = 0.f; // +0x14C, the rim's radius before the 15 degree limit
    float length = 0.f; // +0x150
    u32 marker = 0; // +0x154, the actor's marker (0x0003AE60 / 0x0003B160) the beam leaves
    float fade = 0.f; // +0x158, the distance over which the rim's alpha builds
    float spot_offset = 0.f; // +0x15C, radii from `at` to the spot
};
struct RuntimeLightCones
{
    std::array<RuntimeLightCone, 16> records{}; // DAT_0050EF20.
    i32 count = 0; // DAT_00510560.
    std::array<i32, 16> order{}; // DAT_00510520, nearest last (0x00119BE0).
    std::span<const u8> texture{}; // DAT_00510564, the spot's texture (0x00035B60).
};
// A free record takes the actor, marker and shape; its index, or -1 when all 16 are used.
i32 add_runtime_light_cone(RuntimeLightCones& cones, Actor& actor, u32 actor_address, u32 marker, float pitch,
    float yaw, float spread, float length, float fade, float spot_offset); // Xbox 0x0011AEE0
// Every record is cleared and the count zeroed; the texture is released through `release`.
void clear_runtime_light_cones(RuntimeLightCones& cones,
    const std::function<void(std::span<const u8>)>& release); // Xbox 0x0011AEA0
// The 3x4 rotation by `angle` about `axis` (normalized first).
void build_runtime_axis_rotation(float output[12], const Vec3& axis, float angle); // Xbox 0x001047E0, ESI output
// The cone from `from` to `at`: its spot, axis, rim, colours and plane.
void build_runtime_light_cone(RuntimeLightCones& cones, i32 index, const Vec3& at, const Vec3& from,
    float spread); // Xbox 0x00119F40, ECX index, EAX at
// The order indices sorted with the drawn cones last, the farthest first.
void sort_runtime_light_cones(RuntimeLightCones& cones, const Vec3& view); // Xbox 0x00119BE0
struct RuntimeLightConeDraw
{
    RuntimeLightCones& cones;
    RuntimeBackgroundClothDraw& projector; // 0x000644F0 / 0x00064820, and the pass's D3D.
    std::function<std::span<const u8>(const char*)> acquire; // 0x00035B60.
};
// Each drawn cone is built from its actor's marker, its spot projected on the background grid and its rim drawn.
void draw_runtime_light_cones(RuntimeLightConeDraw& context); // Xbox 0x0011AF90
}
