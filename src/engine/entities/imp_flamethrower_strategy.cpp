#include "imp_flamethrower_strategy.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_messages.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../graphics/actor_effect_stream.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/anim.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = imp_flamethrower_strategy_field;

RuntimePlayerStrategyGoals& goals_of(RuntimeImpFlamethrowerStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeImpFlamethrowerStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeImpFlamethrowerStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeImpFlamethrowerStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeImpFlamethrowerStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

RuntimeMindTarget* select_target(bool& found, RuntimeImpFlamethrowerStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

bool join_queue(const RuntimeMindTarget* target, RuntimeImpFlamethrowerStrategy& context)
{
    return join_runtime_strategy_target_queue(context.shared.base.link, target == nullptr ? nullptr : target->record,
        &goals_of(context).entry.actor, context.shared.queue); // 0x0002B910, EDI the target
}

float roll(RuntimeImpFlamethrowerStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vec3(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

// 30 units (DAT_0032B108) from the point along the heading, at its height.
Vec3 point_along(const Vec3& from, float heading)
{
    const float x = float(retail_sin(heading) * 30.L + from.x); // 0x00153320
    const float z = float(retail_cos(heading) * 30.L + from.z); // 0x00153230
    return Vec3{x, from.y, z};
}

// 0x00111420, EAX 0x1C, ESI output: a clear point within 20 of the centre.
bool clear_point(i32 floor, const Vec3& origin, const Vec3& center, Vec3& output,
    RuntimeImpFlamethrowerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    return select_navigation_spawn_position(floor, &origin, center, 0.f, 20.f, goals.room, goals.entry.allowed, 0x1C,
        i16(read_u16(goals.entry.actor_bytes, 6)), output, goals.random, context.shared.checks.occupancy_initial);
}

// The two mind requests both moves end with. 0xF: +0x10 the pace, +0x14 / +0x1C 1, +0x18 25 or 5 by +0x3C,
// +0x24 0 (written 30 first), +0x28 0.25; 0x29: +0x10 and +0x14.
void request_move(float pace, float wait, float linger, RuntimeImpFlamethrowerStrategy& context)
{
    RuntimeMindGoalContext& mind = goals_of(context).entry.goals;
    RuntimePlayerMindRequest* move = allocate_runtime_mind_request(mind.anchor, 0xF, mind.sizes); // 0x00029D90, EBX 0xF
    put_float(move->bytes, 0x10, pace);
    put_float(move->bytes, 0x14, 1.f); // DAT_0032B0F8.
    put_float(move->bytes, 0x1C, 1.f);
    put_float(move->bytes, 0x24, 0.f);
    put_float(move->bytes, 0x18, get(strategy_of(context), field::fast) != 0 ? 25.f : 5.f);
    put_float(move->bytes, 0x28, 0.25f); // DAT_0032B104.
    RuntimePlayerMindRequest* hold = allocate_runtime_mind_request(mind.anchor, 0x29, mind.sizes); // 0x00029D90, EBX 0x29
    put_float(hold->bytes, 0x10, wait);
    put_float(hold->bytes, 0x14, linger);
}
}

// Xbox 0x000B4FE0
bool approach_runtime_imp_flamethrower_target(const RuntimeMindTarget* target,
    RuntimeImpFlamethrowerStrategy& context)
{
    if (target == nullptr)
    {
        return false;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> mind = mind_of(context);
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    const Vec3 at{get(target->bytes, mind_target_field::shape_position).x, get(target->bytes, mind_target_field::shape_position).y, get(target->bytes, mind_target_field::shape_position).z};
    const Vec3 toward{at.x - position.x, at.y - position.y, at.z - position.z};
    float angle = 0.f; // 0x00105F90 leaves the slot unwritten for a zero direction.
    runtime_direction_angle(toward, angle); // 0x00105F90, EBX the angle
    const float side = 0.4363323152065277f; // DAT_0032B598, 25 degrees.
    const i32 floor = target->bytes[0x19];
    Vec3 first_point;
    const bool first = clear_point(floor, position, point_along(position, angle + side), first_point, context);
    Vec3 second_point;
    const bool second = clear_point(floor, position, point_along(position, angle - side), second_point, context);
    if (!first && !second)
    {
        put_vec3(mind, 0x70, at);
    }
    else
    {
        const Vec3* chosen;
        if (!(0.5f < roll(context))) // DAT_0032B0F4.
        {
            chosen = first ? &first_point : &second_point;
        }
        else
        {
            chosen = second ? &second_point : &first_point;
        }
        put_vec3(mind, 0x70, *chosen);
    }
    request_move(1.f, 0.2f, 1.f, context); // DAT_0032B0F8, DAT_0032B1AC.
    return !(45.f < runtime_strategy_target_gap(*target, context.shared)); // 0x00141650, DAT_0032B578.
}

// Xbox 0x000B52B0
void roam_runtime_imp_flamethrower(RuntimeImpFlamethrowerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> mind = mind_of(context);
    set(strategy_of(context), field::fast, !(0.5f < roll(context)) ? 1u : 0u); // DAT_0032B0F4.
    const i32 floor = i32(get(goals.entry.actor_bytes, player_field::floor));
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    struct Candidate
    {
        Vec3 point{};
        bool clear = false;
    };
    std::array<Candidate, 4> candidates{};
    i32 count = 0;
    float heading = 0.7853981852531433f; // DAT_0032B2D0, 45 degrees.
    for (Candidate& candidate : candidates)
    {
        candidate.clear = clear_point(floor, position, point_along(position, heading), candidate.point, context);
        if (candidate.clear)
        {
            ++count;
        }
        heading = heading + 1.5707963705062866f; // DAT_0032B140.
    }
    if (count == 0)
    {
        return;
    }
    const i32 pick = i32(roll(context) * float(count));
    i32 chosen = pick;
    i32 seen = 0;
    for (i32 index = 0; index < 4; ++index)
    {
        if (!candidates[size_t(index)].clear)
        {
            continue;
        }
        if (seen == pick)
        {
            chosen = index;
            break;
        }
        ++seen;
    }
    put_vec3(mind, 0x70, candidates[size_t(chosen)].point);
    request_move(0.4f, 1.f, 5.f, context); // DAT_0032B11C, DAT_0032B0F8, DAT_0032B184.
}

// Xbox 0x000B1350, ESI strategy
void choose_runtime_imp_flamethrower_attack(RuntimeImpFlamethrowerStrategy& context)
{
    const std::span<const u8> attributes = context.shared.checks.attributes; // Actor +0x964; +8 its aggression.
    const float aggression = attributes.data() != nullptr ? read_f32(attributes, 8) : 0.f;
    // The inlined 0x0002B8D0.
    change_state(roll(context) <= aggression ? 0x3EF : 0x3ED, context);
}

// Xbox 0x000B5570
void update_runtime_imp_flamethrower_strategy(RuntimeImpFlamethrowerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    bool found = false;
    // LAB 0x000B5644: off the target's queue, the imp waits on it (0x40D).
    const auto queue_or_wait = [&context](const RuntimeMindTarget* target)
    {
        if (!join_queue(target, context))
        {
            change_state(0x40D, context);
        }
    };
    switch (state)
    {
    case 0x3ED:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        if (get(strategy, field::in_reach) == 0)
        {
            if (requests_pending(context))
            {
                queue_or_wait(target);
                return;
            }
            const bool reached = approach_runtime_imp_flamethrower_target(target, context); // 0x000B4FE0
            set(strategy, field::in_reach, reached ? 1u : 0u);
            if (reached)
            {
                clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80
                load_runtime_mind_goal(context.goal_fireball, goals.entry.goals); // 0x0002A8E0
            }
        }
        if (requests_pending(context))
        {
            queue_or_wait(target);
            return;
        }
        change_state(0x416, context);
        return;
    }
    case 0x3EF:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found || target == nullptr)
        {
            return;
        }
        if (!requests_pending(context))
        {
            change_state(0x40C, context);
            return;
        }
        queue_or_wait(target);
        return;
    }
    case 0x401:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (found)
        {
            if (runtime_strategy_abandons_target(*target, context.shared.checks) == 0) // 0x00138410, EDX target
            {
                goals.room = nullptr; // Actor +0xCA0.
                change_state(0x40C, context);
                return;
            }
            if (retreat_runtime_strategy_from_reached_target(context.shared.checks) != 0) // 0x001383A0
            {
                return;
            }
        }
        check_runtime_spider_goal_cell(context.shared); // 0x000C0AF0
        if (requests_pending(context))
        {
            return;
        }
        float delay = get(strategy, field::delay);
        count_down_runtime_strategy_timer(delay, goals); // 0x0002B9F0, ECX +0x38
        set(strategy, field::delay, delay);
        if (get(strategy, field::delay) == 0.f)
        {
            roam_runtime_imp_flamethrower(context); // 0x000B52B0
        }
        return;
    }
    case 0x40C:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        check_runtime_spider_goal_cell(context.shared); // 0x000C0AF0
        const bool reached = !requests_pending(context)
            && approach_runtime_imp_flamethrower_target(target, context); // 0x000B4FE0
        if (join_queue(target, context))
        {
            choose_runtime_imp_flamethrower_attack(context); // 0x000B1350
            return;
        }
        if (!reached)
        {
            return;
        }
        change_state(actor_event_chance(goals.random, 0.75f) ? 0x40C : 0x40D, context); // 0x00019200, DAT_0032B2AC
        return;
    }
    case 0x40D:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        float delay = get(strategy, field::delay);
        count_down_runtime_strategy_timer(delay, goals); // 0x0002B9F0, ECX +0x38
        set(strategy, field::delay, delay);
        if (join_queue(target, context))
        {
            choose_runtime_imp_flamethrower_attack(context); // 0x000B1350
            return;
        }
        if (runtime_squared_target_distance(goals.entry.actor, target->bytes) > 6400.f) // 0x000415A0, DAT_0032B6C8
        {
            change_state(0x40C, context);
            return;
        }
        if (get(strategy, field::delay) == 0.f)
        {
            change_state(0x40C, context);
        }
        return;
    }
    case 0x416:
        select_target(found, context);
        if (!found || requests_pending(context))
        {
            return;
        }
        change_state(0x40C, context);
        return;
    default:
        return;
    }
}

// Xbox 0x000B5860
void dispatch_runtime_imp_flamethrower_message(std::span<const u32> message,
    RuntimeImpFlamethrowerStrategy& context)
{
    RuntimeSpiderStrategy& shared = context.shared;
    if (react_runtime_strategy_message(message, shared.checks) != 0) // 0x001384F0
    {
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        return;
    }
    switch (message[0])
    {
    case 0x31:
        react_runtime_strategy_watched_actor(message, shared.checks, shared.views); // 0x001397B0
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x11, shared.checks); // 0x0013A4F0
        return;
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals_of(context)) != 0) // 0x0013A490
        {
            return;
        }
        if (retaliate_runtime_strategy_contact(message, -1, shared.checks, shared.views) == 0) // 0x00139390
        {
            react_runtime_strategy_contact(message, -1.f, shared.checks); // The tail jump to 0x001395C0, DAT_0032B120.
        }
        return;
    default:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B58F0
void enter_runtime_imp_flamethrower_state(u16 state, u16 previous, RuntimeImpFlamethrowerStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = goals.entry.body; // Actor +0x95C.
    RuntimeMindGoalContext& mind = goals.entry.goals;
    const u32 target_address = get(mind_of(context), mind_field::target_record);
    const RuntimeMindTarget* target = target_address == 0 ? nullptr : goals.target_at(target_address);
    body[0x328] = 2;
    strategy_base(strategy).attacking = 0;
    switch (state)
    {
    case 0x3ED:
        body[0x328] = 4;
        strategy_base(strategy).attacking = 1;
        set(strategy, field::fast, 0u);
        set(strategy, field::in_reach, 0u);
        approach_runtime_imp_flamethrower_target(target, context); // 0x000B4FE0
        return;
    case 0x3EF:
        body[0x328] = 6;
        strategy_base(strategy).attacking = 1;
        set(strategy, field::fast, 1u);
        set(strategy, field::in_reach, 1u);
        load_runtime_mind_goal(context.goal_flamethrower, mind); // 0x0002A8E0
        return;
    case 0x401:
        if (actor_event_chance(goals.random, 0.25f)) // 0x00019200, DAT_0032B104
        {
            set(strategy, field::delay, roll(context) * 2.f + 2.f); // DAT_0032B110.
        }
        else
        {
            set(strategy, field::delay, 0.f);
        }
        roam_runtime_imp_flamethrower(context); // 0x000B52B0
        return;
    case 0x40C:
        body[0x328] = 3;
        approach_runtime_imp_flamethrower_target(target, context); // 0x000B4FE0
        return;
    case 0x40D:
        set(strategy, field::delay, roll(context) + 1.f); // DAT_0032B0F8.
        allocate_runtime_mind_request(mind.anchor, 0x17, mind.sizes); // 0x00029D90, EBX 0x17
        return;
    case 0x416:
        load_runtime_mind_goal(context.goal_wait, mind); // 0x0002A8E0
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}
}
