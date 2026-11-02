#include "butler_strategy.h"
#include "mind_fields.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "../graphics/actor_effect_stream.h"
#include "../scripts/script_actor_query.h"
#include "../scripts/script_conditions.h"
#include "player_fields.h"
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeButlerStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeButlerStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> mind_of(RuntimeButlerStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

void change_state(u16 state, RuntimeButlerStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

// Actor +0xCDC == 2 or a +0x93C link: the actor is a player.
bool actor_is_player(std::span<const u8> actor)
{
    return get(actor, player_field::mode) == 2 || get(actor, player_field::case_link) != 0;
}

// The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
u16 first_goal_kind(RuntimeButlerStrategy& context)
{
    const RuntimePlayerGoalRing* ring = goals_of(context).entry.ring;
    if (ring->tail == ring->capacity)
    {
        return read_u16(goals_of(context).entry.address_zero, 0);
    }
    return read_u16(ring->rows, size_t(ring->stride) * ring->head);
}
}

// Xbox 0x000AB020
void butler_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = 0x98;
    output.parameters = 0x1C;
}

// Xbox 0x000AB090
bool initialize_runtime_butler_strategy(std::span<u8> strategy, std::span<u8> actor, const char* name)
{
    if (name != nullptr)
    {
        std::memcpy(strategy.data() + 0x38, name, std::strlen(name) + 1);
    }
    put(actor, 0xCF0, 1);
    return true;
}

// Xbox 0x000AB260
u32 find_runtime_butler_threat(RuntimeButlerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> mind = mind_of(context);
    float distance = 0.f;
    Actor* found = find_nearest_runtime_script_actor(nullptr, goals.entry.actor, 0, 0.f, &distance, 0x40240,
        nullptr, context.base.query);
    if (found == nullptr)
    {
        // The search's distance slot is reused as the selection's output.
        RuntimeMindTarget* target = nullptr;
        select_runtime_strategy_target(target, 0, goals);
        return 0;
    }
    set(mind, mind_field::target, context.actor_address(*found));
    const u32 target = context.first_live_attachment_at(context.actor_address(*found)); // Actor +0x8F8.
    if (target != 0)
    {
        set_runtime_mind_target(goals.entry.events, target);
        return 1;
    }
    set(mind, mind_field::target_record, 0);
    return 1;
}

// Xbox 0x000AB0C0
void update_runtime_butler_strategy(RuntimeButlerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    update_runtime_base_strategy(goals);
    switch (read_u16(strategy_of(context), 0xE))
    {
    case 0x3FB:
        if (goals.goal_volume == nullptr)
        {
            find_runtime_butler_threat(context);
        }
        break;
    case 0x401:
        if (find_runtime_butler_threat(context) != 0)
        {
            change_state(0x411, context);
        }
        break;
    case 0x411:
        if (goals.setup.mind->first == nullptr)
        {
            load_runtime_mind_goal(context.goal_scared, goals.entry.goals);
        }
        if (find_runtime_butler_threat(context) == 0)
        {
            change_state(0x401, context);
        }
        break;
    default:
        break;
    }
    alert_runtime_goal_listener(goals);
}

// Xbox 0x000AB170
void dispatch_runtime_butler_message(std::span<const u32> message, RuntimeButlerStrategy& context)
{
    dispatch_runtime_base_strategy_message(message, context.base);
    if (message[0] == 0x6E)
    {
        react_runtime_butler_hit(message, context);
    }
}

// Xbox 0x000AB2D0
void react_runtime_butler_hit(std::span<const u32> message, RuntimeButlerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 self = read_u32(strategy_of(context), 8);
    const u32 first = message[1];
    const u32 second = message[2];
    if (first == self || second == 0 || strategy_of(context)[0x1C] != 0)
    {
        return;
    }
    if (runtime_player_can_resume_strategy(goals.entry.actor_bytes, goals.resume) == 0)
    {
        return;
    }
    if (goals.entry.ring != nullptr && first_goal_kind(context) != 0)
    {
        return;
    }
    constexpr float chance = 0.5f; // DAT_0032B0F4.
    if (actor_is_player(context.actor_bytes_at(second)) && message[4] != 0)
    {
        if (actor_event_chance(goals.random, chance) || count_runtime_live_actors() == 0)
        {
            change_state(0x3F2, context);
        }
        return;
    }
    if (actor_is_player(context.actor_bytes_at(first)) && message[4] != 0
        && actor_event_chance(goals.random, chance))
    {
        change_state(0x418, context);
    }
}

// Xbox 0x000AB1A0
void enter_runtime_butler_state(u16 state, u16 previous, RuntimeButlerStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    switch (state)
    {
    case 0x401:
        load_runtime_mind_goal(context.goal_far, goals.entry.goals);
        return;
    case 0x411:
    {
        load_runtime_mind_goal(context.goal_scared, goals.entry.goals);
        const char* name = reinterpret_cast<const char*>(strategy_of(context).data() + 0x38);
        if (*name != '\0')
        {
            std::memcpy(goals.entry.body.data() + 0x288, name, std::strlen(name) + 1);
        }
        return;
    }
    default:
        enter_runtime_base_strategy_state(goals.entry, state);
        return;
    }
}

// Xbox 0x000AB220
void leave_runtime_butler_state(u16 state, RuntimeButlerStrategy& context)
{
    if (state == 0x411)
    {
        goals_of(context).entry.body[0x288] = 0;
    }
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_of(context)[0x1C];
    }
}
}
