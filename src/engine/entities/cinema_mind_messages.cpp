#include "cinema_mind_messages.h"
#include "mind_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x0007C8B0
void dispatch_runtime_cinema_mind_message(RuntimeCinemaMindMessages& context,
    std::span<const u32> message)
{
    auto& parent = context.parent;
    auto mind = parent.events.mind;
    switch (message[0])
    {
    case 0x23:
        refresh_runtime_cinema_mind_action_permission(mind, context.actor_bytes,
            context.allowed_count, context.allowed_rows);
        break;
    case 0x31:
        {
            const std::span<const u8>* target = get(mind, mind_field::action_record) == 0 ? nullptr :
                &context.target_at(get(mind, mind_field::action_record)).bytes;
            notify_runtime_player_mind_attachment_callout(parent, target, message[1]);
            break;
        }
    case 0x37:
        request_runtime_mind_turn(context.boost,
            Vec3{std::bit_cast<float>(message[1]), std::bit_cast<float>(message[2]),
                std::bit_cast<float>(message[3])});
        break;
    case 0x56:
        if (get(mind, mind_field::target_record) != 0
            && !runtime_mind_target_is_eligible(mind, context.actor_bytes, parent.actor,
                context.actor_address, context.target_at(get(mind, mind_field::target_record)), true))
        {
            set(mind, mind_field::target_record, 0);
            parent.target_bytes = {};
            parent.target_actor = nullptr;
        }
        break;
    case 0x5B:
        request_runtime_mind_boost(context.boost);
        break;
    case 0x64:
        if (read_u16(context.actor_bytes, 0xCDC) == 1)
        {
            set(mind, mind_field::target_flags, get(mind, mind_field::target_flags) | 0x20);
        }
        break;
    case 0x6E:
        if (message[1] == get(mind, mind_field::actor))
        {
            const float timer = get(mind, mind_field::alert_timer) + 5.f;
            set(mind, mind_field::alert_timer, timer);
            if (timer > 15.f)
            {
                set(mind, mind_field::alert_timer, 15.f);
            }
        }
        break;
    case 0x7D:
        initialize_runtime_player_mind_navigation(mind, context.floor, context.grids,
            context.nodes, context.storage, context.address_of);
        break;
    case 0x94:
        if (message[1] == get(mind, mind_field::action_actor))
        {
            word(mind, 0x90, 1);
        }
        break;
    default:
        break;
    }
    dispatch_runtime_parent_mind_message(parent.events, message, parent.target_bytes);
}
}
