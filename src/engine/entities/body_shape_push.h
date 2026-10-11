#pragma once
#include "player_state.h"
#include "actor_shape_bounds.h"
#include "entity_types.h"

namespace gbtg
{
struct RuntimeBodyShapePush
{
    entity_detail::ShapeProbe*& first; // DAT_00511B04.
    i32& contact_count; // DAT_00511188.
    std::span<entity_detail::ShapeProbe*> contacts; // DAT_0051118C, actual scene-owned pointer map.
};
void merge_runtime_collision_cylinders(const Vec3& point, EntityCylinderBound* shapes,
    i32 count, EntityCylinderBound& output); // Xbox 0x001202C0
void shift_runtime_collision_cylinders(const Vec3& push, EntityCylinderBound* shapes,
    i32 count, EntityCylinderBound& merged); // Xbox 0x00120D10
i32 sum_runtime_collision_push(const Vec3& point, const Actor* actor,
    EntityCylinderBound* shapes, i32 count, Vec3& output,
    RuntimeBodyShapePush& context); // Xbox 0x00120480
void push_runtime_collision_point(const Vec3& source, Vec3& target, const Actor* actor,
    float height, float radius, RuntimeBodyShapePush& context); // Xbox 0x001203E0
void transform_runtime_shape_sphere(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, EntityCylinderBound& output); // Xbox 0x00056720
bool separate_runtime_collision_triangle(std::span<const u8> shape, Vec3& point,
    float radius); // Xbox 0x0010ACD0
bool separate_runtime_collision_sphere(std::span<const u8> shape, Vec3& point,
    float radius); // Xbox 0x0010ABD0
void scale_runtime_collision_vector(Vec3& vector, float length); // Xbox 0x00120250
bool push_runtime_collision_cylinder(const EntityCylinderBound& moving,
    const EntityCylinderBound& fixed, const Vec3& offset, Vec3& push,
    Vec3& contact); // Xbox 0x00120760
void push_runtime_player_body_shapes(std::span<u8> body, Actor& actor, Vec3& position,
    RuntimeBodyShapeStorage& shapes, RuntimeBodyShapePush& context); // Xbox 0x000756C0
}
