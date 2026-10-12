#include "mesh_sphere_geometry.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0015A2E0
bool runtime_projected_point_in_triangle(const Vec3& normal, const Vec3& point,
    const Vec3& second_edge, const Vec3& third_edge)
{
    size_t first_axis, second_axis;
    const float nx = animation_absolute(normal.x);
    const float ny = animation_absolute(normal.y);
    if (nx > ny)
    {
        const float nz = animation_absolute(normal.z);
        const float x = animation_absolute(normal.x);
        if (nz > x)
        {
            first_axis = 0;
            second_axis = 1;
        }
        else
        {
            first_axis = 1;
            second_axis = 2;
        }
    }
    else
    {
        const float nz = animation_absolute(normal.z);
        const float y = animation_absolute(normal.y);
        first_axis = 0;
        second_axis = nz > y ? 1 : 2;
    }
    const float p[3]{point.x, point.y, point.z};
    const float a[3]{second_edge.x, second_edge.y, second_edge.z};
    const float b[3]{third_edge.x, third_edge.y, third_edge.z};
    const float first_weight = a[second_axis] * p[first_axis] - a[first_axis] * p[second_axis];
    const float second_weight = b[first_axis] * p[second_axis] - b[second_axis] * p[first_axis];
    const float determinant = b[first_axis] * a[second_axis] - b[second_axis] * a[first_axis];
    if (!(0.f > determinant))
    {
        return !(first_weight + second_weight > determinant || 0.f > second_weight || 0.f > first_weight);
    }
    return !(determinant > first_weight + second_weight || second_weight > 0.f || first_weight > 0.f);
}

// Xbox 0x0015A440
bool runtime_sphere_intersects_triangle(const Vec3& point, float radius,
    const Vec3& first, const Vec3& second, const Vec3& third)
{
    const Vec3 b{third.x - first.x, third.y - first.y, third.z - first.z};
    const Vec3 a{second.x - first.x, second.y - first.y, second.z - first.z};
    Vec3 p{point.x - first.x, point.y - first.y, point.z - first.z};
    const Vec3 normal{b.z * a.y - b.y * a.z, b.x * a.z - b.z * a.x, b.y * a.x - b.x * a.y};
    const float distance = (normal.z * p.z + normal.y * p.y) + p.x * normal.x;
    const float radius_squared = radius * radius;
    const float normal_squared = (normal.z * normal.z + normal.y * normal.y) + normal.x * normal.x;
    bool second_unchecked = true;
    bool third_unchecked = true;
    if (normal_squared < std::bit_cast<float>(0x3727C5ACu) || normal_squared * radius_squared < distance * distance)
    {
        return false;
    }
    if ((p.z * p.z + p.y * p.y) + p.x * p.x <= radius_squared)
    {
        return true;
    }
    const float first_projection = (p.y * a.y + p.z * a.z) + p.x * a.x;
    const float first_length = (a.x * a.x + a.y * a.y) + a.z * a.z;
    if (first_projection < first_length)
    {
        if (first_projection > 0.f)
        {
            const Vec3 delta{a.x * first_projection - p.x * first_length,
                a.y * first_projection - p.y * first_length, a.z * first_projection - p.z * first_length};
            if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= first_length * first_length * radius_squared)
            {
                return true;
            }
        }
    }
    else
    {
        second_unchecked = false;
        const Vec3 delta{a.x - p.x, a.y - p.y, a.z - p.z};
        if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= radius_squared)
        {
            return true;
        }
    }
    const float second_projection = (p.z * b.z + p.y * b.y) + p.x * b.x;
    const float second_length = (b.z * b.z + b.y * b.y) + b.x * b.x;
    if (second_projection < second_length)
    {
        if (second_projection > 0.f)
        {
            const Vec3 delta{b.x * second_projection - p.x * second_length,
                b.y * second_projection - p.y * second_length, b.z * second_projection - p.z * second_length};
            if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= second_length * second_length * radius_squared)
            {
                return true;
            }
        }
    }
    else
    {
        third_unchecked = false;
        const Vec3 delta{b.x - p.x, b.y - p.y, b.z - p.z};
        if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= radius_squared)
        {
            return true;
        }
    }
    const Vec3 edge{b.x - a.x, b.y - a.y, b.z - a.z};
    const float length = (edge.z * edge.z + edge.y * edge.y) + edge.x * edge.x;
    const float projection = ((second_projection - first_projection) + first_length)
        - ((b.y * a.y + b.z * a.z) + b.x * a.x);
    if (projection < length)
    {
        if (projection >= 0.f)
        {
            const Vec3 delta{edge.x * projection - (p.x - a.x) * length,
                edge.y * projection - (p.y - a.y) * length, edge.z * projection - (p.z - a.z) * length};
            if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= length * length * radius_squared)
            {
                return true;
            }
        }
        else if (second_unchecked)
        {
            const Vec3 delta{a.x - p.x, a.y - p.y, a.z - p.z};
            if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= radius_squared)
            {
                return true;
            }
        }
    }
    else if (third_unchecked)
    {
        const Vec3 delta{b.x - p.x, b.y - p.y, b.z - p.z};
        if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x <= radius_squared)
        {
            return true;
        }
    }
    const float scale = 0.f - distance / normal_squared;
    p = {scale * normal.x + p.x, normal.y * scale + p.y, normal.z * scale + p.z};
    return runtime_projected_point_in_triangle(normal, p, a, b);
}
}
