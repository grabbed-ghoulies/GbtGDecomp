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
namespace light_cone_field
{
constexpr RetailField<u32, 0x00> used{};
constexpr RetailField<u32, 0x04> visible{};
constexpr RetailField<u32, 0x08> actor{}; // the actor the beam leaves (host: its native view is beside the record)
constexpr RetailField<Vec3, 0x0C> from{}; // the marker the beam leaves
constexpr RetailField<Vec3, 0x18> spot{}; // `at` moved +0x15C radii on along the beam
constexpr RetailField<Vec3, 0x24> at{}; // where the beam ends
constexpr RetailField<Vec3, 0x30> axis{}; // from -> at, unit
constexpr RetailRecord<0x3C, 0x14, 12> rim{}; // around the spot, 30 degrees apart
constexpr RetailField<float, 0x12C> radius{}; // the rim's radius at the spot
constexpr RetailField<float, 0x130> distance{}; // squared, from the view to the spot (0x00119BE0)
constexpr RetailField<std::array<float, 4>, 0x134> plane{}; // the axis and its distance through `from`
constexpr RetailField<float, 0x144> pitch{}; // radians added to the marker's euler
constexpr RetailField<float, 0x148> yaw{}; // radians added to the marker's euler
constexpr RetailField<float, 0x14C> spread{}; // the rim's radius before the 15 degree limit
constexpr RetailField<float, 0x150> length{};
constexpr RetailField<u32, 0x154> marker{}; // the actor's marker (0x0003AE60 / 0x0003B160) the beam leaves
constexpr RetailField<float, 0x158> fade{}; // the distance over which the rim's alpha builds
constexpr RetailField<float, 0x15C> spot_offset{}; // radii from `at` to the spot
constexpr u32 kSize = 0x160;
static_assert(retail_fits(kSize, spot_offset));
} // namespace light_cone_field
// One rim point: its position, colour and the constant word the cone's vertices end with.
namespace light_cone_rim_field
{
constexpr RetailField<Vec3, 0x0> position{};
constexpr RetailField<u32, 0xC> color{};
constexpr RetailField<u32, 0x10> word{};
} // namespace light_cone_rim_field
struct RuntimeLightCone
{
    std::array<u8, light_cone_field::kSize> bytes{};
    Actor* actor = nullptr; // +8.
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
