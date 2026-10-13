#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
float sample_runtime_angle_range(float upper, float lower, u32& random); // Xbox 0x00104FC0
void sample_runtime_rectangle_position(const Vec3& extent, Vec3& output,
    i32 surface, u32& random); // Xbox 0x00105550
void sample_runtime_ellipse_position(const Vec3& extent, Vec3& output,
    i32 surface, u32& random); // Xbox 0x00105480
void sample_runtime_ellipsoid_position(const Vec3& extent, Vec3& output, i32 surface,
    float pitch_upper, float pitch_lower, float yaw_upper, float yaw_lower,
    u32& random); // Xbox 0x001053B0
void sample_runtime_cylinder_position(const Vec3& extent, Vec3& output, i32 surface,
    float pitch_upper, float pitch_lower, u32& random); // Xbox 0x001056B0
void sample_runtime_line_position(Vec3& output, float extent, u32& random); // Xbox 0x00105170
void sample_runtime_box_position(const Vec3& extent, Vec3& output,
    i32 surface, u32& random); // Xbox 0x001051E0
}
