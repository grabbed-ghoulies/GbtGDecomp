#include "dialogue_draw_context.h"
#include "gbtg/anim.h"
#include <algorithm>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32 bits(float value)
{
    return std::bit_cast<u32>(value);
}
void prepare_quad(u32 color, RuntimeDialogueDrawContext& c)
{
    std::array<float, 4> channels;
    for (u32 i = 0; i != 4; ++i)
    {
        channels[i] = static_cast<float>((color >> (i * 8)) & 255) * (1.f / 255.f);
    }
    c.d3d.vertex_shader(c.vertex_shader);
    if (c.shader_constants != nullptr)
    {
        c.d3d.constants_not_inline(0x60, c.shader_constants, 0xC);
    }
    c.d3d.pixel_shader(c.pixel_shader);
    std::array<float, 16> world, view, projection, combined;
    c.d3d.get_transform(6, world.data());
    c.d3d.get_transform(0, view.data());
    c.d3d.get_transform(1, projection.data());
    c.d3d.multiply(combined.data(), world.data(), view.data());
    c.d3d.multiply(combined.data(), combined.data(), projection.data());
    c.d3d.transpose(combined.data(), combined.data());
    c.d3d.constants(0x60, combined.data(), 4);
    c.d3d.constants(0x64, channels.data(), 1);
}
void push_quad(const float* top, const float* bottom, u32 zero,
    const float* uv_top, const float* uv_bottom, RuntimeDialogueDrawContext& c)
{
    u32* p = c.d3d.begin_push(0x19);
    p[0] = 0x417FC;
    p[1] = 8;
    p[2] = 0x40501818;
    const std::array<u32, 20> vertices{
        bits(top[0]), bits(bottom[1]), zero, bits(uv_top[0]), bits(uv_bottom[1]),
        bits(bottom[0]), bits(bottom[1]), zero, bits(uv_bottom[0]), bits(uv_bottom[1]),
        bits(bottom[0]), bits(top[1]), zero, bits(uv_bottom[0]), bits(uv_top[1]),
        bits(top[0]), bits(top[1]), zero, bits(uv_top[0]), bits(uv_top[1])};
    std::copy(vertices.begin(), vertices.end(), p + 3);
    p[23] = 0x417FC;
    p[24] = 0;
    c.d3d.end_push(p + 25);
}
}
// Xbox 0x0005E380
void draw_dialogue_quad(u32 color, RuntimeDialogueDrawContext& c)
{
    prepare_quad(color, c);
    const float zero = std::bit_cast<float>(c.quad_zero[0]);
    const float one = std::bit_cast<float>(c.quad_one);
    const float uv_top[2]{zero, zero};
    const float uv_bottom[2]{one, one};
    push_quad(c.quad_top.data(), c.quad_bottom.data(), c.quad_zero[0], uv_top, uv_bottom, c);
}
// Xbox 0x0005E570
void draw_dialogue_sized_quad(const float* dimensions, u32 color, RuntimeDialogueDrawContext& c)
{
    const float right = dimensions[0] - 1.f;
    const float bottom = dimensions[1] - 1.f;
    prepare_quad(color, c);
    const float zero = std::bit_cast<float>(c.quad_zero[1]);
    const float uv_top[2]{zero, zero};
    const float uv_bottom[2]{right, bottom};
    push_quad(c.quad_top.data(), c.quad_bottom.data(), c.quad_zero[1], uv_top, uv_bottom, c);
}
// Xbox 0x0005E770
void draw_dialogue_uv_quad(const float* top, const float* bottom, u32 color,
    const float* uv_top, const float* uv_bottom, RuntimeDialogueDrawContext& c)
{
    prepare_quad(color, c);
    push_quad(top, bottom, c.quad_zero[2], uv_top, uv_bottom, c);
}
// Xbox 0x0005CA60
void draw_dialogue_texture(i32 frame, const float* uv_top, u8* texture, const float* scale,
    const float* uv_bottom, float angle, u32 color, const float* position,
    RuntimeDialogueDrawContext& c)
{
    c.current_affine = c.affine.data();
    std::fill_n(c.current_affine, 24, 0.f);
    c.current_affine[0] = c.current_affine[5] = c.current_affine[10] = 1.f;
    float* scratch = c.current_affine + 12;
    scratch[0] = scratch[5] = scratch[10] = 1.f;
    scratch[3] = position[0];
    scratch[7] = position[1];
    multiply_affine_matrices(c.current_affine, c.current_affine, scratch);
    if (angle != 0.f)
    {
        rotate_dialogue_affine('z', angle, c);
    }
    u16 width, height;
    std::memcpy(&width, texture + 8, 2);
    std::memcpy(&height, texture + 10, 2);
    float sx = static_cast<float>(width);
    float sy = static_cast<float>(height);
    if (scale != nullptr && (scale[0] != 1.f || scale[1] != 1.f))
    {
        sx *= scale[0];
        sy *= scale[1];
    }
    scratch = c.current_affine + 12;
    std::fill_n(scratch, 12, 0.f);
    scratch[0] = sx * .5f;
    scratch[5] = sy * .5f;
    scratch[10] = 1.f;
    multiply_affine_matrices(c.current_affine, c.current_affine, scratch);
    for (u32 row = 0; row != 4; ++row)
    {
        for (u32 col = 0; col != 3; ++col)
        {
            c.sprite_world[row * 4 + col] = c.current_affine[col * 4 + row];
        }
        c.sprite_world[row * 4 + 3] = 0.f;
    }
    c.sprite_world[15] = 1.f;
    c.d3d.set_transform(6, c.sprite_world.data());
    if (frame != -1 && frame < static_cast<i32>(texture[0x11]))
    {
        texture[0x13] = static_cast<u8>(frame);
    }
    bind_dialogue_texture(texture, 0, c);
    if (uv_top != nullptr)
    {
        // 0x0005E770: EDI (EDX on entry) is the top texel pair and ESI (the third stack word) the bottom one.
        draw_dialogue_uv_quad(c.quad_top.data(), c.quad_bottom.data(), color, uv_top, uv_bottom, c);
        return;
    }
    u32 type;
    std::memcpy(&type, texture, 4);
    if (type != 0x12 && type != 0x1E && type != 0x3F)
    {
        draw_dialogue_quad(color, c);
        return;
    }
    const float dimensions[2]{static_cast<float>(width), static_cast<float>(height)};
    draw_dialogue_sized_quad(dimensions, color, c);
}
}
