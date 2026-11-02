#include "body_sound_commands.h"
#include "sound_names.h"
#include "bank_lookup.h"
#include "gbtg/anim.h"
#include "../entities/body_fields.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00107780
const char* runtime_actor_tag_suffix(u16 tag, std::span<const char* const> names)
{
    const char* name = names[tag];
    i32 last = 0;
    i32 index = 0;
    u8 character = u8(name[0]);
    while (character != 0)
    {
        const bool underscore = character == '_';
        character = u8(name[index + 1]);
        if (underscore)
        {
            last = index;
        }
        ++index;
    }
    return name + last + 1;
}

// Xbox 0x000259C0
float runtime_body_step_speed(std::span<const u8> body, const Actor& actor, const RuntimeBodyDamageTables& tables)
{
    return get(body, body_field::step_speed) * runtime_class_damage_value(body[0x328], actor.damage_class, tables);
}

// Xbox 0x0001A020
void format_runtime_body_step_sound(char* output, std::span<const u8> body,
    u32 alternate, std::span<const char* const> names, const FootstepSoundNames& sounds)
{
    const u8 surface = get(body, body_field::surface_override_frames) == 0 ? get(body, body_field::surface)
                                                                           : get(body, body_field::surface_override);
    const u16 tag = read_u16(body, 0x87C);
    output[0] = 0;
    if (surface != 0)
    {
        const u8 mode = alternate == 0 ? body[0x2D3] : body[0x2D4];
        if (mode == 0)
        {
            format_footstep_sound_name(output, surface, "", runtime_actor_tag_suffix(tag, names), sounds);
        }
        else if (mode == 1)
        {
            format_footstep_sound_name(output, surface, "alt", runtime_actor_tag_suffix(tag, names), sounds);
            return;
        }
    }
}

// Xbox 0x0001A0A0
void format_runtime_body_impact_sound(char* output, std::span<const u8> body,
    const char* kind, std::span<const char* const> names, const FootstepSoundNames& sounds)
{
    const u16 tag = read_u16(body, 0x87C);
    output[0] = 0;
    const char* name = names[tag];
    i32 last = 0;
    i32 index = 0;
    u8 character = u8(name[0]);
    while (character != 0)
    {
        const bool underscore = character == '_';
        character = u8(name[index + 1]);
        if (underscore)
        {
            last = index;
        }
        ++index;
    }
    format_sound_name(output, "XACT_SOUNDBANK_G%sIMPACT%s_%s", kind, sounds.surfaces[1], name + last + 1);
    if (output[0] == 0 || !resolve_audio_bank_cue(output, nullptr, nullptr))
    {
        format_sound_name(output, "XACT_SOUNDBANK_G%sIMPACT%s_DEFAULT", kind, sounds.surfaces[1]);
    }
}

// Xbox 0x00019B40
void command_runtime_body_step_sound(std::span<const u8> body,
    std::span<const u8> command, RuntimeBodySoundCommandContext& context)
{
    const float x = context.motion_actor.anchor.x;
    const float z = context.motion_actor.anchor.z;
    const float speed = float(retail_square_root(x * x + z * z));
    const float maximum = runtime_body_step_speed(context.motion_body, context.motion_actor, context.tables);
    float fraction = speed / maximum;
    if (!(1.f > fraction))
    {
        fraction = 1.f;
    }
    fraction = fraction * std::bit_cast<float>(0x3E4CCCCCu) + std::bit_cast<float>(0x3F4CCCCDu);
    float volume;
    if (!(fraction > 1.f))
    {
        volume = std::bit_cast<float>(0x3F4CCCCDu);
        if (!(volume > fraction))
        {
            volume = fraction;
        }
    }
    else
    {
        volume = 1.f;
    }
    const float radius = read_u32(command, 0x14) == 0 ? context.actor.radius_xz : context.actor.scale_200 * context.actor.scale_x;
    std::array<char, 128> name;
    format_runtime_body_step_sound(name.data(), body, read_u32(command, 0x10), context.names, context.sounds);
    if (name[0] != 0)
    {
        queue_actor_sound(context.sound, 1, &context.actor, 3, nullptr, nullptr, radius * volume, name.data(), context.queues);
    }
}

// Xbox 0x00019F80
void command_runtime_body_floor_sound(std::span<const u8> body, RuntimeBodySoundCommandContext& context)
{
    std::array<char, 128> name;
    format_runtime_body_impact_sound(name.data(), body, "floor", context.names, context.sounds);
    if (name[0] != 0)
    {
        queue_actor_sound(context.sound, 0, &context.actor, 5, nullptr, nullptr, 1.f, name.data(), context.queues);
    }
}

// Xbox 0x00019FD0
void command_runtime_body_impact_sound(std::span<const u8> body, RuntimeBodySoundCommandContext& context)
{
    std::array<char, 128> name;
    format_runtime_body_impact_sound(name.data(), body, "body", context.names, context.sounds);
    if (name[0] != 0)
    {
        queue_actor_sound(context.sound, 0, &context.actor, 5, nullptr, nullptr, 1.f, name.data(), context.queues);
    }
}
}
