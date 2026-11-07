#include "zombie_strategy.h"
#include "gbtg/retail_field.h"
#include "weapon_fields.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_action_selection.h"
#include "player_children.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "../navigation/mind_navigation_angles.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
const ZombieParameters& parameters_of(RuntimeZombieStrategy& context)
{
    return retail_view<ZombieParameters>(context.parameters);
}

ZombieStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<ZombieStrategyState>(strategy);
}

const ZombieStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<ZombieStrategyState>(strategy);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeZombieStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeZombieStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of_actor(RuntimeZombieStrategy& context)
{
    return goals_of(context).entry.body;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeZombieStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeZombieStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// Body +0x30 clear and +0x34 at zero (DAT_0032B100).
bool body_settled(RuntimeZombieStrategy& context)
{
    const std::span<const u8> body = body_of_actor(context);
    return body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f;
}

// Mind +0x10: the current target record, or null.
RuntimeMindTarget* mind_target(RuntimeZombieStrategy& context)
{
    const u32 address = mind_record(mind_of(context)).target_record;
    return address == 0 ? nullptr : goals_of(context).target_at(address);
}

void change_state(u16 state, RuntimeZombieStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

bool selected_target(RuntimeZombieStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    return select_runtime_strategy_target(target, 0, goals_of(context));
}

// The inlined test in 0x000C4F40 and 0x000C5670: a target the zombie gives up and retreats from.
bool retreats_from_target(RuntimeZombieStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    return target != nullptr && runtime_strategy_abandons_target(*target, context.checks) != 0
        && retreat_runtime_strategy_from_reached_target(context.checks) != 0;
}

bool target_attackable(RuntimeZombieStrategy& context)
{
    return runtime_strategy_target_attackable(100000.f, context.base.link, context.checks, context.queue);
}

// 0x00138310 with the block's width (+0) and range (+4), then 0x00139FA0 with the block as its settings.
RuntimePlayerMindRequest* request_position(float heading, RuntimeZombieStrategy& context)
{
    Vec3 position;
    if (!select_runtime_strategy_roam_position(position, heading, parameters_of(context).move.range,
        parameters_of(context).move.width, context.checks))
    {
        return nullptr;
    }
    return request_runtime_strategy_position(position, parameters_of(context).move, goals_of(context));
}
}

// Xbox 0x000C57F0
bool initialize_runtime_zombie_strategy(std::span<u8> strategy)
{
    fields_of(strategy).parameters = 0x004288B0u;
    return true;
}

// Xbox 0x000C53C0
bool runtime_zombie_body_state_shown(RuntimeZombieStrategy& context)
{
    const std::span<const u8> body = body_of_actor(context);
    if (body_record(body).unk_2D0 != 0 && body_record(body).unk_2D0 == 1)
    {
        return body_record(body).unk_2D2 != 0;
    }
    return false;
}

// Xbox 0x000C5390
bool runtime_zombie_body_state_hidden(RuntimeZombieStrategy& context)
{
    const std::span<const u8> body = body_of_actor(context);
    if (body_record(body).unk_2D0 != 0 && body_record(body).unk_2D0 == 1)
    {
        return body_record(body).unk_2D2 == 0;
    }
    return true;
}

// Xbox 0x000C4E70
void wait_runtime_zombie_weapon(RuntimeZombieStrategy& context)
{
    if (body_settled(context) && !requests_pending(context))
    {
        change_state(0x40C, context);
    }
}

// Xbox 0x000C4EE0
void hold_runtime_zombie_target(RuntimeZombieStrategy& context)
{
    if (runtime_strategy_link_target(context.base.link, context.queue) == nullptr)
    {
        clear_runtime_player_mind_requests(*goals_of(context).setup.mind);
    }
    if (body_settled(context) && !requests_pending(context))
    {
        choose_runtime_zombie_state(context);
    }
}

// Xbox 0x000C4F40
void approach_runtime_zombie_target(RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    if (!body_settled(context))
    {
        return;
    }
    if (retreats_from_target(context))
    {
        return;
    }
    if (target_attackable(context))
    {
        change_state(choose_runtime_zombie_attack(context), context);
        return;
    }
    if (requests_pending(context))
    {
        return;
    }
    const u32 approach = fields_of(strategy).approach;
    if (approach == 1)
    {
        if (actor_event_chance(goals.random, parameters_of(context).aimless_swipe_chance)
            && mind_record(mind_of(context)).target_record == 0)
        {
            fields_of(strategy).approach = 2u;
            load_runtime_mind_goal(context.goal_swipe_aimless, goals.entry.goals);
            return;
        }
    }
    else if (approach != 2)
    {
        return;
    }
    set_runtime_zombie_approach(1, context);
}

// Xbox 0x000C5010
RuntimePlayerMindRequest* request_runtime_zombie_flank(const RuntimeMindTarget& target, u32& beside,
    RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const Actor& actor = goals.entry.actor;
    const Vec3 position = actor_world_position(actor);
    const Vec3 toward{mind_target_record(target.bytes).shape.position.x - position.x,
        mind_target_record(target.bytes).shape.position.y - position.y,
        mind_target_record(target.bytes).shape.position.z - position.z};
    // The angle goes to the target argument's stack slot; a zero direction leaves the target's address there.
    float angle = std::bit_cast<float>(target.address);
    runtime_direction_angle(toward, angle);
    float heading = angle;
    const Vec3 again = actor_world_position(actor);
    const float dx = again.x - mind_target_record(target.bytes).shape.position.x;
    const float dy = again.y - mind_target_record(target.bytes).shape.position.y;
    const float dz = again.z - mind_target_record(target.bytes).shape.position.z;
    const float near = parameters_of(context).sidestep_reach;
    if (near * near > dz * dz + dy * dy + dx * dx)
    {
        goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
        const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
        float side = 1.5707964f; // DAT_0032B140.
        if (!(0.5f < roll)) // DAT_0032B0F4.
        {
            side = -1.5707964f; // DAT_0032B13C.
        }
        heading = angle + side;
        beside = 1;
    }
    else
    {
        beside = 0;
    }
    return request_position(heading, context);
}

// Xbox 0x000C5190
RuntimePlayerMindRequest* request_runtime_zombie_roam(RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    return request_position(read_f32(goals.entry.actor_bytes, 0x18), context);
}

// Xbox 0x000C51E0
void set_runtime_zombie_approach(u32 mode, RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    fields_of(strategy_of(context)).approach = mode;
    if (mode == 2)
    {
        load_runtime_mind_goal(context.goal_swipe_aimless, goals.entry.goals);
        return;
    }
    if (mode != 1)
    {
        return;
    }
    u32 beside = 1;
    RuntimePlayerMindRequest* request = nullptr;
    u8 stance = 0;
    const RuntimeMindTarget* target = mind_target(context);
    if (target != nullptr && runtime_strategy_abandons_target(*target, context.checks) == 0)
    {
        request = request_runtime_zombie_flank(*target, beside, context);
        stance = 3;
    }
    else
    {
        request = request_runtime_zombie_roam(context);
        stance = 2;
    }
    body_record(body_of_actor(context)).move_class = stance;
    if (request == nullptr)
    {
        return;
    }
    if (beside != 0 && !runtime_zombie_body_state_shown(context))
    {
        return;
    }
    set(request->bytes, navigation_request::speed_low, 1.f); // DAT_0032B0F8.
    set(request->bytes, navigation_request::speed_high, 1.f);
}

// Xbox 0x000C5290
void begin_runtime_zombie_special(RuntimeZombieStrategy& context)
{
    std::span<u8> strategy = strategy_of(context);
    fields_of(strategy).special_hit = 0u;
    load_runtime_mind_goal(context.goal_special, goals_of(context).entry.goals);
    strategy_base(strategy).attacking = 1;
}

// Xbox 0x000C52C0
void end_runtime_zombie_special(RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    strategy_base(strategy).attacking = 0;
    if (fields_of(strategy).special_hit != 0 || !body_settled(context))
    {
        return;
    }
    const u16 key = goals.body_action_key(0x2C); // Body vtable +0x2C.
    select_runtime_body_action(body_of_actor(context), key, goals.actions); // 0x00027650, EBX body, AX key
}

// Xbox 0x000C5300
void update_runtime_zombie_special(RuntimeZombieStrategy& context)
{
    if (fields_of(strategy_of(context)).special_hit != 0)
    {
        strategy_base(strategy_of(context)).attacking = 0;
    }
    if (body_settled(context) && !requests_pending(context))
    {
        choose_runtime_zombie_state(context);
    }
}

// Xbox 0x000C5350
void update_runtime_zombie_wait(RuntimeZombieStrategy& context)
{
    if (body_settled(context) && !requests_pending(context))
    {
        choose_runtime_zombie_state(context);
    }
}

// Xbox 0x000C5410
void react_runtime_zombie_message(std::span<const u32> message, RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 actor = strategy_base(strategy_of(context)).actor;
    switch (message[0])
    {
    case 0x36:
        notice_runtime_strategy_hostile_target(0x1F, context.checks);
        return;
    case 0x6E:
    {
        const bool own = message[1] == actor;
        dispatch_runtime_base_strategy_message(message, context.base);
        if (check_runtime_player_knockout(goals) != 0)
        {
            return;
        }
        if (own)
        {
            return_runtime_zombie_to_approach(context);
            retaliate_runtime_strategy_contact(message, -1, context.checks, context.views);
            return;
        }
        if (!runtime_message_hits_mind_target(message, actor, mind_of(context), goals.target_at))
        {
            return;
        }
        return_runtime_zombie_to_approach(context);
        if (runtime_zombie_body_state_hidden(context))
        {
            react_runtime_strategy_contact(message, -1.f, context.checks); // DAT_0032B120.
        }
        return;
    }
    case 0x7A:
        mind_record(mind_of(context)).action_actor = message[3];
        return;
    case 0x82:
        if (runtime_zombie_body_state_hidden(context))
        {
            react_runtime_strategy_cower(context.checks);
        }
        return;
    case 0x84:
        if (runtime_zombie_body_state_hidden(context))
        {
            react_runtime_strategy_startle(context.checks);
        }
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    }
}

// Xbox 0x000C5570
void return_runtime_zombie_to_approach(RuntimeZombieStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    if ((state >= 0x3ED && state <= 0x3EF) || state == 0x401)
    {
        change_state(0x40C, context);
    }
}

// Xbox 0x000C55D0
u16 choose_runtime_zombie_attack(RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const RuntimeMindTarget* target = mind_target(context);
    if (!runtime_zombie_body_state_shown(context) && roll_runtime_actor_attack(context.checks.attributes, goals.random))
    {
        return 0x3EF;
    }
    const EntityBitBlock& bits = *target->body.bits; // The target actor's body +0x15C.
    if ((bits.bytes[3] & 2) != 0)
    {
        return 0x3EE;
    }
    if ((bits.bytes[2] & 2) != 0 && runtime_body_target_state_active(context.views.body_of(target->actor).bytes))
    {
        return 0x3EE;
    }
    if (roll_runtime_actor_knockdown(context.checks.attributes, goals.random))
    {
        return 0x3EE;
    }
    return 0x3ED;
}

// Xbox 0x000C5670
void choose_runtime_zombie_state(RuntimeZombieStrategy& context)
{
    if (retreats_from_target(context))
    {
        return;
    }
    u16 state = 0;
    if (!runtime_zombie_body_state_shown(context))
    {
        state = target_attackable(context) ? choose_runtime_zombie_attack(context) : 0x40C;
    }
    else
    {
        state = choose_runtime_zombie_weapon_state(context);
    }
    change_state(state, context);
}

// Xbox 0x000C5730
u16 choose_runtime_zombie_weapon_state(RuntimeZombieStrategy& context)
{
    const std::span<const u8> mind = mind_of(context);
    const u32 weapon = mind.data() == nullptr ? 0 : mind_record(mind).action_actor;
    if (weapon != 0)
    {
        const Actor& held = *context.views.actor_at(weapon);
        const std::span<const u8> bytes = context.views.actor_bytes_at(weapon);
        // Weapon +0x918's first dword is the parent node: none, the root's (its own first dword null), or a node of
        // no actor (the node minus 0x918 is null).
        const auto* parent = held.link == nullptr ? nullptr : static_cast<const ActorLink*>(held.link->head);
        if ((parent == nullptr || parent->head == nullptr || held.link->owner == nullptr) && weapon_base(bytes).mask != 0
            && weapon_base(bytes).spawn_blocked == 0 && weapon_base(bytes).spawn_point_count != 0)
        {
            return 0x405;
        }
    }
    if (target_attackable(context))
    {
        return choose_runtime_zombie_attack(context);
    }
    return 0x40C;
}

// Xbox 0x000C5810
void update_runtime_zombie_strategy(RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals);
    switch (state)
    {
    case 0x3ED:
    case 0x3EE:
        if (selected_target(context))
        {
            hold_runtime_zombie_target(context);
        }
        return;
    case 0x3EF:
        if (selected_target(context))
        {
            update_runtime_zombie_special(context);
        }
        return;
    case 0x405:
        wait_runtime_zombie_weapon(context);
        return;
    case 0x40C:
        selected_target(context);
        if (state == strategy_base(strategy).state)
        {
            approach_runtime_zombie_target(context);
        }
        return;
    case 0x416:
        update_runtime_zombie_wait(context);
        return;
    default:
        return;
    }
}

// Xbox 0x000C5920
void dispatch_runtime_zombie_message(std::span<const u32> message, RuntimeZombieStrategy& context)
{
    if (react_runtime_strategy_message(message, context.checks) != 0)
    {
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    }
    if (strategy_base(strategy_of(context)).state == 0x3EF && message[0] == 0x3B)
    {
        fields_of(strategy_of(context)).special_hit = 1u;
        return;
    }
    react_runtime_zombie_message(message, context);
}

// Xbox 0x000C5970
void enter_runtime_zombie_state(u16 state, u16 previous, RuntimeZombieStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeMindGoalContext& mind = goals.entry.goals;
    std::span<u8> body = body_of_actor(context);
    if ((previous < 0x3ED || previous > 0x3EF) && state >= 0x3ED && state <= 0x3EF)
    {
        goals.room = nullptr; // Actor +0xCA0.
    }
    body_record(body).move_class = 2;
    switch (state)
    {
    case 0x3ED:
        body_record(body).move_class = 4;
        load_runtime_mind_goal(context.goal_swipe, mind);
        return;
    case 0x3EE:
        body_record(body).move_class = 5;
        load_runtime_mind_goal(context.goal_kick, mind);
        return;
    case 0x3EF:
        body_record(body).move_class = 6;
        begin_runtime_zombie_special(context);
        return;
    case 0x405:
        load_runtime_mind_goal(context.goal_get_weapon, mind);
        return;
    case 0x40C:
        fields_of(strategy_of(context)).approach = 0u;
        set_runtime_zombie_approach(1, context);
        return;
    case 0x416:
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state);
        return;
    }
}

// Xbox 0x000C5AD0
void leave_runtime_zombie_state(u16 state, u16 next, RuntimeZombieStrategy& context)
{
    if (state >= 0x3ED && state <= 0x3EF && (next < 0x3ED || next > 0x3EF))
    {
        detach_runtime_strategy_link(context.base.link);
    }
    switch (state)
    {
    case 0x3ED:
    case 0x3EE:
    case 0x405:
    case 0x416:
        return;
    case 0x3EF:
        end_runtime_zombie_special(context);
        return;
    case 0x40C:
        fields_of(strategy_of(context)).approach = 0u;
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            --strategy_base(strategy_of(context)).pause_count;
        }
        return;
    }
}
}
