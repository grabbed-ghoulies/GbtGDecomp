#pragma once
#include "point_graph.h"
#include "gbtg/entity.h"
#include "gbtg/retail_field.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace gbtg
{
namespace entity_detail
{
struct ListHead;
}
// A background cloth (a curtain or flag the background model's part table describes): a block of 0x1F0 header
// bytes, then the points (stride 0x30), the edges (stride 0x1C), one colour word per point and one normal per point.
// The header starts with the point graph descriptor the solver steps (0x00102500, 0x00102030); the rest is the
// cloth's grid, bounds, hooks and draw state. Hooks are retail function addresses called with the cloth.
struct BackgroundClothHeader
{
    u32 points = 0; // +0
    u32 points_address = 0; // +4
    u32 edges = 0; // +8
    u32 edge_capacity = 0; // +0xC
    u32 free_edges = 0; // +0x10
    u32 edges_address = 0; // +0x14
    u8 sorted = 0; // +0x18
    u8 squared = 0; // +0x19
    std::array<u8, 2> unk_1A{}; // +0x1A
    float damping = 0.f; // +0x1C
    u32 points_address_copy = 0; // +0x20
    u32 edges_address_copy = 0; // +0x24
    u32 normals_address = 0; // +0x28, one Vec3 per point (the quads' shading normals)
    u32 colors_address = 0; // +0x2C, one D3DCOLOR per point
    u16 rows = 0; // +0x30
    u16 columns = 0; // +0x32
    float row_step = 0.f; // +0x34
    float column_step = 0.f; // +0x38
    Vec3 light{}; // +0x3C, (1, 0, 0) unless the part faces its room (0x00050880's kinds)
    Vec3 low{}; // +0x48, bounds of every first (or third) point, 0x0004C0E0
    Vec3 high{}; // +0x54
    Vec3 view_low{}; // +0x60, 0x00114090: the bounds' centre less 60
    Vec3 view_high{}; // +0x6C, ...plus 60
    Vec3 center{}; // +0x78
    Vec3 normal{}; // +0x84
    float radius = 0.f; // +0x90
    u32 constrain_hook = 0; // +0x94, called after each relaxation while `constrain_enabled` is set
    u32 asleep_hook = 0; // +0x98
    u32 awake_hook = 0; // +0x9C
    u32 before_hook = 0; // +0xA0
    u32 constrain_enabled = 0; // +0xA4
    u32 texture = 0; // +0xA8, the part table's texture for the row's +0x32 index, or 0
    u32 shadow_texture = 0; // +0xAC
    u32 shadow_surface = 0; // +0xB0
    std::array<float, 16> shadow_view{}; // +0xB4
    std::array<float, 16> shadow_projection{}; // +0xF4
    Vec3 shadow_eye{}; // +0x134
    Vec3 shadow_at{}; // +0x140
    float shadow_radius = 0.f; // +0x14C
    std::array<u8, 4> planes_word{}; // +0x150, the host keeps RuntimeBackgroundCloth::planes
    float sound_wait = 0.f; // +0x154
    u16 sound_voice = 0; // +0x158
    std::array<char, 0x80> sound{}; // +0x15A
    std::array<u8, 2> unk_1DA{}; // +0x1DA
    u32 always_stepped = 0; // +0x1DC, stepped even when its bounds are off screen
    u32 always_drawn = 0; // +0x1E0
    u8 unk_1e4 = 0; // +0x1E4
    u8 unk_1e5 = 0; // +0x1E5
    u8 awake = 0; // +0x1E6
    u8 casts_shadow = 0; // +0x1E7
    u8 unk_1E8 = 0; // +0x1E8
    u8 faces_room = 0; // +0x1E9, flips the shading's sign
    u8 rustles = 0; // +0x1EA, plays `sound` when it moves hard
    u8 own_shaders = 0; // +0x1EB, 0: the strip draw binds its own shaders
    u8 second_draw = 0; // +0x1EC, drawn by 0x0005D710 instead of 0x0005D470
    u8 layer = 0; // +0x1ED
    u8 specular = 0; // +0x1EE, shaded by 0x0004BA90
    u8 unk_1EF = 0; // +0x1EF
};
static_assert(sizeof(BackgroundClothHeader) == 0x1F0);
static_assert(offsetof(BackgroundClothHeader, points) == offsetof(PointGraphHeader, points));
static_assert(offsetof(BackgroundClothHeader, edges) == offsetof(PointGraphHeader, edges));
static_assert(offsetof(BackgroundClothHeader, free_edges) == offsetof(PointGraphHeader, free_edges));
static_assert(offsetof(BackgroundClothHeader, sorted) == offsetof(PointGraphHeader, sorted));
static_assert(offsetof(BackgroundClothHeader, squared) == offsetof(PointGraphHeader, squared));
static_assert(offsetof(BackgroundClothHeader, damping) == offsetof(PointGraphHeader, damping));
static_assert(offsetof(BackgroundClothHeader, sound) == 0x15A);
static_assert(offsetof(BackgroundClothHeader, always_stepped) == 0x1DC);

// The block's header. Every cloth's block starts with one, whatever its points and edges add.
inline BackgroundClothHeader& cloth_header(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(BackgroundClothHeader));
    return *reinterpret_cast<BackgroundClothHeader*>(bytes.data());
}
inline const BackgroundClothHeader& cloth_header(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(BackgroundClothHeader));
    return *reinterpret_cast<const BackgroundClothHeader*>(bytes.data());
}
struct RuntimeBackgroundCloth
{
    std::span<u8> bytes; // The whole block.
    u32 address = 0; // Its retail address.
    RuntimePointGraph graph; // The header's descriptor over the points and edges.
    std::span<u8> colors;
    std::span<u8> normals;
    std::vector<std::array<float, 16>> planes; // +0x150's pool (stride 0x40, growth 2): planes it collides with.
    std::uintptr_t shadow_surface = 0; // +0xB0's surface handle.
    PointGraphPoint& point(u32 index)
    {
        return graph.point(index);
    }
};
struct RuntimeBackgroundClothContext
{
    std::function<u32(const void*, size_t)> heap_address; // A host heap block's retail address.
    std::function<const Actor*()> player; // DAT_00511B40 minus 0x93C, or null.
    std::function<const Actor*(const void* node)> wide_actor; // The actor whose +0x1E8 node this is.
    void* const& wide_first; // DAT_00511AFC.
    std::function<std::span<const u8>(u32, size_t)> resolve; // Model memory by retail address.
    // 0x000426F0 with mask -1, not averaged, over the actor's +0x8FC shape pool: start +4, axis +0x10, radius +0x1C.
    std::function<void(const Actor&, u8* output)> actor_bound;
    const Vec3& origin; // DAT_0050DD80, each water point's starting force.
    const float& scene_time; // DAT_004312CC, the time 0x0012B1F0 advances; the water ripple's phase.
    std::function<Vec3()> camera; // DAT_005077A8 +0: the display view's position, for 0x0004BA90.
    // DAT_0050E398 (the player's) and DAT_0050E3D8 (the nearest actor's): the bound, then its start (+0x20), end
    // (+0x2C) and 0.6 of its radius (+0x38), as 0x00114910 leaves them.
    std::array<u8, 0x40> player_capsule{};
    std::array<u8, 0x40> nearest_capsule{};
};
// The block for `rows` x `columns` points and `edges` edges (AX columns).
RuntimeBackgroundCloth* create_runtime_background_cloth(u16 rows, u16 columns, u16 edges,
    RuntimeBackgroundClothContext& context); // Xbox 0x0004B5F0
void release_runtime_background_cloth(RuntimeBackgroundCloth* cloth); // Xbox 0x0004B770
void measure_runtime_background_cloth(RuntimeBackgroundCloth& cloth); // Xbox 0x0004C0E0, EAX cloth
// One step: the solver's step and two relaxations, then the shading, with the hooks; asleep, only +0x98.
void step_runtime_background_cloth(RuntimeBackgroundCloth& cloth, float step,
    RuntimeBackgroundClothContext& context); // Xbox 0x0004C000, EAX cloth
// A thousand steps of 0.1 s.
void settle_runtime_background_cloth(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothContext& context); // Xbox 0x0004C0B0, EDI cloth
// The mean squared distance each point moved in its last step.
float runtime_background_cloth_motion(RuntimeBackgroundCloth& cloth); // Xbox 0x0004C1B0, EBX cloth
// Each quad's normal (scaled by 2s / (|n|^2 + s^2), s the cell area) and its grey from the light.
void shade_runtime_background_cloth(RuntimeBackgroundCloth& cloth); // Xbox 0x0004B800, EBX cloth
// 0x0004B800's normals, then each point's light from them toward the camera (DAT_005077A8): red the grey, and, three
// points in from every edge, green and blue the grey plus twice the 32nd power of the camera's reflection off the
// five-point average normal (at most 1).
void shade_runtime_background_cloth_specular(RuntimeBackgroundCloth& cloth,
    const Vec3& camera); // Xbox 0x0004BA90, ESI cloth
// Copies the quad (four points) and orders it: highest two first, the corner nearer the first third.
void order_runtime_cloth_quad(const float* quad, float* output); // Xbox 0x0004FA40, EAX quad, ESI output
int compare_runtime_cloth_corners(const void* left, const void* right); // Xbox 0x0004FA10
// A rows x columns cloth over the quad (EDI), waved two units along its normal, pinned every `pin_step` rows on its
// first column and at its last row, falling at `gravity`.
RuntimeBackgroundCloth* build_runtime_background_cloth(u16 rows, u16 columns, u16 edges, std::span<const u8> part,
    u16 pin_step, const Vec3& gravity, std::span<const u8> part_table, const float* quad,
    RuntimeBackgroundClothContext& context); // Xbox 0x0004FB30, EAX part table, EDI quad
// A hanging flag or banner (kinds 1, 2 and 8; none for kind 3): 20 x 16 points over the part's quad, pinned every
// 19 rows (kind 1) or 5 rows (kinds 2 and 8).
RuntimeBackgroundCloth* build_runtime_background_flag(std::span<const u8> part, std::span<const u8> part_table,
    RuntimeBackgroundClothContext& context); // Xbox 0x00050880, EAX part, EBX model
// Plastic strip curtains (kind 6): six 2 x 16 strips side by side across the part's quad, each a sixth of its first
// edge less a 0.05 gap at either side, pinned along the top, falling at -10.5 and rustling.
u32 build_runtime_background_strips(std::span<const u8> part, std::span<const u8> part_table,
    std::array<RuntimeBackgroundCloth*, 6>& output,
    RuntimeBackgroundClothContext& context); // Xbox 0x000509E0, the part on the stack, EBP the output
// A flat `size` x `size` sheet over the quad (EDI) at its first corner's height, every border point pinned, each point
// starting with DAT_0050DD80's force.
RuntimeBackgroundCloth* build_runtime_background_water_sheet(u16 size, u16 edges, std::span<const u8> part,
    std::span<const u8> part_table,
    RuntimeBackgroundClothContext& context); // Xbox 0x00050220, ECX size, EAX part table, EDI quad
// A water surface (kind 7): a 28 x 28 sheet over the part's quad that splashes with the water footstep.
RuntimeBackgroundCloth* build_runtime_background_water(std::span<const u8> part, std::span<const u8> part_table,
    RuntimeBackgroundClothContext& context); // Xbox 0x00050940, EAX model, ECX part
// The water's before hook: each point's vertical force pulls it back toward the first point's height, 0.2 of the
// squared offset.
void level_runtime_background_water(RuntimeBackgroundCloth& cloth); // Xbox 0x00114580
// The water's hook: each actor on DAT_00511AFC inside its bounds (30 deeper and higher) whose body spans the middle
// height pushes the inner points out of a ripple ring (4.4 + 0.4 sin(5 DAT_004312CC)) 3.4 above that height.
void ripple_runtime_background_water(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothContext& context); // Xbox 0x001145F0
// The point (ESI) pushed out of the capsule from `start` (ECX) to `end` (EDI); true when it was inside.
bool push_runtime_point_from_capsule(PointGraphPoint& point, const Vec3& start, const Vec3& end,
    float radius); // Xbox 0x001025E0
// The cloth's point (EAX) pushed out of the sphere at `center` (EDI).
void push_runtime_cloth_point_from_sphere(RuntimeBackgroundCloth& cloth, u32 index, const Vec3& center,
    float radius); // Xbox 0x00102860
// The flag hook: points pushed out of the player's capsule (within 60 of the cloth) and the nearest other actor's,
// then out of the cloth's collision planes.
void constrain_runtime_background_cloth(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothContext& context); // Xbox 0x00114910
// The part's two overlapping curtain halves (kind 5), the second a unit along the quad's normal.
void build_runtime_background_curtains(std::span<const u8> part, std::span<const u8> part_table,
    RuntimeBackgroundCloth*& first, RuntimeBackgroundCloth*& second,
    RuntimeBackgroundClothContext& context); // Xbox 0x00050CC0, EBX part
// The view box (the bounds' centre +-60) against the frustum.
bool runtime_background_cloth_in_view(RuntimeBackgroundCloth& cloth, std::span<const float, 42> frustum,
    u8& hint); // Xbox 0x00114090, ECX cloth
// Hooks 0x001151F0 (asleep once still), 0x001152C0 (woken by a near actor), 0x00114910 (the flag's pushes),
// 0x00114580 and 0x001145F0 (the water's level and ripple); the others raise.
void call_runtime_background_cloth_hook(u32 hook, RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothContext& context);
// The actor on DAT_00511AFC nearest the point (the player left out when `skip_player`); true within 2500 squared.
bool find_runtime_nearest_wide_actor(const Vec3& point, const Actor*& found, float& distance, bool skip_player,
    RuntimeBackgroundClothContext& context); // Xbox 0x00114490
}
