#include "pirate_strategy.h"
#include "strategy_fields.h"
#include "weapon_fields.h"
#include "mind_fields.h"
#include "body_message_helpers.h"
#include "body_timers.h"
#include "mind_target_policy.h"
#include "player_goal_cleanup.h"
#include "player_mind_events.h"
#include "player_mind_feedback.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "player_parent_lifecycle.h"
#include "player_strategy_goals.h"
#include "../graphics/actor_effect_stream.h"
#include "../scripts/script_actor_query.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
PirateStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<PirateStrategyState>(strategy);
}

const PirateStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<PirateStrategyState>(strategy);
}


RuntimePlayerStrategyGoals& goals_of(RuntimePirateStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimePirateStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimePirateStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimePirateStrategy& context)
{
    return goals_of(context).entry.body;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimePirateStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimePirateStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimePirateStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimeMindTarget* selected_target(RuntimePirateStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

bool join_target(RuntimeMindTarget& target, RuntimePirateStrategy& context)
{
    return join_runtime_strategy_target_queue(context.base.link, target.record, &goals_of(context).entry.actor,
        context.queue); // 0x0002B910, EDI target
}

// States 0x3EF and 0x3FA leave for 0x401 once he wants no chest or holds a sword, else fetch one (0x405).
void after_throw(RuntimePirateStrategy& context)
{
    if (fields_of(strategy_of(context)).wants_chest == 0 || runtime_pirate_holds_sword(context))
    {
        change_state(0x401, context);
        return;
    }
    change_state(0x405, context);
}

// Body +0x4C dropped (0x0001B6E0, EAX body).
void drop_secondary(RuntimePirateStrategy& context)
{
    drop_runtime_body_secondary(body_of(context), context.attachment.secondary,
        context.attachment.actions.second_receiver);
}
}

// Xbox 0x000BE0C0
void pirate_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(PirateStrategyState);
    output.parameters = 0x98;
}

// Xbox 0x000BE130
bool initialize_runtime_pirate_strategy(std::span<u8> strategy, std::span<const u8> parameters)
{
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(fields_of(strategy).angry_effect.data(), name, std::strlen(name) + 1);
    fields_of(strategy).wants_chest = 1u;
    return true;
}

// Xbox 0x000BEB50
bool runtime_pirate_holds_sword(RuntimePirateStrategy& context)
{
    const u32 held = read_u32(body_of(context), 0x48);
    if (held == 0)
    {
        return false;
    }
    return context.views.actor_bytes_at(held)[0x93D] == 1;
}

// Xbox 0x000BEB80
bool runtime_strategy_weapon_free(std::span<const u8> strategy, RuntimeStrategyActorViews& views)
{
    const u32 chest = strategy_view<CarriedWeaponStrategy>(strategy).weapon;
    if (chest == 0)
    {
        return false;
    }
    const Actor& held = *views.actor_at(chest);
    const WeaponBase& weapon = weapon_base(views.actor_bytes_at(chest));
    // Chest +0x918's first dword is the parent node: none, the root's (its own first dword null), or a node of no
    // actor (the node minus 0x918 is null).
    const auto* parent = held.link == nullptr ? nullptr : static_cast<const ActorLink*>(held.link->head);
    return (parent == nullptr || parent->head == nullptr || held.link->owner == nullptr) && weapon.mask != 0
        && weapon.spawn_blocked == 0 && weapon.spawn_point_count != 0;
}

// 0x000BEB80 over the pirate's chest (+0x38).
bool runtime_pirate_chest_free(RuntimePirateStrategy& context)
{
    return runtime_strategy_weapon_free(strategy_of(context), context.views);
}

// Xbox 0x000BEBD0
void choose_runtime_pirate_attack(RuntimePirateStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> attributes = context.checks.attributes; // Actor +0x964.
    const float chance = attributes.data() != nullptr ? read_f32(attributes, 8) : 0.f;
    goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
    const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    if (chance >= roll)
    {
        change_state(0x3EF, context);
        return;
    }
    const bool many_hits = i32(fields_of(strategy_of(context)).hits) >= 5;
    if (runtime_pirate_holds_sword(context)
        && (roll_runtime_actor_knockdown(attributes, goals.random) || many_hits)) // 0x00019750
    {
        change_state(0x3EE, context);
        return;
    }
    change_state(0x3ED, context);
}

// Xbox 0x000BECF0
bool runtime_pirate_reacts(RuntimePirateStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    return state != 0x3EF && state != 0x3FA && state != 0x405;
}

// Xbox 0x000BED10, EAX message, ECX strategy, ESI actor
void count_runtime_pirate_hit(std::span<const u32> message, RuntimePirateStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (message[2] != strategy_base(strategy).actor || message[4] != 0)
    {
        return;
    }
    fields_of(strategy).hits = fields_of(strategy).hits + 1;
    fields_of(strategy).hit_window = 2.f; // DAT_0032B110.
}

// Xbox 0x000BE160
void update_runtime_pirate_strategy(RuntimePirateStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    float window = fields_of(strategy).hit_window;
    const bool closed = count_down_runtime_strategy_live_timer(window, goals); // 0x0002BA60, EDX +0xD0
    fields_of(strategy).hit_window = window;
    if (closed)
    {
        fields_of(strategy).hits = 0u;
    }
    bool found;
    RuntimeMindTarget* target = nullptr;
    switch (state)
    {
    case 0x3EC:
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (join_target(*target, context))
        {
            choose_runtime_pirate_attack(context);
        }
        if (!requests_pending(context))
        {
            change_state(0x40D, context);
        }
        return;
    case 0x3ED:
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (!join_target(*target, context))
        {
            change_state(0x40D, context);
            return;
        }
        if (!requests_pending(context))
        {
            change_state(0x3EE, context);
        }
        return;
    case 0x3EE:
        target = selected_target(context, found);
        if (!found || requests_pending(context))
        {
            return;
        }
        if (fields_of(strategy).wants_chest != 0 && !runtime_pirate_holds_sword(context))
        {
            change_state(0x405, context);
            return;
        }
        if (join_target(*target, context))
        {
            choose_runtime_pirate_attack(context);
            return;
        }
        change_state(0x40D, context);
        return;
    case 0x3EF:
        target = selected_target(context, found);
        if (!found || requests_pending(context))
        {
            return;
        }
        if (fields_of(strategy).angry_object == 0)
        {
            change_state(0x3FA, context);
            return;
        }
        after_throw(context);
        return;
    case 0x3FA:
    {
        const EntityBitBlock* bits = goals.body_bits; // Body +0x15C.
        if (bits == nullptr)
        {
            throw Error("pirate strategy 0x000BE160 reads the body +0x15C bit block of a body with none");
        }
        fields_of(strategy).drink_open = u32((bits->bytes[3] >> 2) & 1);
        if (requests_pending(context))
        {
            return;
        }
        after_throw(context);
        return;
    }
    case 0x401:
    {
        if (!runtime_pirate_holds_sword(context) && runtime_pirate_chest_free(context))
        {
            change_state(0x405, context);
            return;
        }
        target = selected_target(context, found);
        if (found)
        {
            if (runtime_strategy_abandons_target(*target, context.checks) == 0) // 0x00138410
            {
                goals.room = nullptr; // Actor +0xCA0.
                choose_runtime_pirate_attack(context);
                return;
            }
            if (retreat_runtime_strategy_from_reached_target(context.checks) != 0) // 0x001383A0
            {
                return;
            }
        }
        float delay = fields_of(strategy).drink_delay;
        count_down_runtime_strategy_timer(delay, goals); // 0x0002B9F0, ECX +0x48
        fields_of(strategy).drink_delay = delay;
        if (requests_pending(context))
        {
            return;
        }
        if (fields_of(strategy).drink_delay == 0.f && actor_event_chance(goals.random, 0.25f)) // 0x00019200
        {
            goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
            const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            fields_of(strategy).drink_delay = roll * 5.f + 5.f; // DAT_0032B184.
            change_state(0x3FA, context);
            return;
        }
        load_goal(context.goal_far, context);
        return;
    }
    case 0x405:
        if (requests_pending(context)
            && (fields_of(strategy).fetching == 0 || runtime_pirate_chest_free(context)))
        {
            return;
        }
        fields_of(strategy).wants_chest = 0u;
        change_state(0x401, context);
        return;
    case 0x40D:
    {
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (runtime_squared_target_distance(goals.entry.actor, target->bytes) > 6400.f) // 0x000415A0, DAT_0032B6C8
        {
            change_state(0x3EC, context);
        }
        if (join_target(*target, context))
        {
            choose_runtime_pirate_attack(context);
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000BE520
void dispatch_runtime_pirate_message(std::span<const u32> message, RuntimePirateStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    if (message[0] == 0x64 && strategy_base(strategy).state == 0x3FA)
    {
        return;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
    if (runtime_pirate_reacts(context) && react_runtime_strategy_message(message, context.checks) != 0) // 0x001384F0
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10 again
        return;
    }
    switch (message[0])
    {
    case 0x14:
        if (fields_of(strategy).angry_object == message[1])
        {
            fields_of(strategy).angry_object = 0u;
        }
        return;
    case 0x31:
        if (runtime_pirate_reacts(context))
        {
            react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x18, context.checks); // 0x0013A4F0
        return;
    case 0x3B:
    {
        if (runtime_pirate_holds_sword(context) || runtime_pirate_chest_free(context))
        {
            return;
        }
        Actor* chest = find_runtime_player_nearest_spawn(goals.entry.actor, nullptr, 0x1F); // 0x00129C20
        const u32 address = chest == nullptr ? 0u : context.actor_address(*chest);
        fields_of(strategy).chest = address;
        if (address != 0)
        {
            fields_of(strategy).wants_chest = 1u;
            fields_of(strategy).fetching = 0u;
        }
        return;
    }
    case 0x6E:
    {
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        if (script_actor_current_mask(goals.entry.actor, context.base.query) != 0) // 0x00028360
        {
            return;
        }
        retaliate_runtime_strategy_contact(message, -1, context.checks, context.views); // 0x00139390
        if (runtime_pirate_reacts(context))
        {
            react_runtime_strategy_contact(message, -1.f, context.checks); // 0x001395C0, DAT_0032B120
        }
        if (message[1] != actor)
        {
            count_runtime_pirate_hit(message, context); // 0x000BED10
            return;
        }
        const std::span<u8> body = body_of(context);
        u16 next = 0x401;
        if (strategy_base(strategy).state == 0x3FA && fields_of(strategy).drink_open != 0 && message[2] != 0)
        {
            const char* effect = fields_of(strategy).angry_effect.data();
            fields_of(strategy).angry_object = context.create_effect(effect, actor); // 0x0003DF40
            increment_runtime_body_latch_mask(body, 0x3F, goals.entry.actor, context.latches); // 0x0001F3F0
            std::memset(body.data() + 0x240, 0, 4);
            std::memset(body.data() + 0x23C, 0, 4);
            next = 0x3EF;
            RuntimeMindTarget* target = runtime_actor_active_target(*context.base.message_actor(message[2])); // 0x00042070
            if (target != nullptr)
            {
                const std::span<const u8> actor_bytes = goals.entry.actor_bytes;
                if (runtime_mind_target_is_eligible(mind_of(context), actor_bytes, goals.entry.actor,
                        goals.address_of(actor_bytes.data()), *target, true) // 0x00029C80
                    || (context.views.story_flags & 0x100) == 0) // DAT_0050EA98
                {
                    set_runtime_mind_target(goals.entry.events, target->address); // 0x0002A030
                }
            }
        }
        drop_secondary(context); // 0x0001B6E0
        change_state(next, context);
        return;
    }
    case 0x7C:
        // The tail jump to 0x00140A40 over strategy +0x38.
        clear_runtime_player_mind_reference(strategy, offsetof(PirateStrategyState, chest), message[1]);
        return;
    case 0x7D:
        fields_of(strategy).chest = read_u32(body_of(context), 0x48);
        return;
    case 0x82:
        if (runtime_pirate_reacts(context))
        {
            react_runtime_strategy_cower(context.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (runtime_pirate_reacts(context))
        {
            react_runtime_strategy_startle(context.checks); // The tail jump to 0x001399A0.
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000BE820
void enter_runtime_pirate_state(u16 state, u16 previous, RuntimePirateStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> mind = mind_of(context);
    const u32 target_address = get(mind, mind_field::target_record);
    const std::span<u8> body = body_of(context);
    RuntimeMindGoalContext& goal_context = goals.entry.goals;
    body[0x328] = 2;
    strategy_base(strategy).attacking = 0;
    const auto clear_hits = [&]()
    {
        fields_of(strategy).hits = 0u;
        fields_of(strategy).hit_window = 0.f;
    };
    switch (state)
    {
    case 0x3EC:
        body[0x328] = 3;
        append_runtime_range_request(goal_context.anchor, goal_context.sizes, 40.f); // 0x00136AC0
        return;
    case 0x3ED:
        body[0x328] = 4;
        strategy_base(strategy).attacking = 1;
        load_goal(context.goal_attack, context);
        return;
    case 0x3EE:
        body[0x328] = 5;
        strategy_base(strategy).attacking = 1;
        load_goal(runtime_pirate_holds_sword(context) ? context.goal_knockdown : context.goal_knockdown_confused,
            context);
        clear_hits();
        return;
    case 0x3EF:
        body[0x328] = 6;
        strategy_base(strategy).attacking = 1;
        if (fields_of(strategy).angry_object != 0)
        {
            load_goal(context.goal_angry, context);
        }
        if (target_address != 0
            && runtime_strategy_abandons_target(*goals.target_at(target_address), context.checks) == 0) // 0x00138410
        {
            load_goal(runtime_pirate_holds_sword(context) ? context.goal_special : context.goal_special_no_sword,
                context);
            load_goal(fields_of(strategy).angry_object != 0 ? context.goal_throw_chests : context.goal_throw_chest,
                context);
        }
        clear_hits();
        return;
    case 0x3FA:
        fields_of(strategy).drink_open = 0u;
        load_goal(context.goal_drink, context);
        return;
    case 0x401:
        load_goal(context.goal_far, context);
        return;
    case 0x405:
    {
        body[0x328] = 3;
        const bool free = runtime_pirate_chest_free(context);
        fields_of(strategy).fetching = free ? 1u : 0u;
        set(mind, mind_field::action_actor, fields_of(strategy).chest);
        load_goal(free ? context.goal_get_weapon : context.goal_get_weapon_confused, context);
        return;
    }
    case 0x40D:
        allocate_runtime_mind_request(goal_context.anchor, 0x17, goal_context.sizes); // 0x00029D90
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000BEA60
void leave_runtime_pirate_state(u16 state, RuntimePirateStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20.
        return;
    case 0x3EF:
    {
        const std::span<u8> body = body_of(context);
        const u32 angry = fields_of(strategy).angry_object;
        if (angry != 0)
        {
            RuntimeBodyTimerRecipient recipient{[&](std::span<const u32> message)
                {
                    context.object_message(angry, message);
                }};
            expire_runtime_body_timer_recipient(recipient); // 0x0013B240
            decrement_runtime_body_latch_mask(body, 0x3F, goals_of(context).entry.actor, context.latches); // 0x0001F2F0
            fields_of(strategy).angry_object = 0u;
        }
        if (context.attachment.secondary != nullptr) // Body +0x4C.
        {
            drop_secondary(context); // 0x0001B6E0
        }
        // The body's vtable +0xC with {0x25, ...}: the second word is the caller's stack, which the body's 0x25
        // handlers do not read.
        const std::array<u32, 2> message{0x25, 0};
        context.object_message(goals_of(context).address_of(body.data()), message);
        return;
    }
    case 0x3FA:
        fields_of(strategy).drink_open = 0u;
        if (context.attachment.secondary != nullptr) // Body +0x4C.
        {
            drop_secondary(context); // The tail jump to 0x0001B6E0.
        }
        return;
    case 0x405:
        fields_of(strategy).fetching = 0u;
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            --strategy_base(strategy).pause_count;
        }
        return;
    }
}
}
