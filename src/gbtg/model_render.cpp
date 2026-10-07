#include "gbtg/model_render.hpp"
#include "gbtg/sound_runtime.hpp"

namespace gbtg
{

// Xbox 0x000FD5C0
void configure_runtime_scene_fog(RuntimeModelRenderContext& context, const RuntimeSceneFog& fog)
{
    u8 enabled;
    if (fog.disabled == 0 && fog.enabled != 0)
    {
        enabled = 1;
    }
    else
    {
        enabled = 0;
    }
    set_runtime_render_fog_enabled(context, enabled);
    set_runtime_render_fog(context, fog.start, fog.end,
        fog.color[0], fog.color[1], fog.color[2], fog.color[3]);
}

// Xbox 0x000FD9C0
Vec3& runtime_scene_light_direction(RuntimeSceneLightDirection& state,
    const std::function<void(Vec3&, const Vec3&)>& normalize)
{
    state.scratch = {};
    float pitch = state.pitch;
    float yaw = state.yaw;
    if (state.override_mode != 0)
    {
        pitch = state.override_pitch_degrees * 0.017453292f;
        yaw = state.override_yaw_degrees * 0.017453292f;
    }
    add_sound_direction(state.scratch, pitch, yaw + 3.1415927f, 1.f);
    if (state.camera_relative != 0)
    {
        AnimationMatrix inverse;
        invert_model_matrix(inverse.data(), state.camera.data());
        const Vec3 original = state.scratch;
        state.scratch.y = original.y * inverse[5] + original.z * inverse[6] + original.x * inverse[4];
        state.scratch.z = original.y * inverse[9] + original.z * inverse[10] + original.x * inverse[8];
        state.scratch.x = original.z * inverse[2] + original.x * inverse[0] + original.y * inverse[1];
        normalize(state.scratch, state.scratch);
    }
    return state.scratch;
}

// Xbox 0x001599E0
void initialize_runtime_light(RuntimeRenderLight& light)
{
    light.previous = nullptr;
    light.next = nullptr;
}

// Xbox 0x00159AC0
RuntimeRenderLight* next_runtime_light(const RuntimeRenderLight& light)
{
    return light.next;
}

// Xbox 0x00159A30
void append_runtime_light(RuntimeRenderLight& list, RuntimeRenderLight& light)
{
    RuntimeRenderLight* tail = &list;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &light;
    light.previous = tail;
    light.next = nullptr;
}

// Xbox 0x00159A60
void insert_runtime_light_before(RuntimeRenderLight& before, RuntimeRenderLight& light)
{
    for (RuntimeRenderLight* root = before.previous; root != nullptr; root = root->previous)
    {
    }
    RuntimeRenderLight* previous = before.previous;
    previous->next = &light;
    light.previous = previous;
    light.next = &before;
    before.previous = &light;
}

// Xbox 0x00158D20
void refresh_runtime_lighting_flags(RuntimeModelRenderContext& context)
{
    context.lighting_flags &= 0x80000000;
    u8 index = 0;
    RuntimeRenderLight* light = next_runtime_light(context.lights);
    while (light != nullptr)
    {
        context.lighting_flags |= light->type << ((index * 4) & 0x1F);
        ++index;
        light = next_runtime_light(*light);
    }
}

// Xbox 0x00158D80
void add_runtime_light(RuntimeModelRenderContext& context, RuntimeRenderLight& light)
{
    initialize_runtime_light(light);
    RuntimeRenderLight* current = next_runtime_light(context.lights);
    if (current != nullptr)
    {
        while (current->type <= light.type)
        {
            current = next_runtime_light(*current);
            if (current == nullptr)
            {
                append_runtime_light(context.lights, light);
                refresh_runtime_lighting_flags(context);
                return;
            }
        }
        insert_runtime_light_before(*current, light);
        if (current != nullptr)
        {
            refresh_runtime_lighting_flags(context);
            return;
        }
    }
    append_runtime_light(context.lights, light);
    refresh_runtime_lighting_flags(context);
}

// Xbox 0x001593A0
void clear_runtime_lights(RuntimeModelRenderContext& context)
{
    initialize_runtime_light(context.lights);
    context.lighting_flags &= 0x80000000;
}

// Xbox 0x00159320
void set_runtime_light_type(RuntimeRenderLight& light, u32 type)
{
    light.type = type;
}

// Xbox 0x001592A0
void set_runtime_light_color(RuntimeRenderLight& light, float red, float green, float blue, float alpha)
{
    light.color = {red, green, blue, alpha};
}

// Xbox 0x001592E0
void set_runtime_light_direction(RuntimeRenderLight& light, const Vec3& direction)
{
    light.direction = direction;
}

// Xbox 0x00159300
void set_runtime_light_position(RuntimeRenderLight& light, const Vec3& position)
{
    light.position = position;
}

// Xbox 0x00159330
void set_runtime_light_range(RuntimeRenderLight& light, float start, float end)
{
    light.range = end;
    light.range_reciprocal = 1.f / (end - start);
    light.reserved = 0;
}

// Xbox 0x00159360
void set_runtime_lighting_enabled(RuntimeModelRenderContext& context, u8 enabled)
{
    if (enabled != 0)
    {
        context.lighting_flags |= 0x80000000;
        return;
    }
    context.lighting_flags &= 0x7FFFFFFF;
}

// Xbox 0x001593C0
void set_runtime_lights_world_space(RuntimeModelRenderContext& context, u8 enabled)
{
    context.lights_in_world_space = enabled;
}

// Xbox 0x00156F30
void set_runtime_render_base_color(RuntimeModelRenderContext& context, const std::array<float, 4>& color)
{
    context.state.base_color = color;
}

// Xbox 0x00156F70
void set_runtime_render_ambient(RuntimeModelRenderContext& context, const std::array<float, 4>& color)
{
    context.state.ambient_color = color;
    for (size_t channel = 0; channel < 3; ++channel)
    {
        float& value = context.state.ambient_color[channel];
        if (!(value > 1.f)) // COMISS/JBE also takes this branch for unordered values.
        {
            if (value < 0.f)
            {
                value = 0.f;
            }
        }
        else
        {
            value = 1.f;
        }
    }
    float& alpha = context.state.ambient_color[3];
    if (1.f < alpha)
    {
        alpha = 1.f;
        return;
    }
    if (alpha < 0.f)
    {
        alpha = 0.f;
    }
}

// Xbox 0x00156EA0
void set_runtime_render_fog_enabled(RuntimeModelRenderContext& context, u8 enabled)
{
    context.state.fog_enabled = enabled;
    if (enabled == 0)
    {
        context.changed_render_states |= 0x2000;
        context.fog_render_state = 0;
    }
}

// Xbox 0x00156ED0
void set_runtime_render_fog(RuntimeModelRenderContext& context, float start, float end,
    u8 red, u8 green, u8 blue, u8 alpha)
{
    context.state.fog_reciprocal = 1.f / (end - start);
    context.state.fog_scaled_end = context.state.fog_reciprocal * end;
    context.state.fog_color = (static_cast<u32>(alpha) << 24) |
        (static_cast<u32>(red) << 16) | (static_cast<u32>(green) << 8) | blue;
}

// Xbox 0x00156DD0
RuntimeModelRenderState& runtime_render_state(RuntimeModelRenderContext& context)
{
    return context.state;
}

// Xbox 0x00156DE0
RuntimeModelRenderFrame& runtime_render_frame(RuntimeModelRenderContext& context)
{
    return context.frames[context.frame_index];
}

// Xbox 0x00156300
void set_runtime_render_material(RuntimeModelRenderContext& context, u32 values, u32 material)
{
    RuntimeModelRenderState& state = runtime_render_state(context);
    state.material_values = values;
    state.material = material;
}

// Xbox 0x001565C0
void set_runtime_render_instance(RuntimeModelRenderContext& context, RuntimeModelInstance* instance)
{
    runtime_render_state(context).instance = instance;
}

// Xbox 0x00156370
void set_runtime_render_world(RuntimeModelRenderContext& context, const RuntimeShaderMatrix* matrix)
{
    RuntimeModelRenderFrame& frame = runtime_render_frame(context);
    if (matrix != nullptr)
    {
        frame.world = *matrix;
        return;
    }
    frame.world = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

// Xbox 0x001564A0
void set_runtime_render_color(RuntimeModelRenderContext& context, u8 red, u8 green, u8 blue, u8 alpha)
{
    runtime_render_state(context).color = {red, green, blue, alpha};
}

// Xbox 0x000F94F0
void apply_runtime_model_materials(const RuntimeRelocationImage& image, u32 model_cpu,
    RuntimeModelRenderContext& context)
{
    const u32 count = image.word(model_cpu, 4);
    u32 index = 0;
    if (count != 0)
    {
        u32 record = image.word(model_cpu);
        while (image.word(record) != 7)
        {
            ++index;
            record += 8;
            if (count <= index)
            {
                return;
            }
        }
        record = image.word(image.word(model_cpu), index * 8 + 4);
        if (record != 0)
        {
            const u32 material = image.word(record);
            if (material != 0)
            {
                const u32 values = image.word(image.word(model_cpu, 0x10), 0x28);
                if (values != 0)
                {
                    set_runtime_render_material(context, image.word(values, 4), material);
                    return;
                }
                set_runtime_render_material(context, 0, material);
            }
        }
    }
}

// Xbox 0x001628B0. The retail body is RET.
void runtime_render_empty()
{
}

// Xbox 0x00156240
void draw_runtime_model_tree(const RuntimeRelocationImage& image, u32 node,
    u32 first, u32 second, void* third, RuntimeModelRenderContext& context)
{
    runtime_render_empty();
    RuntimeModelRenderState& state = runtime_render_state(context);
    state.node_descriptor = image.word(node - 0x10);
    state.draw(node, first, second, third);
}

// Xbox 0x000F9120
void draw_runtime_model_instance(RuntimeModelInstance& model, const RuntimeRelocationImage& image,
    u32 model_cpu, const AnimationMatrix& world, u32 parameter,
    RuntimeModelRenderContext& context, const RuntimeShaderMatrixBoundaries& boundaries)
{
    context.draw_parameter = parameter;
    set_runtime_render_instance(context, &model);
    ModelPoseBuffer* pose = model.alternate;
    if (pose == nullptr)
    {
        pose = model.primary;
    }
    bind_runtime_model_pose(model, pose, boundaries);
    context.world_scratch =
    {
        world[0], world[4], world[8], 0.f,
        world[1], world[5], world[9], 0.f,
        world[2], world[6], world[10], 0.f,
        world[3], world[7], world[11], 1.f
    };
    set_runtime_render_world(context, &context.world_scratch);
    context.active_model = &model;
    draw_runtime_model_tree(image, image.word(image.word(model_cpu, 0x10), 0x24),
        0, parameter, &model.tree_storage, context);
    context.rendering = 0;
}

} // namespace gbtg
