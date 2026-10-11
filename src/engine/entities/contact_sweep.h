#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
bool intersect_runtime_collision_triangle(const Vec3& point, const Vec3& movement,
    const Vec3& first, const Vec3& second, const Vec3& third,
    const Vec3& triangle_movement, float limit, float& fraction,
    float& second_weight, float& third_weight); // Xbox 0x00159E20
bool sweep_runtime_collision_triangle(const Vec3& first, const Vec3& second,
    const Vec3& third, const Vec3& point, const Vec3& movement, float radius,
    Vec3& normal, float& fraction, const Vec3& stationary); // Xbox 0x00102E90
bool select_runtime_collision_root(float first, float second, float third,
    float& output); // Xbox 0x00102DA0
bool sweep_runtime_collision_point(const Vec3& vertex, const Vec3& point,
    float radius, const Vec3& movement, Vec3& normal,
    float& fraction); // Xbox 0x00102C30
bool sweep_runtime_collision_edge(const Vec3& end, const Vec3& begin,
    const Vec3& movement, const Vec3& point, float radius,
    Vec3& normal, float& fraction); // Xbox 0x00102970
bool sweep_runtime_collision_spheres(const Vec3& first_position,
    const Vec3& first_movement, const Vec3& second_position, float radius,
    const Vec3& second_movement, float limit,
    float& fraction); // Xbox 0x00159CC0
bool sweep_runtime_collision_sphere_pair(const Vec3& first_position, float first_radius,
    const Vec3& first_movement, const Vec3& second_position, float second_radius,
    const Vec3& second_movement, float limit,
    float& fraction); // Xbox 0x0015AB50
}
