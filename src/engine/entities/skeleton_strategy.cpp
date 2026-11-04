#include "skeleton_strategy.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_mind_targets.h"
#include "player_strategy.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/anim.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
SkeletonStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<SkeletonStrategyState>(strategy);
}

const SkeletonStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<SkeletonStrategyState>(strategy);
}

void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeSkeletonStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeSkeletonStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of_actor(RuntimeSkeletonStrategy& context)
{
    return goals_of(context).entry.body;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeSkeletonStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// Body +0x48: the actor the skeleton holds.
u32 held_actor(RuntimeSkeletonStrategy& context)
{
    return read_u32(body_of_actor(context), 0x48);
}

void change_state(u16 state, RuntimeSkeletonStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

RuntimeMindTarget* selected_target(RuntimeSkeletonStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context));
    return target;
}

i16 actor_tag(RuntimeSkeletonStrategy& context)
{
    return i16(read_u16(goals_of(context).entry.actor_bytes, 6));
}
}

// Xbox 0x000A9FC0
void skeleton_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = 0x40;
    output.parameters = 0x18;
}

// Xbox 0x000C05C0
bool runtime_skeleton_delay_runs(RuntimeSkeletonStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    if (runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) == 0)
    {
        return false;
    }
    return strategy_base(strategy_of(context)).state != 0x405;
}

// Xbox 0x000C0170
bool join_runtime_skeleton_target(RuntimeMindTarget& target, RuntimeSkeletonStrategy& context)
{
    if (held_actor(context) != 0)
    {
        return true;
    }
    return join_runtime_strategy_target_queue(context.base.link, target.record, &goals_of(context).entry.actor,
        context.queue);
}

// Xbox 0x000C01A0
bool select_runtime_skeleton_flank(Vec3* output, RuntimeSkeletonStrategy& context)
{
    Actor& actor = goals_of(context).entry.actor;
    if (held_actor(context) != 0)
    {
        return false;
    }
    bool missing = true;
    const Vec3 position = actor_world_position(actor);
    float distance;
    Actor* nearest = find_nearest_runtime_script_actor(&position, actor, actor_tag(context), 30.f, &distance, 0x88,
        nullptr, context.base.query);
    if (nearest != nullptr)
    {
        missing = false;
        const Vec3 other = actor_world_position(*nearest);
        const Vec3 away{position.x - other.x, position.y - other.y, position.z - other.z};
        // The query's distance slot is reused as the angle; a zero direction leaves it there.
        float angle = distance;
        runtime_direction_angle(away, angle);
        const float sine = float(retail_sin(angle)) * 35.f; // DAT_0032B494.
        if (output != nullptr)
        {
            output->x = other.x + sine;
            output->y = other.y;
            output->z = float(static_cast<long double>(other.z) + retail_cos(angle) * 35.L);
        }
    }
    return !missing;
}

// Xbox 0x000C02D0
void reposition_runtime_skeleton(RuntimeSkeletonStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    Vec3 flank;
    if (!select_runtime_skeleton_flank(&flank, context))
    {
        return;
    }
    const std::span<const u8> actor = goals.entry.actor_bytes;
    Vec3 position;
    if (!entity_detail::select_navigation_spawn_position(
            i32(get(actor, player_field::floor)), nullptr, flank, 0.f, 60.f, goals.room, goals.entry.allowed, 0xC,
            actor_tag(context), position, goals.random,
            0.f)) // Flag 0x10 is clear, so the retained occupancy scratch is not read.
    {
        return;
    }
    change_state(0x3FE, context);
    RuntimeMindGoalContext& mind = goals.entry.goals;
    RuntimePlayerMindRequest* request = allocate_runtime_mind_request(mind.anchor, 0x2E, mind.sizes);
    put_float(request->bytes, 0x1C, 1.f); // DAT_0032B0F8.
    put_float(request->bytes, 0x20, 1.f);
    put_float(request->bytes, 0x28, 30.f); // DAT_0032B108.
    put_float(request->bytes, 0x10, position.x);
    put_float(request->bytes, 0x14, position.y);
    put_float(request->bytes, 0x18, position.z);
}

// Xbox 0x000C03D0
bool runtime_skeleton_target_crowded(const RuntimeMindTarget* target, RuntimeSkeletonStrategy& context)
{
    if (target == nullptr || held_actor(context) != 0)
    {
        return false;
    }
    const Vec3 point{get(target->bytes, mind_target_field::shape_position).x, get(target->bytes, mind_target_field::shape_position).y, get(target->bytes, mind_target_field::shape_position).z};
    float distance;
    return find_nearest_runtime_script_actor(&point, goals_of(context).entry.actor, actor_tag(context),
        get(target->bytes, mind_target_field::shape_radius) + 30.f, &distance, 0x88, nullptr, context.base.query) != nullptr;
}

// Xbox 0x000C0430
u16 choose_runtime_skeleton_attack(const RuntimeMindTarget& target, RuntimeSkeletonStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    if (held_actor(context) != 0)
    {
        return 0x3EF;
    }
    if (fields_of(strategy).fetch_delay == 0.f
        && roll_runtime_actor_attack(context.checks.attributes, goals.random)
        && fields_of(strategy).no_spawn_in_reach == 0)
    {
        float distance;
        if (find_runtime_player_nearest_spawn(goals.entry.actor, &distance, 0x17) == nullptr)
        {
            fields_of(strategy).no_spawn_in_reach = 1;
        }
        fields_of(strategy).fetch_delay = context.fetch_delay;
        return 0x405;
    }
    if (runtime_mind_target_requires_action(target, context.views.body_of(target.actor).bytes))
    {
        return 0x3EE;
    }
    const std::span<const u8> attributes = context.checks.attributes;
    const float chance = attributes.data() != nullptr ? read_f32(attributes, 0xC) : 0.f;
    goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    if (!(chance >= roll))
    {
        return 0x3ED;
    }
    return 0x3EE;
}

// Xbox 0x000C0500
void engage_runtime_skeleton(RuntimeSkeletonStrategy& context)
{
    change_state(held_actor(context) != 0 ? 0x3EF : 0x3F9, context);
}

// Xbox 0x000C0590
void set_runtime_skeleton_stance(u8 stance, RuntimeSkeletonStrategy& context)
{
    body_of_actor(context)[0x328] = held_actor(context) != 0 ? 6 : stance;
}

// Xbox 0x000BFE10
void dispatch_runtime_skeleton_message(std::span<const u32> message, RuntimeSkeletonStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(goals.entry.strategy).actor;
    if (react_runtime_strategy_message(message, context.checks) != 0 || message[0] - 0x31 > 0x53)
    {
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    }
    switch (message[0])
    {
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views);
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x19, context.checks);
        return;
    case 0x6E:
    {
        dispatch_runtime_base_strategy_message(message, context.base);
        if (check_runtime_player_knockout(goals) != 0)
        {
            return;
        }
        const u32 retaliated = retaliate_runtime_strategy_contact(message, -1, context.checks, context.views);
        if (runtime_strategy_target_hit_lands(message, context.checks, context.views) != 0 || message[1] == actor)
        {
            clear_runtime_player_mind_requests(*goals.setup.mind);
        }
        if (retaliated != 0 || strategy_base(strategy).state == 0x405)
        {
            return;
        }
        react_runtime_strategy_contact(message, -1.f, context.checks); // DAT_0032B120.
        return;
    }
    case 0x82:
        react_runtime_strategy_cower(context.checks);
        return;
    case 0x84:
        react_runtime_strategy_startle(context.checks);
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    }
}

// Xbox 0x000BFAC0
void update_runtime_skeleton_strategy(RuntimeSkeletonStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals);
    if (runtime_skeleton_delay_runs(context))
    {
        const float step = runtime_frame_step(goals.clock);
        const float delay = fields_of(strategy).fetch_delay;
        if (0.f < delay)
        {
            const float remaining = delay - step;
            fields_of(strategy).fetch_delay = remaining;
            if (remaining <= 0.f)
            {
                fields_of(strategy).fetch_delay = 0.f;
            }
        }
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
        if (join_runtime_skeleton_target(*target, context))
        {
            engage_runtime_skeleton(context);
        }
        reposition_runtime_skeleton(context);
        if (!requests_pending(context))
        {
            change_state(0x40D, context);
        }
        return;
    case 0x3ED:
    case 0x3EE:
    case 0x3EF:
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (!join_runtime_skeleton_target(*target, context))
        {
            change_state(0x40D, context);
            return;
        }
        if (!requests_pending(context))
        {
            change_state(0x3EC, context);
        }
        reposition_runtime_skeleton(context);
        return;
    case 0x3F9:
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (!requests_pending(context))
        {
            change_state(choose_runtime_skeleton_attack(*target, context), context);
        }
        reposition_runtime_skeleton(context);
        if (!join_runtime_skeleton_target(*target, context))
        {
            change_state(0x40D, context);
        }
        return;
    case 0x3FE:
        target = selected_target(context, found);
        if (found && !requests_pending(context))
        {
            change_state(target != nullptr ? 0x40D : 0x401, context);
        }
        return;
    case 0x401:
        target = selected_target(context, found);
        if (found)
        {
            if (runtime_strategy_abandons_target(*target, context.checks) == 0)
            {
                goals.room = nullptr; // Actor +0xCA0.
                engage_runtime_skeleton(context);
                return;
            }
            if (retreat_runtime_strategy_from_reached_target(context.checks) != 0)
            {
                return;
            }
        }
        reposition_runtime_skeleton(context);
        if (!requests_pending(context))
        {
            load_runtime_mind_goal(context.goal_far, goals.entry.goals);
        }
        return;
    case 0x405:
        if (held_actor(context) != 0)
        {
            target = selected_target(context, found);
        }
        else
        {
            set(goals.entry.events.mind, mind_field::target, 0);
        }
        if (requests_pending(context))
        {
            return;
        }
        if (target == nullptr)
        {
            change_state(0x401, context);
            return;
        }
        change_state(choose_runtime_skeleton_attack(*target, context), context);
        return;
    case 0x40D:
    {
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        reposition_runtime_skeleton(context);
        const Vec3 position = actor_world_position(goals.entry.actor); // 0x000415A0, EBX target.
        const float dx = position.x - get(target->bytes, mind_target_field::shape_position).x;
        const float dy = position.y - get(target->bytes, mind_target_field::shape_position).y;
        const float dz = position.z - get(target->bytes, mind_target_field::shape_position).z;
        if (dz * dz + dx * dx + dy * dy > 6400.f) // DAT_0032B6C8.
        {
            change_state(0x3EC, context);
        }
        if (join_runtime_skeleton_target(*target, context) && !runtime_skeleton_target_crowded(target, context))
        {
            engage_runtime_skeleton(context);
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000BFF80
void enter_runtime_skeleton_state(u16 state, u16 previous, RuntimeSkeletonStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    RuntimeMindGoalContext& mind = goals.entry.goals;
    body_of_actor(context)[0x328] = 2;
    strategy_base(strategy).attacking = 0;
    switch (state)
    {
    case 0x3EC:
        set_runtime_skeleton_stance(3, context);
        append_runtime_range_request(mind.anchor, mind.sizes, 40.f);
        return;
    case 0x3ED:
        set_runtime_skeleton_stance(4, context);
        strategy_base(strategy).attacking = 1;
        load_runtime_mind_goal(context.goal_attack, mind);
        return;
    case 0x3EE:
        set_runtime_skeleton_stance(5, context);
        strategy_base(strategy).attacking = 1;
        load_runtime_mind_goal(context.goal_knockdown, mind);
        return;
    case 0x3EF:
    {
        set_runtime_skeleton_stance(6, context);
        strategy_base(strategy).attacking = 1;
        const u32 held = held_actor(context);
        if (held != 0)
        {
            const u8 kind = context.views.actor_bytes_at(held)[0x93D];
            if (kind >= 2 && kind <= 3)
            {
                load_runtime_mind_goal(context.goal_projectile, mind);
            }
        }
        load_runtime_mind_goal(context.goal_weapon, mind);
        return;
    }
    case 0x3F9:
        set_runtime_skeleton_stance(4, context);
        load_runtime_mind_goal(context.goal_dodge, mind);
        return;
    case 0x401:
        load_runtime_mind_goal(context.goal_far, mind);
        return;
    case 0x405:
        set_runtime_skeleton_stance(6, context);
        load_runtime_mind_goal(context.goal_get_weapon, mind);
        return;
    case 0x40D:
        allocate_runtime_mind_request(mind.anchor, 0x17, mind.sizes);
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state);
        return;
    }
}

// Xbox 0x000C0130
void leave_runtime_skeleton_state(u16 state, RuntimeSkeletonStrategy& context)
{
    if (state >= 0x3ED && state <= 0x3EF)
    {
        detach_runtime_strategy_link(context.base.link);
        return;
    }
    if (state == 0x405)
    {
        return;
    }
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_base(strategy_of(context)).pause_count;
    }
}
}
