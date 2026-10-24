#include "world_sprite_passes.h"
#include "../audio/listener_settings.h"
#include "../camera/display_view.h"
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
void draw_effect_debris(std::span<const u8> particles, std::span<const std::span<const u8>, 10> textures,
    RuntimeDisplaySetupContext& c)
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
        const float scale = read_f32(particles, offset + 0x20) * read_f32(particles, offset + 0x18);
        const std::array<float, 2> size{scale, scale};
        if (i32(read_u32(particles, offset + 0x68)) == -2)
        {
            // Four 0x001673D8 truncations, each keeping its low byte: +0x24 red .. +0x30 alpha.
            const u32 color = u32(u8(truncate_runtime_float(read_f32(particles, offset + 0x24))))
                | u32(u8(truncate_runtime_float(read_f32(particles, offset + 0x28)))) << 8
                | u32(u8(truncate_runtime_float(read_f32(particles, offset + 0x2C)))) << 16
                | u32(u8(truncate_runtime_float(read_f32(particles, offset + 0x30)))) << 24;
            // ECX the record's type (+0x6C) row's +0x80 texture in DAT_005068E8, EDI the record, EBX its +0xC.
            const u32 type = read_u32(particles, offset + 0x6C);
            if (type >= textures.size() || textures[type].data() == nullptr)
            {
                throw Error("debris record type " + std::to_string(type) + " has no +0x80 texture view");
            }
            const Vec3 position{read_f32(particles, offset), read_f32(particles, offset + 4),
                read_f32(particles, offset + 8)};
            const Vec3 euler{read_f32(particles, offset + 0xC), read_f32(particles, offset + 0x10),
                read_f32(particles, offset + 0x14)};
            draw_world_sprite_quad(0, const_cast<u8*>(textures[type].data()), &position, &euler, size.data(), color,
                c); // 0x0005CF90, EAX 0
        }
        offset += 0x74;
    } while (offset < 0x16A8);
    bind_display_projection(c);
    if (--draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
}

// Xbox 0x00065460
void begin_world_line_draw(u8 frame, u8* texture, RuntimeDisplaySetupContext& c)
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
    bind_display_device_view(nullptr, c); // ESI 0
    if (texture == nullptr)
    {
        throw Error("0x00065460 reads the frame count of a missing line texture (EDI 0)");
    }
    // MOVZX of the frame byte against -1.
    if (u32(frame) != 0xFFFFFFFFu && u32(frame) < u32(texture[0x11]))
    {
        texture[0x13] = frame;
    }
    bind_dialogue_texture(texture, 0, draw); // 0x00053550, ESI texture
}

// Xbox 0x000654E0, EAX to, EDI from
void draw_world_line_quad(const Vec3& to, const Vec3& from, float width, float inset, u32 color,
    RuntimeDisplaySetupContext& c)
{
    const entity_detail::DisplayView& view = *entity_detail::g_display_view; // DAT_005077A8.
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float dz = to.z - from.z;
    // The side across the segment and the view's forward (+0x1C).
    float side_y = view.forward.z * dx - dz * view.forward.x;
    float side_z = dy * view.forward.x - view.forward.y * dx;
    float side_x = dz * view.forward.y - dy * view.forward.z;
    const float side_square = side_z * side_z + side_y * side_y + side_x * side_x;
    if (side_square != 0.f)
    {
        const long double scale = retail_reciprocal_sqrt(side_square); // 0x001534C0
        side_x = float(static_cast<long double>(side_x) * scale);
        side_y = float(static_cast<long double>(side_y) * scale);
        side_z = float(static_cast<long double>(side_z) * scale);
    }
    const float half = width * 0.5f; // DAT_0032B0F4.
    float to_left_x = to.x + side_x * half;
    float to_left_y = to.y + side_y * half;
    float to_left_z = to.z + side_z * half;
    const float back = 0.f - half;
    float to_right_x = to.x + back * side_x;
    float to_right_z = to.z + side_z * back;
    float to_right_y = to.y + side_y * back;
    float from_left_x = from.x + side_x * half;
    float from_right_x = from.x + back * side_x;
    float from_right_y = from.y + side_y * back;
    float from_right_z = from.z + side_z * back;
    float from_left_y = from.y + side_y * half;
    float from_left_z = from.z + side_z * half;
    if (inset > 0.f)
    {
        const float length_square = dz * dz + dy * dy + dx * dx;
        if (length_square != 0.f)
        {
            const long double scale = retail_reciprocal_sqrt(length_square); // 0x001534C0
            dx = float(static_cast<long double>(dx) * scale);
            dy = float(static_cast<long double>(dy) * scale);
            dz = float(static_cast<long double>(dz) * scale);
        }
        const float pull = 0.f - inset;
        to_left_x = dx * pull + to_left_x;
        to_left_y = dy * pull + to_left_y;
        to_left_z = dz * pull + to_left_z;
        to_right_x = pull * dx + to_right_x;
        to_right_y = dy * pull + to_right_y;
        to_right_z = dz * pull + to_right_z;
        from_left_x = dx * inset + from_left_x;
        from_left_y = dy * inset + from_left_y;
        from_right_z = dz * inset + from_right_z;
        from_left_z = dz * inset + from_left_z;
        from_right_x = dx * inset + from_right_x;
        from_right_y = dy * inset + from_right_y;
    }
    auto& d3d = c.draw.d3d;
    // Bytes 0 and 2 swap: byte 3 stays the alpha, byte 0 becomes red.
    const u32 swapped = (color & 0xFF000000u) | ((color & 0xFFu) << 16) | (color & 0xFF00u) | ((color >> 16) & 0xFFu);
    d3d.vertex_data_color(3, swapped); // D3D 0x00172CC0
    d3d.begin(8); // D3D 0x00172D10
    d3d.vertex_data2f(9, 0.f, 0.f); // D3D 0x00172C20
    d3d.vertex_data4f(-1, to_left_x, to_left_y, to_left_z, 1.f); // D3D 0x00172C60
    d3d.vertex_data2f(9, 0.f, 1.f);
    d3d.vertex_data4f(-1, from_left_x, from_left_y, from_left_z, 1.f);
    d3d.vertex_data2f(9, 1.f, 1.f);
    d3d.vertex_data4f(-1, from_right_x, from_right_y, from_right_z, 1.f);
    d3d.vertex_data2f(9, 1.f, 0.f);
    d3d.vertex_data4f(-1, to_right_x, to_right_y, to_right_z, 1.f);
    d3d.end(); // D3D 0x00172D50
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

// Xbox 0x000659F0, EAX points, EBX count, ECX texture
void draw_world_line_strip(const Vec3* points, i32 count, u8* texture, u32 depth_mode, float width, u32 color,
    RuntimeDisplaySetupContext& c)
{
    begin_world_line_draw(0, texture, c); // 0x00065460, EDI texture
    set_world_sprite_depth_mode(depth_mode, c); // 0x0005DFF0, EAX mode
    const entity_detail::DisplayView& view = *entity_detail::g_display_view; // DAT_005077A8.
    const float fx = view.forward.x; // +0x1C.
    const float fy = view.forward.y;
    const float fz = view.forward.z;
    auto& d3d = c.draw.d3d;
    // Bytes 0 and 2 swap: byte 3 stays the alpha, byte 0 becomes red.
    const u32 swapped = (color & 0xFF000000u) | ((color & 0xFFu) << 16) | (color & 0xFF00u) | ((color >> 16) & 0xFFu);
    d3d.vertex_data_color(3, swapped); // D3D 0x00172CC0
    const float half = width * 0.5f; // DAT_0032B0F4.
    const float back = 0.f - half;
    // The side across a segment and the view's forward, unit length.
    const auto side_of = [&](const Vec3& to, const Vec3& from)
    {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float dz = to.z - from.z;
        Vec3 side{dz * fy - dy * fz, fz * dx - dz * fx, dy * fx - fy * dx};
        const float square = side.z * side.z + side.y * side.y + side.x * side.x;
        if (square != 0.f) // DAT_0032B100.
        {
            const long double scale = retail_reciprocal_sqrt(square); // 0x001534C0
            side = Vec3{float(side.x * scale), float(side.y * scale), float(side.z * scale)};
        }
        return side;
    };
    if (count <= 1)
    {
        return;
    }
    Vec3 side = side_of(points[1], points[0]);
    Vec3 left{points[0].x + side.x * half, points[0].y + side.y * half, points[0].z + side.z * half};
    Vec3 right{points[0].x + back * side.x, points[0].y + side.y * back, points[0].z + side.z * back};
    for (i32 index = 1; index < count; ++index)
    {
        const Vec3& point = points[index];
        side = side_of(point, points[index - 1]);
        const Vec3 next_left{point.x + side.x * half, point.y + side.y * half, point.z + side.z * half};
        const Vec3 next_right{point.x + back * side.x, point.y + side.y * back, point.z + side.z * back};
        d3d.begin(8); // D3D 0x00172D10
        d3d.vertex_data2f(9, 0.f, 0.f); // D3D 0x00172C20
        d3d.vertex_data4f(-1, left.x, left.y, left.z, 1.f); // D3D 0x00172C60
        d3d.vertex_data2f(9, 0.f, 1.f);
        d3d.vertex_data4f(-1, next_left.x, next_left.y, next_left.z, 1.f);
        d3d.vertex_data2f(9, 1.f, 1.f);
        d3d.vertex_data4f(-1, next_right.x, next_right.y, next_right.z, 1.f);
        d3d.vertex_data2f(9, 1.f, 0.f);
        d3d.vertex_data4f(-1, right.x, right.y, right.z, 1.f);
        d3d.end(); // D3D 0x00172D50
        left = next_left;
        right = next_right;
    }
}
}
