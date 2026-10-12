#pragma once
#include "model_sphere_collision.h"
#include "collision_shape_dispatch.h"

namespace gbtg
{
bool collide_runtime_posed_mesh_segment(RuntimeCollisionShapeCall& call, const ModelPoseBuffer* pose,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x00054520
bool collide_runtime_posed_mesh_sweep(RuntimeCollisionShapeCall& call, const ModelPoseBuffer* pose,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x00054850
bool collect_runtime_posed_mesh_contacts(RuntimeCollisionShapeCall& call, const ModelPoseBuffer* pose,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x00054C40
void runtime_mesh_pose_matrices(const ModelPoseBuffer* pose, i32 joint,
    const float* world, const float* inverse, float* output_world,
    float* output_inverse); // Xbox 0x000544B0
void bind_runtime_physical_mesh_methods(
    std::unordered_map<u32, std::function<u32(RuntimeCollisionShapeCall&)>>& methods,
    std::function<std::span<const u8>(u32, size_t)> resolve); // Native views of physical kinds 1/2 retail table methods.
bool collide_runtime_physical_mesh_segment(RuntimeCollisionShapeCall& call,
    std::span<const u8> shape,
    const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x00053720
bool collide_runtime_physical_mesh_sweep(RuntimeCollisionShapeCall& call,
    std::span<const u8> shape,
    const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x00053AA0
bool collect_runtime_physical_mesh_contacts(RuntimeCollisionShapeCall& call,
    std::span<const u8> shape,
    const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x00053F20
bool collect_runtime_mesh_triangle_contacts(std::span<const u8> triangles,
    u32 triangle_address, const Vec3* vertices, const float* world,
    const Vec3& point, float radius, u32 mask, u32 excluded,
    GhoulySpawnPool** pool, const Vec3& stationary); // Xbox 0x000554D0
void write_runtime_posed_mesh_contact(std::span<u8> record, std::span<const u8> triangle,
    u32 triangle_address, const Vec3* vertices, const Vec3& normal,
    const Vec3& point, float radius, const float* world,
    const float* bone); // Xbox 0x00054DE0
const u8* select_runtime_mesh_swept_triangle(std::span<const u8> triangles,
    const Vec3* vertices, const Vec3& origin, Vec3& movement, float radius,
    Vec3& normal, u32 mask, u32 excluded, float& fraction,
    const Vec3& stationary); // Xbox 0x000553D0
void runtime_mesh_query_cell_bounds(std::span<const u8> shape, const Vec3& first,
    const Vec3& second, float radius, std::array<i32, 3>& lower,
    std::array<i32, 3>& upper); // Xbox 0x00054340
bool intersect_runtime_mesh_triangle(const Vec3& direction, const Vec3& second,
    const Vec3& third, const Vec3& first, const Vec3& origin,
    float& fraction); // Xbox 0x00103400
const u8* select_runtime_mesh_segment_triangle(std::span<const u8> triangles,
    const Vec3* vertices, const Vec3& origin, Vec3& movement, Vec3& normal,
    u32 mask, u32 excluded, float& fraction); // Xbox 0x000551A0
void write_runtime_mesh_triangle_contact(std::span<u8> record, std::span<const u8> triangle,
    u32 triangle_address, const Vec3* vertices, const Vec3& normal, const Vec3& point,
    float radius, const float* world); // Xbox 0x00054170
}
