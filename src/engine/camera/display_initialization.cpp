#include "display_view.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x000F5D30
void set_display_focal_length(i32 source_aspect, float focal)
{
    using namespace entity_detail;
    constexpr std::array<float, 4> sensors
    {
        std::bit_cast<float>(0x4178B780u),
        std::bit_cast<float>(0x41C00625u),
        std::bit_cast<float>(0x41C00625u),
        0.f
    }; // DAT_0042A464, stride 12.
    if (source_aspect != 3)
    {
        const i32 mode = g_display_view->aspect_mode;
        i32 selected = g_aspect_preset;
        if (mode != 3)
        {
            selected = mode;
        }
        if (source_aspect != selected)
        {
            if (mode == 3)
            {
                g_display_view->focal = (sensors[g_aspect_preset] * focal) / sensors[source_aspect];
                return;
            }
            g_display_view->focal = (sensors[mode] * focal) / sensors[source_aspect];
            return;
        }
    }
    g_display_view->focal = focal;
}

// Xbox 0x00104EB0
void camera_euler_to_quaternion(const Vec3& angles, Quat& result)
{
    const float half_x = static_cast<float>(static_cast<long double>(angles.x) * 0.5L);
    const float sin_x = static_cast<float>(retail_sin(half_x));
    const float cos_x = static_cast<float>(retail_cos(half_x));
    const float half_y = static_cast<float>(static_cast<long double>(angles.y) * 0.5L);
    const float sin_y = static_cast<float>(retail_sin(half_y));
    const float cos_y = static_cast<float>(retail_cos(half_y));
    const float half_z = static_cast<float>(static_cast<long double>(angles.z) * 0.5L);
    const float sin_z = static_cast<float>(retail_sin(half_z));
    const float cos_z = static_cast<float>(retail_cos(half_z));
    const float yc_xs = cos_y * sin_x;
    const float ys_xc = sin_y * cos_x;
    const float ys_xs = sin_y * sin_x;
    const float yc_xc = cos_y * cos_x;
    result.y = ys_xc * cos_z - yc_xs * sin_z;
    result.x = ys_xc * sin_z + yc_xs * cos_z;
    result.z = yc_xc * sin_z - ys_xs * cos_z;
    result.w = ys_xs * sin_z + yc_xc * cos_z;
}

// Xbox 0x000F5AE0
void set_display_rotation(const Quat& rotation)
{
    using namespace entity_detail;
    g_display_view->rotation.x = rotation.x;
    g_display_view->rotation.y = rotation.y;
    g_display_view->rotation.z = rotation.z;
    g_display_view->rotation.w = rotation.w;
    g_display_view->forward.z = -1.f;
    g_display_view->forward.x = 0.f;
    g_display_view->forward.y = 0.f;
    auto& direction = g_display_view->forward;
    const float x = (rotation.w * direction.x - direction.y * rotation.z) + direction.z * rotation.y;
    const float y = (rotation.w * direction.y - direction.z * rotation.x) + direction.x * rotation.z;
    const float z = (direction.z * rotation.w - direction.x * rotation.y) + direction.y * rotation.x;
    const float w = (direction.x * rotation.x + direction.z * rotation.z) + direction.y * rotation.y;
    direction.x = ((rotation.w * x + rotation.x * w) + z * rotation.y) - rotation.z * y;
    direction.y = ((rotation.w * y + rotation.z * x) + w * rotation.y) - rotation.x * z;
    direction.z = ((rotation.w * z + rotation.z * w) + rotation.x * y) - x * rotation.y;
    const float length_squared = (direction.x * direction.x + direction.y * direction.y) + direction.z * direction.z;
    if (length_squared != 0.f)
    {
        const long double factor = engine_reciprocal_sqrt(length_squared);
        direction.x = static_cast<float>(factor * direction.x);
        direction.y = static_cast<float>(factor * direction.y);
        direction.z = static_cast<float>(factor * direction.z);
    }
}

// Xbox 0x000F5900
void initialize_display_views(u16 width, u16 height, i32& render_width, i32& render_height)
{
    using namespace entity_detail;
    i32 index = 0;
    DisplayView* record = g_display_records;
    do
    {
        g_display_view = record;
        record->flags ^= ((static_cast<u32>(index == 0) ^ record->flags) & 1);
        g_display_view->focal = -1.f;
        g_display_view->aspect_mode = 3;
        render_width = width; // DAT_0054DD84
        render_height = height; // DAT_0054DD80
        g_display_view->origin_x = 0;
        g_display_view->origin_y = 0;
        g_display_view->width = render_width;
        g_display_view->height = render_height;
        camera_fit_display_view(g_display_view->focal, g_display_view->aspect_mode,
            g_display_view->fitted_width, g_display_view->fitted_x,
            g_display_view->fitted_height, g_display_view->fitted_y,
            g_display_view->field_of_view, g_display_view->pixel_aspect, g_display_view->focal_pixels);
        g_display_view->position.z = 0.f;
        g_display_view->position.y = 0.f;
        g_display_view->position.x = 0.f;
        const Vec3 initial_angles{std::bit_cast<float>(0x40490FDBu), 0.f, 0.f}; // DAT_0042A490
        camera_euler_to_quaternion(initial_angles, g_display_view->rotation);
        set_display_rotation(g_display_view->rotation);
        g_display_view->near_clip = 1.f;
        g_display_view->far_clip = 1000.f;
        g_display_view->transform = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
        g_display_view->projection = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        ++record;
        ++index;
        g_display_view->mark = 0;
    }
    while (record < g_display_records + 4);
}
}
