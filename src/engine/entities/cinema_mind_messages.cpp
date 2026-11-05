#include "cinema_mind_messages.h"
#include "mind_fields.h"
#include "player_fields.h"
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
            const std::span<const u8>* target = mind_record(mind).action_record == 0 ? nullptr :
                &context.target_at(mind_record(mind).action_record).bytes;
            notify_runtime_player_mind_attachment_callout(parent, target, message[1]);
            break;
        }
    case 0x37:
        request_runtime_mind_turn(context.boost,
            Vec3{std::bit_cast<float>(message[1]), std::bit_cast<float>(message[2]),
                std::bit_cast<float>(message[3])});
        break;
    case 0x56:
        if (mind_record(mind).target_record != 0
            && !runtime_mind_target_is_eligible(mind, context.actor_bytes, parent.actor,
                context.actor_address, context.target_at(mind_record(mind).target_record), true))
        {
            mind_record(mind).target_record = 0;
            parent.target_bytes = {};
            parent.target_actor = nullptr;
        }
        break;
    case 0x5B:
        request_runtime_mind_boost(context.boost);
        break;
    case 0x64:
        if (player_record(context.actor_bytes).mode == 1)
        {
            mind_record(mind).target_flags = mind_record(mind).target_flags | 0x20;
        }
        break;
    case 0x6E:
        if (message[1] == mind_record(mind).actor)
        {
            const float timer = mind_record(mind).alert_timer + 5.f;
            mind_record(mind).alert_timer = timer;
            if (timer > 15.f)
            {
                mind_record(mind).alert_timer = 15.f;
            }
        }
        break;
    case 0x7D:
        initialize_runtime_player_mind_navigation(mind, context.floor, context.grids,
            context.nodes, context.storage, context.address_of);
        break;
    case 0x94:
        if (message[1] == mind_record(mind).action_actor)
        {
            cinema_mind_record(mind).notices = 1;
        }
        break;
    default:
        break;
    }
    dispatch_runtime_parent_mind_message(parent.events, message, parent.target_bytes);
}
}
