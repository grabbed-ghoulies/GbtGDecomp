#include "scene_light_upload.h"
#include "scene_render_helpers.h"
#include "scene_ambient_sampling.h"
#include "../audio/listener_settings.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00170250
void store_runtime_vector4(u32* output, u32 x, u32 y, u32 z, u32 w)
{
    output[0] = x;
    output[1] = y;
    output[2] = z;
    output[3] = w;
}
// Xbox 0x00158F20
void upload_runtime_scene_lights(const float* matrix, SceneLightUploadContext& c)
{
    if (c.renderer.lights_in_world_space != 0 || matrix != nullptr)
    {
        std::array<u32, 4> zero;
        store_runtime_vector4(zero.data(), 0, 0, 0, 0x3F800000);
        c.boundaries.set_constant(191, zero.data());
        const auto retained_color = std::bit_cast<std::array<u32, 4>>(c.retained_scene_color);
        c.boundaries.set_constant(190, retained_color.data());
        auto upload_vector = [&c](u32 slot, const Vec3& value, u32 fourth)
        {
            const std::array<u32, 4> words
            {
                std::bit_cast<u32>(value.x), std::bit_cast<u32>(value.y),
                std::bit_cast<u32>(value.z), fourth
            };
            c.boundaries.set_constant(slot, words.data());
        };
        u32 slot = 189;
        RuntimeRenderLight* light = next_runtime_light(c.renderer.lights);
        while (light != nullptr)
        {
            if (light->type != 0)
            {
                std::array<u32, 4> color
                {
                    std::bit_cast<u32>(light->color[0]), std::bit_cast<u32>(light->color[1]),
                    std::bit_cast<u32>(light->color[2]), std::bit_cast<u32>(light->color[3])
                };
                c.boundaries.set_constant(slot, color.data());
                --slot;
                const u32 kind = light->type & 3;
                if (kind == 1)
                {
                    Vec3 direction{light->direction.x * -1.f,
                        light->direction.y * -1.f, light->direction.z * -1.f};
                    if (c.renderer.lights_in_world_space == 0)
                    {
                        c.boundaries.transform_normal(direction, direction, matrix);
                        c.boundaries.normalize(direction, direction);
                    }
                    upload_vector(slot, direction, 0);
                    --slot;
                }
                else if (kind == 2)
                {
                    if (c.renderer.lights_in_world_space == 0)
                    {
                        Vec3 position;
                        c.boundaries.transform_coord(position, light->position, matrix);
                        upload_vector(slot, position, 0);
                    }
                    else
                    {
                        upload_vector(slot, light->position, std::bit_cast<u32>(light->range));
                    }
                    --slot;
                }
                else if (kind == 3)
                {
                    if (c.renderer.lights_in_world_space == 0)
                    {
                        Vec3 position;
                        c.boundaries.transform_coord(position, light->position, matrix);
                        upload_vector(slot, position, 0);
                    }
                    else
                    {
                        upload_vector(slot, light->position, std::bit_cast<u32>(light->range));
                    }
                    --slot;
                    Vec3 direction{light->direction.x * -1.f,
                        light->direction.y * -1.f, light->direction.z * -1.f};
                    if (c.renderer.lights_in_world_space == 0)
                    {
                        c.boundaries.transform_normal(direction, direction, matrix);
                        c.boundaries.normalize(direction, direction);
                    }
                    upload_vector(slot, direction, 0);
                    --slot;
                    c.boundaries.set_constant(slot, c.spot_parameters.at(light).data());
                    --slot;
                }
                if ((light->type & 0xC) != 0)
                {
                    const std::array<u32, 4> range
                    {
                        std::bit_cast<u32>(light->range), std::bit_cast<u32>(light->range_reciprocal),
                        light->reserved, c.spot_parameters.at(light)[0]
                    };
                    c.boundaries.set_constant(slot, range.data());
                    --slot;
                }
            }
            light = next_runtime_light(*light);
        }
        u16 count = 0;
        light = next_runtime_light(c.renderer.lights);
        while (light != nullptr && count < 4)
        {
            if (light->type == 1)
            {
                u32* record = c.records.data() + static_cast<u32>(count) * 5;
                record[1] = std::bit_cast<u32>(light->direction.x);
                record[2] = std::bit_cast<u32>(light->direction.y);
                record[3] = std::bit_cast<u32>(light->direction.z);
                u8* bytes = reinterpret_cast<u8*>(record);
                bytes[0] = static_cast<u8>(truncate_runtime_float(static_cast<long double>(light->color[0]) * 255.f));
                bytes[1] = static_cast<u8>(truncate_runtime_float(static_cast<long double>(light->color[1]) * 255.f));
                bytes[2] = static_cast<u8>(truncate_runtime_float(static_cast<long double>(light->color[2]) * 255.f));
                bytes[3] = static_cast<u8>(truncate_runtime_float(static_cast<long double>(light->color[3]) * 255.f));
                bytes[16] = 1;
                bytes[17] = 1;
                ++count;
            }
            light = next_runtime_light(*light);
        }
        copy_runtime_directional_lights(c.records.data(), count, c.renderer);
        set_runtime_environment_world_space(c.renderer.lights_in_world_space, c.environment_world_space);
    }
}
}
