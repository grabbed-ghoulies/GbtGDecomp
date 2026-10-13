#include "pose_matrix.h"
#include "gbtg/anim.h"

namespace gbtg
{
// Rotation is Ry(euler.y) * Rx(euler.x) * Rz(euler.z).
// A scale component other than 1 multiplies that column. Translation is the position.
// Xbox 0x00103CB0
void build_runtime_pose_matrix(float m[12], const Vec3& pos, const Vec3& euler, const Vec3& scale,
    float* inverse)
{
    const float sx = float(retail_sin(euler.x));
    const float cx = float(retail_cos(euler.x));
    const float sy = float(retail_sin(euler.y));
    const float cy = float(retail_cos(euler.y));
    const float sz = float(retail_sin(euler.z));
    const float cz = float(retail_cos(euler.z));

    m[0] = (sy * sx) * sz + cz * cy;
    m[1] = sy * sx * cz - sz * cy;
    m[2] = sy * cx;
    m[4] = sz * cx;
    m[5] = cz * cx;
    m[6] = 0.f - sx;
    m[8] = sz * cy * sx - cz * sy;
    m[9] = sz * sy + cz * cy * sx;
    m[10] = cy * cx;

    if (scale.x != 1.f)
    {
        m[0] *= scale.x;
        m[4] *= scale.x;
        m[8] *= scale.x;
    }
    if (scale.y != 1.f)
    {
        m[1] *= scale.y;
        m[5] *= scale.y;
        m[9] *= scale.y;
    }
    if (scale.z != 1.f)
    {
        m[2] *= scale.z;
        m[6] *= scale.z;
        m[10] *= scale.z;
    }
    m[3] = pos.x;
    m[7] = pos.y;
    m[11] = pos.z;
    if (inverse != nullptr)
    {
        if (scale.x == 1.f && scale.y == 1.f && scale.z == 1.f)
        {
            inverse[0] = m[0];
            inverse[1] = m[4];
            inverse[2] = m[8];
            inverse[4] = m[1];
            inverse[5] = m[5];
            inverse[6] = m[9];
            inverse[8] = m[2];
            inverse[9] = m[6];
            inverse[10] = m[10];
        }
        else
        {
            float factor = 1.f / (scale.x * scale.x);
            inverse[0] = m[0] * factor;
            inverse[1] = m[4] * factor;
            inverse[2] = factor * m[8];
            factor = 1.f / (scale.y * scale.y);
            inverse[4] = factor * m[1];
            inverse[5] = m[5] * factor;
            inverse[6] = factor * m[9];
            factor = 1.f / (scale.z * scale.z);
            inverse[8] = m[2] * factor;
            inverse[9] = m[6] * factor;
            inverse[10] = m[10] * factor;
        }
        const float x = 0.f - pos.x;
        const float y = 0.f - pos.y;
        const float z = 0.f - pos.z;
        inverse[3] = inverse[1] * y + inverse[0] * x + inverse[2] * z;
        inverse[7] = inverse[6] * z + inverse[4] * x + inverse[5] * y;
        inverse[11] = inverse[9] * y + inverse[10] * z + inverse[8] * x;
    }
}

}
