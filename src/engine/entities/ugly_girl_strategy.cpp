#include "ugly_girl_strategy.h"
#include "strategy_fields.h"
#include "player_mind_goals.h"
#include "player_strategy.h"
#include "gbtg/entity.h"
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = ugly_girl_strategy_field;
namespace parameter = ugly_girl_strategy_parameter;

RuntimePlayerStrategyGoals& goals_of(RuntimeUglyGirlStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeUglyGirlStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeUglyGirlStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

void change_state(u16 state, RuntimeUglyGirlStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup); // 0x0002B8D0
}

bool target_selected(RuntimeUglyGirlStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    return select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
}
}

// Xbox 0x000C1300
void ugly_girl_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000C1370
bool initialize_runtime_ugly_girl_strategy(std::span<u8> strategy, std::span<const u8> parameters)
{
    const auto source = record(parameters, parameter::attachment);
    const auto target = record(strategy, field::attachment);
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
    return true;
}

// Xbox 0x000C1390
void update_runtime_ugly_girl_strategy(RuntimeUglyGirlStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u16 state = strategy_base(strategy_of(context)).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    if (state == 0x3EC)
    {
        if (target_selected(context) && !requests_pending(context))
        {
            load_runtime_mind_goal(context.goal_attack, goals.entry.goals); // 0x0002A8E0
        }
    }
    else if (state == 0x401)
    {
        if (target_selected(context))
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x3EC, context);
        }
    }
}

// Xbox 0x000C1420
void dispatch_runtime_ugly_girl_message(std::span<const u32> message, RuntimeUglyGirlStrategy& context)
{
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
}

// Xbox 0x000C1430
void enter_runtime_ugly_girl_state(u16 state, u16 previous, RuntimeUglyGirlStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = goals.entry.body; // Actor +0x95C.
    body[0x328] = 2;
    if (state == 0x3EC)
    {
        u32 token = 0;
        const char* name = reinterpret_cast<const char*>(record(strategy, field::attachment).data());
        if (read_u32(record(strategy, field::attachment), 0) == 0x5F646961) // "aid_"
        {
            ActorAttachmentNode* node = actor_create_attachment(context.attachments, name, 0, 1); // 0x0003D1B0
            if (node != nullptr)
            {
                const u16 mode = 4;
                std::memcpy(node->bytes.data() + 0xC, &mode, 2);
                token = read_u16(node->bytes, 0x18);
            }
        }
        set(strategy, field::attachment_token, token);
        body[0x328] = 3;
        load_runtime_mind_goal(context.goal_attack, goals.entry.goals); // 0x0002A8E0
        return;
    }
    if (state == 0x401)
    {
        load_runtime_mind_goal(context.goal_far, goals.entry.goals); // 0x0002A8E0
        return;
    }
    enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
}

// Xbox 0x000C14E0
void leave_runtime_ugly_girl_state(u16 state, RuntimeUglyGirlStrategy& context)
{
    std::span<u8> strategy = strategy_of(context);
    if (state == 0x3EC)
    {
        if (get(strategy, field::attachment_token) != 0)
        {
            remove_runtime_body_attachment(u16(get(strategy, field::attachment_token)), context.lists); // 0x0003CFD0
            set(strategy, field::attachment_token, 0u);
        }
        return;
    }
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_base(strategy).pause_count;
    }
}
}
