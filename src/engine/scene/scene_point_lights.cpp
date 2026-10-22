#include "scene_point_lights.h"

namespace gbtg
{
// Xbox 0x00123560
void reset_runtime_scene_point_lights(std::span<const u8> background_model, RuntimeScenePointLights& lights)
{
    for (auto& point : lights.points)
    {
        point.active = 0;
    }
    lights.count = 0;
    lights.free = 0;
    if (background_model.empty())
    {
        lights.enabled = 0;
        lights.unk_fc = 0;
        return;
    }
    const auto& cpu = background_model;
    const u32 header = read_u32(cpu, 0x10); // Model +0x10.
    lights.enabled = read_u32(cpu, header);
    const u32 extra = read_u32(cpu, header + 0x10);
    const u32 count = read_u32(cpu, 4);
    const u32 table = read_u32(cpu, 0);
    u32 found = 0;
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(cpu, table + index * 8) == 0x11)
        {
            const u32 block = read_u32(cpu, table + index * 8 + 4);
            if (block != 0)
            {
                found = read_u32(cpu, block + 0xC);
            }
            break;
        }
    }
    if (extra != 0 && found != 0)
    {
        lights.unk_100 = cpu[extra + 1];
        lights.unk_fc = found;
        return;
    }
    lights.unk_fc = 0;
}

// Xbox 0x00123520
void find_free_runtime_scene_point_light(RuntimeScenePointLights& lights)
{
    while (lights.free < 32)
    {
        if (lights.points.at(size_t(lights.free)).active == 0)
        {
            return;
        }
        ++lights.free;
    }
    lights.free = -1;
}

// Xbox 0x00123720, ECX position
i32 add_runtime_scene_point_light(const Vec3& position, u32 color, u32 second_color, float unk_14, float unk_18,
    float unk_1C, float unk_20, u32 unk_24, RuntimeScenePointLights& lights)
{
    const i32 index = lights.free;
    if (lights.enabled == 0 || lights.count >= 32 || index < 0)
    {
        return -1;
    }
    auto& point = lights.points[size_t(index)];
    point.position = position;
    point.unk_14 = unk_14;
    point.unk_1C = unk_1C;
    point.color = color;
    point.unk_18 = unk_18;
    point.second_color = second_color;
    point.unk_20 = unk_20;
    point.unk_24 = unk_24;
    point.active = 1;
    find_free_runtime_scene_point_light(lights); // 0x00123520
    ++lights.count;
    return index;
}

void release_runtime_scene_point_light(i32 index, RuntimeScenePointLights& lights)
{
    if (index < 0)
    {
        return;
    }
    lights.points.at(size_t(index)).active = 0;
    --lights.count;
    if (index < lights.free)
    {
        lights.free = index;
    }
}
}
