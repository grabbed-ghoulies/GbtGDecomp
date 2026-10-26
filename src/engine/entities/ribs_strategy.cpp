#include "ribs_strategy.h"
#include "strategy_fields.h"
#include "player_children.h"
#include "player_mind_goals.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "gbtg/entity.h"
#include <cstring>

namespace gbtg
{
namespace
{
RuntimePlayerStrategyGoals& goals_of(RuntimeRibsStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeRibsStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeRibsStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

void change_state(u16 state, RuntimeRibsStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup); // 0x0002B8D0
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x000BF7B0
bool initialize_runtime_ribs_strategy(RuntimeRibsStrategy& context)
{
    context.model_nodes(0, 1); // 0x000F96F0 on actor +0x204, then on +0x310, each when set.
    return true;
}

// Xbox 0x000BF7F0
void update_runtime_ribs_strategy(RuntimeRibsStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u16 state = get(strategy_of(context), strategy_field::state);
    update_runtime_base_strategy(goals); // 0x0002C130
    if (state == 0x401)
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_target(target, 0, goals)) // 0x00139AA0
        {
            const long double distance = runtime_target_distance(goals.entry.actor, target->bytes); // 0x00041560
            if (!(65.f < distance)) // DAT_00428888.
            {
                change_state(0x40C, context);
            }
        }
        if (!requests_pending(context))
        {
            load_runtime_mind_goal(context.goal_far, goals.entry.goals); // 0x0002A8E0
        }
    }
    else if (state == 0x40C)
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_target(target, 0, goals) && !requests_pending(context)) // 0x00139AA0
        {
            change_state(0x401, context);
        }
    }
}

// Xbox 0x000BF8B0
void dispatch_runtime_ribs_message(std::span<const u32> message, RuntimeRibsStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 actor = read_u32(strategy_of(context), 8);
    if (react_runtime_strategy_message(message, context.checks) == 0 && message[0] == 0x6E) // 0x001384F0
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) == 0 && message[1] == actor) // 0x0013A490, EBX strategy
        {
            clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
        }
        return;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
}

// Xbox 0x000BF910
void enter_runtime_ribs_state(u16 state, u16 previous, RuntimeRibsStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = goals.entry.body; // Actor +0x95C.
    body[0x328] = 2;
    strategy[0x1A] = 0;
    strategy[0x1B] = 0;
    if (state == 0x401)
    {
        load_runtime_mind_goal(context.goal_far, goals.entry.goals); // 0x0002A8E0
        return;
    }
    if (state != 0x40C)
    {
        enter_runtime_base_strategy_state(goals.entry, state); // The tail jump to 0x0002BAF0.
        return;
    }
    const EntityBitBlock* bits = goals.body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error("0x000BF910 reads the body +0x15C bit block of a body with none");
    }
    if ((bits->bytes[2] & 0x10) != 0)
    {
        select_runtime_body_action(body, context.action_key(), context.actions); // Body vtable +0x20, then 0x00027650
    }
    RuntimeMindGoalContext& mind = goals.entry.goals;
    RuntimePlayerMindRequest* request = allocate_runtime_mind_request(mind.anchor, 0xD, mind.sizes); // 0x00029D90
    put_float(request->bytes, 0x10, 20.f); // DAT_0042888C.
    put_float(request->bytes, 0x14, 25.f); // DAT_00428890.
    put_float(request->bytes, 0x18, 60.f); // DAT_00428894.
    put_float(request->bytes, 0x1C, 65.f); // DAT_00428898.
    const u16 full = read_u16(body, 0x96);
    if (full != 0)
    {
        const float fraction = float(read_u16(body, 0x94)) / float(full);
        if (fraction <= 0.75f) // DAT_0032B2AC.
        {
            if (fraction <= 0.5f) // DAT_0032B0F4.
            {
                put_float(request->bytes, 0x20, fraction <= 0.25f ? 1.f : 0.8f); // DAT_0032B104, DAT_004288A8 / A4.
                return;
            }
            put_float(request->bytes, 0x20, 0.6f); // DAT_004288A0.
            return;
        }
    }
    put_float(request->bytes, 0x20, 0.4f); // DAT_0042889C.
}
}
