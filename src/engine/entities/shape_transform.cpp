#include "shape_transform.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
float g_shape_bone_scratch[12]; // DAT_004DD768
}

// Xbox 0x000315F0
const float* shape_bone_matrix(const EntityBonePose& pose, i32 index)
{
    if (pose.packed_flag() != 0)
    {
        return pose.matrix_data() + index * 12;
    }
    const float* source = pose.matrix_data() + index * 16;
    g_shape_bone_scratch[0] = source[0];
    g_shape_bone_scratch[1] = source[4];
    g_shape_bone_scratch[2] = source[8];
    g_shape_bone_scratch[3] = source[12];
    g_shape_bone_scratch[4] = source[1];
    g_shape_bone_scratch[5] = source[5];
    g_shape_bone_scratch[6] = source[9];
    g_shape_bone_scratch[7] = source[13];
    g_shape_bone_scratch[8] = source[2];
    g_shape_bone_scratch[9] = source[6];
    g_shape_bone_scratch[10] = source[10];
    g_shape_bone_scratch[11] = source[14];
    return g_shape_bone_scratch;
}

// Xbox 0x00056790
void shape_transform_point(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, const Vec3& local, Vec3& output)
{
    if (node.bone == 0xFFFF)
    {
        output = local;
    }
    else
    {
        const float* m = shape_bone_matrix(*pose, std::bit_cast<i16>(node.bone));
        output.x = m[1] * local.y + m[2] * local.z + local.x * m[0] + m[3];
        output.y = m[5] * local.y + m[4] * local.x + m[6] * local.z + m[7];
        output.z = m[9] * local.y + m[8] * local.x + m[10] * local.z + m[11];
    }
    if (world != nullptr)
    {
        const Vec3 point = output;
        output.x = world[2] * point.z + world[1] * point.y + world[0] * point.x + world[3];
        output.y = world[6] * point.z + world[4] * point.x + world[5] * point.y + world[7];
        output.z = world[10] * point.z + world[8] * point.x + world[9] * point.y + world[11];
    }
}

// Xbox 0x00056330
float shape_sphere_metrics(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, Vec3& center)
{
    Vec3 local;
    std::memcpy(&local, reinterpret_cast<const u8*>(&node) + 0x14, sizeof(local));
    shape_transform_point(node, world, pose, local, center);
    local.x = node.along_x + local.x;
    Vec3 arm;
    shape_transform_point(node, world, pose, local, arm);
    const float dx = center.x - arm.x;
    const float dy = center.y - arm.y;
    const float dz = center.z - arm.z;
    return float(std::sqrt(static_cast<long double>(dx) * dx +
        static_cast<long double>(dy) * dy + static_cast<long double>(dz) * dz));
}
}
