#include "actor_camera_effects.h"
#include "gbtg/entity.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <string>

namespace gbtg
{
ActorCameraEffect g_actor_camera_effects{};
GhoulySpawnPool* g_actor_camera_shakes = nullptr;

namespace
{
template<class T>
T read(const u8* bytes, size_t offset)
{
    T value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

template<class T>
void write(ActorCameraEffect& effect, size_t offset, const T& value)
{
    std::memcpy(effect.payload + offset - 8, &value, sizeof(value));
}

template<class T>
T field(const ActorCameraEffect& effect, size_t offset)
{
    T value;
    std::memcpy(&value, effect.payload + offset - 8, sizeof(value));
    return value;
}

// Xbox 0x000E9210, EAX the effect
void remove_actor_camera_effect(ActorCameraEffect* effect)
{
    if (effect->previous != nullptr)
    {
        effect->previous->next = effect->next;
    }
    if (effect->next != nullptr)
    {
        effect->next->previous = effect->previous;
    }
    effect->previous = nullptr;
    effect->next = nullptr;
    engine_heap_free(effect); // HeapFree on the effect's thread heap
}

// State 3's level over the effect's keys (+0x3C, +0x7C of them): (time, level) pairs, searched and lerped.
float actor_camera_effect_curve(const ActorCameraEffect& effect, float along)
{
    const i32 count = field<i32>(effect, 0x7C);
    const auto key = [&effect](i32 index)
    {
        return field<float>(effect, 0x3C + size_t(index) * 8);
    };
    const auto level = [&effect](i32 index)
    {
        return field<float>(effect, 0x40 + size_t(index) * 8);
    };
    i32 low = 0;
    i32 high = count - 1;
    float key_high = key(0);
    float key_low;
    float level_low;
    if (key_high > along)
    {
        key_low = key(count - 1) - 1.f; // DAT_0032B0F8
        level_low = level(count - 1);
        high = 0;
    }
    else if (along > key(count - 1))
    {
        key_high = key(0) + 1.f;
        key_low = key(count - 1);
        level_low = level(count - 1);
        high = 0;
    }
    else
    {
        if (high > 1)
        {
            do
            {
                const i32 middle = (high - low) / 2 + low;
                if (along < key(middle))
                {
                    high = middle;
                }
                else
                {
                    low = middle;
                }
            }
            while (low + 1 < high);
        }
        key_high = key(high);
        key_low = key(low);
        level_low = level(low);
    }
    const float level_high = level(high);
    if (key_high == key_low)
    {
        return level_high;
    }
    const float value = (along - key_low) / (key_high - key_low) * (level_high - level_low) + level_low;
    if (level_high > level_low)
    {
        if (value > level_high)
        {
            return level_high;
        }
        if (level_low > value)
        {
            return level_low;
        }
        return value;
    }
    if (level_high > value)
    {
        return level_high;
    }
    if (value > level_low)
    {
        return level_low;
    }
    return value;
}
}

// Xbox 0x00048F90
void tick_actor_camera_effects(float step)
{
    ActorCameraEffect* next = g_actor_camera_effects.next; // DAT_004E1738
    while (next != nullptr)
    {
        ActorCameraEffect* effect = next;
        next = effect->next;
        const u8 state = field<u8>(*effect, 0x80);
        if (state == 1)
        {
            write(*effect, 0x34, field<float>(*effect, 0x30) >= 0.f ? 1.f : -1.f); // DAT_0032B0F8, DAT_0032B120
            write(*effect, 0x80, u8{2});
            write(*effect, 0x83, u8{0});
        }
        else if (state == 2)
        {
            u8 flip = field<u8>(*effect, 0x83);
            if (field<float>(*effect, 0x38) != 0.f)
            {
                float left = field<float>(*effect, 0x38) - step;
                if (!(0.f < left))
                {
                    left = 0.f;
                }
                write(*effect, 0x38, left);
                if (flip != 0)
                {
                    const float length = field<float>(*effect, 0x20);
                    const float high = field<float>(*effect, 0x28);
                    const float low = field<float>(*effect, 0x2C);
                    float amount = high;
                    if (length != 0.f)
                    {
                        const float value = (left - length) / (0.f - length) * (high - low) + low;
                        if (high > low)
                        {
                            amount = value > high ? high : low > value ? low : value;
                        }
                        else
                        {
                            amount = high > value ? high : value > low ? low : value;
                        }
                    }
                    write(*effect, 0x30, amount);
                }
            }
            else if (flip != 0)
            {
                const float decay = field<float>(*effect, 0x24);
                write(*effect, 0x30, decay * decay * field<float>(*effect, 0x30));
            }
            write(*effect, 0x34, 0.f - field<float>(*effect, 0x34));
            write(*effect, 0x83, u8(flip ^ 1));
            if (field<float>(*effect, 0x24) != 1.f
                && std::bit_cast<float>(0x3A83126Fu) > std::fabs(field<float>(*effect, 0x30))) // DAT_0032B1A4
            {
                remove_actor_camera_effect(effect); // 0x000E9210
            }
        }
        else if (state == 3)
        {
            const float length = field<float>(*effect, 0x20);
            if (!(length > field<float>(*effect, 0x38)))
            {
                remove_actor_camera_effect(effect); // 0x000E9210
                continue;
            }
            float time = field<float>(*effect, 0x38) + step;
            if (time > length)
            {
                time = length;
            }
            write(*effect, 0x38, time);
            const float level = actor_camera_effect_curve(*effect, time / length);
            write(*effect, 0x30, field<float>(*effect, 0x28) * level);
            write(*effect, 0x34, 0.f - field<float>(*effect, 0x34));
        }
    }
}

// Xbox 0x00048F10
void release_actor_camera_effects()
{
    while (ActorCameraEffect* effect = g_actor_camera_effects.next)
    {
        if (effect->previous != nullptr)
        {
            effect->previous->next = effect->next;
        }
        if (effect->next != nullptr)
        {
            effect->next->previous = effect->previous;
        }
        effect->previous = nullptr;
        effect->next = nullptr;
        engine_heap_free(effect); // HeapFree on the effect's thread heap
    }
}

// Xbox 0x000FAE40
void release_actor_camera_shakes(AnimationResourceRegistry& registry)
{
    if (GhoulySpawnPool* pool = g_actor_camera_shakes; pool != nullptr)
    {
        for (u8* row = pool->begin; row < pool->cursor; row += pool->stride)
        {
            auto& shake = *reinterpret_cast<ActorCameraShake*>(row);
            if (shake.resource != nullptr)
            {
                release_animation_resource(registry, {shake.resource, 1}); // 0x00035C00, EBX +0x40
                shake.resource = nullptr;
            }
        }
        engine_heap_free(pool); // HeapFree on the pool's thread heap
        g_actor_camera_shakes = nullptr;
    }
    for (u8 port = 0; port < 4; ++port)
    {
        stop_pad_rumble(port); // 0x00155930
    }
}

// Xbox 0x000494B0
ActorCameraEffect* allocate_actor_camera_effect(u8 type)
{
    auto* effect = static_cast<ActorCameraEffect*>(engine_heap_allocate(sizeof(ActorCameraEffect)));
    std::memset(effect, 0, sizeof(*effect));
    ActorCameraEffect* tail = &g_actor_camera_effects;
    if (g_actor_camera_effects.next != nullptr)
    {
        do
        {
            tail = tail->next;
        }
        while (tail->next != nullptr);
    }
    tail->next = effect;
    effect->previous = tail;
    write(*effect, 0x81, type);
    effect->next = nullptr;
    write(*effect, 0x80, u8{1});
    write(*effect, 0x83, u8{0});
    return effect;
}

// Xbox 0x00049520
ActorCameraEffect* create_positioned_camera_effect(u8 type, float size,
    float value, const Vec3& position)
{
    ActorCameraEffect* effect = allocate_actor_camera_effect(type);
    write(*effect, 0x30, size);
    write(*effect, 0x24, value);
    write(*effect, 8, position);
    write(*effect, 0x82, u8{1});
    return effect;
}

// Xbox 0x000493D0
ActorCameraEffect* load_actor_camera_effect(const char* name, const Vec3* position,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (name == nullptr || std::memcmp(name, "aid_", 4) != 0)
    {
        return nullptr;
    }
    const auto resource = acquire_animation_resource(registry, name);
    if (resource.data() == nullptr)
    {
        resource_error(error, 2);
        return nullptr;
    }
    const u8* bytes = resource.data();
    ActorCameraEffect* effect;
    if (position != nullptr && read<float>(bytes, 0x10) > 0.f)
    {
        effect = create_positioned_camera_effect(bytes[0], read<float>(bytes, 4),
            read<float>(bytes, 8), *position);
        write(*effect, 0x14, read<u32>(bytes, 0xC));
        write(*effect, 0x18, read<float>(bytes, 0x10));
        release_animation_resource(registry, resource);
        return effect;
    }
    effect = allocate_actor_camera_effect(bytes[0]);
    write(*effect, 0x30, read<float>(bytes, 4));
    write(*effect, 0x24, read<float>(bytes, 8));
    release_animation_resource(registry, resource);
    return effect;
}

// Xbox 0x000FB7C0
ActorCameraShake* allocate_actor_camera_shake()
{
    GhoulySpawnPool* pool = g_actor_camera_shakes;
    if (pool == nullptr)
    {
        return nullptr;
    }
    if (static_cast<u32>((pool->cursor - pool->begin) / pool->stride) >= 16)
    {
        return nullptr;
    }
    if (pool->cursor == pool->limit)
    {
        pool = grow_ghouly_spawn_pool(pool);
        g_actor_camera_shakes = pool;
    }
    auto* shake = reinterpret_cast<ActorCameraShake*>(pool->cursor);
    pool->cursor += pool->stride;
    return shake;
}

// Xbox 0x000FB120
void load_actor_camera_shake(const char* name, const Vec3& position, float scale,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
        }
        else
        {
            ActorCameraShake* shake = allocate_actor_camera_shake();
            if (shake != nullptr)
            {
                shake->position = position;
                shake->scale = scale;
                shake->resource = resource.data();
                size_t offset = 0;
                size_t axis = 0;
                do
                {
                    ActorCameraShakeAxis& values = shake->axes[axis];
                    values.middle = (read<float>(shake->resource, offset + 0x24)
                        - read<float>(shake->resource, offset + 0x20)) * 0.5f
                        + read<float>(shake->resource, offset + 0x20);
                    values.reach = 0.f;
                    values.amplitude = (read<float>(shake->resource, offset + 0x24)
                        - read<float>(shake->resource, offset + 0x20)) * 0.5f;
                    values.angle = read<float>(shake->resource, offset + 0x2C);
                    values.phase = 0;
                    values.value = read<float>(shake->resource, offset);
                    values.target = values.value;
                    values.rate = 0.f;
                    offset += 0x3C;
                    ++axis;
                }
                while (offset < 0x78);
                return;
            }
            release_animation_resource(registry, resource);
        }
    }
}
std::array<PadShakeRecord, 4> g_pad_shakes{};
u32 g_pad_shakes_held = 0;

namespace
{
// 0x000F72D0(0x2B0): eight 0x54-byte rows after the pool header.
GhoulySpawnPool* allocate_actor_camera_shake_pool()
{
    constexpr u16 kStride = u16(sizeof(ActorCameraShake));
    constexpr u16 kRows = 8;
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + kStride * kRows)));
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->stride = kStride;
    pool->grow = kRows;
    pool->limit = pool->begin + kStride * kRows;
    return pool;
}

// Xbox 0x000FB220, ESI the axis
void set_actor_camera_shake_target(ActorCameraShakeAxis& axis, float target, float time)
{
    axis.target = target;
    if (time > 0.f)
    {
        axis.rate = float(std::fabs((target - axis.value) / time)); // 0x00152BB0
        return;
    }
    axis.value = target;
    axis.rate = 0.f;
}

// Xbox 0x000FB270, ECX the phase, EDX the axis, EAX the axis record
void enter_actor_camera_shake_phase(u32 phase, ActorCameraShakeAxis& axis, const u8* record)
{
    axis.phase = phase;
    switch (phase)
    {
    case 0:
        axis.value = read<float>(record, 0);
        axis.target = axis.value;
        axis.rate = 0.f;
        break;
    case 1:
        set_actor_camera_shake_target(axis, read<float>(record, 8), read<float>(record, 4)); // 0x000FB220
        break;
    case 2:
        set_actor_camera_shake_target(axis, read<float>(record, 0x10), read<float>(record, 0xC));
        break;
    case 3:
        axis.rate = read<float>(record, 0x14);
        break;
    case 4:
        set_actor_camera_shake_target(axis, read<float>(record, 0x1C), read<float>(record, 0x18));
        break;
    }
}

// The axis moves toward its target at its rate, within [0, 100]; once there it enters the next phase.
// Xbox 0x000FB2F0, ESI the axis
void move_actor_camera_shake_axis(ActorCameraShakeAxis& axis, const u8* record, float step)
{
    const float target = axis.target;
    const float difference = target - axis.value;
    const float distance = float(std::fabs(difference)); // 0x00152BB0
    const float move = axis.rate * step;
    if (axis.rate != 0.f)
    {
        if (move > distance)
        {
            axis.rate = 0.f;
            axis.value = target;
        }
        else if (0.f > difference)
        {
            axis.value = axis.value - move;
        }
        else
        {
            axis.value = axis.value + move;
        }
    }
    float value = axis.value;
    if (0.f > value)
    {
        value = 0.f;
    }
    else if (value > 100.f) // DAT_0032B12C
    {
        value = 100.f;
    }
    axis.value = value;
    if (axis.rate == 0.f)
    {
        enter_actor_camera_shake_phase(axis.phase + 1, axis, record); // 0x000FB270
    }
}

// One axis's step; 1 once it is done (phase 5).
// Xbox 0x000FB3B0, EAX the axis, EDI the axis record
u32 step_actor_camera_shake_axis(ActorCameraShakeAxis& axis, const u8* record, float step)
{
    u32 done = 0;
    switch (axis.phase)
    {
    case 0:
        axis.phase = 1;
        set_actor_camera_shake_target(axis, read<float>(record, 8), read<float>(record, 4)); // 0x000FB220
        break;
    case 1:
    case 2:
    case 4:
        move_actor_camera_shake_axis(axis, record, step); // 0x000FB2F0
        break;
    case 3:
        if (axis.rate <= 0.f) // DAT_0032B100
        {
            axis.phase = 4;
            set_actor_camera_shake_target(axis, read<float>(record, 0x1C), read<float>(record, 0x18));
        }
        else
        {
            axis.rate = axis.rate - step;
        }
        break;
    case 5:
        done = 1;
        break;
    }
    axis.angle = read<float>(record, 0x28) * std::bit_cast<float>(0x3C8EFA35u) * step + axis.angle; // DAT_0032B0FC
    axis.reach = read<float>(record, 0x38) * step + axis.reach;
    return done;
}

// Both axes step; true once both are done.
// Xbox 0x000FB480
bool step_actor_camera_shake(ActorCameraShake& shake, float step)
{
    i32 done = 0;
    for (u32 axis = 0; axis < 2; ++axis)
    {
        if (step_actor_camera_shake_axis(shake.axes[axis], shake.resource + axis * 0x3C, step) != 0) // 0x000FB3B0
        {
            ++done;
        }
    }
    return done == 2;
}

// The shake's strength at a port's position: by distance from its near to its far range, or, with a widening wave
// (record +0x38), from the wave front; times the shake's scale.
// Xbox 0x000FB4D0, ECX the shake, EDX the axis record, EAX the position, ESI the axis
float actor_camera_shake_falloff(const ActorCameraShake& shake, const u8* record, const Vec3& position,
    const ActorCameraShakeAxis& axis)
{
    const double dx = double(shake.position.x) - position.x;
    const double dy = double(shake.position.y) - position.y;
    const double dz = double(shake.position.z) - position.z;
    const float distance = float(std::sqrt(dz * dz + dx * dx + dy * dy));
    const float near_range = read<float>(record, 0x30);
    const float far_range = read<float>(record, 0x34);
    if (near_range == 0.f && far_range == 0.f)
    {
        return 1.f; // DAT_0032B0F8
    }
    float level;
    if (read<float>(record, 0x38) > 0.f)
    {
        const float near_scaled = shake.scale * near_range;
        const float far_scaled = far_range * shake.scale;
        if (distance > axis.reach)
        {
            if (far_scaled == near_scaled)
            {
                return shake.scale * 0.f;
            }
            level = 1.f - ((distance - axis.reach) - near_scaled) / (far_scaled - near_scaled);
            if (0.f > level)
            {
                return shake.scale * 0.f;
            }
            if (level > 1.f)
            {
                level = 1.f;
            }
            return shake.scale * level;
        }
        if (far_scaled == near_scaled)
        {
            return shake.scale * 1.f;
        }
        level = ((distance - axis.reach) - near_scaled) / (far_scaled - near_scaled);
        if (level > 1.f)
        {
            return shake.scale * 1.f;
        }
        if (0.f > level)
        {
            return shake.scale * 0.f;
        }
        return shake.scale * level;
    }
    if (far_range == near_range)
    {
        return shake.scale * 0.f;
    }
    level = 1.f - (distance - near_range) / (far_range - near_range);
    if (0.f > level)
    {
        return shake.scale * 0.f;
    }
    if (level > 1.f)
    {
        return shake.scale * 1.f;
    }
    return shake.scale * level;
}

// The selected player's port for a pad mask (1, 2, 4, 8); retail's -1 reads before the records.
i32 actor_camera_shake_port(u32 port_mask)
{
    switch (port_mask)
    {
    case 1:
        return 0;
    case 2:
        return 1;
    case 4:
        return 2;
    case 8:
        return 3;
    default:
        throw Error("0x000FB650 has no port for pad mask " + std::to_string(port_mask)
            + " (retail reads the record before DAT_00507D84)");
    }
}

// Each axis's wave at the selected port: the strongest this frame becomes the port's motor.
// Xbox 0x000FB650
void shake_actor_camera_ports(ActorCameraShake& shake, u32 port_mask)
{
    for (u32 index = 0; index < 2; ++index)
    {
        const ActorCameraShakeAxis& axis = shake.axes[index];
        const float wave = float(retail_sin(axis.angle) * axis.amplitude + axis.middle); // 0x00153320
        PadShakeRecord& pad = g_pad_shakes[size_t(actor_camera_shake_port(port_mask))]; // DAT_0050E0F8
        const float falloff = actor_camera_shake_falloff(shake, shake.resource + index * 0x3C, pad.position,
            axis); // 0x000FB4D0
        const float strength = falloff * (axis.value * wave);
        if (strength > pad.motors[index])
        {
            pad.motors[index] = strength;
        }
    }
}
}

// Xbox 0x000FADE0
void boot_actor_camera_shakes(std::span<RuntimePadRumbleCommand, 4> commands)
{
    g_actor_camera_shakes = allocate_actor_camera_shake_pool(); // 0x000F72D0(0x2B0)
    g_pad_shakes_held = 0; // DAT_00507DF4
    for (size_t port = 0; port < 4; ++port)
    {
        g_pad_shakes[port].enabled = 1; // DAT_00507D94
        g_pad_shakes[port].unk_0c = 1; // DAT_00507D90
        commands[port].shakes_allowed = 1; // DAT_00507D48
        commands[port].active = 1; // DAT_00507D4C
    }
}

// Xbox 0x000FAEE0
void reset_actor_camera_shakes(u8 unlock_flags, std::span<RuntimePadRumbleCommand, 4> commands)
{
    g_actor_camera_shakes = allocate_actor_camera_shake_pool(); // 0x000F72D0(0x2B0)
    g_pad_shakes_held = 0; // DAT_00507DF4
    const u32 on = (unlock_flags & 4) != 0 ? 0 : 1; // **(DAT_00506C70 + 8)
    for (size_t port = 0; port < 4; ++port)
    {
        g_pad_shakes[port].enabled = on; // DAT_00507D94
        commands[port].shakes_allowed = i32(on); // DAT_00507D48
    }
}

// Xbox 0x000FAF70
void tick_actor_camera_shakes(float step, const float* view, u32 port_mask,
    std::span<RuntimePadRumbleCommand, 4> commands, AnimationResourceRegistry& registry)
{
    for (PadShakeRecord& pad : g_pad_shakes)
    {
        pad.position = Vec3{view[0], view[1], view[2]}; // DAT_005077A8
    }
    if (g_pad_shakes_held == 0 && g_actor_camera_shakes != nullptr)
    {
        GhoulySpawnPool* pool = g_actor_camera_shakes;
        for (i32 index = 0; index < i32((pool->cursor - pool->begin) / pool->stride); ++index)
        {
            auto& shake = *reinterpret_cast<ActorCameraShake*>(pool->begin + pool->stride * index);
            if (shake.resource != nullptr)
            {
                if (step_actor_camera_shake(shake, step)) // 0x000FB480
                {
                    release_animation_resource(registry, {shake.resource, 1}); // 0x00035C00, EBX +0x40
                    // The row is erased; the next row slides into this index, which the loop then passes.
                    u8* row = pool->begin + pool->stride * index;
                    u8* last = pool->cursor - pool->stride;
                    if (row < last)
                    {
                        std::memmove(row, row + pool->stride, size_t(last - row));
                    }
                    pool->cursor = pool->cursor - pool->stride;
                }
                else
                {
                    shake_actor_camera_ports(shake, port_mask); // 0x000FB650
                }
                pool = g_actor_camera_shakes;
            }
        }
    }
    const float threshold = 5.f; // DAT_0032B184
    for (size_t port = 0; port < 4; ++port)
    {
        PadShakeRecord& pad = g_pad_shakes[port];
        RuntimePadRumbleCommand& command = commands[port];
        if (pad.enabled != 0)
        {
            if (g_pad_shakes_held != 0)
            {
                command.left = 0.f;
                command.right = 0.f;
            }
            else
            {
                command.left = (command.shakes_allowed != 0 && !(pad.motors[0] < threshold)) ? pad.motors[0] : 0.f;
                command.right = (command.shakes_allowed != 0 && !(pad.motors[1] < threshold)) ? pad.motors[1] : 0.f;
            }
        }
        pad.motors[0] = 0.f;
        pad.motors[1] = 0.f;
    }
}
}
