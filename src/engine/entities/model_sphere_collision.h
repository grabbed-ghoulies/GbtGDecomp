#pragma once
#include "actor_shape_bounds.h"

namespace gbtg
{
void transform_runtime_collision_sphere(std::span<u8> bound, const float* matrix); // Xbox 0x0010AAE0
void transform_runtime_collision_bound(std::span<u8> bound, const float* matrix); // Xbox 0x0010AF90
void write_runtime_sphere_contact(std::span<u8> record, std::span<const u8> shape,
    u32 shape_address, const Vec3& point, const Vec3& center, float radius,
    const float* world); // Xbox 0x000563F0
bool collide_runtime_model_sphere_segment(const EntityShapeNode& shape, u32 shape_address,
    const float* world, const float* inverse, const EntityBonePose* pose,
    const Vec3& start, Vec3& end, std::span<u8> record, const Vec3& stationary); // Xbox 0x00055CB0
bool collide_runtime_model_sphere_sweep(const EntityShapeNode& shape, u32 shape_address,
    const float* world, const float* inverse, const EntityBonePose* pose,
    const Vec3& start, Vec3& end, float radius, std::span<u8> record,
    const Vec3& stationary); // Xbox 0x00055F80
bool collect_runtime_model_sphere_contact(const EntityShapeNode& shape, u32 shape_address,
    const float* world, const float* inverse, const EntityBonePose* pose,
    const Vec3& point, float radius, GhoulySpawnPool** pool); // Xbox 0x00056580
u32 reject_runtime_model_shape_collision(); // Xbox 0x00055A90
}
