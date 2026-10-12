#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
bool runtime_projected_point_in_triangle(const Vec3& normal, const Vec3& point,
    const Vec3& second_edge, const Vec3& third_edge); // Xbox 0x0015A2E0
bool runtime_sphere_intersects_triangle(const Vec3& point, float radius,
    const Vec3& first, const Vec3& second, const Vec3& third); // Xbox 0x0015A440
}
