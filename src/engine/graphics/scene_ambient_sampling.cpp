#include "scene_ambient_sampling.h"
#include "../frontend/frontend_slots.h"
#include "../audio/listener_settings.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
template<class T>
T grid_value(const u8* grid, u32 offset)
{
    T value;
    std::memcpy(&value, grid + offset, sizeof(value));
    return value;
}
i32 grid_index(i32 index, i32 limit)
{
    if (index < 0)
    {
        index = 0;
    }
    else if (limit < index)
    {
        index = limit;
    }
    return index;
}
u32 grid_color(const u8* grid, u32 index)
{
    const u8* pixel = grid + 0x2C + index * 3;
    return 0xFF000000u | pixel[0] | (u32(pixel[1]) << 8) | (u32(pixel[2]) << 16);
}
u8 saturated_channel(u32 first, u32 second)
{
    const u16 sum = static_cast<u16>(first + second);
    u8 value = 0xFF;
    if (sum < 0x100)
    {
        value = static_cast<u8>(sum);
    }
    return value;
}
}
// Xbox 0x001018A0
u32 add_saturated_runtime_colors(u32 first, u32 second)
{
    const u32 red = saturated_channel(first & 0xFF, second & 0xFF);
    const u32 green = saturated_channel((first >> 8) & 0xFF, (second >> 8) & 0xFF);
    const u32 blue = saturated_channel((first >> 16) & 0xFF, (second >> 16) & 0xFF);
    const u32 alpha = saturated_channel(first >> 24, second >> 24);
    return red | (green << 8) | (blue << 16) | (alpha << 24);
}
// Xbox 0x000524A0
u32 sample_runtime_environment_grid(const u8* grid, const Vec3& position)
{
    const long double x = (static_cast<long double>(position.x) - grid_value<float>(grid, 0))
        / grid_value<float>(grid, 0x18);
    const i32 raw_x = std::bit_cast<i32>(static_cast<u32>(truncate_runtime_float(x)));
    const i32 next_x = std::bit_cast<i32>(static_cast<u32>(raw_x) + 1);
    long double integral_x = raw_x;
    if (raw_x < 0)
    {
        integral_x += std::bit_cast<float>(0x4F800000u);
    }
    const float fraction_x = static_cast<float>(x - integral_x);
    const i32 width = grid_value<u16>(grid, 0x24);
    const i32 left = grid_index(raw_x, width - 1);
    const i32 right = grid_index(next_x, width - 1);
    const long double z = (static_cast<long double>(position.z) - grid_value<float>(grid, 8))
        / grid_value<float>(grid, 0x20);
    const i32 raw_z = std::bit_cast<i32>(static_cast<u32>(truncate_runtime_float(z)));
    const i32 next_z = std::bit_cast<i32>(static_cast<u32>(raw_z) + 1);
    long double integral_z = raw_z;
    if (raw_z < 0)
    {
        integral_z += std::bit_cast<float>(0x4F800000u);
    }
    const float fraction_z = static_cast<float>(z - integral_z);
    const i32 depth = grid_value<u16>(grid, 0x28);
    const i32 top = grid_index(raw_z, depth - 1);
    const i32 bottom = grid_index(next_z, depth - 1);
    const u32 top_row = static_cast<u32>(top) * static_cast<u32>(width);
    const u32 bottom_row = static_cast<u32>(bottom) * static_cast<u32>(width);
    const u32 top_color = frontend_detail::menu_color(grid_color(grid, top_row + static_cast<u32>(right)),
        grid_color(grid, top_row + static_cast<u32>(left)), fraction_x);
    const u32 bottom_color = frontend_detail::menu_color(grid_color(grid, bottom_row + static_cast<u32>(right)),
        grid_color(grid, bottom_row + static_cast<u32>(left)), fraction_x);
    return frontend_detail::menu_color(bottom_color, top_color, fraction_z);
}
// Xbox 0x000FEE90
void sample_runtime_environment_color(const Vec3& position, float* output,
    const SceneAmbientContext& context)
{
    const i32 count = context.grid_count;
    u32 color = 0xFF000000;
    if (count != 0)
    {
        i32 index = 0;
        if (0 < count)
        {
            do
            {
                color = add_saturated_runtime_colors(color,
                    sample_runtime_environment_grid(context.grids[index], position));
                ++index;
            }
            while (index < count);
        }
        constexpr float reciprocal = std::bit_cast<float>(0x3B808081u);
        output[0] = static_cast<float>(color & 0xFF) * reciprocal;
        output[1] = static_cast<float>((color >> 8) & 0xFF) * reciprocal;
        output[2] = static_cast<float>((color >> 16) & 0xFF) * reciprocal;
        output[3] = 1.f;
        return;
    }
    output[0] = 1.f;
    output[1] = 1.f;
    output[2] = 1.f;
    output[3] = 1.f;
}
// Xbox 0x000FD850
void sample_runtime_scene_ambient(const Vec3& position, float* output,
    const SceneAmbientContext& context)
{
    if (context.override_count != 0)
    {
        output[0] = context.override_color[0];
        output[1] = context.override_color[1];
        output[2] = context.override_color[2];
        output[3] = context.override_color[3];
        return;
    }
    sample_runtime_environment_color(position, output, context);
    output[0] *= context.scale[0];
    output[1] *= context.scale[1];
    output[2] = context.scale[2] * output[2];
    if (context.points_enabled != 0)
    {
        i32 index = 0;
        while (context.points[index].active == 0)
        {
            ++index;
            if (index >= 32)
            {
                return;
            }
        }
        while (index >= 0)
        {
            const Vec3& point = context.points[index].position;
            const float x = position.x - point.x;
            const float y = position.y - point.y;
            const float z = position.z - point.z;
            const float distance = (x * x + y * y) + z * z;
            if (0.f < distance)
            {
                const float contribution = (1.f / distance) * 5000.f;
                output[0] = contribution + output[0];
                output[1] += contribution;
                output[2] = contribution + output[2];
                output[3] += contribution;
            }
            ++index;
            if (index >= 32)
            {
                return;
            }
            while (context.points[index].active == 0)
            {
                ++index;
                if (index >= 32)
                {
                    return;
                }
            }
        }
    }
}
// Xbox 0x0015ACA0
void set_runtime_environment_world_space(u8 enabled, u8& retained)
{
    retained = enabled;
}
}
