#include "../src/engine/camera/display_setup_math.h"
#include "../src/engine/camera/runtime_display_setup.h"
#include "gbtg/anim.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <algorithm>

namespace
{
using namespace gbtg;
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw Error(message);
    }
}
bool near(float a, float b)
{
    return std::fabs(a - b) < 0.0002f;
}
void test_math()
{
    Quat rotation{0.f, 0.f, 0.f, 2.f};
    normalize_display_quaternion(rotation, rotation);
    require(near(rotation.w, 1.f) && rotation.x == 0.f, "in-place quaternion normalization failed");
    std::array<float, 12> matrix;
    make_display_quaternion_matrix(rotation, matrix.data());
    require(near(matrix[0], 1.f) && near(matrix[5], 1.f) && near(matrix[10], 1.f),
        "quaternion view matrix was not identity");
    float first, second, third;
    extract_display_quaternion_euler(rotation, 0, first, second, third);
    require(near(first, 0.f) && near(second, 0.f) && near(third, 0.f), "identity Euler extraction failed");
    const Quat nonunit{0.f, 0.f, 0.f, 3.f};
    make_display_quaternion_matrix(nonunit, matrix.data());
    require(near(matrix[0], 1.f) && near(matrix[10], 1.f), "quaternion matrix lost norm division");
    const Quat invalid{0.f, 0.f, 0.f, 0.f};
    make_display_quaternion_matrix(invalid, matrix.data());
    require(std::isnan(matrix[0]), "zero quaternion was replaced with invented rotation");
    const std::array<float, 12> affine{1.f, 0.f, 0.f, 4.f, 0.f, 1.f, 0.f, 5.f, 0.f, 0.f, 1.f, 6.f};
    std::array<float, 18> points{1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f,
        10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f, 18.f};
    const auto original = points;
    transform_display_points(affine.data(), points.data(), points.data(), 6);
    for (size_t index = 0; index < 6; ++index)
    {
        require(points[index * 3] == original[index * 3] + 4.f
            && points[index * 3 + 1] == original[index * 3 + 1] + 5.f
            && points[index * 3 + 2] == original[index * 3 + 2] + 6.f,
            "group and remainder point transforms changed in-place inputs");
    }
    std::array<float, 16> expanded;
    expand_display_affine(affine.data(), expanded.data());
    require(expanded[12] == 4.f && expanded[13] == 5.f && expanded[14] == 6.f
        && expanded[15] == 1.f, "affine transpose/translation expansion failed");
    require(near(static_cast<float>(display_tangent(.25f)), std::tan(.25f))
        && near(static_cast<float>(display_tangent(1.2f)), std::tan(1.2f))
        && near(static_cast<float>(display_tangent(-1.2f)), std::tan(-1.2f)),
        "tangent polynomial folding failed");
    Vec3 position{0.f, 0.f, 0.f};
    apply_display_offset(0, Vec3{}, 2.f, position);
    apply_display_offset(1, Vec3{}, 3.f, position);
    apply_display_offset(2, Vec3{}, 4.f, position);
    apply_display_offset(3, Vec3{}, 5.f, position);
    require(near(position.x, 3.f) && near(position.y, 7.f) && near(position.z, 4.f),
        "camera offset dispatcher axis mapping changed");
}
void test_affine_order()
{
    const std::array<float, 12> a
    {
        1e20f, 1.f, -1e20f, 0.f,
        1e20f, 1.f, -1e20f, 0.f,
        1.f, -1e20f, 1e20f, 0.f
    };
    const std::array<float, 12> b
    {
        1.f, 1.f, 1.f, 0.f,
        1.f, 1.f, 1.f, 0.f,
        1.f, 1.f, 1.f, 0.f
    };
    const std::array<float, 12> expected
    {
        1.f, 1.f, 1.f, 0.f,
        1.f, 1.f, 1.f, 0.f,
        1.f, 1.f, 1.f, 0.f
    };
    std::array<float, 12> output;
    multiply_affine_matrices(output.data(), a.data(), b.data());
    require(output == expected, "affine retail cancellation order changed");
    auto left = a;
    multiply_affine_matrices(left.data(), left.data(), b.data());
    require(left == expected, "affine left-alias temporary failed");
    auto right = b;
    multiply_affine_matrices(right.data(), a.data(), right.data());
    require(right == expected, "affine right-alias temporary failed");
}
void test_frustum()
{
    using namespace entity_detail;
    struct RestoreDisplay
    {
        DisplayView* selected = g_display_view;
        DisplayView record = g_display_records[0];
        u8 exact = g_aspect_exact;
        ~RestoreDisplay()
        {
            g_display_view = selected;
            g_display_records[0] = record;
            g_aspect_exact = exact;
        }
    } restore;
    g_display_view = &g_display_records[0];
    auto& view = *g_display_view;
    view.aspect_mode = 0;
    g_aspect_exact = 1;
    view.field_of_view = 60.f;
    view.far_clip = 100.f;
    view.near_clip = .5f;
    view.inverse_transform = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    rebuild_display_frustum();
    std::array<float, 18> original;
    std::copy_n(view.frustum.data(), 18, original.begin());
    require(near(original[2], -100.f) && near(original[17], -.5f),
        "frustum far/near origins failed");
    std::array<float, 24> original_planes;
    std::copy_n(view.frustum.data() + 18, 24, original_planes.begin());
    for (size_t index = 0; index < 6; ++index)
    {
        const auto* plane = original_planes.data() + index * 4;
        require(near(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2], 1.f),
            "frustum normal is not normalized");
    }
    require(near(original_planes[19], 100.f) && near(original_planes[23], -.5f),
        "frustum plane offsets failed");
    view.inverse_transform[3] = 4.f;
    view.inverse_transform[7] = 5.f;
    view.inverse_transform[11] = 6.f;
    rebuild_display_frustum();
    for (size_t index = 0; index < 6; ++index)
    {
        require(near(view.frustum[index * 3], original[index * 3] + 4.f)
            && near(view.frustum[index * 3 + 1], original[index * 3 + 1] + 5.f)
            && near(view.frustum[index * 3 + 2], original[index * 3 + 2] + 6.f),
            "inverse view translation was not applied to frustum origins");
    }
}
}
void check_camera_display()
{
    test_math();
    test_affine_order();
    test_frustum();
    std::cout << "native camera math, effects and frustum passed\n";
}
