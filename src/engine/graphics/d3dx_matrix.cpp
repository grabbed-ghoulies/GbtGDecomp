#include "d3dx_matrix.h"
#include <array>
#include <algorithm>

namespace gbtg
{
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
