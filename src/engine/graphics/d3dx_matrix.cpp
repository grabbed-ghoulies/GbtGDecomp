#include "d3dx_matrix.h"
#include "extended_float.h"
#include <array>
#include <algorithm>

namespace gbtg
{
// Xbox 0x00170250
void store_runtime_quaternion(Quat& output, float x, float y, float z, float w)
{
    output.x = x;
    output.y = y;
    output.z = z;
    output.w = w;
}

// Xbox 0x00184D48
void runtime_graphics_quaternion_matrix(float* output, const Quat& quaternion)
{
    const float x2 = float(ExtendedFloat{quaternion.x} * ExtendedFloat{2.});
    const float y2 = float(ExtendedFloat{quaternion.y} * ExtendedFloat{2.});
    const ExtendedFloat z2 = ExtendedFloat{2.} * ExtendedFloat{quaternion.z};
    const ExtendedFloat wx = ExtendedFloat{x2} * ExtendedFloat{quaternion.w};
    const ExtendedFloat wy = ExtendedFloat{y2} * ExtendedFloat{quaternion.w};
    const ExtendedFloat wz = z2 * ExtendedFloat{quaternion.w};
    const float xx = float(ExtendedFloat{x2} * ExtendedFloat{quaternion.x});
    const float xy = float(ExtendedFloat{y2} * ExtendedFloat{quaternion.x});
    const float xz = float(z2 * ExtendedFloat{quaternion.x});
    const float yy = float(ExtendedFloat{y2} * ExtendedFloat{quaternion.y});
    const float yz = float(z2 * ExtendedFloat{quaternion.y});
    const ExtendedFloat zz = z2 * ExtendedFloat{quaternion.z};
    output[0] = float((ExtendedFloat{1.} - ExtendedFloat{yy}) - zz);
    output[1] = float(ExtendedFloat{xy} + wz);
    output[2] = float(ExtendedFloat{xz} - wy);
    output[3] = 0.f;
    output[4] = float(ExtendedFloat{xy} - wz);
    const ExtendedFloat complement = ExtendedFloat{1.} - ExtendedFloat{xx};
    const float spilled_complement = float(complement);
    output[5] = float(complement - zz);
    output[6] = float(ExtendedFloat{yz} + wx);
    output[7] = 0.f;
    output[8] = float(ExtendedFloat{xz} + wy);
    output[9] = float(ExtendedFloat{yz} - wx);
    output[10] = float(ExtendedFloat{spilled_complement} - ExtendedFloat{yy});
    output[11] = 0.f;
    output[12] = 0.f;
    output[13] = 0.f;
    output[14] = 0.f;
    output[15] = 1.f;
}

// Xbox 0x00153BE0
void compose_runtime_graphics_matrix(float* output, Vec3 scale, Quat rotation, Vec3 translation)
{
    if (rotation.w == 1.f)
    {
        for (size_t index : {14u,13u,12u,11u,9u,8u,7u,6u,4u,3u,2u,1u})
        {
            output[index] = 0.f;
        }
        output[15] = 1.f;
        output[10] = 1.f;
        output[5] = 1.f;
        output[0] = 1.f;
    }
    else
    {
        Quat quaternion;
        store_runtime_quaternion(quaternion, rotation.x, rotation.y, rotation.z, rotation.w);
        runtime_graphics_quaternion_matrix(output, quaternion);
    }
    output[12] = translation.x;
    output[13] = translation.y;
    output[14] = translation.z;
    if (scale.x != 1.f)
    {
        output[0] *= scale.x;
        output[1] *= scale.x;
        output[2] *= scale.x;
    }
    if (scale.y != 1.f)
    {
        output[4] *= scale.y;
        output[5] *= scale.y;
        output[6] *= scale.y;
    }
    if (scale.z != 1.f)
    {
        output[8] *= scale.z;
        output[9] *= scale.z;
        output[10] *= scale.z;
    }
    output[3] = 0.f;
    output[7] = 0.f;
    output[11] = 0.f;
    output[15] = 1.f;
}

// Xbox 0x001850CA
void multiply_runtime_graphics_matrices(float* output, const float* first, const float* second)
{
    // Both retail paths (a copied second operand when it aliases the output, or the column-wise pass
    // when only it aliases) form each element as (a0*b0 + a2*b2) + (a1*b1 + a3*b3) in x87.
    std::array<float, 16> left;
    std::array<float, 16> right;
    std::copy_n(first, 16, left.begin());
    std::copy_n(second, 16, right.begin());
    for (size_t row = 0; row < 4; ++row)
    {
        for (size_t column = 0; column < 4; ++column)
        {
            const long double p0 = static_cast<long double>(left[row * 4]) * right[column];
            const long double p1 = static_cast<long double>(left[row * 4 + 1]) * right[4 + column];
            const long double p2 = static_cast<long double>(left[row * 4 + 2]) * right[8 + column];
            const long double p3 = static_cast<long double>(left[row * 4 + 3]) * right[12 + column];
            output[row * 4 + column] = float((p0 + p2) + (p1 + p3));
        }
    }
}

// Xbox 0x00184B7B
void runtime_graphics_translation_matrix(float* output, float x, float y, float z)
{
    output[0xB] = 0.f;
    output[0xC] = x;
    output[9] = 0.f;
    output[0xD] = y;
    output[8] = 0.f;
    output[0xE] = z;
    output[7] = 0.f;
    output[6] = 0.f;
    output[4] = 0.f;
    output[3] = 0.f;
    output[2] = 0.f;
    output[1] = 0.f;
    output[0xF] = 1.f;
    output[0xA] = 1.f;
    output[5] = 1.f;
    output[0] = 1.f;
}

// Xbox 0x0018462D
void transform_runtime_graphics_normal(Vec3& output, const Vec3& normal, const float* matrix)
{
    const Vec3 v = normal;
    const long double x = (static_cast<long double>(v.x) * matrix[0] + static_cast<long double>(v.y) * matrix[4])
        + static_cast<long double>(v.z) * matrix[8];
    const long double y = (static_cast<long double>(v.x) * matrix[1] + static_cast<long double>(v.y) * matrix[5])
        + static_cast<long double>(v.z) * matrix[9];
    const long double z = (static_cast<long double>(v.x) * matrix[2] + static_cast<long double>(v.y) * matrix[6])
        + static_cast<long double>(v.z) * matrix[10];
    output.x = float(x);
    output.y = float(y);
    output.z = float(z);
}
}
