#include "actor_effect_nodes.h"
#include "effect_particles.h"
#include "gbtg/entity.h"

#include <bit>
#include <cstring>
#include <new>

namespace gbtg
{
std::array<ActorEffectNode*, 150> g_actor_effect_nodes{};
i32 g_last_actor_effect = -1;

namespace
{
template<class T>
T read(const ActorEffectNode& effect, size_t offset)
{
    T result;
    std::memcpy(&result, effect.bytes.data() + offset, sizeof(T));
    return result;
}
template<class T>
void write(ActorEffectNode& effect, size_t offset, const T& value)
{
    std::memcpy(effect.bytes.data() + offset, &value, sizeof(T));
}
// Particle slot `index`'s free link (its next_free) from the node's start.
i32 particle_link_offset(i32 index)
{
    return i32(actor_effect_field::particles_offset) + index * i32(effect_particle_field::kSize)
        + i32(effect_particle_field::next_free.offset);
}
}

// Xbox 0x000ED170
void initialize_actor_effect_free_chain(ActorEffectNode& effect)
{
    i16 slot = 0;
    if (i16(get(effect.bytes, actor_effect_field::capacity)) - 1 > 0)
    {
        i32 index = 0;
        do
        {
            slot = static_cast<i16>(slot + 1);
            write(effect, particle_link_offset(index), slot);
            index = slot;
        }
        while (index < i16(get(effect.bytes, actor_effect_field::capacity)) - 1);
    }
    write(effect, particle_link_offset(i16(get(effect.bytes, actor_effect_field::capacity)) - 1), u16{0xFFFF});
}

// Xbox 0x000ED120
void set_actor_effect_script(ActorEffectNode& effect, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (effect.script.data() != nullptr)
    {
        release_animation_resource(registry, effect.script);
    }
    if (name != nullptr && std::memcmp(name, "aid_", 4) == 0)
    {
        const auto script = acquire_animation_resource(registry, name);
        if (script.data() == nullptr)
        {
            resource_error(error, 2);
        }
        effect.script = script;
    }
}

// Xbox 0x000ED1C0
ActorEffectNode* create_actor_effect_node(const char* name, u16 count,
    const Vec3& position, const Vec3& euler, AnimationResourceRegistry& registry,
    ResourceErrorState& error)
{
    const size_t bytes = static_cast<size_t>(count) * effect_particle_field::kSize + 0x4E0;
    auto* allocation = engine_heap_allocate(static_cast<i32>(sizeof(ActorEffectNode) + bytes));
    auto* effect = new (allocation) ActorEffectNode{};
    effect->bytes = {reinterpret_cast<u8*>(effect + 1), bytes};
    std::memset(effect->bytes.data(), 0, bytes);
    store_effect_field(effect->bytes, actor_effect_field::emitting, -3.f);
    store_effect_field(effect->bytes, actor_effect_field::capacity, count);
    store_effect_field(effect->bytes, actor_effect_field::life, -1.f);
    store_effect_field(effect->bytes, actor_effect_field::scale, 1.f);
    store_effect_field(effect->bytes, actor_effect_field::particle_size, 1.f);
    store_effect_field(effect->bytes, actor_effect_field::volume, u32{0});
    store_effect_field(effect->bytes, actor_effect_field::unk_BC, -1.f);
    initialize_actor_effect_free_chain(*effect);
    store_effect_field(effect->bytes, actor_effect_field::position, position);
    store_effect_field(effect->bytes, actor_effect_field::euler, euler);
    store_effect_field(effect->bytes, actor_effect_field::velocity_mode, u32{0});
    store_effect_field(effect->bytes, actor_effect_field::draw_mode, u32{3});
    set_actor_effect_script(*effect, name, registry, error);
    store_effect_field(effect->bytes, actor_effect_field::unk_2E4, u16{0xFFFF});
    store_effect_field(effect->bytes, actor_effect_field::unk_36E, u8{1});
    return effect;
}

// Xbox 0x000ED550
i32 allocate_actor_effect_slot(i32 category, const char* name, u16 count,
    const Vec3& position, const Vec3& euler, AnimationResourceRegistry& registry,
    ResourceErrorState& error)
{
    constexpr i32 ranges[3][2] = {{0, 150}, {0, 30}, {0, 20}};
    i32 slot = ranges[category][0];
    const i32 end = slot + ranges[category][1];
    if (slot < end)
    {
        while (g_actor_effect_nodes[slot] != nullptr)
        {
            ++slot;
            if (end <= slot)
            {
                return -1;
            }
        }
        if (slot != -1)
        {
            g_actor_effect_nodes[slot] = create_actor_effect_node(name, count, position, euler, registry, error);
            return slot;
        }
    }
    return -1;
}

// Xbox 0x000EF790
void recycle_actor_effect_particles(ActorEffectNode& effect)
{
    i32 index = 0;
    if (i16(get(effect.bytes, actor_effect_field::capacity)) > 0)
    {
        size_t offset = particle_link_offset(0);
        do
        {
            if (read<i16>(effect, offset) == -2)
            {
                write(effect, offset, i16(get(effect.bytes, actor_effect_field::free_head)));
                store_effect_field(effect.bytes, actor_effect_field::used_count,
                    u16(get(effect.bytes, actor_effect_field::used_count) - 1));
                store_effect_field(effect.bytes, actor_effect_field::free_head,
                    static_cast<i16>((offset - particle_link_offset(0)) / effect_particle_field::kSize));
            }
            ++index;
            offset += effect_particle_field::kSize;
        }
        while (index < i16(get(effect.bytes, actor_effect_field::capacity)));
    }
}

// Xbox 0x000EF7F0
void set_actor_effect_movement(ActorEffectNode& effect, u32 mode, const Vec3& velocity,
    const Vec3& minimum, const Vec3& maximum, i32 relative)
{
    store_effect_field(effect.bytes, actor_effect_field::volume, mode);
    store_effect_field(effect.bytes, actor_effect_field::volume_size, velocity);
    store_effect_field(effect.bytes, actor_effect_field::volume_lower, minimum);
    store_effect_field(effect.bytes, actor_effect_field::volume_upper, maximum);
    if (relative != 0)
    {
        effect.bytes[0x18] |= 4;
        return;
    }
    effect.bytes[0x18] &= 0xFB;
}

// Xbox 0x0013E750
bool retain_last_actor_effect(float duration)
{
    const i16 slot = static_cast<i16>(g_last_actor_effect);
    g_last_actor_effect = -1;
    if (slot != -1)
    {
        ActorEffectNode& effect = *g_actor_effect_nodes[slot];
        effect.bytes[0x18] |= 8;
        if (duration > 0.f)
        {
            store_effect_field(effect.bytes, actor_effect_field::life, duration);
        }
        return true;
    }
    return false;
}

// Xbox 0x0003A9A0
float actor_effect_event_duration(i32 enabled, std::span<const u8> event,
    const ActorAnimationState& animation)
{
    if (enabled == 0)
    {
        return -2.f;
    }
    float begin;
    float end;
    std::memcpy(&begin, event.data() + 4, 4);
    std::memcpy(&end, event.data() + 8, 4);
    const float interval = end - begin;
    if (animation.controller != nullptr && animation.controller->slots[0].player != nullptr)
    {
        return animation.controller->slots[0].player->duration * interval;
    }
    return 0.f * interval;
}
}
