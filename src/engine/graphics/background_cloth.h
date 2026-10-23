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
namespace background_cloth_field
{
constexpr RetailField<u32, 0x00> points{};
constexpr RetailField<u32, 0x04> points_address{};
constexpr RetailField<u32, 0x08> edges{};
constexpr RetailField<u32, 0x0C> edge_capacity{};
constexpr RetailField<u32, 0x10> free_edges{};
constexpr RetailField<u32, 0x14> edges_address{};
constexpr RetailField<u8, 0x18> sorted{};
constexpr RetailField<u8, 0x19> squared{};
constexpr RetailField<float, 0x1C> damping{};
constexpr RetailField<u32, 0x20> points_address_copy{};
constexpr RetailField<u32, 0x24> edges_address_copy{};
constexpr RetailField<u32, 0x28> normals_address{}; // One Vec3 per point (the quads' shading normals).
constexpr RetailField<u32, 0x2C> colors_address{}; // One D3DCOLOR per point.
constexpr RetailField<u16, 0x30> rows{};
constexpr RetailField<u16, 0x32> columns{};
constexpr RetailField<float, 0x34> row_step{};
constexpr RetailField<float, 0x38> column_step{};
constexpr RetailField<Vec3, 0x3C> light{}; // (1, 0, 0) unless the part faces its room (0x00050880's kinds)
constexpr RetailField<u32, 0x40> unk_40{};
constexpr RetailField<u32, 0x44> unk_44{};
constexpr RetailField<Vec3, 0x48> low{}; // Bounds of every first (or third) point, 0x0004C0E0.
constexpr RetailField<Vec3, 0x54> high{};
constexpr RetailField<Vec3, 0x60> view_low{}; // 0x00114090: the bounds' centre less 60.
constexpr RetailField<Vec3, 0x6C> view_high{}; // ...plus 60.
constexpr RetailField<Vec3, 0x78> center{};
constexpr RetailField<Vec3, 0x84> normal{};
constexpr RetailField<float, 0x90> radius{};
constexpr RetailField<u32, 0x94> constrain_hook{}; // Called after each relaxation while +0xA4 is set.
constexpr RetailField<u32, 0x98> asleep_hook{};
constexpr RetailField<u32, 0x9C> awake_hook{};
constexpr RetailField<u32, 0xA0> before_hook{};
constexpr RetailField<u32, 0xA4> constrain_enabled{};
constexpr RetailField<u32, 0xA8> texture{}; // The part table's texture for the row's +0x32 index, or 0.
constexpr RetailField<u32, 0xAC> shadow_texture{};
constexpr RetailField<u32, 0xB0> shadow_surface{};
constexpr RetailField<std::array<float, 16>, 0xB4> shadow_view{};
constexpr RetailField<std::array<float, 16>, 0xF4> shadow_projection{};
constexpr RetailField<Vec3, 0x134> shadow_eye{};
constexpr RetailField<Vec3, 0x140> shadow_at{};
constexpr RetailField<float, 0x14C> shadow_radius{};
constexpr RetailField<float, 0x154> sound_wait{};
constexpr RetailField<u16, 0x158> sound_voice{};
constexpr RetailField<std::array<char, 0x80>, 0x15A> sound{};
constexpr RetailField<u32, 0x1DC> always_stepped{}; // Stepped even when its bounds are off screen.
constexpr RetailField<u32, 0x1E0> always_drawn{};
constexpr RetailField<u8, 0x1E4> unk_1e4{};
constexpr RetailField<u8, 0x1E5> unk_1e5{};
constexpr RetailField<u8, 0x1E6> awake{};
constexpr RetailField<u8, 0x1E7> casts_shadow{};
constexpr RetailField<u8, 0x1E9> faces_room{}; // Flips the shading's sign.
constexpr RetailField<u8, 0x1EA> rustles{}; // Plays +0x15A when it moves hard.
constexpr RetailField<u8, 0x1EB> own_shaders{}; // 0: the strip draw binds its own shaders.
constexpr RetailField<u8, 0x1EC> second_draw{}; // Drawn by 0x0005D710 instead of 0x0005D470.
constexpr RetailField<u8, 0x1ED> layer{};
constexpr RetailField<u8, 0x1EE> specular{}; // Shaded by 0x0004BA90.
constexpr u32 kHeader = 0x1F0;
} // namespace background_cloth_field
struct RuntimeBackgroundCloth
{
    std::span<u8> bytes; // The whole block.
    u32 address = 0; // Its retail address.
    RuntimePointGraph graph; // The header's descriptor over the points and edges.
    std::span<u8> colors;
    std::span<u8> normals;
    std::vector<std::array<float, 16>> planes; // +0x150's pool (stride 0x40, growth 2): planes it collides with.
    std::uintptr_t shadow_surface = 0; // +0xB0's surface handle.
    float* point(u32 index)
    {
        return reinterpret_cast<float*>(graph.points.data() + size_t(index) * 0x30);
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
    // DAT_0050E398 (the player's) and DAT_0050E3D8 (the nearest actor's): the bound, then its start (+0x20), end
    // (+0x2C) and 0.6 of its radius (+0x38), as 0x00114910 leaves them.
    std::array<u8, 0x40> player_capsule{};
    std::array<u8, 0x40> nearest_capsule{};
};
// The block for `rows` x `columns` points and `edges` edges (AX columns).
RuntimeBackgroundCloth* create_runtime_background_cloth(u16 rows, u16 columns, u16 edges,
    RuntimeBackgroundClothContext& context); // Xbox 0x0004B5F0
void release_runtime_background_cloth(RuntimeBackgroundCloth* cloth); // The block's 0x0014D360 release.
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
// The point (ESI) pushed out of the capsule from `start` (ECX) to `end` (EDI); true when it was inside.
bool push_runtime_point_from_capsule(float* point, const Vec3& start, const Vec3& end,
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
// Hooks 0x001151F0 (asleep once still) and 0x001152C0 (woken by a near actor); the others raise.
void call_runtime_background_cloth_hook(u32 hook, RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothContext& context);
// The actor on DAT_00511AFC nearest the point (the player left out when `skip_player`); true within 2500 squared.
bool find_runtime_nearest_wide_actor(const Vec3& point, const Actor*& found, float& distance, bool skip_player,
    RuntimeBackgroundClothContext& context); // Xbox 0x00114490
}
