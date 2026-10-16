#include "glow_pass.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"

#include <bit>

namespace gbtg
{
namespace
{
u32& deferred(RuntimeGlowPass& context, u32 address)
{
    return context.outer.draw.deferred[(address - 0x00181968) / 4];
}

void simple(RuntimeGlowPass& context, u32 method, u32 value, u32 shadow)
{
    context.outer.draw.d3d.simple_state(method, value); // D3D 0x00171050.
    deferred(context, shadow) = value;
}

std::array<float, 4> color_channels(u32 color)
{
    constexpr float scale = 1.f / 255.f; // DAT_0032B154.
    return {float(color & 0xFF) * scale, float((color >> 8) & 0xFF) * scale, float((color >> 16) & 0xFF) * scale,
        float(color >> 24) * scale};
}
}

// Xbox 0x0013BCC0
bool runtime_screen_cylinder_rect(const Vec3& center, float radius, float height, float* minimum, float* maximum,
    const entity_detail::DisplayView& view)
{
    const float right = center.x + radius;
    const float left = center.x - radius;
    const float front = center.z + radius;
    const float back = center.z - radius;
    const float top = center.y + height;
    const std::array<Vec3, 8> corners{Vec3{right, center.y, back}, Vec3{left, center.y, front},
        Vec3{right, center.y, front}, Vec3{left, center.y, back}, Vec3{right, top, back}, Vec3{left, top, front},
        Vec3{right, top, front}, Vec3{left, top, back}};
    const i32 x = view.fitted_x;
    const i32 y = view.fitted_y;
    const float half_width = float(view.fitted_width) * 0.5f; // DAT_0032B0F4.
    const float half_height = float(view.fitted_height) * 0.5f;
    const float far_x = float(x + view.fitted_width);
    const float far_y = float(y + view.fitted_height);
    const float near_x = float(x);
    const float near_y = float(y);
    minimum[0] = far_x;
    minimum[1] = far_y;
    maximum[0] = near_x;
    maximum[1] = near_y;
    const auto& m = view.transform;
    for (const Vec3& corner : corners)
    {
        const float tx = m[2] * corner.z + m[1] * corner.y + m[0] * corner.x + m[3];
        const float ty = m[6] * corner.z + m[4] * corner.x + m[5] * corner.y + m[7];
        const float tz = m[10] * corner.z + m[8] * corner.x + m[9] * corner.y + m[11];
        const float k = view.focal_pixels / tz;
        const float sx = half_width - tx * k * view.pixel_aspect;
        const float sy = ty * k + half_height;
        const float snapped = float(static_cast<long double>(sx) - animation_remainder(sx, 4.f));
        if (minimum[0] > snapped)
        {
            minimum[0] = snapped;
        }
        if (snapped > maximum[0])
        {
            maximum[0] = snapped;
        }
        if (minimum[1] > sy)
        {
            minimum[1] = sy;
        }
        if (sy > maximum[1])
        {
            maximum[1] = sy;
        }
    }
    if (near_x > minimum[0])
    {
        if (near_x > maximum[0])
        {
            return false;
        }
        minimum[0] = near_x;
    }
    if (!(maximum[0] < far_x))
    {
        if (minimum[0] >= far_x)
        {
            return false;
        }
        maximum[0] = far_x;
    }
    if (near_y > minimum[1])
    {
        if (near_y > maximum[1])
        {
            return false;
        }
        minimum[1] = near_y;
    }
    if (!(maximum[1] < far_y))
    {
        if (minimum[1] >= far_y)
        {
            return false;
        }
        maximum[1] = far_y;
    }
    constexpr float wide = 4.f; // DAT_0032B1B0.
    constexpr float tall = 2.f; // DAT_0032B110.
    if (wide > maximum[0] - minimum[0])
    {
        const float moved = maximum[0] - wide;
        if (moved > near_x)
        {
            minimum[0] = moved;
        }
        else
        {
            maximum[0] = minimum[0] + wide;
        }
    }
    if (tall > maximum[1] - minimum[1])
    {
        // The dump tests against the 4.0 still in XMM1 but moves by 2.0.
        if (maximum[1] - wide > near_y)
        {
            minimum[1] = maximum[1] - tall;
        }
        else
        {
            maximum[1] = minimum[1] + tall;
        }
    }
    return !(minimum[0] == maximum[0]) && !(minimum[1] == maximum[1]);
}

// Xbox 0x00101640
u32 scale_runtime_color(u32 color, float scale)
{
    u32 result = 0;
    for (u32 channel = 0; channel < 4; ++channel)
    {
        const long double value = static_cast<long double>(i32((color >> (channel * 8)) & 0xFF)) * scale;
        result |= u32(u8(truncate_runtime_float(value))) << (channel * 8);
    }
    return result;
}

// Xbox 0x00062220
void begin_runtime_glow_capture(const float* minimum, const float* maximum, RuntimeGlowPass& context)
{
    if (context.transition_guard != 0)
    {
        return;
    }
    auto& d3d = context.outer.d3d;
    auto& state = context.outer.targets;
    auto& glow = context.targets;
    state.back_buffer = d3d.back_buffer(0);
    state.depth_buffer = d3d.depth_buffer();
    d3d.render_target(glow.page_surface, glow.depth_surface);
    if (minimum != nullptr && maximum != nullptr)
    {
        glow.corners = {minimum[0], minimum[1], minimum[0], maximum[1], maximum[0], maximum[1], maximum[0],
            minimum[1]};
        glow.rectangle = {i32(minimum[0]), i32(minimum[1]), i32(maximum[0]) + 2, i32(maximum[1]) + 2};
    }
    else
    {
        const auto& view = *entity_detail::g_display_view; // DAT_005077A8.
        const i32 x = view.fitted_x;
        const i32 y = view.fitted_y;
        const i32 right = view.fitted_width + x;
        const i32 bottom = view.fitted_height + y;
        glow.corners = {float(x), float(y), float(x), float(bottom), float(right), float(bottom), float(right),
            float(y)};
        glow.rectangle = {x, y, right + 2, bottom + 2};
    }
    d3d.clear(1, glow.rectangle.data(), 0xF3, 0, 1.f, 0);
    d3d.release(state.back_buffer);
    d3d.release(state.depth_buffer);
    state.active = 1;
}

// Xbox 0x00061D00
void set_runtime_glow_depth_states(RuntimeGlowPass& context)
{
    auto& d3d = context.outer.draw.d3d;
    d3d.depth(1); // D3DDevice_SetRenderState_ZEnable.
    simple(context, 0x4035C, 1, 0x00181C70);
    simple(context, 0x40354, 0x203, 0x00181C54);
    d3d.stencil_enable(1);
    simple(context, 0x40364, 0x207, 0x00181C88);
    simple(context, 0x40368, 1, 0x00181C8C);
    simple(context, 0x4036C, 0, 0x00181C90);
    simple(context, 0x40360, 0xFFFFFFFFu, 0x00181C94);
    simple(context, 0x40374, 0x1E01, 0x00181C80);
    simple(context, 0x40378, 0, 0x00181C84);
    simple(context, 0x40358, 0, 0x00181C7C);
    d3d.texture(0, nullptr);
    d3d.texture(1, context.targets.depth_texture);
    d3d.texture(2, nullptr);
    d3d.texture(3, context.targets.depth_texture);
    deferred(context, 0x00181968) |= 0xB;
    for (const u32 address : {0x00181970u, 0x00181974u, 0x001819F0u, 0x001819F4u, 0x00181AF0u, 0x00181AF4u})
    {
        deferred(context, address) = 3;
    }
    for (const u32 address : {0x00181980u, 0x0018197Cu, 0x00181A00u, 0x001819FCu, 0x00181B00u, 0x00181AFCu})
    {
        deferred(context, address) = 1;
    }
    simple(context, 0x40300, 1, 0x00181C60);
    simple(context, 0x40340, 5, 0x00181C64);
    simple(context, 0x40304, 1, 0x00181C5C);
    d3d.vertex_shader(context.word_at(0x0042720C));
    if (context.word_at(0x00427200) != 0)
    {
        d3d.constants_not_inline(0x60, 0xC);
    }
    d3d.pixel_shader(context.word_at(0x00425DEC));
    context.texture_modes(0x54420);
    std::array<float, 16> world;
    for (u32 index = 0; index < 16; ++index)
    {
        world[index] = std::bit_cast<float>(context.word_at(0x0042A7F0 + index * 4));
    }
    d3d.set_transform(6, world.data());
}

// Xbox 0x00061EE0
void set_runtime_glow_color_states(u32 vertex_shader, u32 pixel_shader, u32 color, RuntimeGlowPass& context)
{
    auto& d3d = context.outer.draw.d3d;
    d3d.texture(2, nullptr);
    d3d.texture(3, nullptr);
    simple(context, 0x40358, 0x01010101, 0x00181C7C);
    d3d.depth(0);
    deferred(context, 0x00181968) |= 1;
    deferred(context, 0x0018199C) = 4;
    simple(context, 0x40350, 0x8006, 0x00181C98);
    simple(context, 0x40344, 0x302, 0x00181C68);
    simple(context, 0x40348, 0x303, 0x00181C6C);
    simple(context, 0x40304, 1, 0x00181C5C);
    simple(context, 0x40374, 0x1E00, 0x00181C80);
    context.stencil_fail(0x1E00);
    simple(context, 0x40378, 0x1E00, 0x00181C84);
    simple(context, 0x40364, 0x202, 0x00181C88);
    simple(context, 0x4036C, 0xFFFFFFFFu, 0x00181C90);
    simple(context, 0x40368, 0, 0x00181C8C);
    d3d.vertex_shader(context.word_at(0x004271F4 + vertex_shader * 0x18));
    if (context.word_at(0x004271E8 + vertex_shader * 0x18) != 0)
    {
        d3d.constants_not_inline(0x60, 0xC);
    }
    d3d.texture(0, context.targets.page_texture);
    d3d.pixel_shader(context.word_at(0x00425DEC + pixel_shader * 0x10));
    const auto channels = color_channels(color);
    context.outer.d3d.pixel_constants(0, channels.data(), 1);
}

// Xbox 0x000623C0
u32 begin_runtime_glow_composite(u32 color, u32 vertex_shader, u32 pixel_shader, RuntimeGlowPass& context)
{
    if (context.transition_guard != 0)
    {
        return 0;
    }
    auto& state = context.outer.targets;
    context.outer.d3d.render_target(state.back_buffer, state.depth_buffer);
    state.active = 0;
    set_runtime_glow_depth_states(context);
    auto& d3d = context.outer.draw.d3d;
    u32* push = d3d.begin_push(0xD);
    push[0] = 0x417FC;
    push[1] = 8;
    push[2] = 0x40201818;
    for (u32 index = 0; index < 8; ++index)
    {
        push[3 + index] = std::bit_cast<u32>(context.targets.corners[index]);
    }
    push[11] = 0x417FC;
    push[12] = 0;
    d3d.end_push(push + 13);
    set_runtime_glow_color_states(vertex_shader, pixel_shader, color, context);
    return 1;
}

// Xbox 0x00062750
void composite_runtime_glow(u32 first_texture, u32 pixel_shader, u32 second_texture, u32 first, u32 second,
    RuntimeGlowPass& context)
{
    if (begin_runtime_glow_composite(first, 4, pixel_shader, context) == 0)
    {
        return;
    }
    auto& draw = context.outer.draw;
    if (first_texture != 0)
    {
        bind_dialogue_texture(context.texture_at(first_texture), 1, draw); // 0x00053550, ESI texture.
    }
    if (second_texture != 0)
    {
        bind_dialogue_texture(context.texture_at(second_texture), 2, draw);
    }
    const auto low = color_channels(first);
    const auto high = color_channels(second);
    const std::array<float, 8> constants{low[0], low[1], low[2], low[3], high[0], high[1], high[2], high[3]};
    context.outer.d3d.pixel_constants(1, constants.data(), 2);
    // DAT_0042518C..DAT_004251A8: each corner's texture coordinates, given to both stages.
    std::array<u32, 8> coordinates;
    for (u32 index = 0; index < 8; ++index)
    {
        coordinates[index] = context.word_at(0x0042518C + index * 4);
    }
    auto& d3d = draw.d3d;
    u32* push = d3d.begin_push(0x1D);
    push[0] = 0x417FC;
    push[1] = 8;
    push[2] = 0x40601818;
    for (u32 corner = 0; corner < 4; ++corner)
    {
        u32* vertex = push + 3 + corner * 6;
        vertex[0] = std::bit_cast<u32>(context.targets.corners[corner * 2]);
        vertex[1] = std::bit_cast<u32>(context.targets.corners[corner * 2 + 1]);
        vertex[2] = coordinates[corner * 2];
        vertex[3] = coordinates[corner * 2 + 1];
        vertex[4] = coordinates[corner * 2];
        vertex[5] = coordinates[corner * 2 + 1];
    }
    push[27] = 0x417FC;
    push[28] = 0;
    d3d.end_push(push + 0x1D);
    deferred(context, 0x00181968) |= 1;
    deferred(context, 0x0018199C) = 0;
    d3d.depth(1);
    d3d.stencil_enable(0);
    d3d.vertex_shader(0);
    d3d.pixel_shader(0);
    for (u32 stage = 0; stage < 4; ++stage)
    {
        d3d.texture(stage, nullptr);
    }
}
}
