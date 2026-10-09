#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
const float* shape_bone_matrix(const EntityBonePose& pose, i32 index); // Xbox 0x000315F0
void shape_transform_point(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, const Vec3& local, Vec3& output); // Xbox 0x00056790
float shape_sphere_metrics(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, Vec3& center); // Xbox 0x00056330
}
