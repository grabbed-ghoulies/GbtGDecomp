#include "effect_commands.h"
#include "effect_command_selection.h"
#include "effect_channels.h"
#include "extended_float.h"
#include "../entities/body_animation_commands.h"
#include "../entities/entity.h"
#include "../runtime/random_state.h"
#include "../audio/listener_settings.h"
#include "effect_particles.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float command_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
float command_range(float first, float second, u32& random)
{
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
bool execute_actor_effect_commands(EffectParticle& particle, ActorEffectNode& owner, u32& cursor,
    const std::function<std::span<const u8>(u32)>& resolve, u32& random,
    const i32& pal, const i32& override_pal)
{
    constexpr float degrees = std::bit_cast<float>(0x3C8EFA35u);
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    const auto detach = [&]
    {
        particle.next_free = 0xFFFD;
        set(owner.bytes, actor_effect_field::free_count, u16(get(owner.bytes, actor_effect_field::free_count) + 1));
    };
    const auto update_duration = [&](std::span<const u8> command)
    {
        if (particle.duration_low != get(command, timed_command::duration_low)
            || particle.duration_high != get(command, timed_command::duration_high))
        {
            particle.duration = command_range(get(command, timed_command::duration_low),
                get(command, timed_command::duration_high), random);
            particle.duration_low = get(command, timed_command::duration_low);
            particle.duration_high = get(command, timed_command::duration_high);
        }
    };
    while (true)
    {
        const u32 address = cursor;
        const auto command = resolve(address);
        const i32 opcode = std::bit_cast<i32>(get(command, effect_command_field::opcode));
        switch (opcode)
        {
        case 2:
            return true;
        case 0x1B59:
        {
            cursor = address + 0x10;
            float* const values = particle.colour_channels.values.data();
            const auto bytes = get(command, colour_command::values);
            for (size_t index = 0; index < 5; ++index)
            {
                values[index] = float(bytes[index]);
            }
            break;
        }
        case 0x1B5A:
        {
            const float fraction = advance_runtime_random_fraction(random);
            const auto sampled = resolve(cursor);
            cursor += 0x14;
            std::array<u8, 5> color;
            for (size_t reverse = 5; reverse > 0; --reverse)
            {
                const size_t index = reverse - 1;
                const u8 low = index == 4 ? get(sampled, colour_range_command::fifth_low)
                    : get(sampled, colour_range_command::low)[index];
                const u8 high = index == 4 ? get(sampled, colour_range_command::fifth_high)
                    : get(sampled, colour_range_command::high)[index];
                const long double value = static_cast<long double>(i32(high) - i32(low)) * fraction + i32(low);
                color[index] = u8(truncate_runtime_float(value));
            }
            float* const values = particle.colour_channels.values.data();
            for (size_t index = 0; index < 5; ++index)
            {
                values[index] = float(color[index]);
            }
            break;
        }
        case 0x1B5B:
            cursor = address + 0xC;
            particle.gravity = get(command, value_command::value) * get(owner.bytes, actor_effect_field::scale);
            break;
        case 0x1B5C:
            cursor = address + 0x10;
            particle.gravity = command_range(get(command, range_command::low), get(command, range_command::high), random)
                * get(owner.bytes, actor_effect_field::scale);
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
            if (particle.duration_low != get(command, timed_command::duration_low)
                || particle.duration_high != get(command, timed_command::duration_high))
            {
                particle.duration = runtime_body_random_range(random,
                    get(command, timed_command::duration_low), get(command, timed_command::duration_high));
                particle.duration_low = get(command, timed_command::duration_low);
                particle.duration_high = get(command, timed_command::duration_high);
            }
            target_actor_effect_alpha(particle.colour_channels, get(command, timed_command::colour)[0],
                particle.duration);
            break;
        case 0x1B62:
            cursor = address + 0x14;
            update_duration(command);
            target_actor_effect_color(particle.colour_channels,
                get(command, timed_command::colour)[0], get(command, timed_command::colour)[1],
                get(command, timed_command::colour)[2], particle.duration);
            break;
        case 0x1B63:
            cursor = address + 0xC;
            particle.life = get(command, value_command::value);
            break;
        case 0x1B64:
            cursor = address + 0x10;
            particle.life = command_range(get(command, range_command::low), get(command, range_command::high), random);
            break;
        case 0x1B65:
            cursor = address + 0xC;
            particle.spin = Vec3{0.f, 0.f, get(command, value_command::value) * degrees};
            break;
        case 0x1B66:
        {
            cursor = address + 0x10;
            const float low = get(command, range_command::low) * degrees;
            const float high = get(command, range_command::high) * degrees;
            const float fraction = command_fraction(random);
            particle.spin = Vec3{0.f, 0.f, (high - low) * fraction + low};
            break;
        }
        case 0x1B67:
            cursor = address + 0xC;
            particle.size = get(command, value_command::value) * particle.size_scale;
            break;
        case 0x1B68:
            cursor = address + 0x10;
            particle.size = command_range(get(command, range_command::low), get(command, range_command::high), random)
                * particle.size_scale;
            break;
        case 0x1B69:
        {
            cursor = address + 0x14;
            particle.target_size = get(command, value_command::value) * particle.size_scale;
            update_duration(command);
            const float duration = particle.duration;
            if (duration > epsilon)
            {
                const float difference = float(ExtendedFloat{particle.target_size} -
                    ExtendedFloat{particle.size});
                const float rate = float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration});
                particle.size_rate = rate;
            }
            else
            {
                particle.size = particle.target_size;
                particle.size_rate = 0.f;
            }
            break;
        }
        case 0x1B6A:
            cursor = address + 0x10;
            if (get(command, face_command::faces) != 0)
            {
                particle.motion_flags |= 2;
            }
            if (get(command, face_command::faces_fully) != 0)
            {
                particle.motion_flags |= 8;
            }
            break;
        case 0x1B6B:
            cursor = address + 0xC;
            particle.motion_flags |= 1;
            particle.collision_radius = get(command, value_command::value);
            break;
        case 0x1B6C:
            cursor = address + 0x3C;
            configure_actor_effect_oscillation(
                particle, get(command, channels_command::rates), get(command, channels_command::start),
                get(command, channels_command::lower), get(command, channels_command::upper),
                i32(get(command, channels_command::random_start)), random);
            break;
        case 0x1B6D:
            cursor = address + 0xC;
            if (get(command, value_command::word) == 0)
            {
                detach();
            }
            else if (get(command, value_command::word) == 1)
            {
                return false;
            }
            break;
        case 0x1B6E:
            cursor = address + 0x10;
            if (particle.duration_low == get(command, range_command::low)
                && particle.duration_high == get(command, range_command::high))
            {
                particle.unk_108 = particle.duration;
            }
            else
            {
                particle.unk_108 =
                    command_range(get(command, range_command::low), get(command, range_command::high), random);
            }
            particle.duration_low = -1.f;
            particle.duration_high = particle.duration_low;
            particle.duration = particle.duration_low;
            break;
        case 0x1B6F:
        {
            cursor = address + 0x20;
            const Vec3 lower = get(command, spin_range_command::lower);
            const Vec3 upper = get(command, spin_range_command::upper);
            const float x = signed_command_range(lower.x, upper.x, random);
            const float y = signed_command_range(lower.y, upper.y, random) * degrees;
            const float z = signed_command_range(lower.z, upper.z, random) * degrees;
            particle.spin = Vec3{x * degrees, y, z};
            break;
        }
        case 0x1B70:
            cursor = address + 0x10;
            particle.bounce = get(command, bounce_command::bounce);
            particle.size = get(command, bounce_command::unk_0C);
            break;
        case 0x1B71:
            cursor = address + 8;
            particle.motion_flags |= 4;
            break;
        case 0x1B72:
            cursor = address + 0xC;
            particle.drag = get(command, value_command::value);
            break;
        case 0x1B73:
            cursor = address + 0x3C;
            configure_actor_effect_bounded_channels(
                particle, get(command, channels_command::lower), get(command, channels_command::upper),
                get(command, channels_command::start), get(command, channels_command::rates),
                i32(get(command, channels_command::random_start)), pal, override_pal, random);
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
            particle.motion_flags |= opcode == 0x1B76 || opcode == 0x1B77 ? 0x11 : 1;
            if (get(command, value_command::word) != 0)
            {
                particle.motion_flags |= 0x20;
            }
            break;
        }
        case 0x1B79:
            cursor = address + 0x98;
            if (get(command, value_command::word) != 0)
            {
                particle.motion_flags |= 0x20;
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
