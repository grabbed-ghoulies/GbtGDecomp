#include "dialogue_draw_outer.h"
#include "../camera/runtime_display_setup.h"
#include "../cutscenes/event_runtime.h"
#include <algorithm>
#include <bit>

namespace gbtg
{
// Xbox 0x001417A0
DialogueSurfaceHandle invoke_dialogue_render_target(u8 index,
    std::span<const DialogueRenderTargetHook> hooks)
{
    const auto& hook = hooks[index];
    if (hook.function)
    {
        return hook.function(hook.first, hook.second);
    }
    return 0;
}
// Xbox 0x000629D0
void begin_runtime_dialogue_target(DialogueSurfaceHandle color, DialogueSurfaceHandle depth,
    RuntimeDialogueOuterDrawContext& c)
{
    auto& state = c.targets;
    state.back_buffer = c.d3d.back_buffer(0);
    state.depth_buffer = c.d3d.depth_buffer();
    if (color == 0)
    {
        state.unused_surface = c.d3d.surface(depth, 0);
        c.d3d.render_target(state.back_buffer, state.depth_surface);
        state.color_texture = 0;
        state.depth_texture = depth;
        state.active = 1;
        return;
    }
    state.color_surface = c.d3d.surface(color, 0);
    if (depth != 0)
    {
        state.depth_surface = c.d3d.surface(depth, 0);
        c.d3d.render_target(state.color_surface, state.depth_surface);
        state.color_texture = color;
        state.depth_texture = depth;
        state.active = 1;
        return;
    }
    c.d3d.render_target(state.color_surface, state.depth_buffer);
    state.color_texture = color;
    state.depth_texture = 0;
    state.active = 1;
}
// Xbox 0x00062A90
void end_runtime_dialogue_target(RuntimeDialogueOuterDrawContext& c)
{
    auto& state = c.targets;
    c.d3d.render_target(state.back_buffer, state.depth_buffer);
    c.d3d.release(state.back_buffer);
    c.d3d.release(state.depth_buffer);
    if (state.color_surface != 0)
    {
        c.d3d.release(state.color_surface);
        state.color_surface = state.color_texture = 0;
    }
    if (state.depth_surface != 0)
    {
        c.d3d.release(state.depth_surface);
        state.depth_surface = state.depth_texture = 0;
    }
    state.active = 0;
}
// Xbox 0x000F5DB0
void dialogue_viewport_corners(float* output, std::span<const u32, 4> viewport)
{
    output[0] = static_cast<float>(static_cast<i32>(viewport[0]));
    output[1] = static_cast<float>(static_cast<i32>(viewport[1]));
    output[2] = static_cast<float>(static_cast<i32>(viewport[0]));
    output[3] = static_cast<float>(static_cast<i32>(viewport[3] + viewport[1]));
    output[4] = static_cast<float>(static_cast<i32>(viewport[2] + viewport[0]));
    output[5] = static_cast<float>(static_cast<i32>(viewport[3] + viewport[1]));
    output[6] = static_cast<float>(static_cast<i32>(viewport[2] + viewport[0]));
    output[7] = static_cast<float>(static_cast<i32>(viewport[1]));
}
// Xbox 0x00061250
void draw_runtime_dialogue_lines(const float* positions, float width, u32 color,
    RuntimeDialogueOuterDrawContext& c)
{
    c.d3d.line_width(width);
    c.draw.d3d.vertex_shader(c.line_vertex_shader);
    if (c.line_shader_constants != 0)
    {
        c.draw.d3d.constants_not_inline(0x60, 0xC);
    }
    c.draw.d3d.pixel_shader(c.line_pixel_shader);
    const float channels[4]{
        static_cast<float>(color & 255) * (1.f / 255.f),
        static_cast<float>((color >> 8) & 255) * (1.f / 255.f),
        static_cast<float>((color >> 16) & 255) * (1.f / 255.f),
        static_cast<float>(color >> 24) * (1.f / 255.f)};
    c.d3d.pixel_constants(0, channels, 1);
    u32* push = c.draw.d3d.begin_push(0xD);
    push[0] = 0x417FC;
    push[1] = 3;
    push[2] = 0x40201818;
    for (u32 i = 0; i != 8; ++i)
    {
        push[i + 3] = std::bit_cast<u32>(positions[i]);
    }
    push[11] = 0x417FC;
    push[12] = 0;
    c.draw.d3d.end_push(push + 13);
}
// Xbox 0x000619D0
void draw_runtime_dialogue_border(float width, u32 color,
    RuntimeDialogueOuterDrawContext& c)
{
    std::array<float, 8> positions;
    dialogue_viewport_corners(positions.data(), c.draw.fitted_viewport);
    draw_runtime_dialogue_lines(positions.data(), width, color, c);
    c.line_retained = 0;
}
// Xbox 0x0005CE30
void draw_runtime_dialogue_sprite(const u8* texture, const float* position, const float* scale,
    float angle, u32 color, i32 frame, RuntimeDialogueDrawContext& c)
{
    begin_dialogue_sprite_draw(c);
    draw_dialogue_texture(frame, 0, const_cast<u8*>(texture), scale, nullptr,
        angle, color, position, c);
    --c.override_count;
    if (c.override_count == 0)
    {
        c.fog_disabled = 0;
    }
}
// Xbox 0x001221B0
void queue_runtime_dialogue_sprite(const float* scale, const float* position,
    const u8* texture, u32 color, u8 frame, float angle,
    RuntimeDialogueSpriteQueue& queue)
{
    auto& row = queue.records[queue.count];
    row.texture = texture;
    row.position = {position[0], position[1]};
    row.scale = {scale[0], scale[1]};
    row.angle = angle;
    row.color = color;
    row.frame = frame;
    ++queue.count;
}
// Xbox 0x00122230
void draw_queued_runtime_dialogue_sprites(RuntimeDialogueSpriteQueue& queue,
    RuntimeDialogueDrawContext& c)
{
    begin_dialogue_sprite_draw(c);
    i32 index = 0;
    if (0 < queue.count)
    {
        do
        {
            auto& row = queue.records[index];
            draw_dialogue_texture(row.frame, 0, const_cast<u8*>(row.texture),
                row.scale.data(), nullptr, row.angle, row.color, row.position.data(), c);
            ++index;
        }
        while (index < queue.count);
    }
    --c.override_count;
    queue.count = 0;
    if (c.override_count == 0)
    {
        c.fog_disabled = 0;
    }
}
// Xbox 0x00142720
void draw_runtime_dialogue_widget(RuntimeDialogueWidget& widget, i32 immediate,
    RuntimeDialogueOuterDrawContext& c)
{
    if ((widget.flags & c.visibility_mask) == 0 && widget.current_set != nullptr
        && !(std::bit_cast<float>(0x3A83126Fu) > widget.timer))
    {
        const float scale[2]{widget.timer, widget.timer};
        i32 index = 0;
        if (0 < widget.count)
        {
            do
            {
                const auto icon = widget.icons[index];
                if (!icon.empty())
                {
                    if (immediate == 0)
                    {
                        queue_runtime_dialogue_sprite(scale, widget.positions[index].data(),
                            icon.data(), widget.transition_flags, 0xFF, 0.f, c.sprites);
                    }
                    else
                    {
                        draw_runtime_dialogue_sprite(icon.data(), widget.positions[index].data(),
                            scale, 0.f, widget.transition_flags, -1, c.draw);
                    }
                }
                ++index;
            }
            while (index < widget.count);
        }
        if (widget.bat != nullptr)
        {
            const float bat_scale = widget.bat_scale * widget.timer;
            if (widget.bat->scale_200 != bat_scale)
            {
                widget.bat->scale_200 = bat_scale;
                widget.bat->xform_valid = 0;
            }
            const i32 view = static_cast<i32>(c.current_display_address - 0x507018u) / 0x1E4;
            c.draw.dialogue.cutscenes.draws.at(widget.bat)(*widget.bat, view, 3);
        }
    }
}
// Xbox 0x00058150
void render_runtime_dialogue(RuntimeDialogueOuterDrawContext& c)
{
    auto& activation = c.draw.dialogue;
    auto& state = activation.state;
    if (state.cutscene != nullptr && state.mode != 0)
    {
        ++c.draw.override_count;
        c.draw.fog_disabled = 1;
        set_runtime_render_fog_enabled(c.draw.renderer, 0);
        if (state.entries[state.read].flag != 0)
        {
            const DialogueSurfaceHandle texture = invoke_dialogue_render_target(state.entries[state.read].flag, c.hooks);
            begin_runtime_dialogue_target(texture, 0, c);
            c.d3d.clear(0, nullptr, 0xF3, 0, 1.f, 0);
            c.capture_enabled = 0;
        }
        setup_explicit_camera_display(&state.camera, 0, c.display);
        if (state.cutscene != nullptr)
        {
            const i32 view = static_cast<i32>(c.current_display_address - 0x507018u) / 0x1E4;
            draw_runtime_cutscene(*state.cutscene, view, activation.cutscenes);
        }
        if (state.visible != 0)
        {
            if (activation.measurement.tags.state.heads != 0)
            {
                draw_dialogue_speaker(c.draw);
            }
            if (static_cast<std::int8_t>(c.visibility_mask & 0xFF) >= 0)
            {
                draw_runtime_dialogue_page(activation.page, state.progress, c.draw);
            }
            ++c.text_draw_count;
        }
        if (state.entries[state.read].flag != 0)
        {
            draw_runtime_dialogue_border(14.f, c.draw.white, c);
            end_runtime_dialogue_target(c);
            c.capture_enabled = c.capture_configuration != 0;
        }
        if (activation.measurement.tags.state.buttons != 0)
        {
            draw_runtime_dialogue_widget(*state.widget, 0, c);
        }
        setup_active_camera_display(0, c.display);
        --c.draw.override_count;
        if (c.draw.override_count == 0)
        {
            c.draw.fog_disabled = 0;
        }
    }
}
}
