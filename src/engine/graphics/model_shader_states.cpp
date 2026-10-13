#include "model_shader_states.h"

namespace gbtg
{
// Xbox 0x0015B380
void set_runtime_model_opaque_states(RuntimeModelShaderStates& states)
{
    states.simple(0x40354, 0x203);
    states.depth_function = 0x203;
    states.z_enable(1);
    states.simple(0x4035C, 1);
    states.depth_write = 1;
    states.simple(0x40304, 0);
    states.blend = 0;
    states.simple(0x40300, 0);
    states.alpha_test = 0;
}

// Xbox 0x0015B3F0
void set_runtime_model_alpha_depth_states(RuntimeModelShaderStates& states)
{
    states.simple(0x40354, 0x203);
    states.depth_function = 0x203;
    states.z_enable(1);
    states.simple(0x4035C, 1);
    states.depth_write = 1;
    states.simple(0x40304, 1);
    states.blend = 1;
    states.simple(0x40300, 0);
    states.alpha_test = 0;
    states.simple(0x40350, 0x8006);
    states.blend_operation = 0x8006;
    states.simple(0x40344, 0x302);
    states.source_blend = 0x302;
    states.simple(0x40348, 0x303);
    states.destination_blend = 0x303;
}

// Xbox 0x0015B4B0
void set_runtime_model_alpha_states(RuntimeModelShaderStates& states)
{
    states.simple(0x40354, 0x203);
    states.depth_function = 0x203;
    states.z_enable(1);
    states.simple(0x4035C, 0);
    states.depth_write = 0;
    states.simple(0x40304, 1);
    states.blend = 1;
    states.simple(0x40300, 0);
    states.alpha_test = 0;
    states.simple(0x40350, 0x8006);
    states.blend_operation = 0x8006;
    states.simple(0x40344, 0x302);
    states.source_blend = 0x302;
    states.simple(0x40348, 0x303);
    states.destination_blend = 0x303;
    states.simple(0x40340, 5);
    states.alpha_function = 5;
}

// Xbox 0x0015B580
void set_runtime_model_additive_states(RuntimeModelShaderStates& states)
{
    set_runtime_model_alpha_states(states);
    states.simple(0x40350, 0x8006);
    states.blend_operation = 0x8006;
    states.simple(0x40344, 0x302);
    states.source_blend = 0x302;
    states.simple(0x40348, 1);
    states.destination_blend = 1;
}

// Xbox 0x0015B660
void bind_runtime_model_shader_callback(std::function<void(u32)> callback,
    RuntimeModelShaderDraw& context)
{
    context.post_shader = std::move(callback);
}

// Xbox 0x0015B5E0
void bind_runtime_model_shader(u32 shader, RuntimeModelShaderDraw& context)
{
    switch (context.tree.image.word(shader, 0))
    {
    case 0:
        set_runtime_model_opaque_states(context.states);
        break;
    case 1:
        set_runtime_model_alpha_states(context.states);
        break;
    case 2:
        set_runtime_model_alpha_depth_states(context.states);
        break;
    case 3:
        set_runtime_model_additive_states(context.states);
        break;
    }
    context.pixel_shader(context.tree.image.word(shader, 0xC));
    if (context.post_shader)
    {
        context.post_shader(shader);
    }
}

// Xbox 0x0015B670
u32 runtime_model_shader_texture_stage(u32 shader, const RuntimeRelocationImage& image)
{
    return image.word(image.word(shader, 4), 0xD4) & 0xF;
}

// Xbox 0x00157CE0
void dispatch_runtime_model_shader(u32 node, RuntimeModelShaderDraw& context)
{
    bind_runtime_model_shader(context.tree.image.word(node, 0x20), context);
    queue_runtime_model_group(node, context.tree);
}

namespace
{
template<bool ShadowFirst>
void write_combiner_state(i32 index, u32 value, RuntimeModelShaderCombiner& context)
{
    if (index < 0x5C)
    {
        context.draw.states.simple(context.draw.tree.image.word(0x0020A368, u32(index) * 4), value);
        context.shadows[index] = value;
    }
    else if (index < 0x88)
    {
        const u32 dirty = context.dirty | context.draw.tree.image.word(0x0020A148, u32(index) * 4);
        if constexpr (ShadowFirst)
        {
            context.shadows[index] = value;
            context.dirty = dirty;
        }
        else
        {
            context.dirty = dirty;
            context.shadows[index] = value;
        }
    }
    else if (index == 0x88)
    {
        context.specialized_state(0x00171020, value);
    }

    else if (index == 0x89)
    {
        context.specialized_state(0x00171870, value);
    }

    else if (index == 0x8a)
    {
        context.specialized_state(0x001713d0, value);
    }

    else if (index == 0x8b)
    {
        context.specialized_state(0x00171740, value);
    }

    else if (index == 0x8c)
    {
        context.specialized_state(0x00171790, value);
    }

    else if (index == 0x8d)
    {
        context.specialized_state(0x001717f0, value);
    }

    else if (index == 0x8e)
    {
        context.specialized_state(0x001714d0, value);
    }

    else if (index == 0x8f)
    {
        context.specialized_state(0x001722a0, value);
    }

    else if (index == 0x90)
    {
        context.specialized_state(0x00172330, value);
    }

    else if (index == 0x91)
    {
        context.specialized_state(0x001723c0, value);
    }

    else if (index == 0x93)
    {
        context.specialized_state(0x00171420, value);
    }

    else if (index == 0x92)
    {
        context.specialized_state(0x00171490, value);
    }

    else if (index == 0x94)
    {
        context.specialized_state(0x00171510, value);
    }

    else if (index == 0x95)
    {
        context.specialized_state(0x00171660, value);
    }

    else if (index == 0x96)
    {
        context.specialized_state(0x001716e0, value);
    }

    else if (index == 0x97)
    {
        context.specialized_state(0x00171350, value);
    }

    else if (index == 0x98)
    {
        context.specialized_state(0x00172600, value);
    }

    else if (index == 0x99)
    {
        context.specialized_state(0x00172680, value);
    }

    else if (index == 0x9a)
    {
        context.specialized_state(0x001725a0, value);
    }

    else if (index == 0x9b)
    {
        context.specialized_state(0x001725d0, value);
    }

    else if (index == 0x9c)
    {
        context.specialized_state(0x00171390, value);
    }

    else if (index == 0x9d)
    {
        context.specialized_state(0x00171570, value);
    }

    else if (index == 0x9e)
    {
        context.specialized_state(0x001726d0, value);
    }

    else if (index == 0x9f)
    {
        context.specialized_state(0x001715e0, value);
    }

    else if (index == 0xa0)
    {
        context.specialized_state(0x00172430, value);
    }

    else if (index == 0xa1)
    {
        context.specialized_state(0x00172460, value);
    }

    else if (index == 0xa2)
    {
        context.specialized_state(0x001724d0, value);
    }

    else if (index == 0xa3)
    {
        context.specialized_state(0x00172540, value);
    }

    else if (index == 0xa4)
    {
        context.specialized_state(0x00172560, value);
    }

    else if (index == 0xa5)
    {
        context.specialized_state(0x00172580, value);
    }
}
}

// Xbox 0x000F7420
void configure_runtime_model_texture_combiner(u32 flags, i32 stage, RuntimeModelShaderCombiner& context)
{
    const u32 control = (u32(stage) + 1) | (flags << 8);
    context.draw.states.simple(0x41E60, control);
    context.control = control;
    write_combiner_state<false>(i32(u32(stage) + 0x22), 0xCC22C1C2, context);
    write_combiner_state<false>(i32(u32(stage) + 0x2D), 0xC00, context);
    write_combiner_state<false>(stage, 0xDC111010, context);
    write_combiner_state<true>(i32(u32(stage) + 0x1A), 0xC0, context);
}

// Xbox 0x0005E0C0
void set_runtime_model_alpha_test_states(RuntimeModelShaderStates& states)
{
    states.simple(0x40300, 1);
    states.alpha_test = 1;
    states.simple(0x40340, 5);
    states.alpha_function = 5;
    states.simple(0x40304, 1);
    states.blend = 1;
}

// Xbox 0x000F84A0
void apply_runtime_model_shader_overrides(RuntimeModelInstance& model, u32 shader,
    RuntimeModelShaderCombiner& context)
{
    if (model.state_1c0 != 0)
    {
        if (u8(model.state_1c8 >> 24) != 0 && u8(model.state_1c4 >> 24) < 0xFF)
        {
            context.draw.states.simple(0x40354, 0x203);
            context.draw.states.depth_function = 0x203;
            context.draw.states.simple(0x4035C, 0);
            context.draw.states.depth_write = 0;
            context.draw.states.z_enable(1);
            set_runtime_model_alpha_test_states(context.draw.states);
        }
        const i32 stage = i32(runtime_model_shader_texture_stage(shader, context.draw.tree.image));
        context.texture_stage = u32(stage);
        if (stage < 7)
        {
            const auto& image = context.draw.tree.image;
            configure_runtime_model_texture_combiner(image.word(image.word(shader, 4), 0xD4) >> 8, stage, context);
            return;
        }
        context.texture_stage = 0;
    }
}

// Xbox 0x000F9B40
void post_runtime_model_shader(u32 shader, RuntimeModelShaderCombiner& context)
{
    auto& renderer = context.draw.tree.renderer;
    if (u8(renderer.rendering) != 0)
    {
        context.draw.states.simple(0x40354, 0x207);
        context.draw.states.depth_function = 0x207;
        context.draw.states.simple(0x4035C, 0);
        context.draw.states.depth_write = 0;
        context.draw.states.z_enable(1);
    }
    apply_runtime_model_shader_overrides(*renderer.active_model, shader, context);
}

// Xbox 0x000F7FA0
void set_runtime_model_stage_colors(u32 first, u32 second, RuntimeModelShaderCombiner& context)
{
    // Both colours swap their red and blue bytes (ARGB to ABGR).
    const auto swap = [](u32 color)
    {
        return (((color >> 24) << 8 | (color & 0xFF)) << 8 | ((color >> 8) & 0xFF)) << 8 | ((color >> 16) & 0xFF);
    };
    write_combiner_state<false>(i32(context.texture_stage + 10), swap(first), context);
    write_combiner_state<true>(i32(context.texture_stage + 0x12), swap(second), context);
}

// Xbox 0x000F9B90
void apply_runtime_model_material_colors(RuntimeModelShaderCombiner& context)
{
    if (context.texture_stage != 0)
    {
        const RuntimeModelInstance& model = *context.draw.tree.renderer.active_model;
        if (model.state_1c0 != 0)
        {
            set_runtime_model_stage_colors(model.state_1c4, model.state_1c8, context);
        }
    }
}
}
