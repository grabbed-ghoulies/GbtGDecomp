#include "background_messages.h"
#include "../entities/actor_bind_tree.h"
#include "../entities/actor_matrix_setup.h"
#include "../entities/actor_world_lists.h"
#include "../entities/body_damage_tables.h"
#include "../entities/runtime_list.h"
#include "../runtime/crt_strings.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32 read_word(BackgroundMessageContext& context, u32 address)
{
    return read_u32(context.resolve(address, 4), 0);
}

void send_rumble(Actor& actor, i32 part, Actor& receiver, BackgroundMessageContext& context)
{
    const BackgroundRumbleMessage reply{0x50, background_part_rumble(actor, part, context.parts.dispatch.settings,
        context.parts.dispatch.setting_count, context.rumble_fallback, context.zero)};
    context.message(receiver, &reply);
}
}

// Xbox 0x000441F0
u32 break_background_part_hit(Actor& actor, std::span<const u8> hit, u32 award, u32 category, u32 value,
    BackgroundMessageContext& context)
{
    std::span<const u8> header = background_part_header(actor);
    if (header.empty())
    {
        header = context.zero;
    }
    const i32 part = std::bit_cast<i32>(read_u32(hit, 0x18));
    const auto& rows = context.parts.removal.volumes;
    const std::span<const BackgroundAnimationVolume> live(rows.data(), size_t(context.parts.row_count));
    if (part == -1 || find_active_background_part(live, part) != -1)
    {
        return 0;
    }
    if (size_t(part) >= rows.size())
    {
        throw Error("background hit part lies outside actor +0x94C");
    }
    if (read_u32(rows[size_t(part)].fields, 0xC) != 0)
    {
        return 0;
    }
    if (background_part_can_be_destroyed(actor, live, part, context.zero))
    {
        Vec3 source;
        std::memcpy(&source, hit.data(), 12);
        dispatch_background_part_destruction(actor, part, source, value, context.parts);
    }
    ++context.broken_parts;
    if (award != 0)
    {
        const u32 strings = read_u32(header, 4);
        const char* name = "unknown"; // DAT_00322F70.
        if (strings != 0)
        {
            const size_t metadata = u32(read_u32(hit, 0x18)) * 12;
            name = reinterpret_cast<const char*>(header.data() + strings + header[metadata + 0x17]);
        }
        char key[32]{};
        scene_detail::crt_copy_string_count(key, name, 0x1F);
        if (lookup_borrowed_localtext(context.text_resource, "backgroundparttable", key, context.scratch,
                context.text) == 0)
        {
            return 0;
        }
        const u32 points = background_part_settings_value(key, context.parts.dispatch.settings,
            context.parts.dispatch.setting_count);
        award_runtime_score(std::bit_cast<i32>(points), category, context.scratch, 0, context.score);
    }
    return 1;
}

// Xbox 0x000441A0
i32 break_background_part_hits(Actor& actor, u32 hits, u32 award, u32 category, u32 value, i32& part,
    BackgroundMessageContext& context)
{
    const u32 begin = read_word(context, hits + 4);
    const u32 end = read_word(context, hits + 8);
    i32 broken = 0;
    for (u32 hit = begin; hit < end; hit += 0x54)
    {
        const auto record = context.resolve(hit, 0x54);
        if (break_background_part_hit(actor, record, award, category, value, context) != 0)
        {
            ++broken;
            part = std::bit_cast<i32>(read_u32(record, 0x18));
        }
    }
    return broken;
}

// Xbox 0x00043E50
void handle_background_message(Actor& actor, std::span<u32> message, BackgroundMessageContext& context)
{
    i32 part = 0;
    switch (message[0])
    {
    case 8:
    {
        const u32 attacker = message[4];
        u32 category = 0;
        u32 found = 0;
        if (attacker != 0 && !runtime_actor_is_player(*context.actor_at(attacker)))
        {
            const std::span<const u8> body = context.body_of(attacker); // Attacker +0x95C.
            const u32 first = read_u32(body, 0x270);
            const u32 second = read_u32(body, 0x274);
            if (first != 0 && runtime_actor_is_player(*context.actor_at(first)))
            {
                category = 2;
                found = 1;
            }
            else if (second != 0 && runtime_actor_is_player(*context.actor_at(second)))
            {
                category = 3;
                found = 1;
            }
        }
        const u32 award = message[3] != 0 && found != 0 ? 1 : 0;
        const i32 broken = break_background_part_hits(actor, message[2], award, category, attacker, part, context);
        message[1] = u32(broken);
        if (broken != 0 && message[4] != 0)
        {
            send_rumble(actor, part, *context.actor_at(message[4]), context);
        }
        break;
    }
    default:
        context.base_message(message);
        break;
    case 0x15:
    {
        const u32 award = runtime_actor_is_player(*context.actor_at(message[2])) ? 1 : 0;
        const i32 broken = break_background_part_hits(actor, message[6], award, 1, message[2], part, context);
        message[1] = u32(broken);
        if (broken != 0)
        {
            send_rumble(actor, part, *context.actor_at(message[2]), context);
        }
        break;
    }
    case 0x19:
    {
        const u32 attacker = message[7];
        const u32 owner = read_word(context, attacker + 0xD8C);
        const u32 award = owner == 0 ? 0 : runtime_actor_is_player(*context.actor_at(owner)) ? 1 : 0;
        const i32 broken = break_background_part_hits(actor, message[0xB], award, 5, attacker, part, context);
        message[4] = u32(broken);
        message[6] = 0;
        if (owner != 0 && broken != 0)
        {
            send_rumble(actor, part, *context.actor_at(owner), context);
        }
        break;
    }
    case 0x1D:
    {
        const u32 attacker = message[5];
        const u32 owner = read_word(context, attacker + 0xD8C);
        const u32 award = owner == 0 ? 0 : runtime_actor_is_player(*context.actor_at(owner)) ? 1 : 0;
        const i32 broken = break_background_part_hits(actor, message[9], award, 4, attacker, part, context);
        message[4] = u32(broken);
        if (broken != 0)
        {
            send_rumble(actor, part, *context.actor_at(owner), context);
        }
        break;
    }
    case 0x23:
        context.base_message(message);
        unregister_actor_render_bins(context.base.render);
        if (actor_model_link_head_offset(actor) != 0)
        {
            append_runtime_list_node(context.model_list, context.base.render.anchor);
        }
        clear_actor_shape_probe(actor, context.base.render.shape);
        if (actor.link == nullptr)
        {
            throw Error("background activation unlinks a null actor +0x918 node");
        }
        unlink_actor_bind_node(*actor.link);
        break;
    case 0x66:
        prepare_background_volume_navigation(actor, context.parts.removal.volumes);
        break;
    case 0x67:
    {
        const std::span<const u8> cpu = actor.model == nullptr ? std::span<const u8>{} : actor.model->cpu;
        register_selected_background_doors(cpu,
            std::span<const u8>(reinterpret_cast<const u8*>(message.data()), message.size_bytes()), context.doors);
        break;
    }
    case 0x8B:
        break;
    }
}
}
