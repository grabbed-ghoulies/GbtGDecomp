#pragma once
#include "body_shape_push.h"
#include <functional>

namespace gbtg
{
struct RuntimeActorShapePush
{
    Actor& actor;
    const u32* model_words; // Signed shape +2 indexes model +0x16C.
    Vec3& contact; // DAT_005111AC[DAT_00511188].
    std::function<void(Vec3&, const Vec3&)> normalize; // D3DXVec3Normalize, 0x00184F99.
    std::function<void(const Actor&, const Vec3&, const Vec3&, u16)> message; // Actor vtable +0xC.
};
void runtime_actor_cylinder_metrics(const Actor& actor, Vec3& position,
    float& height, float& radius); // Xbox 0x0003DD50
bool runtime_offset_cylinders_apart(const EntityCylinderBound& fixed,
    const EntityCylinderBound& moving, const Vec3& offset); // Xbox 0x001206A0
void runtime_actor_planar_delta(const Actor& first, const Actor& second,
    Vec3& output); // Xbox 0x000418B0
void sweep_runtime_directed_cylinder_push(const Vec3& moving, const Vec3& fixed,
    float moving_radius, float fixed_radius, const Vec3& direction, Vec3& output,
    const std::function<void(Vec3&, const Vec3&)>& normalize); // Xbox 0x00120BA0
bool push_runtime_attached_cylinder(const Actor& first, const Actor& second,
    const EntityCylinderBound& fixed, const EntityCylinderBound& moving,
    const Vec3* query, const Vec3& offset, Vec3& push, Vec3& contact,
    const std::function<void(Vec3&, const Vec3&)>& normalize); // Xbox 0x00120950
bool push_runtime_attachment_projection(const Actor& first, const Actor& second,
    const EntityCylinderBound& candidate, Vec3& output); // Xbox 0x0003C470
bool accumulate_runtime_actor_shape_push(const Actor& first, const Actor* second,
    const Vec3* query, const EntityCylinderBound& fixed, const EntityCylinderBound& merged,
    EntityCylinderBound* shapes, i32 count, bool attached, Vec3& output,
    RuntimeActorShapePush& context); // Xbox 0x0003C3A0
void notify_runtime_actor_shape_push(const Actor* other, const Vec3& output,
    u16 bone, RuntimeActorShapePush& context); // Xbox 0x0003C5D0
bool push_runtime_actor_shapes(const Actor* other, const Vec3* query,
    const EntityCylinderBound& merged, EntityCylinderBound* shapes, i32 count,
    Vec3& output, RuntimeActorShapePush& context); // Xbox 0x0003C0F0
}
