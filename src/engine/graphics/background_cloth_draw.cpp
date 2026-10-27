#include "background_cloth_draw.h"
#include "d3dx_view.h"
#include "scene_d3d_device.h"
#include "vector_normalize.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_frustum.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = background_cloth_field;
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
void bind_shaders(RuntimeBackgroundClothDraw& context, u32 pixel, u32 vertex, u32 table)
{
    d3d(context).pixel_shader(context.pass.word(pixel)); // 0x00175370
    d3d(context).vertex_shader(context.pass.word(vertex)); // 0x001739F0
    const u32 constants = context.pass.word(table);
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
    const float* point = cloth.point(index);
    return {point[0], point[1], point[2]};
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
        if (get(bytes, field::layer) != layer || get(bytes, field::casts_shadow) == 0
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
        if (get(bytes, field::layer) != layer)
        {
            continue;
        }
        if (get(bytes, field::always_drawn) == 0
            && classify_runtime_box_frustum(view_frustum(context), get(bytes, field::low), get(bytes, field::high),
                view_hint(context)) == 0) // 0x00159BA0
        {
            continue;
        }
        if (get(bytes, field::second_draw) != 0)
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
    bind_shaders(context, 0x00425ECC, 0x00427344, 0x00427338);
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
    const u32 rows = get(bytes, field::rows);
    const u32 columns = get(bytes, field::columns);
    const float row_step = 1.f / float(i32(rows));
    const float column_step = 1.f / float(i32(columns));
    if (get(bytes, field::unk_1e4) == 1)
    {
        simple(context, 0x40344, 1, 0x00181C68);
        simple(context, 0x40348, 1, 0x00181C6C);
        simple(context, 0x4035C, 0, 0x00181C70);
    }
    if (get(bytes, field::own_shaders) == 0)
    {
        bind_shaders(context, 0x00425EBC, 0x0042732C, 0x00427320);
    }
    const u32 texture = get(bytes, field::texture);
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
            const float* top = cloth.point(base + column);
            const float* bottom = cloth.point(columns + base + column);
            push[0] = std::bit_cast<u32>(top[0]);
            push[1] = std::bit_cast<u32>(top[1]);
            push[2] = std::bit_cast<u32>(top[2]);
            push[3] = colors[base + column];
            push[4] = std::bit_cast<u32>(row_coordinate);
            push[5] = std::bit_cast<u32>(column_coordinate);
            push[6] = std::bit_cast<u32>(bottom[0]);
            push[7] = std::bit_cast<u32>(bottom[1]);
            push[8] = std::bit_cast<u32>(bottom[2]);
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
    if (get(bytes, field::own_shaders) == 0)
    {
        bind_shaders(context, 0x00425ECC, 0x00427344, 0x00427338);
    }
}

// Xbox 0x0005D710, EDI cloth
void draw_runtime_background_water(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothDraw& context)
{
    auto bytes = cloth.bytes;
    const float row_step = 1.f / float(i32(get(bytes, field::rows)));
    const float column_step = 1.f / float(i32(get(bytes, field::columns)));
    if (get(bytes, field::unk_1e4) == 1)
    {
        simple(context, 0x40344, 1, 0x00181C68);
        simple(context, 0x40348, 1, 0x00181C6C);
        simple(context, 0x4035C, 0, 0x00181C70);
    }
    if (get(bytes, field::own_shaders) == 0)
    {
        bind_shaders(context, 0x00425EBC, 0x0042732C, 0x00427320);
    }
    const u32 texture = get(bytes, field::texture);
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
    u32 rows = get(bytes, field::rows);
    for (u32 row = 0; i32(row) < i32(rows) - 1; row = (row + 1) & 0xFFFF)
    {
        const float top = float(std::sin((long double)i32(row) / (long double)i32(rows) * (long double)20.f + phase)
            * (long double)0.02f + row_coordinate); // DAT_0032B234, DAT_0032B3E4
        const float next_row = float((long double)row_coordinate + row_step);
        const float bottom = float(std::sin((long double)i32(row + 1) / (long double)i32(rows) * (long double)20.f
            + phase) * (long double)0.02f + next_row);
        const u32 columns = get(bytes, field::columns);
        float column_coordinate = 0.f;
        u32* push = d3d(context).begin_push(columns * 0xC + 5); // D3DDevice_BeginPush (0x00171E20)
        push[0] = 0x417FC; // NV097_SET_BEGIN_END
        push[1] = 6; // A triangle strip.
        push[2] = columns * 0x300000 + 0x40001818; // NV097_INLINE_ARRAY, twelve words a column.
        push += 3;
        for (u32 column = 0; column < get(bytes, field::columns); column = (column + 1) & 0xFFFF)
        {
            const u32 count = get(bytes, field::columns);
            const float* upper = cloth.point(base + column);
            push[0] = std::bit_cast<u32>(upper[0]);
            push[1] = std::bit_cast<u32>(upper[1]);
            push[2] = std::bit_cast<u32>(upper[2]);
            push[3] = colors[base + column];
            push[4] = std::bit_cast<u32>(top);
            const float across = float(std::cos((long double)i32(column) / (long double)i32(count)
                * (long double)20.f + phase) * (long double)0.02f + column_coordinate);
            column_coordinate = column_coordinate + column_step;
            push[5] = std::bit_cast<u32>(across);
            const float* lower = cloth.point(get(bytes, field::columns) + base + column);
            push[6] = std::bit_cast<u32>(lower[0]);
            push[7] = std::bit_cast<u32>(lower[1]);
            push[8] = std::bit_cast<u32>(lower[2]);
            push[9] = colors[get(bytes, field::columns) + base + column];
            push[10] = std::bit_cast<u32>(bottom);
            push[11] = std::bit_cast<u32>(across);
            push += 12;
        }
        push[0] = 0x417FC;
        push[1] = 0;
        d3d(context).end_push(push + 2); // D3DDevice_EndPush
        rows = get(bytes, field::rows);
        base += get(bytes, field::columns);
        row_coordinate = next_row;
    }
    if (get(bytes, field::own_shaders) == 0)
    {
        bind_shaders(context, 0x00425ECC, 0x00427344, 0x00427338);
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
    if (get(bytes, field::shadow_texture) == 0)
    {
        set(bytes, field::shadow_texture, context.pass.address_of(context.pass.pages.page_texture)); // DAT_005047DC
        cloth.shadow_surface = context.pass.pages.page_surface; // DAT_005047E0
        set(bytes, field::shadow_surface, u32(cloth.shadow_surface));
    }
    outer.render_target(cloth.shadow_surface, 0); // D3DDevice_SetRenderTarget
    outer.clear(0, nullptr, 0xF0, 0, 1.f, 0); // D3DDevice_Clear
    const Vec3 low = get(bytes, field::low);
    const Vec3 high = get(bytes, field::high);
    const Vec3 at{(high.x + low.x) * 0.5f, (high.y + low.y) * 0.5f, (high.z + low.z) * 0.5f}; // DAT_0032B0F4
    set(bytes, field::shadow_at, at);
    const long double dx = (long double)low.x - high.x;
    const long double dy = (long double)low.y - high.y;
    const long double dz = (long double)low.z - high.z;
    set(bytes, field::shadow_radius, float(std::sqrt(dy * dy + dz * dz + dx * dx) * 0.5L));
    const Vec3 light = get(bytes, field::light);
    Vec3 eye{light.x * -30.f + at.x, light.y * -30.f + at.y, light.z * -30.f + at.z}; // DAT_0032B3FC
    eye.y = eye.y + 30.f; // DAT_0032B108
    set(bytes, field::shadow_eye, eye);
    const long double field_of_view = runtime_arc_sine((long double)get(bytes, field::shadow_radius)
        / distance(at, eye)) * 2.0L; // 0x001706E0
    std::array<float, 16> projection{};
    perspective_runtime_graphics_matrix(projection.data(), float(field_of_view), 1.f, 0.1f, 100.1f); // 0x00152425
    set(bytes, field::shadow_projection, projection);
    std::array<float, 16> view{};
    look_at_runtime_xmath_matrix(view.data(), eye, at, up); // 0x0015233A
    set(bytes, field::shadow_view, view);
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
    const u32 rows = get(bytes, field::rows);
    const u32 columns = get(bytes, field::columns);
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
    project_runtime_texture_on_grid(grid, get(bytes, field::shadow_at), get(bytes, field::shadow_eye),
        get(bytes, field::shadow_texture), get(bytes, field::shadow_radius), 0xB4000000u, false, 1,
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
        bind_shaders(context, 0x00425E6C, 0x0042729C, 0x00427290);
    }
    else
    {
        bind_shaders(context, 0x00425E7C, 0x004272B4, 0x004272A8);
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
    if (drawn != 0 && parts)
    {
        throw Error("0x00064820's background part pass is not translated");
    }
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
}
