#include "d3dx_view.h"
#include "matrix_inverse.h"
#include "vector_normalize.h"

#include <cmath>

namespace gbtg
{
// Xbox 0x00184F74
void subtract_runtime_graphics_vectors(float* output, const float* first, const float* second)
{
    output[0] = first[0] - second[0];
    output[1] = first[1] - second[1];
    output[2] = first[2] - second[2];
}

// Xbox 0x00184F28
void cross_runtime_graphics_vectors(float* output, const float* first, const float* second)
{
    const float first_z = first[2];
    const float second_x = second[0];
    const float second_z = second[2];
    const float first_x = first[0];
    const float second_y = second[1];
    const float first_y = first[1];
    output[0] = second[2] * first[1] - first[2] * second[1];
    output[1] = first_z * second_x - second_z * first_x;
    output[2] = first_x * second_y - second_x * first_y;
}

// Xbox 0x0018468C
void transform_runtime_graphics_vector4(float* output, const float* v, const float* m)
{
    const float x = v[0];
    const float y = v[1];
    const float z = v[2];
    const float w = v[3];
    output[0] = w * m[12] + z * m[8] + y * m[4] + x * m[0];
    output[1] = w * m[13] + z * m[9] + y * m[5] + x * m[1];
    output[2] = w * m[14] + z * m[10] + y * m[6] + x * m[2];
    output[3] = w * m[15] + z * m[11] + y * m[7] + x * m[3];
}

// Xbox 0x00184725
void transpose_runtime_graphics_matrix(float* output, const float* input)
{
    float swap = input[1];
    output[1] = input[4];
    output[4] = swap;
    swap = input[2];
    output[2] = input[8];
    output[8] = swap;
    swap = input[3];
    output[3] = input[12];
    output[12] = swap;
    swap = input[6];
    output[6] = input[9];
    output[9] = swap;
    swap = input[7];
    output[7] = input[13];
    output[13] = swap;
    swap = input[11];
    output[11] = input[14];
    output[14] = swap;
    if (output != input)
    {
        output[0] = input[0];
        output[5] = input[5];
        output[10] = input[10];
        output[15] = input[15];
    }
}

// Xbox 0x00184792
bool inverse_runtime_graphics_matrix(float* output, float* determinant, const float* m)
{
    float a3 = m[4];
    float a1 = m[0];
    float a2 = m[1];
    float a4 = m[5];
    const float a5 = m[8];
    const float a6 = m[9];
    const float a7 = m[12];
    const float s19 = a4 * a1 - a3 * a2;
    const float a8 = m[13];
    const float a9 = m[14];
    const float a10 = m[15];
    const float s18 = a6 * a1 - a5 * a2;
    const float s17 = a8 * a1 - a7 * a2;
    const float s20 = a6 * a3 - a5 * a4;
    const float s21 = a8 * a3 - a7 * a4;
    const float s22 = a8 * a5 - a7 * a6;
    a1 = m[2];
    a2 = m[3];
    a3 = m[6];
    a4 = m[7];
    const float b5 = m[10];
    const float b6 = m[11];
    const float s31 = a1 * a4 - a3 * a2;
    const float s30 = a1 * b6 - b5 * a2;
    const float s23 = a1 * a10 - a9 * a2;
    const float c11 = m[0];
    const float c12 = m[4];
    const float c13 = m[8];
    const float s28 = a3 * b6 - b5 * a4;
    const float c14 = m[9];
    const float s29 = a3 * a10 - a9 * a4;
    const float c15 = m[12];
    const float s26 = b5 * a10 - a9 * b6;
    const float c7 = m[1];
    const float c8 = m[5];
    const float c16 = m[13];
    const float s27 = (s30 * c8 - s31 * c14) - s28 * c7;
    const float s24 = s31 * c16 + (s29 * c7 - s23 * c8);
    const float s25 = (s23 * c14 - s30 * c16) - s26 * c7;
    const float s7 = s28 * c16 + (s26 * c8 - s29 * c14);
    float value = s7 * c11 + s25 * c12 + s24 * c13 + s27 * c15;
    if (determinant != nullptr)
    {
        *determinant = value;
    }
    value = 1.f / value;
    if (!runtime_graphics_double_is_finite(double(value)))
    {
        return false;
    }
    output[0] = s7 * value;
    output[1] = s25 * value;
    output[2] = s24 * value;
    output[3] = s27 * value;
    output[4] = ((s29 * c13 - s28 * c15) - s26 * c12) * value;
    output[5] = (s30 * c15 + (c11 * s26 - s23 * c13)) * value;
    output[6] = ((s23 * c12 - s31 * c15) - s29 * c11) * value;
    output[7] = (s31 * c13 + (s28 * c11 - s30 * c12)) * value;
    output[8] = (s20 * a10 + (s22 * a4 - s21 * b6)) * value;
    output[9] = ((s17 * b6 - s18 * a10) - s22 * a2) * value;
    output[10] = (s19 * a10 + (s21 * a2 - s17 * a4)) * value;
    output[11] = ((s18 * a4 - s19 * b6) - s20 * a2) * value;
    output[12] = ((b5 * s21 - a9 * s20) - a3 * s22) * value;
    output[13] = (a9 * s18 + (a1 * s22 - b5 * s17)) * value;
    output[14] = ((a3 * s17 - a9 * s19) - a1 * s21) * value;
    output[15] = (b5 * s19 + (a1 * s20 - a3 * s18)) * value;
    return true;
}

// Xbox 0x00185345
void plane_from_runtime_point_normal(float* output, const float* point, const float* normal)
{
    output[0] = normal[0];
    output[1] = normal[1];
    output[2] = normal[2];
    output[3] = -(point[0] * normal[0] + point[2] * normal[2] + point[1] * normal[1]);
}

// Xbox 0x001852A1
void normalize_runtime_graphics_plane(float* output, const float* plane)
{
    float squared = plane[2] * plane[2] + plane[1] * plane[1] + plane[0] * plane[0];
    if (!runtime_graphics_scalars_equal(squared, 1.f))
    {
        if (squared <= std::bit_cast<float>(0x2EDBE6FEu)) // DAT_00203738.
        {
            output[0] = 0.f;
            output[1] = 0.f;
            output[2] = 0.f;
            squared = 0.f;
        }
        else
        {
            squared = 1.f / std::sqrt(squared);
            output[0] = squared * plane[0];
            output[1] = squared * plane[1];
            output[2] = squared * plane[2];
            squared = squared * plane[3];
        }
        output[3] = squared;
    }
    else if (output != plane)
    {
        output[0] = plane[0];
        output[1] = plane[1];
        output[2] = plane[2];
        output[3] = plane[3];
    }
}

// Xbox 0x0018537D
void reflect_runtime_graphics_matrix(float* output, const float* plane)
{
    float normalized[4];
    normalize_runtime_graphics_plane(normalized, plane);
    const float a = normalized[0] * -2.f; // DAT_0032B14C.
    const float b = normalized[1] * -2.f;
    const float c = normalized[2] * -2.f;
    output[0] = normalized[0] * a + 1.f;
    output[1] = b * normalized[0];
    output[2] = c * normalized[0];
    output[3] = 0.f;
    output[4] = normalized[1] * a;
    output[5] = normalized[1] * b + 1.f;
    output[6] = c * normalized[1];
    output[7] = 0.f;
    output[8] = normalized[2] * a;
    output[9] = normalized[2] * b;
    output[10] = normalized[2] * c + 1.f;
    output[11] = 0.f;
    output[12] = normalized[3] * a;
    output[13] = normalized[3] * b;
    output[14] = normalized[3] * c;
    output[15] = 1.f;
}

// Xbox 0x001851AD
void look_at_runtime_graphics_matrix(float* output, const float* eye, const float* at, const float* up)
{
    Vec3 z;
    subtract_runtime_graphics_vectors(&z.x, at, eye);
    normalize_runtime_graphics_vector(z, z);
    Vec3 x;
    cross_runtime_graphics_vectors(&x.x, up, &z.x);
    normalize_runtime_graphics_vector(x, x);
    Vec3 y;
    cross_runtime_graphics_vectors(&y.x, &z.x, &x.x);
    output[0] = x.x;
    output[4] = x.y;
    output[8] = x.z;
    output[1] = y.x;
    output[5] = y.y;
    output[9] = y.z;
    output[12] = -(x.y * eye[1] + x.z * eye[2] + x.x * eye[0]);
    output[2] = z.x;
    output[6] = z.y;
    output[10] = z.z;
    output[13] = -(y.y * eye[1] + y.z * eye[2] + y.x * eye[0]);
    output[14] = -(z.y * eye[1] + z.z * eye[2] + z.x * eye[0]);
    output[3] = 0.f;
    output[7] = 0.f;
    output[11] = 0.f;
    output[15] = 1.f;
}

// Xbox 0x00184E2A
void perspective_runtime_graphics_matrix(float* output, float field_of_view, float aspect, float near_plane,
    float far_plane)
{
    const long double cosine = std::cos(static_cast<long double>(field_of_view * 0.5f));
    const long double sine = std::sin(static_cast<long double>(field_of_view * 0.5f));
    output[0] = (float(cosine) / float(sine)) / aspect;
    output[1] = 0.f;
    output[2] = 0.f;
    output[3] = 0.f;
    output[4] = 0.f;
    output[5] = float(cosine) / float(sine);
    output[6] = 0.f;
    output[7] = 0.f;
    output[8] = 0.f;
    output[9] = 0.f;
    const float depth = far_plane / (far_plane - near_plane);
    output[10] = depth;
    output[11] = 1.f;
    output[12] = 0.f;
    output[13] = 0.f;
    output[14] = -(depth * near_plane);
    output[15] = 0.f;
}

// Xbox 0x001706E0
long double runtime_arc_sine(long double value)
{
    // 0x001706FD: FPATAN of x over sqrt((1 + x)(1 - x)); |x| == 1 loads +-pi/2.
    return std::atan2(value, std::sqrt((1.L + value) * (1.L - value)));
}
}
