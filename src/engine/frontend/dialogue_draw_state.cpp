#include "dialogue_draw_context.h"
#include "gbtg/anim.h"
#include "../audio/listener_settings.h"
#include <algorithm>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}
void identity(float* m)
{
    std::fill_n(m, 16, 0.f);
    m[0] = m[5] = m[10] = m[15] = 1.f;
}
}
// Xbox 0x001538A0
void make_dialogue_projection(float* m, float top, float bottom, float left,
    float right, float near_plane, float far_plane)
{
    float reciprocal = 1.f / (right - left);
    m[0] = 2.f * reciprocal;
    m[4] = m[8] = 0.f;
    m[12] = 0.f - (left + right) * reciprocal;
    reciprocal = 1.f / (top - bottom);
    m[1] = m[9] = 0.f;
    m[5] = 2.f * reciprocal;
    m[13] = 0.f - (top + bottom) * reciprocal;
    m[2] = m[6] = 0.f;
    reciprocal = 1.f / (far_plane - near_plane);
    m[10] = 0.f - reciprocal;
    m[14] = (0.f - ((near_plane + far_plane) * reciprocal - 1.f)) * .5f;
    m[3] = m[7] = m[11] = 0.f;
    m[15] = 1.f;
}
// Xbox 0x00156400
void set_dialogue_render_projection(const float* matrix, RuntimeDialogueDrawContext& c)
{
    auto& frame = c.frames.at(&runtime_render_frame(c.renderer));
    if (matrix != nullptr)
    {
        std::copy_n(matrix, 16, frame.projection.begin());
        return;
    }
    identity(frame.projection.data());
}
// Xbox 0x00156330
void set_dialogue_render_view(const float* matrix, RuntimeDialogueDrawContext& c)
{
    c.frames.at(&runtime_render_frame(c.renderer)).view = matrix;
}
// Xbox 0x001046A0
void make_dialogue_axis_rotation(float* m, u8 axis, float sine, float cosine)
{
    switch (axis)
    {
    case 'X':
    case 'x':
        m[0] = 1.f;
        m[1] = m[4] = 0.f;
        m[5] = cosine;
        m[6] = 0.f - sine;
        m[9] = sine;
        break;
    case 'Y':
    case 'y':
        m[5] = 1.f;
        m[0] = cosine;
        m[1] = m[4] = m[6] = m[9] = 0.f;
        m[2] = sine;
        m[8] = 0.f - sine;
        m[3] = m[7] = m[11] = 0.f;
        m[10] = cosine;
        return;
    case 'Z':
    case 'z':
        m[0] = m[5] = cosine;
        cosine = 1.f;
        m[1] = 0.f - sine;
        m[4] = sine;
        m[6] = m[9] = 0.f;
        break;
    default:
        return;
    }
    m[2] = m[8] = m[3] = m[7] = m[11] = 0.f;
    m[10] = cosine;
}
// Xbox 0x00104160
void rotate_dialogue_affine(u8 axis, float angle, RuntimeDialogueDrawContext& c)
{
    float* scratch = c.current_affine + 12;
    const float sine = static_cast<float>(retail_sin(angle));
    const float cosine = static_cast<float>(retail_cos(angle));
    make_dialogue_axis_rotation(scratch, axis, sine, cosine);
    multiply_affine_matrices(c.current_affine, c.current_affine, scratch);
}
// Xbox 0x000F6260
void initialize_dialogue_orthographic(RuntimeDialogueDrawContext& c)
{
    make_dialogue_projection(c.projection.data(), 0.f, static_cast<float>(c.logical_width),
        0.f, static_cast<float>(c.logical_height), 0.f, 100.f);
    c.d3d.set_transform(1, c.projection.data());
    const std::array<u32, 6> viewport{c.fitted_viewport[0], c.fitted_viewport[1],
        c.fitted_viewport[2], c.fitted_viewport[3], 0, std::bit_cast<u32>(1.f)};
    c.d3d.viewport(viewport);
    set_dialogue_render_projection(c.projection.data(), c);
    identity(c.view.data());
    identity(c.world.data());
    c.d3d.set_transform(0, c.view.data());
    c.d3d.set_transform(6, c.world.data());
    set_dialogue_render_view(c.view.data(), c);
}
// Xbox 0x0005E110
void reset_dialogue_sprite_state(RuntimeDialogueDrawContext& c)
{
    c.d3d.noise(0);
    deferred(c, 0x181968) |= 0x2000;
    deferred(c, 0x181CF0) = 1;
    c.d3d.simple_state(0x40310, 0);
    deferred(c, 0x181C74) = 0;
    c.d3d.cull(0);
    c.d3d.simple_state(0x40350, 0x8006);
    deferred(c, 0x181C98) = 0x8006;
    c.d3d.simple_state(0x40344, 0x302);
    deferred(c, 0x181C68) = 0x302;
    c.d3d.simple_state(0x40348, 0x303);
    deferred(c, 0x181C6C) = 0x303;
    c.d3d.simple_state(0x40304, 1);
    deferred(c, 0x181C5C) = 1;
    c.d3d.simple_state(0x4033C, 0x204);
    deferred(c, 0x181968) |= 0x1200;
    deferred(c, 0x181C58) = 0x204;
    deferred(c, 0x181D08) = 0;
    c.d3d.simple_state(0x40358, 0x1010101);
    deferred(c, 0x181C7C) = 0x1010101;
    c.d3d.simple_state(0x40354, 0x203);
    deferred(c, 0x181C54) = 0x203;
    c.d3d.simple_state(0x4035C, 1);
    deferred(c, 0x181C70) = 1;
    c.d3d.depth(1);
    for (u32 stage = 0, address = 0x1819B0; address < 0x181BB0; ++stage, address += 0x80)
    {
        c.d3d.texture(stage, nullptr);
        deferred(c, address - 0x10) = 1;
        deferred(c, address) = 1;
        deferred(c, address - 0x30) = 2;
        deferred(c, 0x181968) |= (1u << (stage & 31)) | 0x800;
        deferred(c, address - 0x34) = 2;
    }
    c.d3d.shader_input(0, 0, nullptr);
    for (u32 stream = 0; stream < 16; ++stream)
    {
        c.d3d.stream(stream, nullptr, 0);
    }
    c.d3d.pixel_shader(0);
    c.d3d.vertex_shader(0);
}
// Xbox 0x0005E0C0
void set_dialogue_alpha_state(RuntimeDialogueDrawContext& c)
{
    c.d3d.simple_state(0x40300, 1);
    deferred(c, 0x181C60) = 1;
    c.d3d.simple_state(0x40340, 5);
    deferred(c, 0x181C64) = 5;
    c.d3d.simple_state(0x40304, 1);
    deferred(c, 0x181C5C) = 1;
}
// Xbox 0x0005E2A0
void set_dialogue_texture_state(RuntimeDialogueDrawContext& c)
{
    deferred(c, 0x1819A0) = deferred(c, 0x1819B0) = 4;
    deferred(c, 0x181968) |= 0xC01;
    deferred(c, 0x1819AC) = deferred(c, 0x1819BC) = 2;
    deferred(c, 0x181970) = deferred(c, 0x181974) = 3;
    deferred(c, 0x18198C) = 0;
    deferred(c, 0x181980) = deferred(c, 0x18197C) = deferred(c, 0x181984) = 2;
    deferred(c, 0x1819C4) = 0;
    c.d3d.texture_coordinates(0, 0);
    deferred(c, 0x181968) |= 0x800;
    deferred(c, 0x1819A8) = deferred(c, 0x1819B8) = 0;
    c.d3d.vertex_shader(2);
}
// Xbox 0x0005CDC0
void begin_dialogue_sprite_draw(RuntimeDialogueDrawContext& c)
{
    initialize_dialogue_orthographic(c);
    reset_dialogue_sprite_state(c);
    set_dialogue_alpha_state(c);
    set_dialogue_texture_state(c);
    c.d3d.simple_state(0x40354, 0x203);
    deferred(c, 0x181C54) = 0x203;
    c.d3d.simple_state(0x4035C, 0);
    deferred(c, 0x181C70) = 0;
    c.d3d.depth(1);
    ++c.override_count;
    c.fog_disabled = 1;
    set_runtime_render_fog_enabled(c.renderer, 0);
}
// Xbox 0x000605A0
void reset_dialogue_font(i32 font, RuntimeDialogueDrawContext& c)
{
    auto& s = c.state;
    s.color = c.white;
    c.metrics.scale_x = c.metrics.scale_y = 1.f;
    s.active = 1;
    s.retained = 0;
    s.half_height = static_cast<float>(c.metrics.fonts[s.font]->drop) * .5f;
    s.half_width = static_cast<float>(c.metrics.fonts[s.font]->cell) * .5f;
    s.angle = c.metrics.spacing = s.wrap = 0.f;
    s.font = font;
    begin_dialogue_sprite_draw(c);
}
// Xbox 0x00060640
void set_dialogue_font_scale(float x, float y, RuntimeDialogueDrawContext& c)
{
    c.metrics.scale_x = x;
    c.metrics.scale_y = y;
    c.state.half_height = static_cast<float>(c.metrics.fonts[c.state.font]->drop) * y * .5f;
    c.state.half_width = static_cast<float>(c.metrics.fonts[c.state.font]->cell) * x * .5f;
}
// Xbox 0x00053550
void bind_dialogue_texture(u8* texture, u32 stage, RuntimeDialogueDrawContext& c)
{
    const u8 frames = texture[0x11];
    if (frames > 1)
    {
        if (texture[0x13] == 0xFF)
        {
            u8 frame = 0;
            if (texture[0x10] != 0)
            {
                frame = static_cast<u8>(static_cast<u32>(truncate_runtime_float(
                    static_cast<long double>(c.animation_time) * texture[0x10])) % frames);
            }
            texture[0x13] = frame;
        }
        texture[0x12] = texture[0x13];
    }
    texture[0x13] = 0xFF;
    u32 offset;
    std::memcpy(&offset, texture + 4, 4);
    c.d3d.texture(stage, texture + offset + texture[0x12] * 0x14);
}
}
