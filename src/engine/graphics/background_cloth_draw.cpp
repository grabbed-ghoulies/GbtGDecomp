#include "background_cloth_draw.h"
#include "boot_shader_tables.h"
#include "d3dx_view.h"
#include "scene_d3d_device.h"
#include "vector_normalize.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_frustum.h"
#include "actor_snapshots.h"
#include "light_volume_records.h"
#include "refraction_pass.h"
#include "gbtg/model_render.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
using Matrix = std::array<float, 16>;

DialogueD3DBoundaries& d3d(RuntimeBackgroundClothDraw& context)
{
    return context.pass.outer.draw.d3d;
}

u32& deferred(RuntimeBackgroundClothDraw& context, u32 address)
{
    const size_t index = (address - 0x00181968u) / 4;
    auto block = context.pass.outer.draw.deferred;
    if (index >= block.size())
    {
        throw Error("background cloth draw writes past the DAT_00181968 deferred block");
    }
    return block[index];
}

// D3DDevice_SetRenderState_Simple (0x00171050) and its deferred mirror.
void simple(RuntimeBackgroundClothDraw& context, u32 method, u32 value, u32 shadow)
{
    d3d(context).simple_state(method, value);
    deferred(context, shadow) = value;
}

Matrix get_transform(RuntimeBackgroundClothDraw& context, u32 state)
{
    Matrix matrix{};
    d3d(context).get_transform(state, matrix.data()); // D3DDevice_GetTransform (0x00173EC0)
    return matrix;
}

Matrix multiply(RuntimeBackgroundClothDraw& context, const float* first, const float* second)
{
    Matrix output{};
    d3d(context).multiply(output.data(), first, second); // 0x00151E64
    return output;
}

// The frame's world x view x projection, transposed, at c0x60 (0x00151F62 / 0x00184725, then
// SetVertexShaderConstant4).
void load_frame_transform(RuntimeBackgroundClothDraw& context)
{
    const Matrix world = get_transform(context, 6);
    const Matrix view = get_transform(context, 0);
    const Matrix projection = get_transform(context, 1);
    Matrix product = multiply(context, world.data(), view.data());
    product = multiply(context, product.data(), projection.data());
    Matrix transposed{};
    d3d(context).transpose(transposed.data(), product.data());
    d3d(context).constants(0x60, transposed.data(), 4); // 0x00173440
}

void constant(RuntimeBackgroundClothDraw& context, u32 reg, const std::array<float, 4>& value)
{
    d3d(context).constants(reg, value.data(), 1); // D3DDevice_SetVertexShaderConstant1 (0x001733E0)
}

// SetPixelShader, SetVertexShader and, when the shader's constant table is set, its twelve words at c0x60.
void bind_shaders(RuntimeBackgroundClothDraw& context, u32 pixel, BootVertexShader vertex)
{
    d3d(context).pixel_shader(context.pass.word(pixel)); // 0x00175370
    d3d(context).vertex_shader(context.pass.word(vertex.handle)); // 0x001739F0
    const u32 constants = context.pass.word(vertex.constants);
    if (constants != 0)
    {
        d3d(context).constants(0x60, context.pass.floats(constants), 0xC); // 0x001735D0, ECX 0x60
    }
}

const entity_detail::DisplayView& display_view(RuntimeBackgroundClothDraw& context)
{
    if (context.pass.view == nullptr)
    {
        throw Error("background cloth draw has no DAT_005077A8 display view");
    }
    return *context.pass.view;
}

std::span<const float, 42> view_frustum(RuntimeBackgroundClothDraw& context)
{
    return std::span<const float, 42>(display_view(context).frustum);
}

u8& view_hint(RuntimeBackgroundClothDraw& context)
{
    return const_cast<u8&>(display_view(context).mark); // +0x1E0.
}

void multiply_projector(RuntimeBackgroundClothDraw& context)
{
    auto& projector = context.projector;
    projector.product = multiply(context, projector.screen.data(), projector.bias.data()); // 0x00151E64
}

// x87 length of the difference (the sum in x, y, z order).
long double distance(const Vec3& to, const Vec3& from)
{
    const long double dx = (long double)to.x - from.x;
    const long double dy = (long double)to.y - from.y;
    const long double dz = (long double)to.z - from.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 point_of(RuntimeBackgroundCloth& cloth, u32 index)
{
    const PointGraphPoint& point = cloth.point(index);
    return {point.position.x, point.position.y, point.position.z};
}

// 0x00151C08: the length squared through SSE lanes, (x^2 + y^2) + z^2 in single precision.
float xmath_length_squared(const Vec3& vector)
{
    return (vector.x * vector.x + vector.y * vector.y) + vector.z * vector.z;
}

// 0x00151C38: the cross product through x87.
Vec3 xmath_cross(const Vec3& first, const Vec3& second)
{
    return {float((long double)second.z * first.y - (long double)first.z * second.y),
        float((long double)first.z * second.x - (long double)second.z * first.x),
        float((long double)first.x * second.y - (long double)second.x * first.y)};
}

// Unit vector through 0x001534C0 when its length squared is not zero.
Vec3 scaled_unit(const Vec3& vector, float squared)
{
    if (squared == 0.f) // DAT_0032B100
    {
        return vector;
    }
    const long double scale = retail_reciprocal_sqrt(squared); // 0x001534C0
    return {float(vector.x * scale), float(vector.y * scale), float(vector.z * scale)};
}

// The grid's cell counts (+0x10..+0x18), origin (+0x20..+0x28) and cell size (+0x38), offsets in the grid's model.
struct Grid
{
    i32 count_x;
    i32 count_y;
    i32 count_z;
    Vec3 origin;
    float size;
};

Grid grid_at(RuntimeBackgroundClothDraw& context, u32 grid)
{
    const auto model = context.pass.grid.model;
    return Grid{i32(read_u32(model, grid + 0x10)), i32(read_u32(model, grid + 0x14)), i32(read_u32(model, grid + 0x18)),
        Vec3{read_f32(model, grid + 0x20), read_f32(model, grid + 0x24), read_f32(model, grid + 0x28)},
        read_f32(model, grid + 0x38)};
}

// Within the cell (`fraction` of `size`): the near half (or fifth) takes the cell before, the far fifth the cell
// after, else both.
void cell_range(i32 cell, i32 reach, float fraction, float size, float low_share, i32& low, i32& high)
{
    if (!(size * low_share < fraction))
    {
        low = cell - reach;
        high = cell;
        return;
    }
    if (fraction >= size * 0.8f) // DAT_0032B244
    {
        low = cell;
        high = cell + reach;
        return;
    }
    low = cell - reach;
    high = cell + reach;
}

float cell_fraction(float offset, float size)
{
    return offset - float(i32(offset / size)) * size;
}
}

// Xbox 0x00115A80
void draw_runtime_background_cloths(u32 layer, std::vector<RuntimeBackgroundCloth*>& cloths,
    RuntimeBackgroundClothDraw& context)
{
    if (context.background_model() == 0 || cloths.empty())
    {
        return;
    }
    const u32 grid = runtime_light_grid_offset(context.pass.grid.model); // **(model +0x10)
    for (auto* cloth : cloths)
    {
        auto bytes = cloth->bytes;
        if (cloth_header(bytes).layer != layer || cloth_header(bytes).casts_shadow == 0
            || !runtime_background_cloth_in_view(*cloth, view_frustum(context), view_hint(context))) // 0x00114090
        {
            continue;
        }
        capture_runtime_background_cloth_shadow(*cloth, context); // 0x0005DA50
        begin_runtime_background_cloth_shadow(context); // 0x0005DEB0
        project_runtime_background_cloth_shadow(*cloth, grid, context); // 0x0005DFA0
        end_runtime_background_cloth_shadow(context); // 0x0005DF30
    }
    begin_runtime_background_cloth_pass(context); // 0x0005D250
    for (auto* cloth : cloths)
    {
        auto bytes = cloth->bytes;
        if (cloth_header(bytes).layer != layer)
        {
            continue;
        }
        if (cloth_header(bytes).always_drawn == 0
            && classify_runtime_box_frustum(view_frustum(context), cloth_header(bytes).low, cloth_header(bytes).high,
                view_hint(context)) == 0) // 0x00159BA0
        {
            continue;
        }
        if (cloth_header(bytes).second_draw != 0)
        {
            draw_runtime_background_water(*cloth, context); // 0x0005D710
        }
        else
        {
            draw_runtime_background_cloth(*cloth, context); // 0x0005D470
        }
    }
    d3d(context).simple_state(SceneD3DDevice::kTwoSidedLighting, 0); // 0x001717F0
}

// Xbox 0x0005D250
void begin_runtime_background_cloth_pass(RuntimeBackgroundClothDraw& context)
{
    const std::array<float, 4> light{1.f, 1.f, 1.f, 0.f}; // DAT_0032B0F8
    const std::array<float, 4> ambient{0.333f, 0.333f, 0.333f, 0.f}; // DAT_0032B3E8
    const std::array<float, 4> one{1.f, 1.f, 1.f, 1.f};
    const std::array<float, 4> minus{-1.f, -1.f, -1.f, -1.f}; // DAT_0032B120
    bind_display_projection(context.display); // 0x000F6170
    reset_dialogue_sprite_state(context.pass.outer.draw); // 0x0005E110
    set_dialogue_texture_state(context.pass.outer.draw); // 0x0005E2A0
    d3d(context).stencil_enable(0); // 0x00172330
    d3d(context).pixel_shader(0); // 0x00175370
    bind_shaders(context, kClothRampPixelShader, kClothRampVertexShader);
    load_frame_transform(context);
    constant(context, 0x64, light);
    constant(context, 0x65, ambient);
    constant(context, 0x66, one);
    constant(context, 0x67, minus);
    context.pass.texture(1, context.ramp); // 0x001749E0
    context.pass.texture(2, context.ramp);
    d3d(context).simple_state(SceneD3DDevice::kTwoSidedLighting, 1); // 0x001717F0
    deferred(context, 0x00181968) |= 0x1000;
    deferred(context, 0x00181D14) = 0;
    simple(context, 0x4037C, 0x1D01, 0x00181C78);
    d3d(context).cull(0); // 0x00171420
    simple(context, 0x40300, 0, 0x00181C60);
    simple(context, 0x40354, 0x201, 0x00181C54);
    simple(context, 0x4035C, 1, 0x00181C70);
}

// Xbox 0x0005D470, EDI cloth
void draw_runtime_background_cloth(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothDraw& context)
{
    auto bytes = cloth.bytes;
    const u32 rows = cloth_header(bytes).rows;
    const u32 columns = cloth_header(bytes).columns;
    const float row_step = 1.f / float(i32(rows));
    const float column_step = 1.f / float(i32(columns));
    if (cloth_header(bytes).unk_1e4 == 1)
    {
        simple(context, 0x40344, 1, 0x00181C68);
        simple(context, 0x40348, 1, 0x00181C6C);
        simple(context, 0x4035C, 0, 0x00181C70);
    }
    if (cloth_header(bytes).own_shaders == 0)
    {
        bind_shaders(context, kClothColouredPixelShader, kClothColouredVertexShader);
    }
    const u32 texture = cloth_header(bytes).texture;
    if (texture == 0)
    {
        return;
    }
    u8* record = context.texture_record(texture);
    if (record[0x11] != 0)
    {
        record[0x13] = 0;
    }
    bind_dialogue_texture(record, 0, context.pass.outer.draw); // 0x00053550, ESI texture
    const auto* colors = reinterpret_cast<const u32*>(cloth.colors.data());
    float row_coordinate = 0.f;
    u32 base = 0;
    for (u32 row = 0; i32(row) < i32(rows) - 1; ++row)
    {
        const float next_row = row_coordinate + row_step;
        float column_coordinate = 0.f;
        u32* push = d3d(context).begin_push(columns * 0xC + 5); // D3DDevice_BeginPush (0x00171E20)
        push[0] = 0x417FC; // NV097_SET_BEGIN_END
        push[1] = 6; // A triangle strip.
        push[2] = columns * 0x300000 + 0x40001818; // NV097_INLINE_ARRAY, twelve words a column.
        push += 3;
        for (u32 column = 0; column < columns; ++column)
        {
            const PointGraphPoint& top = cloth.point(base + column);
            const PointGraphPoint& bottom = cloth.point(columns + base + column);
            push[0] = std::bit_cast<u32>(top.position.x);
            push[1] = std::bit_cast<u32>(top.position.y);
            push[2] = std::bit_cast<u32>(top.position.z);
            push[3] = colors[base + column];
            push[4] = std::bit_cast<u32>(row_coordinate);
            push[5] = std::bit_cast<u32>(column_coordinate);
            push[6] = std::bit_cast<u32>(bottom.position.x);
            push[7] = std::bit_cast<u32>(bottom.position.y);
            push[8] = std::bit_cast<u32>(bottom.position.z);
            push[9] = colors[columns + base + column];
            push[10] = std::bit_cast<u32>(next_row);
            push[11] = std::bit_cast<u32>(column_coordinate);
            push += 12;
            column_coordinate = column_coordinate + column_step;
        }
        push[0] = 0x417FC;
        push[1] = 0;
        d3d(context).end_push(push + 2); // D3DDevice_EndPush
        base += columns;
        row_coordinate = next_row;
    }
    if (cloth_header(bytes).own_shaders == 0)
    {
        bind_shaders(context, kClothRampPixelShader, kClothRampVertexShader);
    }
}

// Xbox 0x0005D710, EDI cloth
void draw_runtime_background_water(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothDraw& context)
{
    auto bytes = cloth.bytes;
    const float row_step = 1.f / float(i32(cloth_header(bytes).rows));
    const float column_step = 1.f / float(i32(cloth_header(bytes).columns));
    if (cloth_header(bytes).unk_1e4 == 1)
    {
        simple(context, 0x40344, 1, 0x00181C68);
        simple(context, 0x40348, 1, 0x00181C6C);
        simple(context, 0x4035C, 0, 0x00181C70);
    }
    if (cloth_header(bytes).own_shaders == 0)
    {
        bind_shaders(context, kClothColouredPixelShader, kClothColouredVertexShader);
    }
    const u32 texture = cloth_header(bytes).texture;
    if (texture == 0)
    {
        return;
    }
    u8* record = context.texture_record(texture);
    if (record[0x11] != 0)
    {
        record[0x13] = 0;
    }
    bind_dialogue_texture(record, 0, context.pass.outer.draw); // 0x00053550, ESI texture
    context.water_phase = context.water_phase + 0.01f; // DAT_005462B0, DAT_0032B124
    const long double phase = context.water_phase;
    const auto* colors = reinterpret_cast<const u32*>(cloth.colors.data());
    float row_coordinate = 0.f;
    u32 base = 0;
    u32 rows = cloth_header(bytes).rows;
    for (u32 row = 0; i32(row) < i32(rows) - 1; row = (row + 1) & 0xFFFF)
    {
        const float top = float(std::sin((long double)i32(row) / (long double)i32(rows) * (long double)20.f + phase)
            * (long double)0.02f + row_coordinate); // DAT_0032B234, DAT_0032B3E4
        const float next_row = float((long double)row_coordinate + row_step);
        const float bottom = float(std::sin((long double)i32(row + 1) / (long double)i32(rows) * (long double)20.f
            + phase) * (long double)0.02f + next_row);
        const u32 columns = cloth_header(bytes).columns;
        float column_coordinate = 0.f;
        u32* push = d3d(context).begin_push(columns * 0xC + 5); // D3DDevice_BeginPush (0x00171E20)
        push[0] = 0x417FC; // NV097_SET_BEGIN_END
        push[1] = 6; // A triangle strip.
        push[2] = columns * 0x300000 + 0x40001818; // NV097_INLINE_ARRAY, twelve words a column.
        push += 3;
        for (u32 column = 0; column < cloth_header(bytes).columns; column = (column + 1) & 0xFFFF)
        {
            const u32 count = cloth_header(bytes).columns;
            const PointGraphPoint& upper = cloth.point(base + column);
            push[0] = std::bit_cast<u32>(upper.position.x);
            push[1] = std::bit_cast<u32>(upper.position.y);
            push[2] = std::bit_cast<u32>(upper.position.z);
            push[3] = colors[base + column];
            push[4] = std::bit_cast<u32>(top);
            const float across = float(std::cos((long double)i32(column) / (long double)i32(count)
                * (long double)20.f + phase) * (long double)0.02f + column_coordinate);
            column_coordinate = column_coordinate + column_step;
            push[5] = std::bit_cast<u32>(across);
            const PointGraphPoint& lower = cloth.point(cloth_header(bytes).columns + base + column);
            push[6] = std::bit_cast<u32>(lower.position.x);
            push[7] = std::bit_cast<u32>(lower.position.y);
            push[8] = std::bit_cast<u32>(lower.position.z);
            push[9] = colors[cloth_header(bytes).columns + base + column];
            push[10] = std::bit_cast<u32>(bottom);
            push[11] = std::bit_cast<u32>(across);
            push += 12;
        }
        push[0] = 0x417FC;
        push[1] = 0;
        d3d(context).end_push(push + 2); // D3DDevice_EndPush
        rows = cloth_header(bytes).rows;
        base += cloth_header(bytes).columns;
        row_coordinate = next_row;
    }
    if (cloth_header(bytes).own_shaders == 0)
    {
        bind_shaders(context, kClothRampPixelShader, kClothRampVertexShader);
    }
}

// Xbox 0x0005DA50, EAX cloth
void capture_runtime_background_cloth_shadow(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothDraw& context)
{
    auto bytes = cloth.bytes;
    auto& outer = context.pass.outer.d3d;
    const Vec3 up{0.f, 1.f, 0.f};
    const DialogueSurfaceHandle depth = outer.depth_buffer(); // D3DDevice_GetDepthStencilSurface2
    const DialogueSurfaceHandle target = outer.back_buffer(0); // D3DDevice_GetRenderTarget2
    d3d(context).depth(0); // D3DDevice_SetRenderState_ZEnable
    d3d(context).stencil_enable(0);
    simple(context, 0x4035C, 0, 0x00181C70);
    simple(context, 0x40354, 0x207, 0x00181C54);
    if (cloth_header(bytes).shadow_texture == 0)
    {
        cloth_header(bytes).shadow_texture = context.pass.address_of(context.pass.pages.page_texture); // DAT_005047DC
        cloth.shadow_surface = context.pass.pages.page_surface; // DAT_005047E0
        cloth_header(bytes).shadow_surface = u32(cloth.shadow_surface);
    }
    outer.render_target(cloth.shadow_surface, 0); // D3DDevice_SetRenderTarget
    outer.clear(0, nullptr, 0xF0, 0, 1.f, 0); // D3DDevice_Clear
    const Vec3 low = cloth_header(bytes).low;
    const Vec3 high = cloth_header(bytes).high;
    const Vec3 at{(high.x + low.x) * 0.5f, (high.y + low.y) * 0.5f, (high.z + low.z) * 0.5f}; // DAT_0032B0F4
    cloth_header(bytes).shadow_at = at;
    const long double dx = (long double)low.x - high.x;
    const long double dy = (long double)low.y - high.y;
    const long double dz = (long double)low.z - high.z;
    cloth_header(bytes).shadow_radius = float(std::sqrt(dy * dy + dz * dz + dx * dx) * 0.5L);
    const Vec3 light = cloth_header(bytes).light;
    Vec3 eye{light.x * -30.f + at.x, light.y * -30.f + at.y, light.z * -30.f + at.z}; // DAT_0032B3FC
    eye.y = eye.y + 30.f; // DAT_0032B108
    cloth_header(bytes).shadow_eye = eye;
    const long double field_of_view = runtime_arc_sine((long double)cloth_header(bytes).shadow_radius
        / distance(at, eye)) * 2.0L; // 0x001706E0
    std::array<float, 16> projection{};
    perspective_runtime_graphics_matrix(projection.data(), float(field_of_view), 1.f, 0.1f, 100.1f); // 0x00152425
    cloth_header(bytes).shadow_projection = projection;
    std::array<float, 16> view{};
    look_at_runtime_xmath_matrix(view.data(), eye, at, up); // 0x0015233A
    cloth_header(bytes).shadow_view = view;
    reset_dialogue_sprite_state(context.pass.outer.draw); // 0x0005E110
    deferred(context, 0x00181968) |= 0x800;
    deferred(context, 0x001819A0) = 2;
    deferred(context, 0x001819B0) = 2;
    deferred(context, 0x001819A8) = 0;
    deferred(context, 0x001819B8) = 0;
    d3d(context).vertex_shader(0x42); // D3DFVF_XYZ | D3DFVF_DIFFUSE
    set_dialogue_alpha_state(context.pass.outer.draw); // 0x0005E0C0
    d3d(context).vertex_shader(2); // D3DFVF_XYZ
    d3d(context).vertex_data_color(3, 0xFFFFFFFF); // D3DDevice_SetVertexDataColor
    const Matrix world = get_transform(context, 6);
    const Matrix saved_view = get_transform(context, 0);
    const Matrix saved_projection = get_transform(context, 1);
    const Matrix identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    d3d(context).set_transform(6, identity.data()); // D3DDevice_SetTransform
    d3d(context).set_transform(0, view.data());
    d3d(context).set_transform(1, projection.data());
    d3d(context).cull(0);
    const u32 rows = cloth_header(bytes).rows;
    const u32 columns = cloth_header(bytes).columns;
    u32 base = 0;
    for (u32 row = 0; i32(row) < i32(rows) - 1; ++row)
    {
        d3d(context).begin(6); // D3DDevice_Begin, a triangle strip
        for (u32 column = 0; column < columns; ++column)
        {
            const Vec3 top = point_of(cloth, base + column);
            const Vec3 bottom = point_of(cloth, columns + base + column);
            d3d(context).vertex_data4f(-1, top.x, top.y, top.z, 1.f); // D3DDevice_SetVertexData4f
            d3d(context).vertex_data4f(-1, bottom.x, bottom.y, bottom.z, 1.f);
        }
        d3d(context).end(); // D3DDevice_End
        base += columns;
    }
    outer.render_target(target, depth);
    outer.release(target); // D3DResource_Release
    outer.release(depth);
    d3d(context).depth(1);
    d3d(context).set_transform(6, world.data());
    d3d(context).set_transform(0, saved_view.data());
    d3d(context).set_transform(1, saved_projection.data());
}

// Xbox 0x0005DEB0
void begin_runtime_background_cloth_shadow(RuntimeBackgroundClothDraw& context)
{
    bind_display_device_view(context.pass.floats(0x0042A7C0), context.display); // 0x000F6430, ESI 0x0042A7C0
    begin_runtime_shadow_projector(context); // 0x000644F0
    auto& projector = context.projector;
    projector.screen[5] = float(context.pass.back_height) * -0.5f; // DAT_005096EA, DAT_0032B1C4
    projector.screen[0] = float(context.pass.back_width) * 0.5f; // DAT_005096E8, DAT_0032B0F4
    projector.bias[9] = float(context.pass.back_height) * 0.5f;
    projector.bias[8] = projector.screen[0];
    multiply_projector(context);
}

// Xbox 0x0005DF30
void end_runtime_background_cloth_shadow(RuntimeBackgroundClothDraw& context)
{
    context.pass.z_bias(0); // D3DDevice_SetRenderState_ZBias
    d3d(context).cull(0);
    context.pass.multisample_antialias(0);
    d3d(context).multisample_mode(0);
    context.pass.multisample_target_mode(0);
    auto& projector = context.projector;
    projector.screen[0] = 0.5f; // DAT_0032B0F4
    projector.screen[5] = -0.5f; // DAT_0032B1C4
    projector.bias[8] = 0.5f;
    projector.bias[9] = 0.5f;
    multiply_projector(context);
}

// Xbox 0x0005DFA0, EAX cloth
void project_runtime_background_cloth_shadow(RuntimeBackgroundCloth& cloth, u32 grid,
    RuntimeBackgroundClothDraw& context)
{
    auto bytes = cloth.bytes;
    project_runtime_texture_on_grid(grid, cloth_header(bytes).shadow_at, cloth_header(bytes).shadow_eye,
        cloth_header(bytes).shadow_texture, cloth_header(bytes).shadow_radius, 0xB4000000u, false, 1,
        context); // 0x00064820
}

// Xbox 0x000644F0
void begin_runtime_shadow_projector(RuntimeBackgroundClothDraw& context)
{
    const std::array<float, 4> one{1.f, 1.f, 1.f, 1.f}; // DAT_0032B0F8
    const std::array<float, 4> zero{0.f, 0.f, 0.f, 0.f};
    reset_dialogue_sprite_state(context.pass.outer.draw); // 0x0005E110
    set_dialogue_texture_state(context.pass.outer.draw); // 0x0005E2A0
    set_dialogue_alpha_state(context.pass.outer.draw); // 0x0005E0C0
    context.pass.multisample_antialias(0); // 0x00172600
    d3d(context).multisample_mode(1); // 0x001725A0
    context.pass.multisample_target_mode(1); // 0x001725D0
    if (context.projector.alternate == 0)
    {
        bind_shaders(context, kTexturedDiffusePixelShader, kShadowProjectorVertexShader);
    }
    else
    {
        bind_shaders(context, kTexturedDiffuseClippedPixelShader, kShadowProjectorClippedVertexShader);
    }
    deferred(context, 0x00181968) |= 0x801;
    deferred(context, 0x001819A0) = 4;
    deferred(context, 0x001819A8) = 2;
    deferred(context, 0x001819AC) = 0;
    deferred(context, 0x00181970) = 4;
    deferred(context, 0x00181974) = 4;
    d3d(context).simple_state(SceneD3DDevice::kBorderColor, 0); // D3DDevice_SetTextureState_BorderColor(0, 0)
    deferred(context, 0x00181968) |= 1;
    deferred(context, 0x00181980) = 2;
    deferred(context, 0x0018197C) = 2;
    simple(context, 0x40354, 0x203, 0x00181C54);
    context.pass.z_bias(4); // 0x00171660
    simple(context, 0x4035C, 0, 0x00181C70);
    d3d(context).cull(0x900); // 0x00171420
    d3d(context).stencil_enable(0);
    load_frame_transform(context);
    constant(context, 0x6A, one);
    constant(context, 0x73, zero);
    auto& projector = context.projector;
    projector.screen = {0.5f, 0.f, 0.f, 0.f, 0.f, -0.5f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    projector.bias = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.5f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    multiply_projector(context);
}

// Xbox 0x00116460
void draw_runtime_actor_snapshot_decals(u32 grid, const std::vector<std::unique_ptr<RuntimeActorSnapshot>>& snapshots,
    RuntimeBackgroundClothDraw& context)
{
    if (grid == 0) // DAT_005462EC
    {
        return;
    }
    begin_runtime_shadow_projector(context); // 0x000644F0
    for (const auto& snapshot : snapshots) // DAT_005462F8, next at +4
    {
        const std::span<const u8> bytes(snapshot->bytes);
        u32 color = read_u32(bytes, 0x28);
        if (read_u32(bytes, 0x30) == 0)
        {
            // The alpha times the fade, clamped to 0 .. 255 (DAT_0032B158), truncated (0x001673D8) into the top byte.
            float alpha = float(i32(color >> 24)) * read_f32(bytes, 0x2C);
            if (0.f > alpha)
            {
                alpha = 0.f;
            }
            else if (alpha > 255.f)
            {
                alpha = 255.f;
            }
            color = (color & 0x00FFFFFFu) | (u32(u8(truncate_runtime_float(alpha))) << 24);
        }
        project_runtime_texture_on_grid(grid, Vec3{read_f32(bytes, 0x14), read_f32(bytes, 0x18), read_f32(bytes, 0x1C)},
            Vec3{read_f32(bytes, 8), read_f32(bytes, 0xC), read_f32(bytes, 0x10)}, read_u32(bytes, 0x20),
            read_f32(bytes, 0x24), color, false, 1, context); // 0x00064820, ECX the grid, EDX +0x14, EAX 1
    }
    context.pass.z_bias(0); // 0x00171660
    d3d(context).cull(0); // 0x00171420
    context.pass.multisample_antialias(0); // 0x00172600
    d3d(context).multisample_mode(0); // 0x001725A0
    context.pass.multisample_target_mode(0); // 0x001725D0
}

// Xbox 0x00064820, ECX grid, EDX at, EAX reach
i32 project_runtime_texture_on_grid(u32 grid, const Vec3& at, const Vec3& eye, u32 texture, float radius, u32 color,
    bool parts, i32 reach, RuntimeBackgroundClothDraw& context)
{
    if (grid == 0)
    {
        return 0;
    }
    const Vec3 up{0.f, 1.f, 0.f};
    std::array<float, 16> view{};
    look_at_runtime_xmath_matrix(view.data(), eye, at, up); // 0x0015233A
    const long double length = distance(at, eye);
    long double sine = 1.0L; // DAT_0032B370
    if (float(length) != 0.f) // DAT_0032B100
    {
        const float share = radius / float(length);
        if (!(share > 1.f))
        {
            sine = share;
        }
    }
    std::array<float, 16> projection{};
    perspective_runtime_graphics_matrix(projection.data(), float(runtime_arc_sine(sine) * 2.0L), 1.f, 0.1f,
        100.1f); // 0x001706E0, 0x00152425
    Matrix product = multiply(context, view.data(), projection.data());
    product = multiply(context, product.data(), context.projector.product.data()); // DAT_00504900
    RuntimeProjectorFrustum frustum{};
    build_runtime_projector_frustum(frustum, view.data(), projection.data(), eye, at, 0.f); // 0x00064030
    Matrix transposed{};
    d3d(context).transpose(transposed.data(), product.data()); // 0x00151F62
    d3d(context).constants(0x66, transposed.data(), 4); // 0x00173440
    Vec3 direction{eye.x - at.x, eye.y - at.y, eye.z - at.z};
    direction = scaled_unit(direction, (direction.z * direction.z + direction.y * direction.y)
        + direction.x * direction.x);
    constant(context, 0x65, {direction.x, direction.y, direction.z, 1.f});
    constexpr float kByte = std::bit_cast<float>(0x3B808081u); // DAT_0032B154
    constant(context, 0x64, {float(color & 0xFF) * kByte, float((color >> 8) & 0xFF) * kByte,
        float((color >> 16) & 0xFF) * kByte, float(color >> 24) * kByte});
    Vec3 toward{eye.x - at.x, eye.y - at.y, eye.z - at.z};
    toward = scaled_unit(toward, (toward.z * toward.z + toward.y * toward.y) + toward.x * toward.x);
    constant(context, 0x74, {eye.x, eye.y, eye.z, 1.f});
    constant(context, 0x75, {at.x + toward.x * radius, at.y + toward.y * radius, at.z + toward.z * radius, 1.f});
    if (context.projector.alternate != 0)
    {
        constant(context, 0x72, context.projector.alternate_color);
    }
    context.pass.texture(0, texture); // 0x001749E0
    const Grid cells = grid_at(context, grid);
    Vec3 point = at;
    if (direction.x != 0.f || direction.z != 0.f)
    {
        float side_x = direction.x;
        float side_y = 0.f;
        float side_z = direction.z;
        const float flat = direction.z * direction.z + direction.x * direction.x;
        if (flat != 0.f)
        {
            const long double scale = retail_reciprocal_sqrt(flat); // 0x001534C0
            side_x = float(direction.x * scale);
            side_y = float(0.0L * scale); // DAT_0032B100
            side_z = float(direction.z * scale);
        }
        const float offset = cells.size * -0.5f; // DAT_0032B1C4
        point = {at.x + offset * side_x, at.y + offset * side_y, at.z + offset * side_z};
    }
    const float inverse = 1.f / cells.size; // DAT_0032B0F8
    const i32 cell_x = i32((point.x - cells.origin.x) * inverse);
    const i32 cell_y = i32((point.y - cells.origin.y) * inverse);
    const i32 cell_z = i32((point.z - cells.origin.z) * inverse);
    i32 y_low;
    i32 y_high;
    cell_range(cell_y, reach, cell_fraction(point.y - cells.origin.y, cells.size), cells.size, 0.5f, y_low,
        y_high); // DAT_0032B0F4
    i32 x_low;
    i32 x_high;
    cell_range(cell_x, reach, cell_fraction(point.x - cells.origin.x, cells.size), cells.size, 0.2f, x_low,
        x_high); // DAT_0032B3A4
    i32 z_low;
    i32 z_high;
    cell_range(cell_z, reach, cell_fraction(point.z - cells.origin.z, cells.size), cells.size, 0.2f, z_low, z_high);
    const auto model = context.pass.grid.model;
    const u32 cell_rows = read_u32(model, grid + 0x3C);
    u8 hint = 0;
    i32 drawn = 0;
    for (i32 z = z_low; z <= z_high; ++z)
    {
        const float z_min = float(z) * cells.size + cells.origin.z;
        const float z_max = cells.size + z_min;
        for (i32 y = y_low; y <= y_high; ++y)
        {
            const float y_min = float(y) * cells.size + cells.origin.y;
            const float y_max = cells.size + y_min;
            for (i32 x = x_low; x <= x_high; ++x)
            {
                if (x >= cells.count_x || y >= cells.count_y || z >= cells.count_z || x < 0 || y < 0 || z < 0
                    || !context.pass.grid.background)
                {
                    continue;
                }
                const u32 cell = cell_rows + u32((cells.count_y * z + y) * cells.count_x + x) * 0x1C;
                if (read_u32(model, cell + 0xC) == 0)
                {
                    continue;
                }
                const float x_min = float(x) * cells.size + cells.origin.x;
                const float x_max = cells.size + x_min;
                const Vec3 minimum{x_min, y_min, z_min};
                const Vec3 maximum{x_max, y_max, z_max};
                if (classify_runtime_box_frustum(view_frustum(context), minimum, maximum, view_hint(context)) == 0
                    || classify_runtime_box_frustum(std::span<const float, 42>(frustum), minimum, maximum,
                        hint) == 0) // 0x00159BA0
                {
                    continue;
                }
                context.pass.stream(0, context.pass.grid.model_address + cell + 0x10, 0x10); // 0x00173610
                context.pass.draw_vertices(5, 0, read_u32(model, cell + 8) >> 4); // 0x00172920
                ++drawn;
            }
        }
    }
    auto& grid_view = context.pass.grid;
    if (drawn == 0 || !parts || grid_view.part_count == 0)
    {
        return drawn;
    }
    // The cells' bounds, the last row included.
    const Vec3 low{float(x_low) * cells.size + cells.origin.x, float(y_low) * cells.size + cells.origin.y,
        float(z_low) * cells.size + cells.origin.z};
    const Vec3 high{float(x_high + 1) * cells.size + cells.origin.x, float(y_high + 1) * cells.size + cells.origin.y,
        float(z_high + 1) * cells.size + cells.origin.z};
    if (grid_view.parts.empty()) // DAT_0050E374 +0x950.
    {
        return drawn;
    }
    const Matrix frame_world = get_transform(context, 6); // D3DDevice_GetTransform (0x00173EC0)
    const Matrix frame_view = get_transform(context, 0);
    const Matrix frame_projection = get_transform(context, 1);
    const Matrix view_projection = multiply(context, frame_view.data(), frame_projection.data());
    for (u32 part = 0; part < grid_view.part_count; ++part)
    {
        const size_t offset = size_t(part) * 0x1E0;
        if (offset + 0x1E0 > grid_view.parts.size())
        {
            throw Error("0x00064820 reads a background part row outside DAT_0050E374 +0x950");
        }
        const u8* row = grid_view.parts.data() + offset;
        const Vec3 point{read_f32(std::span<const u8>(row, 0x1E0), 0x1B0),
            read_f32(std::span<const u8>(row, 0x1E0), 0x1B4), read_f32(std::span<const u8>(row, 0x1E0), 0x1B8)};
        if (low.x > point.x || point.x > high.x || low.y > point.y || point.y > high.y || low.z > point.z
            || point.z > high.z) // COMISS; JA
        {
            continue;
        }
        const auto matrix_at = [row](u32 at)
        {
            return reinterpret_cast<const float*>(row + at);
        };
        const Matrix local = multiply(context, matrix_at(0x130), matrix_at(0xB0)); // 0x00151E64
        Matrix placed = multiply(context, frame_world.data(), local.data());
        placed = multiply(context, placed.data(), view_projection.data());
        Matrix transposed{};
        d3d(context).transpose(transposed.data(), placed.data()); // 0x00151F62
        d3d(context).constants(0x60, transposed.data(), 4); // 0x00173440
        // The projector's direction into the part's space (0x00151DD9: the upper 3 x 3 only).
        const Matrix inverse = multiply(context, matrix_at(0x170), matrix_at(0x30));
        const Vec3 turned{(direction.x * inverse[0] + direction.y * inverse[4]) + direction.z * inverse[8],
            (direction.x * inverse[1] + direction.y * inverse[5]) + direction.z * inverse[9],
            (direction.x * inverse[2] + direction.y * inverse[6]) + direction.z * inverse[10]};
        constant(context, 0x65, {turned.x, turned.y, turned.z, 1.f}); // 0x001733E0
        const Matrix projected = multiply(context, local.data(), product.data());
        d3d(context).transpose(transposed.data(), projected.data());
        d3d(context).constants(0x66, transposed.data(), 4);
        const u32 buffer = grid_view.part_buffers.empty() ? 0u : grid_view.part_buffers.at(part); // DAT_0050E37C
        context.pass.stream(0, buffer, 0x10); // 0x00173610
        const u32 vertices = grid_view.part_buffers.empty() ? 0u : grid_view.part_vertices.at(part); // DAT_0050E380
        context.pass.draw_vertices(5, 0, vertices); // 0x00172920
    }
    load_frame_transform(context); // c0x60 back to world x view x projection.
    return drawn;
}

// Xbox 0x00064030, ECX frustum, EAX view
void build_runtime_projector_frustum(RuntimeProjectorFrustum& frustum, const float* view, const float* projection,
    const Vec3& eye, const Vec3& at, float near_offset)
{
    std::array<float, 16> inverse_view{};
    inverse_runtime_graphics_matrix(inverse_view.data(), nullptr, view); // 0x00151FAD (0x00184792's bytes)
    std::array<float, 16> inverse_projection{};
    inverse_runtime_graphics_matrix(inverse_projection.data(), nullptr, projection);
    // UNK_002FF6E8: the eye and a far corner, four times.
    constexpr std::array<std::array<float, 4>, 8> kCorners{{{0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f}, {-1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f}, {-1.f, -1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f}, {1.f, -1.f, 1.f, 1.f}}};
    std::array<Vec3, 8> points{};
    for (u32 index = 0; index < 8; ++index)
    {
        std::array<float, 4> corner = kCorners[index];
        transform_runtime_xmath_vector4(corner.data(), corner.data(), inverse_projection.data()); // 0x00151E1E
        transform_runtime_xmath_vector4(corner.data(), corner.data(), inverse_view.data());
        const float scale = 1.f / corner[3]; // DAT_0032B0F8
        points[index] = {scale * corner[0], scale * corner[1], scale * corner[2]};
    }
    Vec3 direction{at.x - eye.x, at.y - eye.y, at.z - eye.z};
    normalize_runtime_graphics_vector(direction, direction); // D3DXVec3Normalize (0x00184F99)
    const float far_x = direction.x * 100.f; // DAT_0032B12C
    const float far_y = direction.y * 100.f;
    const float far_z = direction.z * 100.f;
    float* back = frustum.data() + (0x98 - 0x48) / 4 + 18;
    back[0] = 0.f - direction.x;
    back[1] = 0.f - direction.y;
    back[2] = 0.f - direction.z;
    back[3] = (back[1] * (direction.y * near_offset + at.y) + (direction.z * near_offset + at.z) * back[2]
        + back[0] * (direction.x * near_offset + at.x)) / (back[0] * back[0] + back[1] * back[1] + back[2] * back[2]);
    float* front = frustum.data() + (0x88 - 0x48) / 4 + 18;
    front[0] = 0.f - (0.f - direction.x);
    front[1] = 0.f - (0.f - direction.y);
    front[2] = 0.f - back[2];
    front[3] = (front[1] * (far_y + at.y) + front[0] * (far_x + at.x) + (far_z + at.z) * front[2])
        / (front[0] * front[0] + front[1] * front[1] + front[2] * front[2]);
    for (u32 side = 0; side < 4; ++side)
    {
        const Vec3& origin = points[side * 2];
        const Vec3& corner = points[side * 2 + 1];
        const Vec3& next = points[(side * 2 + 3) & 7];
        const float across = corner.x - next.x;
        float nx = (corner.z - next.z) * (corner.y - origin.y) - (corner.y - next.y) * (corner.z - origin.z);
        float ny = (corner.z - origin.z) * across - (corner.z - next.z) * (corner.x - origin.x);
        float nz = (corner.y - next.y) * (corner.x - origin.x) - (corner.y - origin.y) * across;
        const float squared = nz * nz + ny * ny + nx * nx;
        if (squared != 0.f)
        {
            const long double scale = retail_reciprocal_sqrt(squared); // 0x001534C0
            nx = float(nx * scale);
            ny = float(ny * scale);
            nz = float(nz * scale);
        }
        float* plane = frustum.data() + 18 + side * 4;
        plane[0] = nx;
        plane[1] = ny;
        plane[2] = nz;
        plane[3] = (corner.z * nz + corner.y * ny + corner.x * nx) / (nz * nz + nx * nx + ny * ny);
    }
}

// Xbox 0x0015233A
void look_at_runtime_xmath_matrix(float* output, const Vec3& eye, const Vec3& at, const Vec3& up)
{
    Vec3 forward{at.x - eye.x, at.y - eye.y, at.z - eye.z}; // 0x00151C86
    normalize_runtime_xmath_vector(forward, forward); // 0x00151CE0
    Vec3 right = xmath_cross(up, forward); // 0x00151C38
    normalize_runtime_xmath_vector(right, right);
    const Vec3 upward = xmath_cross(forward, right);
    output[0] = right.x;
    output[4] = right.y;
    output[8] = right.z;
    output[1] = upward.x;
    output[5] = upward.y;
    output[9] = upward.z;
    output[12] = -(right.y * eye.y + right.z * eye.z + right.x * eye.x);
    output[2] = forward.x;
    output[6] = forward.y;
    output[10] = forward.z;
    output[13] = -(upward.y * eye.y + upward.z * eye.z + upward.x * eye.x);
    output[14] = -(forward.y * eye.y + forward.z * eye.z + forward.x * eye.x);
    output[3] = 0.f;
    output[7] = 0.f;
    output[11] = 0.f;
    output[15] = 1.f;
}

// Xbox 0x00151CE0
void normalize_runtime_xmath_vector(Vec3& output, const Vec3& vector)
{
    const float squared = xmath_length_squared(vector); // 0x00151C08
    if (runtime_graphics_scalars_equal(squared, 1.f)) // 0x00151CB0 (0x00184600's bounds)
    {
        if (&output != &vector)
        {
            output = vector;
        }
        return;
    }
    if (!(squared > std::bit_cast<float>(0x2EDBE6FEu))) // DAT_00203738
    {
        output = {0.f, 0.f, 0.f};
        return;
    }
    Vec3 divided;
    divide_runtime_graphics_vector(divided, vector, float(std::sqrt((long double)squared))); // 0x00151BE0
    output = divided;
}

// Xbox 0x00151E1E
void transform_runtime_xmath_vector4(float* output, const float* vector, const float* matrix)
{
    const float x = vector[0];
    const float y = vector[1];
    const float z = vector[2];
    const float w = vector[3];
    output[0] = x * matrix[0] + y * matrix[4] + z * matrix[8] + w * matrix[12];
    output[1] = x * matrix[1] + y * matrix[5] + z * matrix[9] + w * matrix[13];
    output[2] = x * matrix[2] + y * matrix[6] + z * matrix[10] + w * matrix[14];
    output[3] = x * matrix[3] + y * matrix[7] + z * matrix[11] + w * matrix[15];
}

// Xbox 0x000FE5A0
void capture_runtime_light_volume(u8 volume, RuntimeBackgroundClothDraw& context, RuntimeLightVolumeCasters& casters)
{
    auto& outer = context.pass.outer.d3d;
    LightVolumeRecord& record = g_light_records.at(volume); // DAT_00509C20 + volume * 0x25C.
    const Vec3 up{0.f, 1.f, 0.f};
    const DialogueSurfaceHandle depth = outer.depth_buffer(); // D3DDevice_GetDepthStencilSurface2
    const DialogueSurfaceHandle target = outer.back_buffer(0); // D3DDevice_GetRenderTarget2
    d3d(context).depth(0); // D3DDevice_SetRenderState_ZEnable
    d3d(context).stencil_enable(0);
    simple(context, 0x4035C, 0, 0x00181C70);
    simple(context, 0x40354, 0x207, 0x00181C54);
    constexpr u32 offset_enabled = 0; // DAT_0050D750: BSS that nothing in the XBE writes.
    constexpr Vec3 offset{}; // DAT_0050D754..DAT_0050D75C.
    if (casters.candle.active != 0) // DAT_0050D740.
    {
        record.light = casters.candle.position; // DAT_0050D744..DAT_0050D74C.
    }
    else if (offset_enabled != 0)
    {
        record.light = {record.caster.position.x - offset.x, record.caster.position.y - offset.y,
            record.caster.position.z - offset.z};
    }
    set_runtime_cull_mode(0x901, casters.cull_mode); // 0x001565E0
    outer.render_target(casters.surface, 0); // D3DDevice_SetRenderTarget(DAT_00504848, 0)
    outer.clear(0, nullptr, 0xF0, 0, 1.f, 0); // D3DDevice_Clear
    record.radius = record.caster.radius; // +0x1CC from +0x14.
    for (i32 index = 0; index < record.caster_count; ++index)
    {
        const LightVolumeCaster& caster = record.casters[size_t(index)];
        const long double dx = (long double)record.caster.position.x - caster.position.x;
        const long double dy = (long double)record.caster.position.y - caster.position.y;
        const long double dz = (long double)record.caster.position.z - caster.position.z;
        const float reach = float(((long double)caster.height + caster.radius) * 0.5L
            + std::sqrt((dx * dx + dz * dz) + dy * dy)); // DAT_0032B0F4
        if (reach > record.radius) // FCOMIP; JBE keeps the old reach.
        {
            record.radius = reach;
        }
    }
    const long double lx = (long double)record.caster.position.x - record.light.x;
    const long double ly = (long double)record.caster.position.y - record.light.y;
    const long double lz = (long double)record.caster.position.z - record.light.z;
    const float span = float(std::sqrt((lx * lx + lz * lz) + ly * ly));
    const float field_of_view = float(runtime_arc_sine((long double)record.radius / span) * 2.0L); // 0x001706E0
    std::array<float, 16> projection{};
    perspective_runtime_graphics_matrix(projection.data(), field_of_view, 1.f, 0.1f, 1000.1f); // 0x00184E2A
    std::array<float, 16> view{};
    const float eye[3]{record.light.x, record.light.y, record.light.z};
    const float at[3]{record.caster.position.x, record.caster.position.y, record.caster.position.z};
    const float upward[3]{up.x, up.y, up.z};
    look_at_runtime_graphics_matrix(view.data(), eye, at, upward); // 0x001851AD
    set_dialogue_render_view(view.data(), context.pass.outer.draw); // 0x00156330
    set_dialogue_render_projection(projection.data(), context.pass.outer.draw); // 0x00156400
    auto& renderer = casters.models.lights.environment.upload.renderer;
    const auto draw_caster = [&](const LightVolumeCaster& caster, u32 kind)
    {
        if (caster.model == nullptr)
        {
            throw Error("0x000FE5A0 draws a light volume caster with no model");
        }
        const u32 cpu = casters.model_cpu(*caster.model);
        AnimationMatrix world{};
        std::memcpy(world.data(), caster.matrix.data(), sizeof(world));
        apply_runtime_model_materials(casters.models.image, cpu, renderer); // 0x000F94F0, EAX the model
        context.pass.multisample_antialias(0); // 0x00172600
        d3d(context).multisample_mode(2); // 0x001725A0
        context.pass.multisample_target_mode(2); // 0x001725D0
        draw_runtime_model_instance(*caster.model, casters.models.image, cpu, world, kind, renderer,
            casters.models.matrices); // 0x000F9120, EBX kind, ESI the +0x1C matrix
        context.pass.multisample_antialias(0);
        d3d(context).multisample_mode(0);
        context.pass.multisample_target_mode(0);
    };
    draw_caster(record.caster, 1);
    for (i32 index = 0; index < record.caster_count; ++index)
    {
        draw_caster(record.casters[size_t(index)], 1);
        draw_caster(record.casters[size_t(index)], 2); // 0x000F9040, EBX 2
    }
    outer.render_target(target, depth);
    outer.release(target); // D3DResource_Release
    if (depth != 0)
    {
        outer.release(depth);
    }
    d3d(context).depth(1);
    simple(context, 0x40354, 0x203, 0x00181C54);
    simple(context, 0x4035C, 1, 0x00181C70);
    set_runtime_cull_mode(0x900, casters.cull_mode); // 0x001565E0
}

// Xbox 0x000FE9B0..0x000FEA65
void project_runtime_light_volume(i32 index, u8 volume, u32 grid, u32 color, RuntimeBackgroundClothDraw& context,
    RuntimeLightVolumeCasters& casters)
{
    capture_runtime_light_volume(volume, context, casters); // 0x000FE5A0
    begin_runtime_shadow_projector(context); // 0x000644F0
    auto& projector = context.projector;
    projector.screen[0] = 0.5f; // DAT_0032B0F4
    projector.screen[5] = -0.5f; // DAT_0032B1C4
    projector.bias[8] = 0.5f;
    projector.bias[9] = 0.5f;
    projector.product = multiply(context, projector.screen.data(), projector.bias.data()); // 0x00151E64
    const LightVolumeRecord& record = g_light_records.at(volume);
    // EBX walks the records by the loop's index, not the ordered volume: +0x1D8's low byte.
    const bool parts = u8(g_light_records.at(size_t(index)).player) != 0;
    project_runtime_texture_on_grid(grid, record.caster.position, record.light, casters.texture, record.radius,
        color, parts, 1, context); // 0x00064820, ECX grid, EDX +8, EAX 1
    context.pass.z_bias(0); // 0x00171660
    d3d(context).cull(0); // 0x00171420
    context.pass.multisample_antialias(0); // 0x00172600
    d3d(context).multisample_mode(0); // 0x001725A0
    context.pass.multisample_target_mode(0); // 0x001725D0
}

// Xbox 0x0005E340
void begin_runtime_untextured_draw(RuntimeBackgroundClothDraw& context)
{
    deferred(context, 0x001819A0) = 2;
    deferred(context, 0x001819B0) = 2;
    deferred(context, 0x001819A8) = 0;
    deferred(context, 0x00181968) |= 0x800;
    deferred(context, 0x001819B8) = 0;
    d3d(context).vertex_shader(0x42); // D3DFVF_XYZ | D3DFVF_DIFFUSE
}

// Xbox 0x0011BA50
void draw_runtime_refraction(RuntimeSceneRefraction& refraction, u32 disabled, RuntimeBackgroundClothDraw& context)
{
    if (refraction.mesh == nullptr || disabled != 0) // DAT_00510580, DAT_00510650.
    {
        return;
    }
    auto& outer = context.pass.outer.d3d;
    // The back buffer so far into book page 0's texture (DAT_005047DC), which the quad samples in stage 2.
    const u32 back_texture = context.pass.address_of(context.pass.pages.page_texture);
    const DialogueSurfaceHandle back = outer.back_buffer(0); // D3DDevice_GetBackBuffer2, DAT_00545B2C
    const DialogueSurfaceHandle level = outer.surface(DialogueSurfaceHandle(back_texture),
        0); // D3DTexture_GetSurfaceLevel2, DAT_00545B28
    outer.copy_rects(back, level); // D3DDevice_CopyRects
    outer.release(back); // D3DResource_Release
    outer.release(level);
    bind_display_projection(context.display); // 0x000F6170
    reset_dialogue_sprite_state(context.pass.outer.draw); // 0x0005E110
    begin_runtime_untextured_draw(context); // 0x0005E340
    // The mesh's triangles mark the stencil: always passing, incremented where they land.
    simple(context, 0x4035C, 1, 0x00181C70);
    d3d(context).stencil_enable(1); // D3DDevice_SetRenderState_StencilEnable
    outer.clear(0, nullptr, 2, 0, 0.f, 0); // D3DDevice_Clear, the stencil to 0
    simple(context, 0x40368, 0, 0x00181C8C);
    simple(context, 0x40364, 0x207, 0x00181C88);
    simple(context, 0x40378, 0x8507, 0x00181C84);
    d3d(context).simple_state(0x40370, 0x1E00); // D3DDevice_SetRenderState_StencilFail (0x001723C0)
    simple(context, 0x40374, 0x1E00, 0x00181C80);
    d3d(context).vertex_shader(2); // D3DFVF_XYZ
    d3d(context).vertex_data_color(3, 0xFF000000); // D3DDevice_SetVertexDataColor
    d3d(context).cull(0); // D3DDevice_SetRenderState_CullMode
    const std::span<const u8> model = refraction.model;
    const size_t mesh_offset = size_t(refraction.mesh - model.data());
    const u32 vertices = read_u32(model, u32(mesh_offset));
    const u32 indices = read_u32(model, u32(mesh_offset + 4));
    const u32 triangles = read_u16(model, u32(mesh_offset + 8));
    u32* push = d3d(context).begin_push(triangles * 9 + 5); // D3DDevice_BeginPush_4
    push[0] = 0x417FC; // NV097_SET_BEGIN_END
    push[1] = 6;
    push[2] = (triangles * 9 << 18) + 0x40001818; // NV097_INLINE_ARRAY, non-incrementing
    u32* cursor = push + 3;
    for (u32 triangle = 0; triangle < triangles; ++triangle)
    {
        for (u32 corner = 0; corner < 3; ++corner)
        {
            const u32 index = read_u16(model, indices + triangle * 6 + corner * 2);
            for (u32 axis = 0; axis < 3; ++axis)
            {
                *cursor++ = read_u32(model, vertices + index * 0xC + axis * 4);
            }
        }
    }
    cursor[0] = 0x417FC;
    cursor[1] = 0;
    d3d(context).end_push(cursor + 2); // D3DDevice_EndPush
    // The quad over the screen, where the stencil is marked: the back buffer through the noise.
    initialize_dialogue_orthographic(context.pass.outer.draw); // 0x000F6260
    set_dialogue_texture_state(context.pass.outer.draw); // 0x0005E2A0
    simple(context, 0x40300, 0, 0x00181C60);
    simple(context, 0x40304, 0, 0x00181C5C);
    simple(context, 0x40364, 0x201, 0x00181C88);
    simple(context, 0x4035C, 0, 0x00181C70);
    d3d(context).vertex_data_color(3, 0xFFFFFFFF);
    deferred(context, 0x00181970) = 1;
    deferred(context, 0x00181968) |= 1;
    deferred(context, 0x00181974) = 1;
    context.pass.texture(0, refraction.noise); // D3DDevice_SetTexture, DAT_00510584
    deferred(context, 0x00181A70) = 3;
    deferred(context, 0x00181A74) = 3;
    deferred(context, 0x00181968) |= 4;
    deferred(context, 0x00181A78) = 3;
    context.pass.texture(2, back_texture);
    d3d(context).pixel_shader(context.pass.word(kRefractionPixelShader)); // D3DDevice_SetPixelShader
    d3d(context).vertex_shader(0x140304); // D3DFVF_XYZRHW | D3DFVF_TEX3, sets 1 and 2 three-dimensional
    d3d(context).draw_vertices_up(8, 4, refraction.quad.data(), 0x30); // D3DDevice_DrawVerticesUP, a quad
    d3d(context).stencil_enable(0);
    simple(context, 0x4035C, 1, 0x00181C70);
    simple(context, 0x40304, 1, 0x00181C5C);
    bind_display_projection(context.display); // JMP 0x000F6170
}
}
