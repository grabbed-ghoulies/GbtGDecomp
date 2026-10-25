#include "actor_snapshots.h"
#include "d3dx_view.h"
#include "../entities/actor_shape_bounds.h"
#include "../frontend/dialogue_draw_context.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x001565E0
void set_runtime_cull_mode(u32 mode, u32& cull_mode)
{
    cull_mode = mode;
}

// Xbox 0x00116530
std::unique_ptr<RuntimeActorSnapshot> snapshot_runtime_actor(Actor& actor, RuntimeActorSnapshots& context)
{
    const std::array<float, 3> up{0.f, 1.f, 0.f}; // DAT_0032B0F8 is 1.0.
    context.begin_capture();
    reset_dialogue_sprite_state(context.display.draw);
    bind_display_projection(context.display);
    auto record = std::make_unique<RuntimeActorSnapshot>(); // 0x000F72D0(0x34).
    const std::span<u8> bytes = record->bytes;
    auto& d3d = context.d3d;
    const u32 texture = d3d.create_texture(0x40, 0x40, 1, 1, 6, 3);
    put_word(bytes, 0x20, texture);
    const std::uintptr_t surface = d3d.surface_level(texture, 0);
    const std::uintptr_t depth = d3d.depth_stencil();
    const std::uintptr_t color = d3d.render_target();
    d3d.set_render_target(surface, 0);
    d3d.z_enable(0);
    d3d.stencil_enable(0);
    d3d.set_state(0x4035C, 0);
    context.depth_mask = 0;
    d3d.set_state(0x40354, 0x207);
    context.depth_function = 0x207;
    d3d.clear(0, nullptr, 0xF0, 0, 1.f, 0);
    Vec3 center{};
    float radius = 0.f;
    std::memcpy(&center, bytes.data() + 0x14, 12);
    std::memcpy(&radius, bytes.data() + 0x24, 4);
    if (actor_model_world_bound(actor, center, radius))
    {
        std::memcpy(bytes.data() + 0x14, &center, 12);
        put_float(bytes, 0x24, radius);
    }
    const Vec3 eye{center.x, center.y + 50.f, center.z - 10.f}; // DAT_0032B1E0, DAT_0032B10C.
    std::memcpy(bytes.data() + 8, &eye, 12);
    const long double dx = static_cast<long double>(eye.x) - center.x;
    const long double dy = static_cast<long double>(eye.y) - center.y;
    const long double dz = static_cast<long double>(eye.z) - center.z;
    const float distance = float(std::sqrt(dx * dx + dy * dy + dz * dz));
    const float field = float(runtime_arc_sine(static_cast<long double>(radius) / distance) * 2.L);
    std::array<float, 16> projection{};
    perspective_runtime_graphics_matrix(projection.data(), field, 1.f, std::bit_cast<float>(0x3DCCCCCDu),
        std::bit_cast<float>(0x42C83333u));
    std::array<float, 16> view{};
    look_at_runtime_graphics_matrix(view.data(), &eye.x, &center.x, up.data());
    set_dialogue_render_view(view.data(), context.display.draw);
    set_dialogue_render_projection(projection.data(), context.display.draw);
    set_runtime_cull_mode(0x901, context.cull_mode);
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, sizeof(world)); // Actor +0x2AC.
    context.draw_model(actor, world, 1);
    set_runtime_cull_mode(0x900, context.cull_mode);
    d3d.set_render_target(color, depth);
    d3d.z_enable(1);
    d3d.release(surface);
    d3d.release(color);
    d3d.release(depth);
    bytes[0x28] = 0;
    bytes[0x2A] = 0;
    bytes[0x2B] = 0x50;
    bytes[0x29] = 0xFF;
    put_word(bytes, 0x30, 1);
    put_float(bytes, 0x2C, 1.f);
    context.present_capture();
    return record;
}

// Xbox 0x00116750
void append_runtime_actor_snapshot(Actor& actor, RuntimeActorSnapshots& context)
{
    context.list.push_back(snapshot_runtime_actor(actor, context));
    ++context.count;
}

// Xbox 0x001162F0
void age_runtime_actor_snapshots(RuntimeActorSnapshots& context)
{
    if (0x10 < context.count) // DAT_005462F0
    {
        for (const auto& snapshot : context.list)
        {
            if (read_u32(snapshot->bytes, 0x30) != 0)
            {
                std::memset(snapshot->bytes.data() + 0x30, 0, 4);
                --context.count;
            }
        }
    }
    for (size_t index = 0; index < context.list.size();)
    {
        std::array<u8, 0x34>& bytes = context.list[index]->bytes;
        if (read_u32(bytes, 0x30) == 0)
        {
            const float timer = read_f32(bytes, 0x2C) - std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124
            std::memcpy(bytes.data() + 0x2C, &timer, 4);
            if (timer < 0.f)
            {
                context.d3d.release(read_u32(bytes, 0x20)); // D3DResource_Release
                context.list.erase(context.list.begin() + std::ptrdiff_t(index)); // Unlinked, HeapFree
                continue;
            }
        }
        ++index;
    }
}

// Xbox 0x001163D0
void release_runtime_actor_snapshots(RuntimeActorSnapshots& context, u32& decal_enable)
{
    while (!context.list.empty())
    {
        context.d3d.release(read_u32(context.list.front()->bytes, 0x20)); // D3DResource_Release
        context.list.erase(context.list.begin()); // Unlinked, HeapFree
    }
    decal_enable = 0; // DAT_005462EC.
    context.count = 0; // DAT_005462F0.
}
}
