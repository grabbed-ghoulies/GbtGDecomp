#pragma once
#include "model_sphere_collision.h"
#include <functional>
#include <unordered_map>

namespace gbtg
{
struct RuntimeCollisionShapeCall
{
    const u8* shape;
    u32 shape_address;
    const float* world;
    const float* inverse;
    const EntityBonePose* pose;
    u32 data_address; // Physical list +0xC; native provider resolves this in per-shape methods.
    const Vec3& candidate;
    Vec3& kept;
    float radius;
    u32 mask;
    i32 mode;
    std::span<u8> record;
    GhoulySpawnPool** pool;
    const Vec3& stationary;
};
struct RuntimeCollisionShapeDispatch
{
    RuntimeCollisionShapeCall call;
    const u32* bone_masks; // Signed shape +2 indexes the model +0x16C words (native shader_indices view).
    std::span<const u8> address_zero;
    std::function<u32(const void*)> address_of;
    const std::unordered_map<u32, std::function<u32(RuntimeCollisionShapeCall&)>>& physical_methods;
};
u32 dispatch_runtime_physical_segment_shapes(std::span<const u8> list,
    RuntimeCollisionShapeDispatch& context); // Xbox 0x00055670
u32 dispatch_runtime_physical_swept_shapes(std::span<const u8> list,
    RuntimeCollisionShapeDispatch& context); // Xbox 0x000557E0
u32 dispatch_runtime_physical_contact_shapes(std::span<const u8> list,
    RuntimeCollisionShapeDispatch& context); // Xbox 0x00055960
u32 dispatch_runtime_model_segment_shapes(std::span<const u8> list,
    RuntimeCollisionShapeDispatch& context); // Xbox 0x000568E0
u32 dispatch_runtime_model_swept_shapes(std::span<const u8> list,
    RuntimeCollisionShapeDispatch& context); // Xbox 0x00056A60
u32 dispatch_runtime_model_contact_shapes(std::span<const u8> list,
    RuntimeCollisionShapeDispatch& context); // Xbox 0x00056BE0
}
