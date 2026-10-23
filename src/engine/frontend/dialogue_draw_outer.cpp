#include "dialogue_draw_outer.h"
#include "../camera/runtime_display_setup.h"
#include "../cutscenes/event_runtime.h"
#include <algorithm>
#include <cmath>
#include <bit>

namespace gbtg
{
// Xbox 0x001417A0
DialogueSurfaceHandle invoke_dialogue_render_target(u8 index,
    const RuntimeSpecialTextures& hooks)
{
    const auto& hook = hooks[index];
    if (hook.get_texture)
    {
        return hook.get_texture(hook.model, hook.index); // 0x000528C0
    }
    return 0;
}
const RuntimeSpecialTextures& dialogue_render_target_hooks(const RuntimeDialogueOuterDrawContext& c)
{
    if (c.hooks == nullptr)
    {
        throw Error("dialogue render target hooks (DAT_00545DC0) are not bound");
    }
    return *c.hooks;
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
    if (c.line_shader_constants != nullptr)
    {
        c.draw.d3d.constants_not_inline(0x60, c.line_shader_constants, 0xC);
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
// Xbox 0x00061360
void draw_runtime_dialogue_panel(float x, float y, float width, float height, float radius, u32 color,
    RuntimeDialogueOuterDrawContext& c)
{
    const float scaled = height * radius;
    i32 cap = i32(scaled);
    if (float(cap) > scaled)
    {
        --cap;
    }
    const float right = x + width;
    const float r = float(cap);
    const float top = r + y;
    const float bottom = (y + height) - r;
    auto& draw = c.draw;
    auto& d3d = draw.d3d;
    initialize_dialogue_orthographic(draw); // 0x000F6260
    reset_dialogue_sprite_state(draw); // 0x0005E110
    draw.deferred[(0x181968 - 0x181968) / 4] |= 0x800;
    draw.deferred[(0x1819A0 - 0x181968) / 4] = 2;
    draw.deferred[(0x1819B0 - 0x181968) / 4] = 2;
    draw.deferred[(0x1819A8 - 0x181968) / 4] = 0;
    draw.deferred[(0x1819B8 - 0x181968) / 4] = 0;
    d3d.vertex_shader(0x42); // D3D 0x001739F0
    d3d.simple_state(0x40354, 0x203); // D3D 0x00171050
    draw.deferred[(0x181C54 - 0x181968) / 4] = 0x203;
    d3d.simple_state(0x4035C, 0);
    draw.deferred[(0x181C70 - 0x181968) / 4] = 0;
    d3d.depth(1); // D3D 0x001722A0
    // The color's bytes 0..2 swap into a D3DCOLOR: byte 3 stays the alpha, byte 0 becomes red.
    const u32 swapped = (color & 0xFF000000u) | ((color & 0xFFu) << 16) | (color & 0xFF00u)
        | ((color >> 16) & 0xFFu);
    d3d.vertex_data_color(3, swapped); // D3D 0x00172CC0
    d3d.begin(8); // D3D 0x00172D10
    d3d.vertex_data4f(-1, x, bottom, 0.f, 1.f); // D3D 0x00172C60
    d3d.vertex_data4f(-1, right, bottom, 0.f, 1.f);
    const float cap_top = float(double(top) + 1.0); // DAT_0032B0F8
    d3d.vertex_data4f(-1, right, cap_top, 0.f, 1.f);
    d3d.vertex_data4f(-1, x, cap_top, 0.f, 1.f);
    d3d.end(); // D3D 0x00172D50
    c.d3d.line_width(std::bit_cast<float>(c.panel_line_width)); // D3D 0x00171570, DAT_00425168
    d3d.begin(2);
    float row = 0.f;
    if (0.f < r)
    {
        const float squared = r * r;
        do
        {
            const double reach = std::sqrt(double(squared - row * row)); // 0x00152B30
            const float inset = float(double(r) - reach);
            const float upper = float(double(top) - double(row));
            const float left = float(double(inset) + double(x));
            d3d.vertex_data4f(-1, left, upper, 0.f, 1.f);
            const float far_right = float(double(right) - double(inset));
            d3d.vertex_data4f(-1, far_right, upper, 0.f, 1.f);
            const float lower = float(double(row) + double(bottom));
            d3d.vertex_data4f(-1, left, lower, 0.f, 1.f);
            d3d.vertex_data4f(-1, far_right, lower, 0.f, 1.f);
            row += 1.f; // DAT_0032B0F8
        }
        while (row < r);
    }
    d3d.end();
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
            c.draw.dialogue.cutscenes.draws(*widget.bat, view, 3);
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
            const DialogueSurfaceHandle texture = invoke_dialogue_render_target(state.entries[state.read].flag,
                dialogue_render_target_hooks(c));
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
