#include "scene_environment_setup.h"
#include "scene_environment_matrices.h"
#include "scene_render_helpers.h"
#include "../camera/display_view.h"
#include <bit>

namespace gbtg
{
// Xbox 0x000FD3F0
void build_runtime_scene_lights(const Vec3& position, SceneLightConstructionContext& c)
{
    SceneEnvironmentSetupContext& environment = c.environment;
    RuntimeModelRenderContext& renderer = environment.upload.renderer;
    set_runtime_scene_color(c.scene_color[0], c.scene_color[1], c.scene_color[2],
        1.f, c.retained_color, renderer);
    clear_runtime_lights(renderer);
    float intensity = c.override_intensity;
    if (environment.ambient.override_count == 0)
    {
        intensity = c.directional_intensity;
    }
    set_runtime_light_type(c.directional, 1);
    set_runtime_light_color(c.directional, intensity, intensity, intensity, 1.f);
    Vec3& direction = runtime_scene_light_direction(environment.direction,
        environment.upload.boundaries.normalize);
    set_runtime_light_direction(c.directional, direction);
    add_runtime_light(renderer, c.directional);
    std::int8_t index = 0;
    do
    {
        if (c.local_lights[index] != nullptr)
        {
            goto lights_selected;
        }
        ++index;
    } while (index < 40);
    index = -1;
lights_selected:
    if (index != -1)
    {
        u32 slot = 0;
        do
        {
            if (slot >= 2)
            {
                break;
            }
            RuntimeRenderLight& light = c.temporary[slot];
            const SceneLocalLightView& source = *c.local_lights[index];
            set_runtime_light_type(light, 0xE);
            set_runtime_light_position(light, source.position);
            set_runtime_light_range(light, source.radius * .5f, source.radius);
            const u32 color = source.color;
            const float reciprocal = std::bit_cast<float>(0x3B808081U);
            set_runtime_light_color(light, static_cast<float>(color & 0xFF) * reciprocal,
                static_cast<float>((color >> 8) & 0xFF) * reciprocal,
                static_cast<float>((color >> 16) & 0xFF) * reciprocal,
                static_cast<float>(color >> 24) * reciprocal);
            add_runtime_light(renderer, light);
            do
            {
                ++index;
                if (index > 39)
                {
                    index = -1;
                    break;
                }
            } while (c.local_lights[index] == nullptr);
            ++slot;
        } while (index != -1);
    }
    configure_runtime_scene_environment(position, 1, environment);
}
// Xbox 0x000FD2A0
void configure_runtime_scene_environment(const Vec3& position, u8 enabled,
    SceneEnvironmentSetupContext& c)
{
    RuntimeModelRenderContext& renderer = c.upload.renderer;
    configure_runtime_scene_fog(renderer, c.fog);
    const auto& view = entity_detail::g_display_view->transform;
    std::array<float, 16> matrix
    {
        view[0], view[4], view[8], 0.f,
        view[1], view[5], view[9], 0.f,
        view[2], view[6], view[10], 0.f,
        view[3], view[7], view[11], 1.f
    };
    set_runtime_lighting_enabled(renderer, enabled);
    set_runtime_lights_world_space(renderer, 0);
    upload_runtime_scene_lights(matrix.data(), c.upload);
    const std::array<float, 4>* base = &c.override_base_color;
    if (c.ambient.override_count == 0)
    {
        base = &c.base_color;
    }
    set_runtime_render_base_color(renderer, *base);
    std::array<float, 4> ambient;
    sample_runtime_scene_ambient(position, ambient.data(), c.ambient);
    set_runtime_render_ambient(renderer, ambient);
    std::array<float, 16> primary, secondary;
    build_runtime_environment_matrices(primary.data(), secondary.data(), c.direction,
        c.upload.boundaries.normalize);
    const auto primary_words = std::bit_cast<std::array<u32, 16>>(primary);
    const auto secondary_words = std::bit_cast<std::array<u32, 16>>(secondary);
    copy_runtime_primary_environment_matrix(primary_words.data(), c.primary);
    copy_runtime_secondary_environment_matrix(secondary_words.data(), c.secondary);
}
}
