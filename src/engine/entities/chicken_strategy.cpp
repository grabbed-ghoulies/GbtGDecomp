#include "chicken_strategy.h"
#include "strategy_fields.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "strategy_target_selection.h"
#include <cmath>

namespace gbtg
{
namespace
{
ChickenStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<ChickenStrategyState>(strategy);
}


RuntimePlayerStrategyGoals& goals_of(RuntimeChickenStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeChickenStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeChickenStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeChickenStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}
}

// Xbox 0x000AA030
bool initialize_runtime_chicken_strategy(std::span<u8> strategy)
{
    fields_of(strategy).parameters = 0x00302B5Cu;
    fields_of(strategy).provoked = 0u;
    return true;
}

// Xbox 0x000AA050
void update_runtime_chicken_strategy(RuntimeChickenStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    if (state != 0x401)
    {
        return;
    }
    RuntimeMindTarget* target = nullptr;
    if (select_runtime_strategy_target(target, 0, goals) // 0x00139AA0
        && runtime_strategy_abandons_target(*target, context.shared.checks) != 0 // 0x00138410, EDX target
        && retreat_runtime_strategy_from_reached_target(context.shared.checks) != 0) // 0x001383A0
    {
        return;
    }
    if (fields_of(strategy).provoked != 0)
    {
        const EntityBitBlock* bits = goals.body_bits; // Body +0x15C.
        if (bits == nullptr)
        {
            throw Error("0x000AA050 reads the body +0x15C bit block of a body with none");
        }
        if ((bits->bytes[5] & 2) != 0)
        {
            change_state(0x3EF, context);
            return;
        }
    }
    if (goals.setup.mind->first == nullptr) // Mind +0x58.
    {
        load_goal(context.goal_far, context); // The tail jump to 0x0002A8E0.
    }
}

// Xbox 0x000AA0F0
void dispatch_runtime_chicken_message(std::span<const u32> message, RuntimeChickenStrategy& context)
{
    RuntimeSpiderStrategy& shared = context.shared;
    const std::span<u8> strategy = strategy_of(context);
    react_runtime_strategy_message(message, shared.checks); // 0x001384F0; its result is not read.
    switch (message[0])
    {
    case 0x6E:
    {
        const u32 sender = message[1];
        const u32 actor = strategy_base(strategy).actor;
        if (sender == actor)
        {
            fields_of(strategy).provoked = 1u;
        }
        else if (read_u16(shared.views.actor_bytes_at(sender), 6) == 7)
        {
            const Vec3 own = actor_world_position(goals_of(context).entry.actor); // 0x000412A0
            const Vec3 other = actor_world_position(*shared.views.actor_at(sender)); // 0x000412A0
            const long double dx = (long double)own.x - other.x;
            const long double dy = (long double)own.y - other.y;
            const long double dz = (long double)own.z - other.z;
            const long double distance = std::sqrt(dz * dz + dy * dy + dx * dx);
            if ((long double)read_f32(context.parameters, 0) > distance) // FCOMPI; JBE keeps it unarmed.
            {
                fields_of(strategy).provoked = 1u;
            }
        }
        break;
    }
    case 0x82:
        react_runtime_strategy_cower(shared.checks); // 0x001398A0
        break;
    case 0x84:
        react_runtime_strategy_startle(shared.checks); // 0x001399A0
        break;
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
}

// Xbox 0x000AA1D0: it reads the new state from strategy +0xE.
void enter_runtime_chicken_state(u16 state, RuntimeChickenStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    switch (strategy_base(strategy_of(context)).state)
    {
    case 0x3EF:
        clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
        load_goal(context.goal_transform, context);
        return;
    case 0x3F4:
        goals.entry.body[0x328] = 6;
        enter_runtime_base_strategy_state(goals.entry, state); // The jump to 0x0002BAF0.
        return;
    case 0x401:
        goals.entry.body[0x328] = 2;
        load_goal(context.goal_far, context);
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // The jump to 0x0002BAF0.
        return;
    }
}
}
