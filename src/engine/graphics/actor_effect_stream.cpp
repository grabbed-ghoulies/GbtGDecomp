#include "actor_effect_stream.h"

#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
RuntimeActorCreationOptions g_actor_creation_options;
ActorEffectBinding g_actor_effect_binding;
// Xbox 0x00019200
bool actor_event_chance(u32& random_state, float chance)
{
    random_state = random_state * 0x19660D + 0x3C6EF35F;
    const float random = std::bit_cast<float>((random_state & 0x7FFFFF) | 0x3F800000) - 1.f;
    return chance >= random;
}

// Xbox 0x00055A90
u32 reject_unused_effect_nodes()
{
    return 0;
}
namespace
{
char resource_type_scratch[0x20]; // DAT_004DF220
template<class T>
T read(const u8* data, size_t offset)
{
    T value;
    std::memcpy(&value, data + offset, sizeof(T));
    return value;
}
template<class T>
void write(ActorEffectNode& effect, size_t offset, const T& value)
{
    std::memcpy(effect.bytes.data() + offset, &value, sizeof(T));
}
}

// Xbox 0x00035870
bool resource_has_type(const char* name, const char* type)
{
    i32 index = 0;
    char value = name[4];
    while (value != '_')
    {
        resource_type_scratch[index] = value;
        ++index;
        value = name[4 + index];
    }
    resource_type_scratch[index] = '\0';
    return compare_animation_resource_type(type, resource_type_scratch) == 0;
}

// Xbox 0x000380D0
bool equal_sound_names(const char* name, const char* expected)
{
    char a = *expected;
    char b = *name;
    if (a == b)
    {
        do
        {
            ++expected;
            ++name;
            if (a == '\0')
            {
                return true;
            }
            a = *expected;
            b = *name;
        }
        while (a == b);
    }
    return a == b;
}

// Xbox 0x00040AF0
u32 actor_effect_joint(const Actor& actor, i32 kind)
{
    if (kind != -1 && actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(actor.model->cpu, static_cast<i16>(kind));
        if ((joint & 0xFF) != 0xFF)
        {
            return joint & 0xFF;
        }
    }
    return 0xFFFFFFFF;
}

// Xbox 0x000EF870
void set_actor_effect_sound(ActorEffectNode& effect, const char* name,
    RuntimeActorSoundQueues& queues)
{
    if (effect.sound.previous != nullptr)
    {
        release_actor_sound(effect.sound, queues);
    }
    if (*name != '\0')
    {
        if (!equal_sound_names(name, "XACT_SOUNDBANK_GMISC_NOSOUND"))
        {
            const Vec3 position = read<Vec3>(effect.bytes.data(), 0x1C);
            queue_actor_sound(effect.sound, 0, nullptr, 0, &position, nullptr, 1.f, name, queues);
        }
    }
}

// Xbox 0x000ED970
u32 interpret_new_actor_effect(const u8*& cursor, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context)
{
    return interpret_actor_effect_nodes(nullptr, cursor, options, context);
}

// Xbox 0x00053060
u32 dispatch_actor_effect_node(const u8*& cursor, i32& more,
    const RuntimeActorCreationOptions* options, ActorEffectStreamContext& context)
{
    const u8* node = cursor;
    const i32 type = read<i32>(node, 4);
    constexpr i32 ranges[4][2] = {{1, 6}, {0x7D0, 0x7D4}, {0xFA0, 0xFA9}, {0x1F40, 0x1F5C}};
    size_t index = 0;
    while (type <= ranges[index][0] || ranges[index][1] <= type)
    {
        ++index;
        if (index >= 4)
        {
            return 0;
        }
    }
    if (index == 1 || index == 2)
    {
        return reject_unused_effect_nodes();
    }
    if (index == 3)
    {
        return interpret_new_actor_effect(cursor, options, context);
    }
    if (type == 2)
    {
        more = 0;
    }
    else
    {
        if (type != 3)
        {
            if (type != 5)
            {
                return 0;
            }
            cursor = node + 0x88;
            return load_actor_effect_stream(reinterpret_cast<const char*>(node + 8), options, context);
        }
        cursor = node + 0xC;
        context.random_state = context.random_state * 0x19660D + 0x3C6EF35F;
        const float random = std::bit_cast<float>((context.random_state & 0x7FFFFF) | 0x3F800000) - 1.f;
        const i32 count = read<i32>(node, 8);
        const i32 selected = _mm_cvtt_ss2si(_mm_set_ss(random * static_cast<float>(count)));
        if (selected != std::bit_cast<i32>(static_cast<u32>(count) - 1))
        {
            more = 0;
            return 0;
        }
    }
    return 1;
}

// Xbox 0x00053140
u32 run_actor_effect_stream(const u8*& cursor, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context)
{
    u32 result = 1;
    i32 more = 1;
    do
    {
        if (result == 0)
        {
            return result;
        }
        result = dispatch_actor_effect_node(cursor, more, options, context);
    }
    while (more != 0);
    return result;
}

// Xbox 0x00053170
u32 load_actor_effect_stream(const char* name, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context)
{
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(context.models.registry, name);
        if (resource.data() != nullptr)
        {
            const u8* cursor = resource.data();
            const u32 result = run_actor_effect_stream(cursor, options, context);
            release_animation_resource(context.models.registry, resource);
            return result;
        }
        resource_error(context.models.error, 2);
    }
    return 0;
}

// Xbox 0x000ED9F0
u32 interpret_actor_effect_nodes(ActorEffectNode* effect, const u8*& cursor,
    const RuntimeActorCreationOptions* options, ActorEffectStreamContext& context)
{
    constexpr float radians = std::bit_cast<float>(0x3C8EFA35u);
    i32 retained_mode; // Original stack slot is written only by supported mode values.
    while (true)
    {
        const u8* node = cursor;
        switch (read<u32>(node, 4))
        {
        case 0x1F41:
            cursor = node + 8;
            write(*effect, 0xDC, u32{0});
            write(*effect, 0xE0, 0.f);
            write(*effect, 0xE4, 0.f);
            write(*effect, 0xE8, 0.f);
            break;
        case 0x1F42:
            cursor = node + 0xC;
            write(*effect, 0xDC, u32{1});
            write(*effect, 0xE0, read<u32>(node, 8));
            write(*effect, 0xE4, 0.f);
            write(*effect, 0xE8, 0.f);
            break;
        case 0x1F43:
        case 0x1F44:
        {
            const bool separate = read<u32>(node, 4) == 0x1F44;
            cursor = node + (separate ? 0x1C : 0x18);
            write(*effect, 0xDC, u32{2});
            write(*effect, 0xE0, read<float>(node, 0xC) * 0.5f);
            write(*effect, 0xE4, 0.f);
            write(*effect, 0xE8, read<float>(node, separate ? 0x10 : 0xC) * 0.5f);
            if (read<i32>(node, 8) != 0)
            {
                effect->bytes[0x18] |= 4;
            }
            const float minimum = read<float>(node, separate ? 0x14 : 0x10);
            float maximum = read<float>(node, separate ? 0x18 : 0x14);
            write(*effect, 0xF0, minimum * radians);
            if (minimum > maximum)
            {
                maximum += 360.f;
            }
            write(*effect, 0xFC, maximum * radians);
            break;
        }
        case 0x1F45:
        case 0x1F46:
            cursor = node + 0x18;
            if (read<u32>(node, 4) == 0x1F45)
            {
                write(*effect, 0xDC, u32{3});
                write(*effect, 0xE0, read<u32>(node, 0x10));
                write(*effect, 0xE4, 0.f);
                write(*effect, 0xE8, read<u32>(node, 0x14));
            }
            else
            {
                write(*effect, 0xDC, u32{4});
                write(*effect, 0xE0, read<Vec3>(node, 0xC));
            }
            if (read<i32>(node, 8) != 0)
            {
                effect->bytes[0x18] |= 4;
            }
            break;
        case 0x1F47:
        case 0x1F48:
        case 0x1F49:
        {
            const u32 type = read<u32>(node, 4);
            if (type == 0x1F47)
            {
                cursor = node + 0x20;
                write(*effect, 0xDC, u32{5});
                const float value = read<float>(node, 0xC) * 0.5f;
                write(*effect, 0xE0, Vec3{value, value, value});
            }
            else
            {
                cursor = node + (type == 0x1F48 ? 0x28 : 0x20);
                write(*effect, 0xDC, u32{type == 0x1F48 ? 5u : 6u});
                write(*effect, 0xE0, read<Vec3>(node, 0xC));
            }
            if (read<i32>(node, 8) != 0)
            {
                effect->bytes[0x18] |= 4;
            }
            if (type != 0x1F49)
            {
                const size_t offset = type == 0x1F47 ? 0x10 : 0x18;
                const float minimum = read<float>(node, offset);
                float maximum = read<float>(node, offset + 4);
                write(*effect, 0xF0, minimum * radians);
                if (minimum > maximum)
                {
                    maximum += 360.f;
                }
                write(*effect, 0xFC, maximum * radians);
            }
            const size_t offset = type == 0x1F48 ? 0x20 : 0x18;
            const float minimum = read<float>(node, offset);
            float maximum = read<float>(node, offset + 4);
            write(*effect, 0xEC, minimum * radians);
            if (minimum > maximum)
            {
                maximum += 360.f;
            }
            write(*effect, 0xF8, maximum * radians);
            break;
        }
        case 0x1F4A:
        case 0x1F4B:
        case 0x1F4C:
        case 0x1F4D:
        case 0x1F4E:
        {
            const u32 type = read<u32>(node, 4);
            cursor = node + 0x14;
            const size_t offset = type == 0x1F4A ? 0xA0 : type == 0x1F4B ? 0xAC
                : type == 0x1F4C ? 0x80 : type == 0x1F4D ? 0x5C : 0x68;
            const Vec3 value = read<Vec3>(node, 8);
            if (type == 0x1F4C)
            {
                write(*effect, offset, value);
            }
            else
            {
                write(*effect, offset, Vec3{value.x * radians, value.y * radians, value.z * radians});
            }
            break;
        }
        case 0x1F4F:
            cursor = node + 0xA0;
            if (read<u32>(node, 8) == 0x5F646961)
            {
                if (!resource_has_type(reinterpret_cast<const char*>(node + 8), "texture"))
                {
                    set_actor_effect_model(*effect, reinterpret_cast<const char*>(node + 8), context.models);
                }
                else
                {
                    set_actor_effect_texture(*effect, reinterpret_cast<const char*>(node + 8),
                        read<float>(node, 0x90), read<float>(node, 0x94) * 0.01f,
                        read<u32>(node, 0x88), read<i32>(node, 0x8C), read<i32>(node, 0x98),
                        read<i32>(node, 0x9C), context.models);
                }
            }
            break;
        case 0x1F50:
            cursor = node + 0xC;
            if (effect->texture.data() != nullptr)
            {
                release_animation_resource(context.models.registry, effect->texture);
                effect->texture = {};
            }
            write(*effect, 0x13C, read<u32>(node, 8));
            break;
        case 0x1F51:
            cursor = node + 0x110;
            if (effect == nullptr)
            {
                const Vec3& euler = options == nullptr || (options->flags & 4) == 0
                    ? context.default_euler : options->euler;
                const Vec3& position = options == nullptr || (options->flags & 1) == 0
                    ? context.default_position : options->position;
                g_last_actor_effect = allocate_actor_effect_slot(node[8],
                    reinterpret_cast<const char*>(node + 0xC), read<u16>(node, 0xA),
                    position, euler, context.models.registry, context.models.error);
                if (g_last_actor_effect == -1)
                {
                    return 0;
                }
                effect = g_actor_effect_nodes[g_last_actor_effect];
                if (effect == nullptr)
                {
                    return 0;
                }
                write(*effect, 8, -2.f);
                if (read<i32>(node, 0x10C) == 0)
                {
                    effect->bytes[0x18] &= 0x7F;
                }
                else
                {
                    effect->bytes[0x18] |= 0x80;
                }
                if (node[0x8C] != 0)
                {
                    set_actor_effect_sound(*effect, reinterpret_cast<const char*>(node + 0x8C), context.sounds);
                }
                if (options != nullptr)
                {
                    if ((options->flags & 0x40) != 0)
                    {
                        write(*effect, 0xB8, options->scale);
                    }
                    if ((options->flags & 8) != 0)
                    {
                        const auto* binding = static_cast<const ActorEffectBinding*>(options->user);
                        if (binding == nullptr)
                        {
                            write(*effect, 0xC4, u32{0});
                            write(*effect, 0xC8, i32{-1});
                        }
                        else
                        {
                            effect->binding_actor = binding->actor;
                            if (binding->mode == 0)
                            {
                                write(*effect, 0xC8, binding->index);
                                write(*effect, 0xC4, u32{0});
                            }
                            else if (binding->mode == 1)
                            {
                                write(*effect, 0xD0, static_cast<i16>(binding->index));
                                write(*effect, 0xC4, u32{1});
                            }
                            else if (binding->mode == 2)
                            {
                                effect->binding_joint = static_cast<i32>(actor_effect_joint(*binding->actor, binding->index));
                                write(*effect, 0xC4, u32{2});
                            }
                        }
                        break;
                    }
                }
                effect->binding_actor = nullptr;
            }
            break;
        case 0x1F52:
            cursor = node + 0x10;
            if (read<i32>(node, 0xC) == 0)
            {
                const float interval = 1.f / read<float>(node, 8);
                write(*effect, 8, interval);
                context.random_state = context.random_state * 0x19660D + 0x3C6EF35F;
                const float random = std::bit_cast<float>((context.random_state & 0x7FFFFF) | 0x3F800000) - 1.f;
                write(*effect, 0xC, random * interval);
            }
            else
            {
                write(*effect, 8, -2.f);
            }
            break;
        case 0x1F53:
            cursor = node + 0x10;
            write(*effect, 0x10, read<i32>(node, 0xC) == 0 ? read<float>(node, 8) : -1.f);
            break;
        case 0x1F54:
            cursor = node + 0x88;
            set_actor_effect_script(*effect, reinterpret_cast<const char*>(node + 8), context.models.registry, context.models.error);
            break;
        case 0x1F55:
            cursor = node + 0x108;
            std::strcpy(reinterpret_cast<char*>(effect->bytes.data() + 0x15C), reinterpret_cast<const char*>(node + 8));
            std::strcpy(reinterpret_cast<char*>(effect->bytes.data() + 0x1DC), reinterpret_cast<const char*>(node + 0x88));
            break;
        case 0x1F56:
            cursor = node + 0x88;
            std::strcpy(reinterpret_cast<char*>(effect->bytes.data() + 0x25C), reinterpret_cast<const char*>(node + 8));
            break;
        case 0x1F57:
            cursor = node + 0x18;
            if (read<i32>(node, 8) == 0)
            {
                write(*effect, 0x110, u32{1});
            }
            else if (read<i32>(node, 8) == 1)
            {
                write(*effect, 0x110, u32{2});
            }
            write(*effect, 0x114, read<u32>(node, 0xC));
            write(*effect, 0x118, read<u32>(node, 0x10));
            write(*effect, 0x4C, read<u32>(node, 0x14));
            break;
        case 0x1F58:
        {
            cursor = node + 0x7C;
            if (read<i32>(node, 8) == 0)
            {
                retained_mode = 3;
            }
            else if (read<i32>(node, 8) == 1)
            {
                retained_mode = 4;
            }
            else if (read<i32>(node, 8) == 2)
            {
                retained_mode = 5;
            }
            write(*effect, 0x114, read<u32>(node, 0xC));
            write(*effect, 0x118, read<u32>(node, 0x10));
            write(*effect, 0x4C, read<u32>(node, 0x14));
            write(*effect, 0x110, read<i32>(node, 0x78) == 0 ? retained_mode : 2);
            write(*effect, 0x11C, read<float>(node, 0x18) * radians);
            write(*effect, 0x128, read<float>(node, 0x1C) * radians);
            write(*effect, 0x120, read<float>(node, 0x20) * radians);
            write(*effect, 0x12C, read<float>(node, 0x24) * radians);
            write(*effect, 0x124, read<float>(node, 0x28) * radians);
            write(*effect, 0x130, read<float>(node, 0x2C) * radians);
            set_actor_effect_movement(*effect, read<u32>(node, 0x4C), read<Vec3>(node, 0x50),
                read<Vec3>(node, 0x5C), read<Vec3>(node, 0x68), read<i32>(node, 0x74));
            const Vec3 euler = read<Vec3>(node, 0x3C);
            if (read<i32>(node, 0x48) == 0)
            {
                write(*effect, 0xA0, euler);
            }
            else
            {
                const Vec3 base = read<Vec3>(effect->bytes.data(), 0x50);
                write(*effect, 0xA0, Vec3{euler.x + base.x, euler.y + base.y, euler.z + base.z});
            }
            write(*effect, 0xAC, read<Vec3>(node, 0x30));
            break;
        }
        case 0x1F59:
            cursor = node + 0x14;
            if (read<float>(node, 8) == 0.f && read<float>(node, 0xC) == 0.f && read<float>(node, 0x10) == 0.f)
            {
                effect->bytes[0x18] &= 0xEF;
            }
            else
            {
                write(*effect, 0x74, read<Vec3>(node, 8));
                effect->bytes[0x18] |= 0x10;
            }
            break;
        case 0x1F5A:
            cursor = node + 0x88;
            if (read<u32>(node, 8) == 0x5F646961)
            {
                set_actor_effect_model(*effect, reinterpret_cast<const char*>(node + 8), context.models);
            }
            break;
        case 0x1F5B:
            cursor = node + 0x10;
            write(*effect, 0x98, read<u32>(node, 8));
            write(*effect, 0x9C, read<u32>(node, 0xC));
            break;
        default:
            if (read<i32>(cursor, 4) > 7999 && read<i32>(cursor, 4) < 0x1F5D)
            {
                return 0;
            }
            return 1;
        }
    }
}
}
