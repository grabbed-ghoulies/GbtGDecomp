#include "mind_action_requests.h"
#include "actor_transform.h"
#include "actor_live_attachments.h"
#include "../scripts/script_vm.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x00137A80
bool request_runtime_mind_boost(RuntimeMindBoost& context)
{
    auto body = context.attachment.body;
    if (get(context.actor_bytes, player_field::mode) == 2 || get(context.actor_bytes, player_field::case_link) != 0)
    {
        if ((runtime_player_can_resume_strategy(context.actor_bytes, context.resume) != 0
                || (context.story_word & 0x80) != 0)
            && body_record(body).energy != 0 && (context.flags.bytes[7] & 2) == 0
            && (context.flags.bytes[7] & 4) == 0)
        {
            const u16 key = context.range_action_key();
            if (runtime_body_has_action_id(context.attachment.actions.table, key))
            {
                put(context.boost, 0xC, 0);
                put(context.boost, 8, 0);
                put(context.boost, 0x14, 0);
                put(context.boost, 0x10, 0);
                put(context.boost, 0x18, 0);
                put(context.boost, 0x1C, 1);
                put(context.boost, 0x24, 0);
                put(context.boost, 0x20, 0);
                put(context.boost, 0x2C, 0);
                put(context.boost, 0x28, 0);
                put(context.boost, 0x34, 1);
                put(context.boost, 0x30, 0);
                if (!(read_f32(context.boost, 4) > 0.f))
                {
                    const u32 fresh = test_and_set_runtime_story_bit(context.story_bits, 1);
                    put(context.boost, 0, fresh);
                    put(context.boost, 4, std::bit_cast<u32>(fresh != 0 ? 2.5f : 5.f));
                    set_runtime_body_bit(context.flags, 0x2C);
                    release_runtime_player_attachment(0, 0, context.attachment);
                    if (runtime_body_timer_is_idle(body))
                    {
                        select_runtime_body_action(body, key, context.attachment.actions);
                    }
                    ++body_record(body).unk_309;
                    ++body_record(body).unk_30A;
                    ++body_record(body).unk_30B;
                    ++body_record(body).unk_30C;
                    const std::array<i32, 2> message{0x77, i32(context.actor_address)};
                    broadcast_actor_pulse_message(message.data());
                    return true;
                }
                put(context.boost, 4,
                    std::bit_cast<u32>(read_u32(context.boost, 0) != 0 ? 2.5f : 5.f));
                if (!runtime_body_bit_set(context.flags, 0x1E))
                {
                    select_runtime_body_action(body, key, context.attachment.actions);
                }
            }
            return false;
        }
    }
    return false;
}

// Xbox 0x00137C30
bool request_runtime_mind_turn(RuntimeMindBoost& context, const Vec3& point)
{
    auto body = context.attachment.body;
    if ((get(context.actor_bytes, player_field::mode) == 2 || get(context.actor_bytes, player_field::case_link) != 0)
        && !(read_f32(context.boost, 4) > 0.f)
        && (context.flags.bytes[7] & 2) == 0 && (context.flags.bytes[7] & 4) == 0
        && body_record(body).energy != 0)
    {
        if (runtime_body_timer_is_idle(body) && body_record(body).unk_6A == 0)
        {
            const u16 key = context.contact_action_key();
            if (!runtime_body_has_action_id(context.attachment.actions.table, key))
            {
                return false;
            }
            const float angle = runtime_actor_direction_to_point(context.actor, point);
            set_runtime_player_actor_rotation(context.actor, context.body_actor, body, Vec3{0.f, angle, 0.f});
            select_runtime_body_action(body, key, context.attachment.actions);
            return true;
        }
    }
    return false;
}

}
