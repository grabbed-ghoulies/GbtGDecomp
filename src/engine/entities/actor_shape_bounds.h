#pragma once
#include "shape_transform.h"
#include "ghouly_spawn_record.h"

namespace gbtg
{
GhoulySpawnPool* allocate_runtime_shape_pool(u16 stride, u16 count); // Xbox 0x00066260
GhoulySpawnPool* grow_runtime_shape_pool(GhoulySpawnPool* pool); // Xbox 0x000662A0
u32 count_runtime_actor_bound_shapes(const Actor& actor, u32 mask); // Xbox 0x00042640
u32 runtime_actor_physical_shape_flags(const Actor& actor); // Xbox 0x00042500
long double actor_model_sweep_radius(const Actor& actor); // Xbox 0x0003D9A0, EDI actor
// The model's type-5 block centre (+4..+0xC) times +0x200 * +0x38, or zero without one.
void actor_model_bound_center(const Actor& actor, Vec3& output); // Xbox 0x0003D900, EBX actor, ECX output
// The block centre through the actor's +0x2AC matrix and its 0x0003D9A0 radius; false without a block.
bool actor_model_world_bound(const Actor& actor, Vec3& center, float& radius); // Xbox 0x0003DA00, EAX actor
long double actor_model_bound_radius(const Actor& actor); // Xbox 0x0003DB30
void transform_runtime_shape_direction(const float* matrix, const Vec3& direction,
    Vec3& output); // Xbox 0x00103BE0
void measure_runtime_shape_cylinder(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, Vec3& center, Vec3& direction, float& radius,
    float& up, float& down); // Xbox 0x00055B30
void transform_runtime_shape_cylinder(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, u8* output); // Xbox 0x00055AA0
i32 build_runtime_actor_bound(Actor& actor, u8* output, u32 mask, bool average,
    GhoulySpawnPool*& shapes); // Xbox 0x000426F0
void update_runtime_actor_bound_record(std::span<u8> record, const u8* first,
    const u8* second, const Vec3& position, u8 value); // Xbox 0x0012B160
void apply_runtime_body_actor_bounds(std::span<const u8> body, Actor& actor,
    std::span<u8> actor_bytes, const EntityBitBlock& flags, std::span<u8>& bound,
    GhoulySpawnPool*& shapes); // Xbox 0x00025C50
}
