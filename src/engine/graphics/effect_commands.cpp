#include "effect_commands.h"
#include "effect_command_selection.h"
#include "effect_channels.h"
#include "extended_float.h"
#include "../entities/body_animation_commands.h"
#include "../entities/entity.h"
#include "../runtime/random_state.h"
#include "../audio/listener_settings.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void command_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void command_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
Vec3 command_vector(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
float command_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
float command_range(std::span<const u8> command, size_t low, size_t high, u32& random)
{
    const float first = read_f32(command, low);
    const float second = read_f32(command, high);
    const float fraction = command_fraction(random);
    return (second - first) * fraction + first;
}
float signed_command_range(float low, float high, u32& random)
{
    const float fraction = command_fraction(random);
    if (!(high >= low))
    {
        const float value_fraction = command_fraction(random);
        const float value = (low - high) * value_fraction + high;
        if (fraction > .5f)
        {
            return value;
        }
        return 0.f - value;
    }
    return (high - low) * fraction + low;
}
}

// Xbox 0x000F0390
bool execute_actor_effect_commands(std::span<u8> particle, ActorEffectNode& owner, u32& cursor,
    const std::function<std::span<const u8>(u32)>& resolve, u32& random,
    const i32& pal, const i32& override_pal)
{
    constexpr float degrees = std::bit_cast<float>(0x3C8EFA35u);
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    const auto detach = [&]
    {
        const u16 slot = 0xFFFD;
        std::memcpy(particle.data() + 0x5A, &slot, 2);
        const u16 count = u16(read_u16(owner.bytes, 4) + 1);
        std::memcpy(owner.bytes.data() + 4, &count, 2);
    };
    const auto update_duration = [&](std::span<const u8> command)
    {
        if (read_f32(particle, 0xAC) != read_f32(command, 0xC)
            || read_f32(particle, 0xB0) != read_f32(command, 0x10))
        {
            command_float(particle, 0xB4, command_range(command, 0xC, 0x10, random));
            command_word(particle, 0xAC, read_u32(command, 0xC));
            command_word(particle, 0xB0, read_u32(command, 0x10));
        }
    };
    while (true)
    {
        const u32 address = cursor;
        const auto command = resolve(address);
        const i32 opcode = std::bit_cast<i32>(read_u32(command, 4));
        switch (opcode)
        {
        case 2:
            return true;
        case 0x1B59:
            cursor = address + 0x10;
            for (size_t index = 0; index < 5; ++index)
            {
                command_float(particle, 0x60 + index * 4, float(command[8 + index]));
            }
            break;
        case 0x1B5A:
        {
            const float fraction = advance_runtime_random_fraction(random);
            const auto sampled = resolve(cursor);
            cursor += 0x14;
            std::array<u8, 5> color;
            for (size_t reverse = 5; reverse > 0; --reverse)
            {
                const size_t index = reverse - 1;
                const u8 low = sampled[index == 4 ? 0x10 : 8 + index];
                const u8 high = sampled[index == 4 ? 0x11 : 0xC + index];
                const long double value = static_cast<long double>(i32(high) - i32(low)) * fraction + i32(low);
                color[index] = u8(truncate_runtime_float(value));
            }
            for (size_t index = 0; index < 5; ++index)
            {
                command_float(particle, 0x60 + index * 4, float(color[index]));
            }
            break;
        }
        case 0x1B5B:
            cursor = address + 0xC;
            command_float(particle, 0x4C, read_f32(command, 8) * read_f32(owner.bytes, 0xB8));
            break;
        case 0x1B5C:
            cursor = address + 0x10;
            command_float(particle, 0x4C, command_range(command, 8, 0xC, random) * read_f32(owner.bytes, 0xB8));
            break;
        case 0x1B5E:
            cursor = address + 0xC;
            if (actor_effect_command_condition(command, particle))
            {
                cursor = address;
                return true;
            }
            break;
        case 0x1B5F:
            cursor += 8;
            return false;
        case 0x1B60:
            cursor = address + 8;
            detach();
            break;
        case 0x1B61:
            cursor = address + 0x14;
            if (read_f32(particle, 0xAC) != read_f32(command, 0xC)
                || read_f32(particle, 0xB0) != read_f32(command, 0x10))
            {
                command_float(particle, 0xB4, runtime_body_random_range(random,
                    read_f32(command, 0xC), read_f32(command, 0x10)));
                command_word(particle, 0xAC, read_u32(command, 0xC));
                command_word(particle, 0xB0, read_u32(command, 0x10));
            }
            target_actor_effect_alpha(particle.subspan(0x60), command[8], read_f32(particle, 0xB4));
            break;
        case 0x1B62:
            cursor = address + 0x14;
            update_duration(command);
            target_actor_effect_color(particle.subspan(0x60), command[8], command[9], command[10], read_f32(particle, 0xB4));
            break;
        case 0x1B63:
            cursor = address + 0xC;
            command_word(particle, 0xA8, read_u32(command, 8));
            break;
        case 0x1B64:
            cursor = address + 0x10;
            command_float(particle, 0xA8, command_range(command, 8, 0xC, random));
            break;
        case 0x1B65:
            cursor = address + 0xC;
            command_word(particle, 0x24, 0);
            command_word(particle, 0x28, 0);
            command_float(particle, 0x2C, read_f32(command, 8) * degrees);
            break;
        case 0x1B66:
        {
            cursor = address + 0x10;
            const float low = read_f32(command, 8) * degrees;
            const float high = read_f32(command, 0xC) * degrees;
            const float fraction = command_fraction(random);
            command_word(particle, 0x24, 0);
            command_word(particle, 0x28, 0);
            command_float(particle, 0x2C, (high - low) * fraction + low);
            break;
        }
        case 0x1B67:
            cursor = address + 0xC;
            command_float(particle, 0x48, read_f32(command, 8) * read_f32(particle, 0xA4));
            break;
        case 0x1B68:
            cursor = address + 0x10;
            command_float(particle, 0x48, command_range(command, 8, 0xC, random) * read_f32(particle, 0xA4));
            break;
        case 0x1B69:
        {
            cursor = address + 0x14;
            command_float(particle, 0xA0, read_f32(command, 8) * read_f32(particle, 0xA4));
            update_duration(command);
            const float duration = read_f32(particle, 0xB4);
            if (duration > epsilon)
            {
                const float difference = float(ExtendedFloat{read_f32(particle, 0xA0)} - ExtendedFloat{read_f32(particle, 0x48)});
                const float rate = float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration});
                command_float(particle, 0x9C, rate);
            }
            else
            {
                command_word(particle, 0x48, read_u32(particle, 0xA0));
                command_word(particle, 0x9C, 0);
            }
            break;
        }
        case 0x1B6A:
            cursor = address + 0x10;
            if (read_u32(command, 8) != 0)
            {
                particle[0x56] |= 2;
            }
            if (read_u32(command, 0xC) != 0)
            {
                particle[0x56] |= 8;
            }
            break;
        case 0x1B6B:
            cursor = address + 0xC;
            particle[0x56] |= 1;
            command_word(particle, 0x3C, read_u32(command, 8));
            break;
        case 0x1B6C:
            cursor = address + 0x3C;
            configure_actor_effect_oscillation(particle, command_vector(command, 0x30), command_vector(command, 0x24),
                command_vector(command, 8), command_vector(command, 0x14), i32(read_u32(command, 0x20)), random);
            break;
        case 0x1B6D:
            cursor = address + 0xC;
            if (read_u32(command, 8) == 0)
            {
                detach();
            }
            else if (read_u32(command, 8) == 1)
            {
                return false;
            }
            break;
        case 0x1B6E:
            cursor = address + 0x10;
            if (read_f32(particle, 0xAC) == read_f32(command, 8)
                && read_f32(particle, 0xB0) == read_f32(command, 0xC))
            {
                command_word(particle, 0x108, read_u32(particle, 0xB4));
            }
            else
            {
                command_float(particle, 0x108, command_range(command, 8, 0xC, random));
            }
            command_float(particle, 0xAC, -1.f);
            command_word(particle, 0xB0, read_u32(particle, 0xAC));
            command_word(particle, 0xB4, read_u32(particle, 0xAC));
            break;
        case 0x1B6F:
        {
            cursor = address + 0x20;
            const float x = signed_command_range(read_f32(command, 8), read_f32(command, 0x14), random);
            const float y = signed_command_range(read_f32(command, 0xC), read_f32(command, 0x18), random) * degrees;
            const float z = signed_command_range(read_f32(command, 0x10), read_f32(command, 0x1C), random) * degrees;
            command_float(particle, 0x24, x * degrees);
            command_float(particle, 0x28, y);
            command_float(particle, 0x2C, z);
            break;
        }
        case 0x1B70:
            cursor = address + 0x10;
            command_word(particle, 0x40, read_u32(command, 8));
            command_word(particle, 0x48, read_u32(command, 0xC));
            break;
        case 0x1B71:
            cursor = address + 8;
            particle[0x56] |= 4;
            break;
        case 0x1B72:
            cursor = address + 0xC;
            command_word(particle, 0x50, read_u32(command, 8));
            break;
        case 0x1B73:
            cursor = address + 0x3C;
            configure_actor_effect_bounded_channels(particle, command_vector(command, 8), command_vector(command, 0x14),
                command_vector(command, 0x24), command_vector(command, 0x30), i32(read_u32(command, 0x20)), pal, override_pal, random);
            break;
        case 0x1B74:
        case 0x1B75:
        case 0x1B76:
        case 0x1B77:
        case 0x1B78:
        case 0x1B7A:
        case 0x1B7B:
        case 0x1B7C:
        case 0x1B7D:
        {
            switch (opcode)
            {
            case 0x1B74: cursor = address + 0x4C; break;
            case 0x1B75:
            case 0x1B7C: cursor = address + 0xA0; break;
            case 0x1B76: cursor = address + 0x24; break;
            case 0x1B77: cursor = address + 0x1C; break;
            case 0x1B78: cursor = address + 0x28; break;
            case 0x1B7A: cursor = address + 0x14; break;
            case 0x1B7B: cursor = address + 0x18; break;
            case 0x1B7D: cursor = address + 0x9C; break;
            }
            particle[0x56] |= opcode == 0x1B76 || opcode == 0x1B77 ? 0x11 : 1;
            if (read_u32(command, 8) != 0)
            {
                particle[0x56] |= 0x20;
            }
            break;
        }
        case 0x1B79:
            cursor = address + 0x98;
            if (read_u32(command, 8) != 0)
            {
                particle[0x56] |= 0x20;
            }
            break;
        default:
            if (opcode > 6999 && opcode < 0x1F5D)
            {
                return false;
            }
            return true;
        }
    }
}
}
