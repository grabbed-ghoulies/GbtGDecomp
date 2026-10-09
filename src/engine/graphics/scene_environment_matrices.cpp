#include "scene_environment_matrices.h"
#include "../camera/display_view.h"
#include "../camera/display_setup_math.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00153EB0
void make_environment_direction_rotation(const Vec3& first, const Vec3& second, Quat& rotation)
{
    const float sum = static_cast<float>(((static_cast<long double>(first.x) * second.x
        + static_cast<long double>(second.z) * first.z)
        + static_cast<long double>(second.y) * first.y) + 1.f);
    if (!(animation_absolute(sum) < std::bit_cast<float>(0x358637BDu)))
    {
        const float x = second.z * first.y - second.y * first.z;
        const float y = second.x * first.z - first.x * second.z;
        const float z = first.x * second.y - second.x * first.y;
        const float factor = static_cast<float>(retail_reciprocal_sqrt(sum))
            * std::bit_cast<float>(0x3F3504F3u);
        rotation.x = factor * x;
        rotation.y = factor * y;
        rotation.z = factor * z;
        rotation.w = factor * sum;
        return;
    }
    if (.5f < animation_absolute(first.x))
    {
        rotation.x = first.z;
        rotation.y = 0.f;
        rotation.z = 0.f - first.x;
        rotation.w = 0.f;
        return;
    }
    rotation.x = 0.f;
    rotation.y = 0.f - first.z;
    rotation.z = first.y;
    rotation.w = 0.f;
}
// Xbox 0x000FCDF0
void build_runtime_environment_matrices(float* primary, float* secondary,
    RuntimeSceneLightDirection& lighting,
    const std::function<void(Vec3&, const Vec3&)>& normalize)
{
    const Vec3 camera = entity_detail::g_display_view->forward;
    const Vec3 light = runtime_scene_light_direction(lighting, normalize);
    Vec3 half_light{(light.x + camera.x) * .5f,
        (light.y + camera.y) * .5f, (light.z + camera.z) * .5f};
    float squared = (half_light.z * half_light.z + half_light.y * half_light.y)
        + half_light.x * half_light.x;
    if (squared != 0.f)
    {
        const long double factor = retail_reciprocal_sqrt(squared);
        half_light.x = static_cast<float>(half_light.x * factor);
        half_light.y = static_cast<float>(half_light.y * factor);
        half_light.z = static_cast<float>(half_light.z * factor);
    }
    const Vec3 reference{-1.f, 0.f, 0.f}; // DAT_0042A554..55C.
    Vec3 half_reference{camera.x + reference.x, camera.y + reference.y, camera.z + reference.z};
    squared = (half_reference.z * half_reference.z + half_reference.y * half_reference.y)
        + half_reference.x * half_reference.x;
    if (!(squared < std::bit_cast<float>(0x3A83126Fu)))
    {
        half_reference.x *= .5f;
        half_reference.y *= .5f;
        half_reference.z *= .5f;
        squared = (half_reference.z * half_reference.z + half_reference.y * half_reference.y)
            + half_reference.x * half_reference.x;
        if (squared != 0.f)
        {
            const long double factor = retail_reciprocal_sqrt(squared);
            half_reference.x = static_cast<float>(half_reference.x * factor);
            half_reference.y = static_cast<float>(half_reference.y * factor);
            half_reference.z = static_cast<float>(half_reference.z * factor);
        }
    }
    else
    {
        half_reference = reference;
    }
    Quat first_rotation, second_rotation;
    make_environment_direction_rotation(half_light, camera, first_rotation);
    make_environment_direction_rotation(half_reference, camera, second_rotation);
    std::array<float, 12> first_matrix, second_matrix;
    make_display_quaternion_matrix(first_rotation, first_matrix.data());
    make_display_quaternion_matrix(second_rotation, second_matrix.data());
    const std::array<float, 12> scale
    {
        .5f, 0.f, 0.f, 0.f,
        0.f, -.5f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f
    };
    multiply_affine_matrices(first_matrix.data(), scale.data(), first_matrix.data());
    multiply_affine_matrices(second_matrix.data(), scale.data(), second_matrix.data());
    const std::array<float, 12> bias
    {
        1.f, 0.f, 0.f, .5f,
        0.f, 1.f, 0.f, .5f,
        0.f, 0.f, 1.f, 1.f
    };
    multiply_affine_matrices(first_matrix.data(), bias.data(), first_matrix.data());
    multiply_affine_matrices(second_matrix.data(), bias.data(), second_matrix.data());
    primary[0] = first_matrix[0];
    primary[1] = first_matrix[1];
    primary[2] = first_matrix[2];
    primary[3] = first_matrix[3];
    primary[4] = first_matrix[4];
    primary[5] = first_matrix[5];
    primary[6] = first_matrix[6];
    primary[7] = first_matrix[7];
    primary[8] = first_matrix[8];
    primary[9] = first_matrix[9];
    primary[10] = first_matrix[10];
    primary[11] = first_matrix[11];
    primary[12] = 0.f;
    primary[13] = 0.f;
    primary[14] = 0.f;
    primary[15] = 1.f;
    secondary[0] = second_matrix[0];
    secondary[1] = second_matrix[1];
    secondary[2] = second_matrix[2];
    secondary[3] = second_matrix[3];
    secondary[4] = second_matrix[4];
    secondary[5] = second_matrix[5];
    secondary[6] = second_matrix[6];
    secondary[7] = second_matrix[7];
    secondary[8] = second_matrix[8];
    secondary[9] = second_matrix[9];
    secondary[10] = second_matrix[10];
    secondary[11] = second_matrix[11];
    secondary[12] = 0.f;
    secondary[13] = 0.f;
    secondary[14] = 0.f;
    secondary[15] = 1.f;
}
}
