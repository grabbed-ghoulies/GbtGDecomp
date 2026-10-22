#include "scene_point_light_pass.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_frustum.h"
#include "../frontend/dialogue_draw_context.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr float kReach = 250000.f; // DAT_0032B3B8, 500 squared.
constexpr float kPartRadius = 30.f; // 0x41F00000.

u32& deferred(RuntimeScenePointLightPass& context, u32 address)
{
    return context.outer.draw.deferred[(address - 0x181968) / 4];
}

DialogueD3DBoundaries& d3d(RuntimeScenePointLightPass& context)
{
    return context.outer.draw.d3d;
}

void set_state(RuntimeScenePointLightPass& context, u32 method, u32 value)
{
    d3d(context).simple_state(method, value); // D3DDevice_SetRenderState_Simple (0x00171050), ECX method, EDX value
}

using Matrix = std::array<float, 16>;

Matrix transform(RuntimeScenePointLightPass& context, u32 state)
{
    Matrix matrix{};
    d3d(context).get_transform(state, matrix.data()); // D3DDevice_GetTransform (0x00173EC0)
    return matrix;
}

Matrix multiply(RuntimeScenePointLightPass& context, const float* first, const float* second)
{
    Matrix output{};
    d3d(context).multiply(output.data(), first, second); // D3DXMatrixMultiply (0x00151E64)
    return output;
}

// The transposed product the vertex programs take at c0x60..c0x63 (0x00151F62, then SetVertexShaderConstant4).
void load_transform(RuntimeScenePointLightPass& context, const Matrix& matrix)
{
    Matrix transposed{};
    d3d(context).transpose(transposed.data(), matrix.data());
    d3d(context).constants(0x60, transposed.data(), 4); // 0x00173440
}

// D3DXVec3TransformCoord (0x00151D5B): the point through the matrix, divided by w unless w is 1.
Vec3 transform_coord(const Vec3& p, const Matrix& m)
{
    const float x = p.x * m[0] + m[12] + p.y * m[4] + p.z * m[8];
    const float y = p.x * m[1] + m[13] + p.y * m[5] + p.z * m[9];
    const float z = p.x * m[2] + m[14] + p.y * m[6] + p.z * m[10];
    const float w = p.x * m[3] + m[15] + p.y * m[7] + p.z * m[11];
    if (w == 1.f)
    {
        return {x, y, z};
    }
    return {x / w, y / w, z / w};
}

// The light-grid header words 0x00062B00 and 0x00063770 read through EAX.
struct Grid
{
    u32 count_x;
    u32 count_y;
    u32 count_z;
    Vec3 origin;
    float size;
};

Grid grid_at(RuntimeScenePointLightPass& context, u32 grid)
{
    const auto model = context.grid.model;
    if (model.empty())
    {
        throw Error("the point-light pass reads a light grid without the first background's model");
    }
    return Grid{read_u32(model, grid + 0x10), read_u32(model, grid + 0x14), read_u32(model, grid + 0x18),
        Vec3{read_f32(model, grid + 0x20), read_f32(model, grid + 0x24), read_f32(model, grid + 0x28)},
        read_f32(model, grid + 0x38)};
}

// The first background's cell record (model +0x10 -> +0 -> +0x3C, stride 0x1C) by index.
u32 cell_at(RuntimeScenePointLightPass& context, u32 index)
{
    const auto model = context.grid.model;
    const u32 header = read_u32(model, 0x10);
    const u32 grid = read_u32(model, header);
    return read_u32(model, grid + 0x3C) + index * 0x1C;
}

float distance_from_view(const entity_detail::DisplayView& view, const Vec3& minimum)
{
    const float dx = minimum.x - view.position.x;
    const float dy = minimum.y - view.position.y;
    const float dz = minimum.z - view.position.z;
    return (dx * dx + dz * dz) + dy * dy;
}

void draw_cell(RuntimeScenePointLightPass& context, u32 cell)
{
    const auto model = context.grid.model;
    context.stream(0, context.grid.model_address + cell + 0x10, 0x10); // 0x00173610
    context.draw_vertices(5, 0, read_u32(model, cell + 8) >> 4); // 0x00172920
}

u8* part_at(RuntimeScenePointLightPass& context, u32 index)
{
    const size_t offset = size_t(index) * 0x1E0;
    if (offset + 0x1E0 > context.grid.parts.size())
    {
        throw Error("the point-light pass reads a part past the background's +0x950 rows");
    }
    return context.grid.parts.data() + offset;
}

const float* matrix_at(const u8* part, size_t offset)
{
    return reinterpret_cast<const float*>(part + offset);
}

void draw_part(RuntimeScenePointLightPass& context, u32 index)
{
    const auto& grid = context.grid;
    context.stream(0, grid.part_buffers.empty() ? 0u : grid.part_buffers[index], 0x10);
    context.draw_vertices(5, 0, grid.part_buffers.empty() ? 0u : grid.part_vertices[index]);
}

// The light texture's clear colour: bytes R, G, B, A of the stack word as A8R8G8B8.
u32 clear_color(u32 color)
{
    const u32 r = color & 0xFF;
    const u32 g = (color >> 8) & 0xFF;
    const u32 b = (color >> 16) & 0xFF;
    const u32 a = color >> 24;
    return (((a << 8 | r) << 8 | g) << 8) | b;
}

void bind_boot_vertex_shader(RuntimeScenePointLightPass& context, u32 handle, u32 constants)
{
    d3d(context).vertex_shader(context.word(handle)); // 0x001739F0
    const u32 table = context.word(constants);
    if (table != 0)
    {
        d3d(context).constants(0x60, context.floats(table), 0xC); // 0x001735D0, ECX 0x60
    }
}

std::array<float, 4> scaled_color(u32 color, float scale)
{
    constexpr float kByte = std::bit_cast<float>(0x3B808081u); // DAT_0032B154, 1/255.
    return {float(color & 0xFF) * kByte * scale, float((color >> 8) & 0xFF) * kByte * scale,
        float((color >> 16) & 0xFF) * kByte * scale, float(color >> 24) * kByte * scale};
}

const entity_detail::DisplayView& display(RuntimeScenePointLightPass& context)
{
    if (context.view == nullptr)
    {
        throw Error("the point-light pass has no DAT_005077A8 display view");
    }
    return *context.view;
}

std::span<const float, 42> frustum_of(const entity_detail::DisplayView& view)
{
    return std::span<const float, 42>(view.frustum);
}

u8& hint_of(const entity_detail::DisplayView& view)
{
    return const_cast<u8&>(view.mark); // +0x1E0.
}
}

// Xbox 0x001237E0
void draw_runtime_scene_point_lights(RuntimeScenePointLightPass& context)
{
    auto& lights = context.lights;
    const u32 grid = lights.enabled; // DAT_005120F8.
    if (grid == 0 || (lights.count == 0 && context.camera.enabled == 0))
    {
        return;
    }
    context.targets.copy_mode = 0;
    context.targets.mode = 1;
    begin_runtime_scene_point_lights(grid, context.camera.ambient, context); // 0x00062B00
    u32 drawn = 0;
    for (const auto& point : lights.points)
    {
        if (i32(drawn) >= i32(lights.count))
        {
            break;
        }
        if (point.active == 0)
        {
            continue;
        }
        add_runtime_scene_point_light(grid, point.position, point.color, point.second_color, point.unk_14,
            point.unk_18, point.unk_1C, point.unk_20, point.unk_24, context); // 0x00063770
        ++drawn;
    }
    if (context.camera.enabled != 0)
    {
        const Vec3 position = display(context).position;
        add_runtime_scene_point_light(grid, position, context.camera.color, context.camera.second_color,
            context.camera.unk_1C, context.camera.unk_20, context.camera.unk_24, context.camera.unk_28, 0,
            context); // 0x00063770
    }
    finish_runtime_scene_point_lights(context); // 0x00063320
}

// Xbox 0x00062B00, EAX grid
void begin_runtime_scene_point_lights(u32 grid_offset, u32 color, RuntimeScenePointLightPass& context)
{
    auto& targets = context.targets;
    auto& outer = context.outer.d3d;
    const std::array<float, 4> half{0.5f, 0.5f, 0.5f, 0.f}; // DAT_0032B0F4.
    targets.active = 1;
    targets.drawn = 0;
    if (targets.light_texture == 0)
    {
        targets.light_texture = context.address_of(context.pages.page_texture); // DAT_005047DC
        targets.light_surface = context.pages.page_surface; // DAT_005047E0
    }
    if (targets.depth_texture == 0 && targets.mode == 1)
    {
        targets.depth_texture = context.create_texture(context.back_width, context.back_height, 1, 1, 0x2E,
            3); // 0x00177840: D3DFMT_LIN_D24S8.
        targets.depth_surface = outer.surface(DialogueSurfaceHandle(targets.depth_texture), 0); // 0x00177900
    }
    reset_dialogue_sprite_state(context.outer.draw); // 0x0005E110
    deferred(context, 0x181968) |= 0x800;
    deferred(context, 0x1819A0) = 2;
    deferred(context, 0x1819B0) = 2;
    deferred(context, 0x1819A8) = 0;
    deferred(context, 0x1819B8) = 0;
    d3d(context).vertex_shader(0x42); // 0x001739F0
    targets.saved_depth = outer.depth_buffer(); // 0x00174790
    targets.saved_target = outer.back_buffer(0); // 0x00174770
    if (targets.mode == 1)
    {
        outer.render_target(targets.light_surface, targets.depth_surface); // 0x00174170
        set_state(context, 0x4035C, 1);
        deferred(context, 0x181C70) = 1;
        outer.clear(0, nullptr, 0xF1, clear_color(color), 1.f, 0); // 0x00176970
        bind_boot_vertex_shader(context, 0x004272CC, 0x004272C0);
    }
    else
    {
        outer.render_target(targets.light_surface, targets.saved_depth);
        outer.clear(0, nullptr, 0xF0, clear_color(color), 1.f, 0);
        context.z_bias(4); // 0x00171660
        set_state(context, 0x4035C, 0);
        deferred(context, 0x181C70) = 0;
        bind_boot_vertex_shader(context, 0x004272E4, 0x004272D8);
        context.multisample_antialias(0); // 0x00172600
        d3d(context).multisample_mode(2); // 0x001725A0
        context.multisample_target_mode(2); // 0x001725D0
    }
    d3d(context).pixel_shader(0); // 0x00175370
    d3d(context).multisample_mode(2);
    set_state(context, 0x40354, 0x203);
    deferred(context, 0x181C54) = 0x203;
    d3d(context).cull(0x900); // 0x00171420
    set_state(context, 0x40300, 0);
    deferred(context, 0x181C60) = 0;
    const Matrix world = transform(context, 6);
    const Matrix view = transform(context, 0);
    const Matrix projection = transform(context, 1);
    load_transform(context, multiply(context, multiply(context, world.data(), view.data()).data(),
        projection.data()));
    d3d(context).constants(0x64, half.data(), 1); // 0x001733E0
    deferred(context, 0x181968) |= 0x2000;
    context.fog_enable = 0;
    set_state(context, 0x40304, 1);
    deferred(context, 0x181C5C) = 1;
    set_state(context, 0x40344, 1);
    deferred(context, 0x181C68) = 1;
    set_state(context, 0x40348, 1);
    deferred(context, 0x181C6C) = 1;
    set_state(context, 0x40350, 0x8006);
    deferred(context, 0x181C98) = 0x8006;
    d3d(context).stencil_enable(0); // 0x00172330
    const std::array<float, 4> quarter{0.25f, 0.25f, 0.25f, 0.25f}; // DAT_0032B104.
    const std::array<float, 4> one{1.f, 1.f, 1.f, 1.f}; // DAT_0032B0F8.
    d3d(context).constants(0x7E, quarter.data(), 1);
    d3d(context).constants(0x7F, one.data(), 1);
    if (targets.mode == 1)
    {
        set_state(context, 0x40358, 0);
        deferred(context, 0x181C7C) = 0;
        const Grid grid = grid_at(context, grid_offset);
        const auto& camera = display(context);
        u32 index = 0;
        for (u32 z = 0; z < grid.count_z; ++z)
        {
            // FILD of the unsigned count (DAT_0032B144 for the top bit), then the x87 sums.
            const double z_low = double(z) * double(grid.size) + double(grid.origin.z);
            const float z_min = float(z_low);
            const float z_max = float(z_low + double(grid.size));
            for (u32 y = 0; y < grid.count_y; ++y)
            {
                const double y_low = double(y) * double(grid.size) + double(grid.origin.y);
                const float y_min = float(y_low);
                const float y_max = float(y_low + double(grid.size));
                for (u32 x = 0; x < grid.count_x; ++x, ++index)
                {
                    if (!context.grid.background)
                    {
                        continue;
                    }
                    const u32 cell = cell_at(context, index);
                    if (read_u32(context.grid.model, cell + 0xC) == 0)
                    {
                        continue;
                    }
                    const double x_low = double(x) * double(grid.size) + double(grid.origin.x);
                    const Vec3 minimum{float(x_low), y_min, z_min};
                    const Vec3 maximum{float(x_low + double(grid.size)), y_max, z_max};
                    const bool inside = classify_runtime_box_frustum(frustum_of(camera), minimum, maximum,
                        hint_of(camera)) != 0; // 0x00159BA0
                    if (inside && distance_from_view(camera, minimum) < kReach)
                    {
                        draw_cell(context, cell);
                        if (!context.grid.visible.empty())
                        {
                            context.grid.visible.at(index) = 1;
                        }
                    }
                    else if (!context.grid.visible.empty())
                    {
                        context.grid.visible.at(index) = 0;
                    }
                }
            }
        }
        if (context.grid.part_count != 0 && !context.grid.parts.empty())
        {
            const Matrix world_part = transform(context, 6);
            const Matrix view_part = transform(context, 0);
            const Matrix projection_part = transform(context, 1);
            const Matrix view_projection = multiply(context, view_part.data(), projection_part.data());
            for (u32 part = 0; part < context.grid.part_count; ++part)
            {
                const u8* row = part_at(context, part);
                const Vec3& center = context.point(part);
                if (classify_runtime_sphere_frustum(frustum_of(camera), center, kPartRadius,
                        hint_of(camera)) == 0) // 0x00159AD0
                {
                    continue;
                }
                const Matrix local = multiply(context, matrix_at(row, 0x130), matrix_at(row, 0xB0));
                const Matrix placed = multiply(context, world_part.data(), local.data());
                load_transform(context, multiply(context, placed.data(), view_projection.data()));
                draw_part(context, part);
            }
            load_transform(context, multiply(context, multiply(context, world_part.data(), view_part.data()).data(),
                projection_part.data()));
        }
        set_state(context, 0x4035C, 0);
        deferred(context, 0x181C70) = 0;
        set_state(context, 0x40358, 0x1010101);
        deferred(context, 0x181C7C) = 0x1010101;
    }
    d3d(context).pixel_shader(context.word(0x00425E8C)); // 0x00175370
}

// Xbox 0x00063770, EAX grid
void add_runtime_scene_point_light(u32 grid_offset, const Vec3& position, u32 color, u32 second_color, float unk_14,
    float unk_18, float unk_1C, float unk_20, u32 unk_24, RuntimeScenePointLightPass& context)
{
    auto& targets = context.targets;
    const float inner = 1.f / unk_1C;
    const std::array<float, 4> inner_scale{inner, inner, inner, 1.f};
    d3d(context).constants(0x70, inner_scale.data(), 1); // 0x001733E0
    const float outer = 1.f / unk_20;
    const std::array<float, 4> outer_scale{outer, outer, outer, 1.f};
    d3d(context).constants(0x71, outer_scale.data(), 1);
    const auto first = scaled_color(color, unk_14);
    d3d(context).constants(0x65, first.data(), 1);
    const auto second = scaled_color(second_color, unk_18);
    d3d(context).constants(0x66, second.data(), 1);
    const std::array<float, 4> light{position.x, position.y, position.z, 1.f};
    d3d(context).constants(0x6A, light.data(), 1);
    const Grid grid = grid_at(context, grid_offset);
    const float reach = unk_20 > unk_1C ? unk_20 : unk_1C; // COMISS; JA keeps +0x20.
    u8 span;
    if (unk_24 != 0)
    {
        span = 1;
    }
    else
    {
        span = u8(u8(u32(i32(double(reach) / double(grid.size))) + 1) >> 1); // _ftol (0x001673D8), INC AL, SHR AL
    }
    const float step = 1.f / grid.size;
    const i32 cell_x = i32((position.x - grid.origin.x) * step); // CVTTSS2SI
    const i32 cell_y = i32((position.y - grid.origin.y) * step);
    const i32 cell_z = i32((position.z - grid.origin.z) * step);
    i32 x_low = cell_x - i32(span);
    if (x_low < 0)
    {
        x_low = 0;
    }
    i32 x_high = cell_x + i32(span);
    if (x_high >= i32(grid.count_x))
    {
        x_high = i32(grid.count_x) - 1;
    }
    i32 y_low = cell_y - i32(span);
    if (y_low < 0)
    {
        y_low = 0;
    }
    i32 y_high = cell_y + i32(span);
    if (y_high >= i32(grid.count_y))
    {
        y_high = i32(grid.count_y) - 1;
    }
    i32 z_low = cell_z - i32(span);
    if (z_low < 0)
    {
        z_low = 0;
    }
    i32 z_high = cell_z + i32(span);
    if (z_high >= i32(grid.count_z))
    {
        z_high = i32(grid.count_z) - 1;
    }
    const auto& camera = display(context);
    for (i32 x = x_low; x <= x_high; ++x)
    {
        const float x_min = float(x) * grid.size + grid.origin.x;
        const float x_max = x_min + grid.size;
        for (i32 y = y_low; y <= y_high; ++y)
        {
            const float y_min = float(y) * grid.size + grid.origin.y;
            const float y_max = y_min + grid.size;
            for (i32 z = z_low; z <= z_high; ++z)
            {
                const u32 index = u32((i32(grid.count_y) * z + y) * i32(grid.count_x) + x);
                if (!context.grid.background)
                {
                    continue;
                }
                const u32 cell = cell_at(context, index);
                if (read_u32(context.grid.model, cell + 0xC) == 0)
                {
                    continue;
                }
                const float z_min = float(z) * grid.size + grid.origin.z;
                const float z_max = z_min + grid.size;
                if (targets.mode == 1)
                {
                    if (!context.grid.visible.empty() && context.grid.visible.at(index) == 0)
                    {
                        continue;
                    }
                }
                else if (targets.mode != 0
                    || classify_runtime_box_frustum(frustum_of(camera), Vec3{x_min, y_min, z_min},
                        Vec3{x_max, y_max, z_max}, hint_of(camera)) == 0) // 0x00159BA0
                {
                    continue;
                }
                const float dx = x_min - camera.position.x;
                const float dz = z_min - camera.position.z;
                const float dy = y_min - camera.position.y;
                if (!((dz * dz + dy * dy) + dx * dx < kReach))
                {
                    continue;
                }
                draw_cell(context, cell);
                ++targets.drawn;
            }
        }
    }
    if (context.grid.part_count == 0)
    {
        return;
    }
    const float part_reach = unk_1C > unk_20 ? unk_1C : unk_20; // COMISS; JBE keeps +0x20.
    if (context.grid.parts.empty())
    {
        return;
    }
    const Matrix world = transform(context, 6);
    const Matrix view = transform(context, 0);
    const Matrix projection = transform(context, 1);
    const Matrix view_projection = multiply(context, view.data(), projection.data());
    const float reach_squared = part_reach * part_reach;
    for (u32 part = 0; part < context.grid.part_count; ++part)
    {
        const u8* row = part_at(context, part);
        const Vec3& center = context.point(part);
        const float dz = center.z - position.z;
        const float dy = center.y - position.y;
        const float dx = center.x - position.x;
        if (!(reach_squared > (dz * dz + dy * dy) + dx * dx))
        {
            continue;
        }
        if (classify_runtime_sphere_frustum(frustum_of(camera), center, kPartRadius, hint_of(camera)) == 0)
        {
            continue;
        }
        const Matrix local = multiply(context, matrix_at(row, 0x130), matrix_at(row, 0xB0));
        const Matrix placed = multiply(context, world.data(), local.data());
        load_transform(context, multiply(context, placed.data(), view_projection.data()));
        const Matrix inverse = multiply(context, matrix_at(row, 0x170), matrix_at(row, 0x30));
        const Vec3 local_light = transform_coord(position, inverse); // 0x00151D5B
        const std::array<float, 4> moved{local_light.x, local_light.y, local_light.z, 1.f};
        d3d(context).constants(0x6A, moved.data(), 1);
        draw_part(context, part);
    }
    load_transform(context, multiply(context, multiply(context, world.data(), view.data()).data(),
        projection.data()));
}

// Xbox 0x00123640
void reset_runtime_scene_point_light_pass(RuntimeScenePointLightPass& context, u32& camera_light, u32& ambient,
    const std::function<void(std::uintptr_t)>& release)
{
    auto& lights = context.lights;
    for (auto& point : lights.points)
    {
        point.active = 0;
    }
    lights.count = 0;
    lights.enabled = 0;
    lights.unk_100 = 0;
    lights.unk_fc = 0;
    lights.free = 0;
    camera_light = 0;
    ambient = 0;
    auto& targets = context.targets;
    targets.light_texture = 0;
    targets.light_surface = 0;
    if (targets.depth_texture != 0)
    {
        release(targets.depth_texture); // 0x001759F0
        targets.depth_texture = 0;
    }
    if (targets.depth_surface != 0)
    {
        release(targets.depth_surface);
        targets.depth_surface = 0;
    }
}

// Xbox 0x00063320
void finish_runtime_scene_point_lights(RuntimeScenePointLightPass& context)
{
    auto& targets = context.targets;
    auto& outer = context.outer.d3d;
    const float width = float(context.back_width);
    const float height = float(context.back_height);
    targets.active = 0;
    reset_dialogue_sprite_state(context.outer.draw); // 0x0005E110
    set_dialogue_texture_state(context.outer.draw); // 0x0005E2A0
    set_dialogue_alpha_state(context.outer.draw); // 0x0005E0C0
    d3d(context).multisample_mode(0);
    outer.render_target(targets.saved_target, targets.saved_depth); // 0x00174170
    outer.release(targets.saved_target); // 0x001759F0
    targets.saved_target = 0;
    outer.release(targets.saved_depth);
    targets.saved_depth = 0;
    set_state(context, 0x40300, 0);
    deferred(context, 0x181C60) = 0;
    if (targets.mode == 0)
    {
        if (targets.drawn == 0)
        {
            context.z_bias(0);
            d3d(context).cull(0);
            d3d(context).stencil_enable(0);
            set_state(context, 0x4035C, 1);
            deferred(context, 0x181C70) = 1;
            set_state(context, 0x40354, 0x203);
            deferred(context, 0x181C54) = 0x203;
            d3d(context).depth(1); // 0x001722A0
            return;
        }
        context.multisample_antialias(0);
        d3d(context).multisample_mode(0);
        context.multisample_target_mode(0);
    }
    deferred(context, 0x181968) |= 1;
    deferred(context, 0x181970) = 4;
    deferred(context, 0x181974) = 4;
    deferred(context, 0x181978) = 4;
    context.texture(0, targets.light_texture); // 0x001749E0
    set_state(context, 0x40304, 1);
    deferred(context, 0x181C5C) = 1;
    initialize_dialogue_orthographic(context.outer.draw); // 0x000F6260
    if (targets.copy_mode == 0)
    {
        d3d(context).vertex_shader(0x102);
        d3d(context).cull(0);
        set_state(context, 0x40354, 0x207);
        deferred(context, 0x181C54) = 0x207;
        set_state(context, 0x4035C, 0);
        deferred(context, 0x181C70) = 0;
        set_state(context, 0x40344, 0);
        deferred(context, 0x181C68) = 0;
        set_state(context, 0x40348, 0x300);
        deferred(context, 0x181C6C) = 0x300;
        context.z_bias(0);
        deferred(context, 0x181968) |= 0x3000;
        deferred(context, 0x181D0C) = 0;
    }
    else
    {
        throw Error("untranslated Xbox 0x0013DF80 (the point-light pass's back-buffer copy)");
    }
    d3d(context).depth(0); // 0x001722A0
    d3d(context).stencil_enable(0);
    d3d(context).begin(5); // 0x00172D10
    const std::array<std::array<float, 2>, 6> corners{{{0.f, 0.f}, {width, 0.f}, {width, height}, {width, height},
        {0.f, height}, {0.f, 0.f}}};
    for (const auto& corner : corners)
    {
        d3d(context).vertex_data2f(9, corner[0], corner[1]); // 0x00172C20
        d3d(context).vertex_data2f(10, corner[0], corner[1]);
        d3d(context).vertex_data4f(0, corner[0], corner[1], 0.f, 1.f); // 0x00172C60
    }
    d3d(context).end(); // 0x00172D50
    bind_display_projection(context.outer.display); // 0x000F6170
    context.z_bias(0);
    d3d(context).cull(0);
    d3d(context).stencil_enable(0);
    set_state(context, 0x4035C, 1);
    deferred(context, 0x181C70) = 1;
    set_state(context, 0x40354, 0x203);
    deferred(context, 0x181C54) = 0x203;
    d3d(context).depth(1);
}
}
