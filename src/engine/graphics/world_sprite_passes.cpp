#include "world_sprite_passes.h"
#include "../camera/runtime_display_setup.h"
#include "../frontend/dialogue_draw_outer.h"
#include "gbtg/anim.h"
#include "gbtg/model_render.h"

#include <array>

namespace gbtg
{
namespace
{
u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}
}

// Xbox 0x0005CF20
void begin_world_sprite_draw(RuntimeDisplaySetupContext& c)
{
    auto& draw = c.draw;
    bind_display_projection(c);
    reset_dialogue_sprite_state(draw);
    set_dialogue_alpha_state(draw);
    set_dialogue_texture_state(draw);
    draw.d3d.simple_state(0x40354, 0x203);
    deferred(draw, 0x181C54) = 0x203;
    draw.d3d.simple_state(0x4035C, 1);
    deferred(draw, 0x181C70) = 1;
    draw.d3d.depth(1);
    ++draw.override_count;
    draw.fog_disabled = 1;
    set_runtime_render_fog_enabled(draw.renderer, 0);
}

// Xbox 0x0005DFF0
void set_world_sprite_depth_mode(u32 mode, RuntimeDisplaySetupContext& c)
{
    auto& draw = c.draw;
    u32 function;
    u32 write;
    switch (mode)
    {
    case 0:
        function = 0x207;
        write = 0;
        break;
    case 1:
        function = 0x203;
        write = 1;
        break;
    case 2:
        function = 0x203;
        write = 0;
        break;
    case 3:
        function = 0x207;
        write = 1;
        break;
    default:
        return;
    }
    draw.d3d.simple_state(0x40354, function);
    deferred(draw, 0x181C54) = function;
    draw.d3d.simple_state(0x4035C, write);
    deferred(draw, 0x181C70) = write;
    draw.d3d.depth(1);
}

// Xbox 0x0005CF90
void draw_world_sprite_quad(i32 frame, u8* texture, const Vec3* position, const Vec3* euler, const float* size,
    u32 color, RuntimeDisplaySetupContext& c)
{
    auto& draw = c.draw;
    if (frame != -1 && frame < i32(texture[0x11]))
    {
        texture[0x13] = u8(frame);
    }
    bind_dialogue_texture(texture, 0, draw);
    // DAT_0050DD70 points at DAT_0050D7A0, reset to the identity; DAT_0050D7D0 (+12) is the scratch matrix.
    draw.current_affine = draw.affine.data();
    float* current = draw.current_affine;
    std::fill_n(current, 12, 0.f);
    current[0] = 1.f; // DAT_0032B0F8.
    current[5] = 1.f;
    current[10] = 1.f;
    if (position != nullptr)
    {
        float* translation = draw.affine.data() + 12;
        std::fill_n(translation, 12, 0.f);
        translation[0] = 1.f;
        translation[3] = position->x;
        translation[5] = 1.f;
        translation[7] = position->y;
        translation[10] = 1.f;
        translation[11] = position->z;
        multiply_affine_matrices(current, current, translation);
    }
    if (euler != nullptr)
    {
        rotate_dialogue_affine(0x79, euler->y, draw);
        rotate_dialogue_affine(0x78, euler->x, draw);
        rotate_dialogue_affine(0x7A, euler->z, draw);
    }
    if (size != nullptr)
    {
        float* scale = draw.current_affine + 12;
        std::fill_n(scale, 12, 0.f);
        scale[0] = size[0];
        scale[5] = 1.f;
        scale[10] = size[1];
        multiply_affine_matrices(draw.current_affine, draw.current_affine, scale);
    }
    // Texture kinds 0x12, 0x1E and 0x3F address texels: the quad spans the +8 / +0xA dimensions.
    float u = 1.f;
    float v = 1.f;
    const u32 kind = read_u32({texture, 0xC}, 0);
    if (kind == 0x12 || kind == 0x1E || kind == 0x3F)
    {
        u = float(read_u16({texture, 0xC}, 8));
        v = float(read_u16({texture, 0xC}, 0xA));
    }
    bind_display_device_view(draw.current_affine, c);
    // Bytes 0 and 2 swap: byte 3 stays the alpha, byte 0 becomes red.
    const u32 swapped = (color & 0xFF000000u) | ((color & 0xFFu) << 16) | (color & 0xFF00u) | ((color >> 16) & 0xFFu);
    draw.d3d.vertex_data_color(3, swapped); // D3D 0x00172CC0
    draw.d3d.begin(8); // D3D 0x00172D10
    draw.d3d.vertex_data2f(9, 0.f, 0.f); // D3D 0x00172C20
    draw.d3d.vertex_data4f(-1, -1.f, 0.f, -1.f, 1.f); // D3D 0x00172C60
    draw.d3d.vertex_data2f(9, 0.f, v);
    draw.d3d.vertex_data4f(-1, -1.f, 0.f, 1.f, 1.f);
    draw.d3d.vertex_data2f(9, u, v);
    draw.d3d.vertex_data4f(-1, 1.f, 0.f, 1.f, 1.f);
    draw.d3d.vertex_data2f(9, u, 0.f);
    draw.d3d.vertex_data4f(-1, 1.f, 0.f, -1.f, 1.f);
    draw.d3d.end(); // D3D 0x00172D50
}

// Xbox 0x000EC470
void draw_effect_debris(std::span<const u8> particles, RuntimeDisplaySetupContext& c)
{
    if (particles.data() == nullptr)
    {
        return;
    }
    auto& draw = c.draw;
    begin_world_sprite_draw(c);
    draw.d3d.simple_state(0x40354, 0x203);
    deferred(draw, 0x181C54) = 0x203;
    draw.d3d.simple_state(0x4035C, 0);
    deferred(draw, 0x181C70) = 0;
    draw.d3d.depth(1);
    u32 offset = 0;
    do
    {
        if (i32(read_u32(particles, offset + 0x68)) == -2)
        {
            // The size (+0x20 * +0x18) and the color from four 0x001673D8 conversions before the quad
            // 0x0005CF90 are not translated.
            throw Error("debris record draw in 0x000EC470 is not translated");
        }
        offset += 0x74;
    } while (offset < 0x16A8);
    bind_display_projection(c);
    if (--draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
}

// Xbox 0x000615E0
void draw_screen_effects(const RuntimeScreenEffects& effects, RuntimeDialogueOuterDrawContext& c)
{
    std::array<float, 8> corners;
    dialogue_viewport_corners(corners.data(), c.draw.fitted_viewport);
    if (effects.flash != 0)
    {
        throw Error("screen flash draw 0x000615E0 (DAT_00504478) is not translated");
    }
    if (effects.capture != 0 && effects.transition_guard < 1)
    {
        throw Error("screen capture overlay 0x000615E0 (DAT_005044A4) is not translated");
    }
    if (effects.lines != 0)
    {
        throw Error("retained line draw 0x00061250 from 0x000615E0 (DAT_005044B0) is not bound");
    }
}
}
