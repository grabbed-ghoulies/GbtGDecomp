#include "door_shadow_draw.h"
#include "../audio/listener_settings.h"

#include <bit>

namespace gbtg
{
// Xbox 0x0005AC40
u32 select_runtime_door_shadow_bounds(std::span<const u8> row, const Vec3& camera,
    RuntimeDoorShadowBounds& bounds, const RuntimeRelocationImage& image)
{
    const auto scalar = [row](u32 offset)
    {
        return std::bit_cast<float>(read_u32(row, offset));
    };
    const float x = ((scalar(0x78) * camera.z + scalar(0x74) * camera.y) + scalar(0x70) * camera.x) + scalar(0x7C);
    const float y = ((scalar(0x88) * camera.z + scalar(0x84) * camera.y) + scalar(0x80) * camera.x) + scalar(0x8C);
    const float z = ((scalar(0x98) * camera.z + scalar(0x94) * camera.y) + scalar(0x90) * camera.x) + scalar(0x9C);
    // COMISS/JC takes the branch for unordered operands as well as less-than.
    const auto inside = [](float value, float first, float second)
    {
        if (first >= value && value >= second)
        {
            return true;
        }
        return value >= first && second >= value;
    };
    if (inside(x, scalar(0xA0), scalar(0xAC)) && inside(y, scalar(0xA4), scalar(0xB0)) &&
        inside(z, scalar(0xA8), scalar(0xB4)))
    {
        return 0;
    }
    const u32 definition = 0x00424998 + read_u32(row, 0) * 0x94;
    const float depth = std::bit_cast<float>(image.word(definition));
    const float margin = std::bit_cast<float>(image.word(0x0032B214));
    const float first = scalar(0xA8) - z;
    const float second = scalar(0xB4) - z;
    if (second * second > first * first)
    {
        bounds.depth_start = depth + margin;
        bounds.depth_end = depth - margin;
        bounds.left = image.word(definition, 4);
        bounds.right = image.word(definition, 8);
    }
    else
    {
        bounds.depth_start = depth - margin;
        bounds.depth_end = depth + margin;
        bounds.left = image.word(definition, 8);
        bounds.right = image.word(definition, 4);
    }
    return 1;
}

// DAT_005462A8 is BSS that no retail code writes (only 0x0005AE40 reads it), so it stays 0.
u32 g_door_shadow_bottom = 0;

// Xbox 0x0005AE40
void draw_runtime_door_shadow(std::span<const u8> row, const float* view, const float* projection,
    const Vec3& camera, const RuntimeRelocationImage& image, DialogueD3DBoundaries& d3d)
{
    RuntimeDoorShadowBounds bounds;
    const float alpha = std::bit_cast<float>(image.word(0x00424994 + read_u32(row, 0) * 0x94));
    const float margin = std::bit_cast<float>(image.word(0x0032B234));
    bounds.alpha_start = alpha - margin;
    bounds.alpha_end = alpha + margin;
    if (select_runtime_door_shadow_bounds(row, camera, bounds, image) == 0)
    {
        return;
    }
    std::array<float, 16> world, matrix;
    std::memcpy(world.data(), row.data() + 0x30, 64);
    d3d.multiply(matrix.data(), world.data(), view);
    d3d.multiply(matrix.data(), matrix.data(), projection);
    d3d.transpose(matrix.data(), matrix.data());
    d3d.constants(0x60, matrix.data(), 4);
    u32* push = d3d.begin_push(0x105);
    *push++ = 0x417FC;
    *push++ = 8;
    *push++ = 0x44001818;
    const float depth_delta = bounds.depth_end - bounds.depth_start;
    const float alpha_delta = bounds.alpha_end - bounds.alpha_start;
    float fraction = 0.f;
    do
    {
        const float depth = depth_delta * fraction + bounds.depth_start;
        const long double opacity = static_cast<long double>(alpha_delta) * fraction + bounds.alpha_start;
        const u32 color = u32(u8(truncate_runtime_float(opacity))) << 24;
        const u32 bottom = g_door_shadow_bottom; // DAT_005462A8.
        const u32 top = image.word(0x00425114);
        const std::array<u32, 16> vertices{bounds.left, bottom, std::bit_cast<u32>(depth), color,
            bounds.right, bottom, std::bit_cast<u32>(depth), color,
            bounds.right, top, std::bit_cast<u32>(depth), color,
            bounds.left, top, std::bit_cast<u32>(depth), color};
        std::memcpy(push, vertices.data(), sizeof(vertices));
        push += vertices.size();
        fraction += std::bit_cast<float>(image.word(0x0032B1F8));
    } while (std::bit_cast<float>(image.word(0x0032B0F8)) > fraction);
    *push++ = 0x417FC;
    *push++ = 0;
    d3d.end_push(push);
}

// Xbox 0x0005B020
void draw_runtime_door_shadows(RuntimeDoorShadowDraw& context)
{
    auto* pool = context.pool;
    if (pool == nullptr)
    {
        return;
    }
    const auto count = (pool->end - pool->begin) / pool->stride;
    if (count == 0)
    {
        return;
    }
    context.clear_model_view();
    context.d3d.cull(0);
    auto& states = context.states;
    const auto set = [&states](u32 method, u32 value, u32& shadow)
    {
        states.simple(method, value);
        shadow = value;
    };
    set(0x40350, 0x8006, states.blend_operation);
    set(0x40344, 0x302, states.source_blend);
    set(0x40348, 0x303, states.destination_blend);
    set(0x40304, 1, states.blend);
    set(0x40300, 1, states.alpha_test);
    set(0x40340, 5, states.alpha_function);
    set(0x40354, 0x203, states.depth_function);
    set(0x4035C, 1, states.depth_write);
    states.z_enable(1);
    context.deferred[2] = 3;
    context.deferred[3] = 3;
    context.deferred[6] = 2;
    context.deferred[5] = 2;
    context.deferred[0] |= 1;
    context.deferred[7] = 2;
    context.vertex_shader(0x13);
    context.pixel_shader(context.image.word(0x00425F1C));
    std::array<float, 16> view, projection;
    context.d3d.get_transform(0, view.data());
    context.d3d.get_transform(1, projection.data());
    for (auto* row = pool->begin; row < pool->end; row += 0x154)
    {
        const std::span<const u8> fields(row, 0x154);
        const auto* definition = context.image.pointer(0x00424990 + read_u32(fields, 0) * 0x94);
        if (read_u32(std::span<const u8>(definition, 4), 0) != 0 &&
            (read_u16(fields, 6) == 0 || (read_u32(fields, 0xB8) != 0 && read_u16(fields, 8) == 0)))
        {
            context.draw(fields, view.data(), projection.data());
        }
    }
}
}
