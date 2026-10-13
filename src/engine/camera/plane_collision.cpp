#include "plane_collision.h"

namespace gbtg
{
// Xbox 0x0013C0B0
bool clip_runtime_camera_plane(const entity_detail::DisplayView& view, const Vec3& plane,
    const Vec3& source, Vec3& candidate, Vec3* normal, std::array<float, 2>* screen)
{
    const auto& matrix = view.transform;
    const float sx = ((matrix[0] * source.x + source.y * matrix[1]) + source.z * matrix[2]) + matrix[3];
    const float sy = ((matrix[4] * source.x + matrix[6] * source.z) + matrix[5] * source.y) + matrix[7];
    const float sz = ((source.z * matrix[10] + source.y * matrix[9]) + source.x * matrix[8]) + matrix[11];
    const float cx = ((candidate.y * matrix[1] + matrix[0] * candidate.x) + candidate.z * matrix[2]) + matrix[3];
    const float cy = ((matrix[6] * candidate.z + matrix[5] * candidate.y) + matrix[4] * candidate.x) + matrix[7];
    const float cz = ((candidate.x * matrix[8] + candidate.z * matrix[10]) + candidate.y * matrix[9]) + matrix[11];
    const float distance = ((plane.z * sz + plane.y * sy) + plane.x * sx) - 5.f;
    if (0.f > distance)
    {
        return false;
    }
    const float travel = 0.f - (((cz - sz) * plane.z + (cy - sy) * plane.y) + (cx - sx) * plane.x);
    if (distance > travel)
    {
        return false;
    }
    float fraction = distance / travel;
    if (travel == 0.f)
    {
        fraction = 0.f;
    }
    const float remainder = 1.f - fraction;
    candidate.x = source.x * remainder + candidate.x * fraction;
    candidate.y = candidate.y * fraction + source.y * remainder;
    candidate.z = source.z * remainder + candidate.z * fraction;
    if (normal != nullptr)
    {
        const float x = ((matrix[0] * plane.x + matrix[2] * plane.z) + plane.y * matrix[1]) + matrix[3];
        const float y = ((matrix[4] * plane.x + matrix[6] * plane.z) + matrix[5] * plane.y) + matrix[7];
        const float z = ((matrix[8] * plane.x + plane.z * matrix[10]) + matrix[9] * plane.y) + matrix[11];
        normal->x = x;
        normal->y = y;
        normal->z = z;
    }
    if (screen != nullptr)
    {
        const float scale = view.focal_pixels / (cz * fraction + sz * remainder);
        const float y = (cy * fraction + sy * remainder) * scale + float(view.fitted_height) * .5f;
        const float x = float(view.fitted_width) * .5f - (view.pixel_aspect * (cx * fraction + sx * remainder)) * scale;
        (*screen)[0] = x;
        (*screen)[1] = y;
        if (0.f > x)
        {
            return false;
        }
        if (x >= float(view.fitted_width))
        {
            return false;
        }
        if (0.f > y)
        {
            return false;
        }
        if (!(float(view.fitted_height) > y))
        {
            return false;
        }
    }
    return true;
}
}
