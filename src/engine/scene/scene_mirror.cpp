#include "scene_mirror.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_frustum.h"
#include "../entities/entity.h"
#include "../frontend/dialogue_draw_outer.h"
#include "../graphics/d3dx_matrix.h"
#include "../graphics/d3dx_view.h"
#include "../graphics/vector_normalize.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
u16 half_at(std::span<const u8> bytes, u32 offset)
{
    if (offset + 2 > bytes.size())
    {
        throw Error("mirror block read outside the background model");
    }
    u16 value;
    std::memcpy(&value, bytes.data() + offset, 2);
    return value;
}

Vec3 vertex_at(std::span<const u8> bytes, u32 offset)
{
    if (offset + 12 > bytes.size())
    {
        throw Error("mirror vertex outside the background model");
    }
    Vec3 value;
    std::memcpy(&value, bytes.data() + offset, 12);
    return value;
}

u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}
}

// Xbox 0x0011F450
void setup_runtime_scene_mirrors(std::span<const u8> background_model, RuntimeSceneMirrors& mirrors)
{
    mirrors.model = background_model;
    mirrors.data = 0;
    if (!background_model.empty())
    {
        mirrors.data = read_u32(background_model, read_u32(background_model, 0x10) + 0x18);
    }
    if (mirrors.data == 0)
    {
        return;
    }
    mirrors.count = half_at(background_model, mirrors.data);
    if (mirrors.count == 0)
    {
        return;
    }
    mirrors.boxes.assign(mirrors.count, {});
    const u32 rows = read_u32(background_model, mirrors.data + 4);
    const u32 vertices = read_u32(background_model, mirrors.data + 8);
    const u32 triangles = read_u32(background_model, mirrors.data + 0xC);
    for (i32 index = 0; index < i32(mirrors.count); ++index)
    {
        const u32 first = half_at(background_model, rows + 2 + u32(index) * 4);
        const Vec3 start = vertex_at(background_model, vertices + half_at(background_model, triangles + first * 6) * 0xC);
        auto& box = mirrors.boxes[size_t(index)];
        box = {start.x, start.y, start.z, start.x, start.y, start.z};
        const i32 corners = i32(half_at(background_model, rows + u32(index) * 4)) * 3;
        if (1 < corners)
        {
            u32 cursor = first * 6;
            for (i32 corner = 1; corner < corners; ++corner)
            {
                cursor += 2;
                const Vec3 point = vertex_at(background_model, vertices + half_at(background_model, triangles + cursor) * 0xC);
                if (point.x < box[0])
                {
                    box[0] = point.x;
                }
                if (point.y < box[1])
                {
                    box[1] = point.y;
                }
                if (point.z < box[2])
                {
                    box[2] = point.z;
                }
                if (box[3] < point.x)
                {
                    box[3] = point.x;
                }
                if (box[4] < point.y)
                {
                    box[4] = point.y;
                }
                if (box[5] < point.z)
                {
                    box[5] = point.z;
                }
            }
        }
    }
}

// Xbox 0x0011F310
u32 runtime_mirror_side(const Vec3& position, const RuntimeSceneMirrors& mirrors)
{
    if (mirrors.data != 0)
    {
        const u32 vertices = read_u32(mirrors.model, mirrors.data + 8);
        const Vec3 a = vertex_at(mirrors.model, vertices);
        const Vec3 b = vertex_at(mirrors.model, vertices + 0xC);
        const Vec3 c = vertex_at(mirrors.model, vertices + 0x18);
        const float x = (c.z - a.z) * (b.y - a.y) - (c.y - a.y) * (b.z - a.z);
        const float z = (c.y - a.y) * (b.x - a.x) - (b.y - a.y) * (c.x - a.x);
        const float y = (b.z - a.z) * (c.x - a.x) - (c.z - a.z) * (b.x - a.x);
        if (0.f <= (0.f - (a.z * z + a.y * y + a.x * x)) + position.z * z + position.y * y + position.x * x)
        {
            return 1;
        }
    }
    return 0;
}

// Xbox 0x0013E070
void build_runtime_view_frustum(std::array<float, 42>& frustum, const float* view, const float* projection,
    const Vec3& eye, const Vec3& target, float behind)
{
    std::array<float, 16> view_inverse;
    std::array<float, 16> projection_inverse;
    inverse_runtime_graphics_matrix(view_inverse.data(), nullptr, view);
    inverse_runtime_graphics_matrix(projection_inverse.data(), nullptr, projection);
    // DAT_00314FD0: eight homogeneous corners, each the eye point (0, 0, 0, 1) then a far corner.
    static constexpr std::array<std::array<float, 4>, 8> kCorners{{
        {0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f}, {-1.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f}, {-1.f, -1.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f}, {1.f, -1.f, 1.f, 1.f}}};
    std::array<Vec3, 8> points;
    for (size_t index = 0; index < kCorners.size(); ++index)
    {
        float corner[4]{kCorners[index][0], kCorners[index][1], kCorners[index][2], kCorners[index][3]};
        transform_runtime_graphics_vector4(corner, corner, projection_inverse.data());
        transform_runtime_graphics_vector4(corner, corner, view_inverse.data());
        const float scale = 1.f / corner[3];
        points[index] = {scale * corner[0], scale * corner[1], scale * corner[2]};
    }
    Vec3 direction{target.x - eye.x, target.y - eye.y, target.z - eye.z};
    normalize_runtime_graphics_vector(direction, direction);
    const float far_z = direction.z * 1000.f; // DAT_0032B164.
    const float far_x = direction.x * 1000.f;
    const float far_y = direction.y * 1000.f;
    frustum[38] = 0.f - direction.x;
    frustum[39] = 0.f - direction.y;
    const float back_z = 0.f - direction.z;
    frustum[40] = back_z;
    frustum[41] = (frustum[39] * (direction.y * behind + eye.y) + (direction.z * behind + eye.z) * back_z
        + frustum[38] * (direction.x * behind + eye.x))
        / (frustum[38] * frustum[38] + frustum[39] * frustum[39] + back_z * back_z);
    frustum[34] = direction.x;
    frustum[35] = direction.y;
    frustum[36] = direction.z;
    frustum[37] = (frustum[35] * (far_y + eye.y) + frustum[34] * (far_x + eye.x) + (far_z + eye.z) * frustum[36])
        / (frustum[34] * frustum[34] + frustum[35] * frustum[35] + frustum[36] * frustum[36]);
    u32 opposite = 3;
    size_t plane = 18;
    for (size_t pair = 0; opposite < 10; ++pair, opposite += 2, plane += 4)
    {
        const Vec3& a = points[pair * 2];
        const Vec3& b = points[pair * 2 + 1];
        const Vec3& c = points[opposite & 7];
        const float across = b.x - c.x;
        float x = (b.z - c.z) * (b.y - a.y) - (b.y - c.y) * (b.z - a.z);
        float y = (b.z - a.z) * across - (b.z - c.z) * (b.x - a.x);
        float z = (b.y - c.y) * (b.x - a.x) - (b.y - a.y) * across;
        const float squared = z * z + y * y + x * x;
        if (squared != 0.f)
        {
            const long double scale = retail_reciprocal_sqrt(squared);
            x = float(x * scale);
            y = float(y * scale);
            z = float(z * scale);
        }
        frustum[plane] = x;
        frustum[plane + 1] = y;
        frustum[plane + 2] = z;
        frustum[plane + 3] = (b.z * z + b.y * y + b.x * x) / (z * z + x * x + y * y);
    }
}

// Xbox 0x0013E520
void build_runtime_mirror_frustum(const Vec3& eye, const Vec3& target, float radius, std::array<float, 42>& frustum)
{
    const float up[3]{0.f, 1.f, 0.f};
    std::array<float, 16> view;
    look_at_runtime_graphics_matrix(view.data(), &eye.x, &target.x, up);
    const float distance = float(std::sqrt(static_cast<long double>(target.x - eye.x) * (target.x - eye.x)
        + static_cast<long double>(target.y - eye.y) * (target.y - eye.y)
        + static_cast<long double>(target.z - eye.z) * (target.z - eye.z)));
    long double sine = 1.L; // DAT_0032B370 (double 1.0).
    if (distance != 0.f)
    {
        const float ratio = radius / distance;
        if (!(ratio > 1.f))
        {
            sine = ratio;
        }
    }
    const long double angle = runtime_arc_sine(sine);
    std::array<float, 16> projection;
    perspective_runtime_graphics_matrix(projection.data(), float(angle + angle), 1.f,
        std::bit_cast<float>(0x3DCCCCCDu), std::bit_cast<float>(0x447A0666u));
    build_runtime_view_frustum(frustum, view.data(), projection.data(), eye, target, -1000.f);
}

// Xbox 0x0011F690
void draw_runtime_scene_mirror(RuntimeSceneMirrorPass& c)
{
    using namespace entity_detail;
    auto& mirrors = c.mirrors;
    if (mirrors.count == 0 || c.enabled == 0)
    {
        return;
    }
    auto& draw = c.display.draw;
    auto& d3d = draw.d3d;
    const Vec3 eye = g_display_view->position;
    float nearest = 0.f;
    bool found = false;
    u32 selected = 0;
    u32 counter = 0;
    u32 index = 0;
    do
    {
        const auto& box = mirrors.boxes.at(index);
        if (classify_runtime_box_frustum(g_display_view->frustum, {box[0], box[1], box[2]}, {box[3], box[4], box[5]},
            g_display_view->mark) != 0)
        {
            const float x = (box[3] - box[0]) * 0.5f + box[0] - eye.x;
            const float y = (box[1] + (box[4] - box[1]) * 0.5f) - eye.y;
            const float z = (box[2] + (box[5] - box[2]) * 0.5f) - eye.z;
            const float squared = z * z + y * y + x * x;
            if (!found || squared < nearest)
            {
                nearest = squared;
                selected = counter;
                found = true;
            }
        }
        ++counter;
        index = counter & 0xFFFF;
    } while (i32(index) < i32(mirrors.count));
    if (!found)
    {
        return;
    }
    selected &= 0xFFFF;
    const auto& box = mirrors.boxes.at(selected);
    if (classify_runtime_box_frustum(g_display_view->frustum, {box[0], box[1], box[2]}, {box[3], box[4], box[5]},
        g_display_view->mark) != 0)
    {
        auto& view = *g_display_view;
        d3d.simple_state(0x4035C, 1);
        deferred(draw, 0x181C70) = 1;
        d3d.simple_state(0x40354, 0x203);
        deferred(draw, 0x181C54) = 0x203;
        d3d.multisample_mode(2);
        bind_display_projection(c.display);
        const i32 saved_x = view.fitted_x;
        const i32 saved_y = view.fitted_y;
        const i32 saved_width = view.fitted_width;
        const i32 saved_height = view.fitted_height;
        const float saved_near = view.near_clip;
        const float saved_far = view.far_clip;
        const Vec3 camera = view.position;
        const u32 rows = read_u32(mirrors.model, mirrors.data + 4);
        const u32 vertices = read_u32(mirrors.model, mirrors.data + 8);
        const u32 triangles = read_u32(mirrors.model, mirrors.data + 0xC);
        const u32 first = triangles + half_at(mirrors.model, rows + selected * 4 + 2) * 6;
        const Vec3 p0 = vertex_at(mirrors.model, vertices + half_at(mirrors.model, first) * 0xC);
        const Vec3 p1 = vertex_at(mirrors.model, vertices + half_at(mirrors.model, first + 2) * 0xC);
        const Vec3 p2 = vertex_at(mirrors.model, vertices + half_at(mirrors.model, first + 4) * 0xC);
        const float ex = p1.x - p0.x;
        const float ey = p1.y - p0.y;
        const float ez = p1.z - p0.z;
        const float fx = p2.x - p0.x;
        const float fz = p2.z - p0.z;
        const float fy = p2.y - p0.y;
        Vec3 normal{fz * ey - fy * ez, ez * fx - fz * ex, fy * ex - ey * fx};
        const float squared = normal.z * normal.z + normal.y * normal.y + normal.x * normal.x;
        if (squared != 0.f)
        {
            const long double scale = retail_reciprocal_sqrt(squared);
            normal.x = float(normal.x * scale);
            normal.y = float(normal.y * scale);
            normal.z = float(normal.z * scale);
        }
        const float offset = 0.f - (p0.z * normal.z + p0.y * normal.y + p0.x * normal.x);
        mirrors.plane_equation[3] = offset;
        const float distance = 0.f - (camera.z * normal.z + camera.y * normal.y + camera.x * normal.x + offset);
        Vec3 reflected;
        reflected.x = ((distance * normal.x + camera.x) - camera.x) * 2.f + camera.x; // DAT_0032B110.
        reflected.z = ((distance * normal.z + camera.z) - camera.z) * 2.f + camera.z;
        reflected.y = ((distance * normal.y + camera.y) - camera.y) * 2.f + camera.y;
        mirrors.plane_equation[0] = normal.x;
        mirrors.plane_equation[1] = normal.y;
        mirrors.plane_equation[2] = normal.z;
        view.position = reflected;
        std::array<float, 16> device_view;
        d3d.get_transform(0, device_view.data());
        Vec3 point = p0;
        transform_runtime_graphics_coord(point, point, device_view.data());
        Vec3 view_normal = normal;
        transform_runtime_graphics_normal(view_normal, view_normal, device_view.data());
        float plane[4];
        plane_from_runtime_point_normal(plane, &point.x, &view_normal.x);
        std::array<float, 16> reflection;
        reflect_runtime_graphics_matrix(reflection.data(), plane);
        multiply_runtime_graphics_matrices(mirrors.reflected_view.data(), device_view.data(), reflection.data());
        set_dialogue_render_view(mirrors.reflected_view.data(), draw);
        const auto& m = mirrors.reflected_view;
        const std::array<float, 12> transform{m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10],
            m[14]};
        set_display_transform(transform.data(), c.display);
        const std::array<float, 12> translation{1.f, 0.f, 0.f, reflected.x, 0.f, 1.f, 0.f, reflected.y, 0.f, 0.f,
            1.f, reflected.z};
        std::array<float, 12> rotation;
        multiply_affine_matrices(rotation.data(), transform.data(), translation.data());
        view.rotation_transform = rotation;
        std::array<float, 12> inverse;
        invert_model_matrix(inverse.data(), transform.data());
        set_display_inverse_transform(inverse.data(), c.display);
        view.position = reflected;
        view.near_clip = 1.f;
        view.far_clip = 1000.f;
        rebuild_display_projection(c.display);
        bind_display_projection_only(mirrors.projection_transpose, c.display);
        const Vec3 center{(box[3] + box[0]) * 0.5f, (box[4] + box[1]) * 0.5f, (box[5] + box[2]) * 0.5f};
        const long double dx = box[0] - box[3];
        const long double dy = box[1] - box[4];
        const long double dz = box[2] - box[5];
        const float half = float(std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5L);
        mirrors.camera = reflected;
        const float cx = reflected.x - center.x;
        const float cy = reflected.y - center.y;
        const float cz = reflected.z - center.z;
        if (half * half < cz * cz + cy * cy + cx * cx)
        {
            build_runtime_mirror_frustum(reflected, center, half, mirrors.frustum);
            mirrors.sphere_test = 1;
        }
        else
        {
            mirrors.sphere_test = 0;
        }
        c.cull_mode = 0x901; // 0x001565E0.
        c.draw_bins(1);
        c.cull_mode = 0x900;
        view.position = camera;
        view.near_clip = saved_near;
        view.far_clip = saved_far;
        set_display_viewport(saved_x, saved_y, saved_width, saved_height);
        rebuild_display_view(c.display);
        rebuild_display_projection(c.display);
        bind_display_projection(c.display);
        d3d.multisample_mode(0);
        c.outer.clear(0, nullptr, 3, 0, 1.f, 0);
        reset_dialogue_sprite_state(draw);
        deferred(draw, 0x181968) |= 0x800;
        deferred(draw, 0x1819A0) = 2;
        deferred(draw, 0x1819B0) = 2;
        deferred(draw, 0x1819A8) = 0;
        deferred(draw, 0x1819B8) = 0;
        d3d.vertex_shader(0x42);
        d3d.simple_state(0x40358, 0);
        deferred(draw, 0x181C7C) = 0;
        d3d.vertex_shader(2);
        d3d.vertex_data_color(3, 0x64646464);
        d3d.cull(0);
        d3d.simple_state(0x4035C, 1);
        deferred(draw, 0x181C70) = 1;
        d3d.stencil_enable(1);
        d3d.vertex_shader(2);
        d3d.vertex_data_color(3, 0xFF000000);
        d3d.cull(0);
        const u32 count = half_at(mirrors.model, rows + selected * 4);
        u32* push = d3d.begin_push(count * 9 + 5);
        push[0] = 0x417FC;
        push[1] = 5;
        push[2] = count * 0x240000 + 0x40001818;
        push += 3;
        for (u32 triangle = 0; triangle < count; ++triangle)
        {
            const u32 row = triangles + (half_at(mirrors.model, rows + selected * 4 + 2) + triangle) * 6;
            for (u32 corner = 0; corner < 3; ++corner)
            {
                const Vec3 vertex = vertex_at(mirrors.model, vertices + half_at(mirrors.model, row + corner * 2) * 0xC);
                std::memcpy(push, &vertex, 12);
                push += 3;
            }
        }
        push[0] = 0x417FC;
        push[1] = 0;
        d3d.end_push(push + 2);
        bind_display_projection(c.display);
        d3d.stencil_enable(0);
        d3d.simple_state(0x4035C, 1);
        deferred(draw, 0x181C70) = 1;
    }
    reset_dialogue_sprite_state(draw);
    c.outer.clear(0, nullptr, 2, 0, 1.f, 0);
}
}
