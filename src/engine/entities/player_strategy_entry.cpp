#include "player_strategy_entry.h"
#include "mind_fields.h"
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
std::span<const u8> current_row(const RuntimePlayerGoalRing& ring, std::span<const u8> address_zero)
{
    if (ring.tail == ring.capacity)
    {
        return address_zero;
    }
    return ring.rows.subspan(size_t(ring.head) * ring.stride);
}
}

// Xbox 0x00138870
void enter_runtime_player_goal(RuntimePlayerStrategyEntry& context)
{
    if (read_u32(context.actor_bytes, 0x98C) != 0)
    {
        return;
    }
    context.body[0x328] = 7;
    u16 kind;
    if (context.ring == nullptr)
    {
        kind = 0;
    }
    else
    {
        kind = read_u16(current_row(*context.ring, context.address_zero), 0);
    }
    std::array<u8, 12> position;
    if (context.ring != nullptr)
    {
        const auto row = current_row(*context.ring, context.address_zero);
        put(position, 0, read_u32(row, 4));
        put(position, 4, read_u32(row, 8));
        put(position, 8, read_u32(row, 0xC));
    }
    switch (kind)
    {
    case 1:
    case 8:
    case 0x15:
    {
        auto* request = append_runtime_position_request(context.goals.anchor, context.goals.sizes, position);
        if (runtime_player_goal_flags(context.ring, context.address_zero) != 0 || context.actor_bytes[0x989] != 0)
        {
            put(request->bytes, 0x28, 0);
            return;
        }
        break;
    }
    case 10:
    {
        auto* request = append_runtime_position_request(context.goals.anchor, context.goals.sizes, position);
        if (runtime_player_goal_flags(context.ring, context.address_zero) != 0 || context.actor_bytes[0x989] != 0)
        {
            put(request->bytes, 0x28, 0);
        }
        float distance;
        auto* target = find_runtime_mind_target(context.actor, context.allowed, 0, 0, &distance,
            get(context.events.mind, mind_field::target_categories), (get(context.events.mind, mind_field::target_flags) & ~0x20u) | 8u,
            context.targets);
        set_runtime_mind_target(context.events, target == nullptr ? 0 : target->address);
        if (!load_runtime_mind_goal(context.goal_target, context.goals))
        {
            load_runtime_mind_goal(context.goal_default, context.goals);
            return;
        }
        break;
    }
    case 0x22:
    {
        auto* request = append_runtime_position_request(context.goals.anchor, context.goals.sizes, position);
        put(request->bytes, 0x28, 0);
        break;
    }
    }
}

// Xbox 0x0002BAF0
void enter_runtime_base_strategy_state(RuntimePlayerStrategyEntry& context, u16 state)
{
    switch (state)
    {
    case 0x3E9:
        load_runtime_mind_goal(context.goal_3e9, context.goals);
        return;
    case 0x3F2:
    case 0x3F3:
        load_runtime_mind_goal(context.goal_default, context.goals);
        return;
    case 0x3F4:
        context.body[0x328] = 8;
        load_runtime_mind_goal(context.goal_3f4, context.goals);
        return;
    case 0x3FB:
        enter_runtime_player_goal(context);
        return;
    case 0x3FC:
        set(context.events.mind, mind_field::target_record, 0);
        for (LiveScript* event = context.events.anchor.next; event != nullptr; event = event->next)
        {
            if (u16(event->type) == 3)
            {
                script_release_event(event);
                break;
            }
        }
        ++context.strategy[0x1C];
        return;
    case 0x3FF:
        context.body[0x328] = 7;
        [[fallthrough]];
    case 0x415:
        ++context.strategy[0x1C];
        return;
    case 0x418:
        load_runtime_mind_goal(context.goal_418, context.goals);
        return;
    default:
        return;
    }
}
}
