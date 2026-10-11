#include "contact_sweep.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00159E20
bool intersect_runtime_collision_triangle(const Vec3& point, const Vec3& movement,
    const Vec3& first, const Vec3& second, const Vec3& third,
    const Vec3& triangle_movement, float limit, float& fraction,
    float& second_weight, float& third_weight)
{
    const Vec3 relative{movement.x - triangle_movement.x,
        movement.y - triangle_movement.y, movement.z - triangle_movement.z};
    const Vec3 edge_third{third.x - first.x, third.y - first.y, third.z - first.z};
    const Vec3 edge_second{second.x - first.x, second.y - first.y, second.z - first.z};
    const Vec3 cross{relative.y * edge_third.z - relative.z * edge_third.y,
        relative.z * edge_third.x - edge_third.z * relative.x,
        edge_third.y * relative.x - relative.y * edge_third.x};
    const float determinant = (cross.z * edge_second.z + cross.y * edge_second.y) + edge_second.x * cross.x;
    const bool negative = 0.f > determinant;
    if (negative)
    {
        if (determinant > std::bit_cast<float>(0xB58637BDu))
        {
            return false;
        }
    }
    else if (std::bit_cast<float>(0x358637BDu) > determinant)
    {
        return false;
    }
    const Vec3 delta{point.x - first.x, point.y - first.y, point.z - first.z};
    const float second_numerator = (delta.z * cross.z + delta.y * cross.y) + delta.x * cross.x;
    if (negative)
    {
        if (second_numerator > 0.f || determinant > second_numerator)
        {
            return false;
        }
    }
    else if (0.f > second_numerator || second_numerator > determinant)
    {
        return false;
    }
    const Vec3 delta_cross{delta.y * edge_second.z - delta.z * edge_second.y,
        delta.z * edge_second.x - delta.x * edge_second.z,
        delta.x * edge_second.y - delta.y * edge_second.x};
    const float third_numerator = (delta_cross.z * relative.z + delta_cross.y * relative.y) + relative.x * delta_cross.x;
    if (negative)
    {
        if (third_numerator > 0.f || determinant > third_numerator + second_numerator)
        {
            return false;
        }
    }
    else if (0.f > third_numerator || third_numerator + second_numerator > determinant)
    {
        return false;
    }
    const float time_numerator = (delta_cross.z * edge_third.z + delta_cross.y * edge_third.y) + edge_third.x * delta_cross.x;
    if (negative)
    {
        if (determinant * limit > time_numerator || time_numerator > 0.f)
        {
            return false;
        }
    }
    else if (time_numerator > determinant * limit || 0.f > time_numerator)
    {
        return false;
    }
    const float reciprocal = 1.f / determinant;
    fraction = reciprocal * time_numerator;
    second_weight = reciprocal * second_numerator;
    third_weight = reciprocal * third_numerator;
    return true;
}

// Xbox 0x00102E90
bool sweep_runtime_collision_triangle(const Vec3& first, const Vec3& second,
    const Vec3& third, const Vec3& point, const Vec3& movement, float radius,
    Vec3& normal, float& fraction, const Vec3& stationary)
{
    const Vec3 a{second.x - first.x, second.y - first.y, second.z - first.z};
    const Vec3 b{third.x - first.x, third.y - first.y, third.z - first.z};
    Vec3 face{b.z * a.y - b.y * a.z, a.z * b.x - b.z * a.x, b.y * a.x - a.y * b.x};
    const float squared = (face.z * face.z + face.y * face.y) + face.x * face.x;
    if (squared != 0.f)
    {
        const long double reciprocal = retail_reciprocal_sqrt(squared);
        face.x = float(static_cast<long double>(face.x) * reciprocal);
        face.y = float(static_cast<long double>(face.y) * reciprocal);
        face.z = float(static_cast<long double>(face.z) * reciprocal);
    }
    const float dot = (movement.z * face.z + movement.y * face.y) + movement.x * face.x;
    if (!(dot > 0.f))
    {
        if (0.f > dot)
        {
            const float inward = 0.f - radius;
            const Vec3 surface{point.x + face.x * inward,
                point.y + face.y * inward, point.z + face.z * inward};
            float second_weight;
            float third_weight;
            if (intersect_runtime_collision_triangle(surface, movement, first, second,
                third, stationary, 1.f, fraction, second_weight, third_weight))
            {
                normal = face;
                return true;
            }
        }
        Vec3 candidate;
        float candidate_fraction;
        bool found = sweep_runtime_collision_edge(second, first, movement, point, radius, candidate, candidate_fraction);
        if (found)
        {
            fraction = candidate_fraction;
            normal = candidate;
        }
        if (sweep_runtime_collision_edge(third, second, movement, point, radius, candidate, candidate_fraction) &&
            (!found || fraction > candidate_fraction))
        {
            fraction = candidate_fraction;
            normal = candidate;
            found = true;
        }
        if (sweep_runtime_collision_edge(first, third, movement, point, radius, candidate, candidate_fraction) &&
            (!found || fraction > candidate_fraction))
        {
            fraction = candidate_fraction;
            normal = candidate;
            found = true;
        }
        if (sweep_runtime_collision_point(first, point, radius, movement, candidate, candidate_fraction) &&
            (!found || fraction > candidate_fraction))
        {
            fraction = candidate_fraction;
            normal = candidate;
            found = true;
        }
        if (sweep_runtime_collision_point(second, point, radius, movement, candidate, candidate_fraction) &&
            (!found || fraction > candidate_fraction))
        {
            fraction = candidate_fraction;
            normal = candidate;
            found = true;
        }
        if (sweep_runtime_collision_point(third, point, radius, movement, candidate, candidate_fraction) &&
            (!found || fraction > candidate_fraction))
        {
            fraction = candidate_fraction;
            normal = candidate;
            found = true;
        }
        return found;
    }
    return false;
}

// Xbox 0x00102DA0
bool select_runtime_collision_root(float first, float second, float third, float& output)
{
    if (first >= std::bit_cast<float>(0xB456BF95u) && first <= std::bit_cast<float>(0x3456BF95u))
    {
        return false;
    }
    const float discriminant = second * second - (first * third) * 4.f;
    if (0.f >= discriminant)
    {
        return false;
    }
    const float root = float(retail_square_root(discriminant));
    const float reciprocal = 1.f / (first * 2.f);
    const float positive = (root - second) * reciprocal;
    const float negative = ((0.f - second) - root) * reciprocal;
    if (!(negative > positive))
    {
        if (negative >= 0.f && negative <= 1.f)
        {
            output = negative;
            return true;
        }
        if (!(positive >= 0.f) || !(positive <= 1.f))
        {
            return false;
        }
        output = positive;
        return true;
    }
    if (positive >= 0.f && positive <= 1.f)
    {
        output = positive;
        return true;
    }
    if (!(negative >= 0.f) || !(negative <= 1.f))
    {
        return false;
    }
    output = negative;
    return true;
}

// Xbox 0x00102C30
bool sweep_runtime_collision_point(const Vec3& vertex, const Vec3& point,
    float radius, const Vec3& movement, Vec3& normal, float& fraction)
{
    const Vec3 delta{point.x - vertex.x, point.y - vertex.y, point.z - vertex.z};
    const float first = (movement.x * movement.x + movement.y * movement.y) + movement.z * movement.z;
    const float second = ((movement.y * delta.y + movement.z * delta.z) + movement.x * delta.x) * 2.f;
    const float third = ((delta.y * delta.y + delta.x * delta.x) + delta.z * delta.z) - radius * radius;
    if (select_runtime_collision_root(first, second, third, fraction))
    {
        normal.x = (point.x + movement.x * fraction) - vertex.x;
        normal.y = (point.y + movement.y * fraction) - vertex.y;
        normal.z = (movement.z * fraction + point.z) - vertex.z;
        const float reciprocal = 1.f / radius;
        normal.x = normal.x * reciprocal;
        normal.y = normal.y * reciprocal;
        normal.z = normal.z * reciprocal;
        return true;
    }
    return false;
}

// Xbox 0x00102970
bool sweep_runtime_collision_edge(const Vec3& end, const Vec3& begin,
    const Vec3& movement, const Vec3& point, float radius, Vec3& normal, float& fraction)
{
    const Vec3 edge{end.x - begin.x, end.y - begin.y, end.z - begin.z};
    const float along = (movement.z * edge.z + movement.x * edge.x) + movement.y * edge.y;
    const float length = (edge.y * edge.y + edge.x * edge.x) + edge.z * edge.z;
    const float first = along * along -
        ((movement.y * movement.y + movement.x * movement.x) + movement.z * movement.z) * length;
    if (!(std::bit_cast<float>(0x3456BF95u) > animation_absolute(first)))
    {
        const Vec3 delta{point.x - begin.x, point.y - begin.y, point.z - begin.z};
        const float projection = (delta.y * edge.y + delta.z * edge.z) + delta.x * edge.x;
        const float second = (projection * along -
            ((movement.x * delta.x + movement.z * delta.z) + movement.y * delta.y) * length) * 2.f;
        const float third = (projection * projection -
            ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x) * length) +
            (length * radius) * radius;
        if (select_runtime_collision_root(first, second, third, fraction))
        {
            const float segment_fraction = (fraction * along + projection) / length;
            if (segment_fraction >= 0.f && segment_fraction <= 1.f)
            {
                const float reciprocal = 1.f / radius;
                normal.x = ((point.x + movement.x * fraction) - (begin.x + edge.x * segment_fraction)) * reciprocal;
                normal.y = ((point.y + movement.y * fraction) - (begin.y + edge.y * segment_fraction)) * reciprocal;
                normal.z = ((point.z + movement.z * fraction) - (begin.z + edge.z * segment_fraction)) * reciprocal;
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00159CC0
bool sweep_runtime_collision_spheres(const Vec3& first_position,
    const Vec3& first_movement, const Vec3& second_position, float radius,
    const Vec3& second_movement, float limit, float& fraction)
{
    const Vec3 delta{first_position.x - second_position.x,
        first_position.y - second_position.y, first_position.z - second_position.z};
    const Vec3 movement{first_movement.x - second_movement.x,
        first_movement.y - second_movement.y, first_movement.z - second_movement.z};
    const float squared = (movement.z * movement.z + movement.x * movement.x) + movement.y * movement.y;
    const float outside = ((delta.x * delta.x + delta.z * delta.z) + delta.y * delta.y) - radius * radius;
    if (!(std::bit_cast<float>(0x358637BDu) > squared))
    {
        const float dot = (movement.x * delta.x + movement.z * delta.z) + movement.y * delta.y;
        if (!(dot > 0.f) || !(outside > 0.f))
        {
            const float discriminant = dot * dot - outside * squared;
            if (!(0.f > discriminant))
            {
                const float end = squared * limit + dot;
                if (end >= 0.f || discriminant >= end * end)
                {
                    fraction = float(-((retail_square_root(discriminant) + static_cast<long double>(dot)) /
                        static_cast<long double>(squared)));
                    return true;
                }
            }
        }
    }
    else if (0.f >= outside)
    {
        fraction = 0.f;
        return true;
    }
    return false;
}

// Xbox 0x0015AB50
bool sweep_runtime_collision_sphere_pair(const Vec3& first_position, float first_radius,
    const Vec3& first_movement, const Vec3& second_position, float second_radius,
    const Vec3& second_movement, float limit, float& fraction)
{
    return sweep_runtime_collision_spheres(first_position, first_movement,
        second_position, first_radius + second_radius, second_movement, limit, fraction);
}
}
