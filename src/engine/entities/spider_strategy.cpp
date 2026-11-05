#include "spider_strategy.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_mind_targets.h"
#include "player_strategy.h"
#include "../navigation/mind_navigation_angles.h"
#include "../navigation/navigation.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
SpiderStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<SpiderStrategyState>(strategy);
}

const SpiderStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<SpiderStrategyState>(strategy);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vec3(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeSpiderStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeSpiderStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of_actor(RuntimeSpiderStrategy& context)
{
    return goals_of(context).entry.body;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeSpiderStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeSpiderStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

void change_state(u16 state, RuntimeSpiderStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

RuntimeMindTarget* selected_target(RuntimeSpiderStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context));
    return target;
}

Vec3 target_point(const RuntimeMindTarget& target)
{
    return Vec3{get(target.bytes, mind_target_field::shape_position).x, get(target.bytes, mind_target_field::shape_position).y, get(target.bytes, mind_target_field::shape_position).z};
}

// 30 units (DAT_0032B108) from the point along the heading, at its height.
Vec3 point_along(const Vec3& from, float heading)
{
    const float x = float(retail_sin(heading) * 30.L + from.x); // 0x00153320
    const float z = float(retail_cos(heading) * 30.L + from.z); // 0x00153230
    return Vec3{x, from.y, z};
}

// 0x00111420, EAX flags, ESI output.
bool clear_point(i32 floor, const Vec3* origin, const Vec3& center, float minimum, float maximum, u8 flags,
    Vec3& output, RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    return select_navigation_spawn_position(floor, origin, center, minimum, maximum, goals.room, goals.entry.allowed,
        flags, i16(read_u16(actor, 6)), output, goals.random, context.checks.occupancy_initial);
}

// The two mind requests both moves end with. 0x2D: +0x10 the pace, +0x14 1 and +0x1C 0 (written 30 first);
// 0x29: +0x10 and +0x14.
void request_move(float pace, float wait, float linger, RuntimeSpiderStrategy& context)
{
    RuntimeMindGoalContext& mind = goals_of(context).entry.goals;
    RuntimePlayerMindRequest* move = allocate_runtime_mind_request(mind.anchor, 0x2D, mind.sizes); // 0x00029D90
    put_float(move->bytes, 0x10, pace);
    put_float(move->bytes, 0x14, 1.f); // DAT_0032B0F8.
    put_float(move->bytes, 0x1C, 0.f);
    RuntimePlayerMindRequest* hold = allocate_runtime_mind_request(mind.anchor, 0x29, mind.sizes); // 0x00029D90
    put_float(hold->bytes, 0x10, wait);
    put_float(hold->bytes, 0x14, linger);
}
}

// Xbox 0x00141490, EAX target
bool runtime_mind_target_climbing(const RuntimeMindTarget& target, RuntimeSpiderStrategy& context)
{
    if (get(target.bytes, mind_target_field::kind) != 0)
    {
        return false;
    }
    const std::span<const u8> body = context.views.body_of(target.actor).bytes; // Target +0x10 actor's +0x95C.
    const u8 kind = body_record(body).surface_override_frames == 0 ? body_record(body).surface
                                                                        : body_record(body).surface_override;
    return kind == 0xC;
}

// Xbox 0x00141650, EAX actor, EBX target
float runtime_strategy_target_gap(const RuntimeMindTarget& target, RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    const long double dx = static_cast<long double>(get(target.bytes, mind_target_field::shape_position).x) - position.x;
    const long double dy = static_cast<long double>(get(target.bytes, mind_target_field::shape_position).y) - position.y;
    const long double dz = static_cast<long double>(get(target.bytes, mind_target_field::shape_position).z) - position.z;
    const long double distance = std::sqrt(dz * dz + dy * dy + dx * dx);
    const long double reach = static_cast<long double>(goals.entry.actor.radius_xz) * read_f32(body_of_actor(context),
        0x888) + get(target.bytes, mind_target_field::shape_radius);
    const long double gap = distance - reach;
    return 0.0L < gap ? float(gap) : 0.f;
}

// Xbox 0x000C0620, EAX strategy
bool approach_runtime_spider_target(const RuntimeMindTarget& target, RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> mind = mind_of(context);
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    const Vec3 at = target_point(target);
    const Vec3 toward{at.x - position.x, at.y - position.y, at.z - position.z};
    // The angle goes to the target argument's stack slot; a zero direction leaves the target's address there.
    float angle = std::bit_cast<float>(target.address);
    runtime_direction_angle(toward, angle); // 0x00105F90
    const float side = std::bit_cast<float>(0x3EDF66F3u); // DAT_0032B598, 25 degrees.
    const i32 floor = get(target.bytes, mind_target_field::floor);
    Vec3 first_point;
    const Vec3 first_center = point_along(position, angle + side);
    const bool first = clear_point(floor, &position, first_center, 0.f, 10.f, 0x1C, first_point, context);
    Vec3 second_point;
    const Vec3 second_center = point_along(position, angle - side);
    const bool second = clear_point(floor, &position, second_center, 0.f, 10.f, 0x1C, second_point, context);
    if (!first && !second)
    {
        put_vec3(mind, 0x70, at);
    }
    else
    {
        goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
        const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
        const Vec3* chosen = nullptr;
        if (!(0.5f < roll)) // DAT_0032B0F4.
        {
            chosen = first ? &first_point : &second_point;
        }
        else
        {
            chosen = second ? &second_point : &first_point;
        }
        put_vec3(mind, 0x70, *chosen);
    }
    request_move(1.f, 0.1f, 0.2f, context); // DAT_0032B254, DAT_0032B1AC.
    return !(30.f < runtime_strategy_target_gap(target, context)); // 0x00141650, DAT_0032B108.
}

// Xbox 0x000C08A0, EAX strategy
void roam_runtime_spider(RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> mind = mind_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const i32 floor = i32(get(actor, player_field::floor));
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    struct Candidate
    {
        Vec3 point{};
        bool clear = false;
    };
    std::array<Candidate, 4> candidates{};
    i32 count = 0;
    float heading = std::bit_cast<float>(0x3F490FDBu); // DAT_0032B2D0, 45 degrees.
    for (Candidate& candidate : candidates)
    {
        const Vec3 center = point_along(position, heading);
        candidate.clear = clear_point(floor, &position, center, 0.f, 10.f, 0x1C, candidate.point, context);
        if (candidate.clear)
        {
            ++count;
        }
        heading = heading + 1.5707964f; // DAT_0032B140.
    }
    if (count == 0)
    {
        const float minimum = float(static_cast<long double>(goals.entry.actor.radius_xz)
            * body_record(body_of_actor(context)).radius_scale);
        candidates[0].clear = clear_point(floor, nullptr, position, minimum, 40.f, 0xC, candidates[0].point, context);
        count = candidates[0].clear ? 1 : 0;
        if (count == 0)
        {
            return;
        }
    }
    goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    const i32 pick = i32(roll * float(count));
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
    request_move(0.6f, 0.1f, 2.f, context); // DAT_0032B248, DAT_0032B254, DAT_0032B110.
}

// Xbox 0x000C0AF0, EAX strategy
void check_runtime_spider_goal_cell(RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> mind = mind_of(context);
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    const i32 floor = i32(get(goals.entry.actor_bytes, player_field::floor));
    const Vec3 goal(get(mind, mind_field::goal_point));
    const i32 goal_cell = entity_detail::navigation_cell_index(floor, goal); // 0x00110EF0, EBX mind +0x70
    if (goal_cell == -1)
    {
        return;
    }
    const i32 own_cell = entity_detail::navigation_cell_index(floor, position);
    const entity_detail::NavGrid* grid = entity_detail::g_nav_grids[floor]; // DAT_0050E240.
    const u32 kind = grid->cells[size_t(goal_cell)].value_6 & 0x1Fu; // Cell +6.
    if (kind > (own_cell == goal_cell ? 1u : 0u))
    {
        clear_runtime_player_mind_requests(*goals.setup.mind);
    }
}

// Xbox 0x000C1150, EAX target, EBX strategy
bool runtime_spider_holds_target(const RuntimeMindTarget& target, RuntimeSpiderStrategy& context)
{
    if (runtime_mind_target_climbing(target, context)) // 0x00141490 inlined
    {
        return true;
    }
    return join_runtime_strategy_target_queue(context.base.link, target.record, &goals_of(context).entry.actor,
        context.queue); // 0x0002B910, EBX strategy, EDI target
}

// Xbox 0x000C1190, EAX strategy, ESI target
bool runtime_spider_bites(const RuntimeMindTarget* target, RuntimeSpiderStrategy& context)
{
    if (target == nullptr || fields_of(strategy_of(context)).web_delay != 0.f)
    {
        return false;
    }
    const bool acts = runtime_mind_target_requires_action(*target, context.views.body_of(target->actor).bytes);
    const bool climbing = runtime_mind_target_climbing(*target, context); // 0x00141490 inlined
    return acts || climbing;
}

// Xbox 0x000C11F0, EAX target, EDI strategy
void choose_runtime_spider_attack(const RuntimeMindTarget& target, RuntimeSpiderStrategy& context)
{
    if (!runtime_spider_bites(&target, context)
        && roll_runtime_actor_attack(context.checks.attributes, goals_of(context).random)) // 0x000196F0, EAX actor
    {
        change_state(0x3EF, context);
        return;
    }
    change_state(0x3ED, context);
}

// Xbox 0x000C1290, EAX body, ECX target, EDI strategy
void end_runtime_spider_web(const RuntimeMindTarget& target, RuntimeSpiderStrategy& context)
{
    const std::span<const u8> body = body_of_actor(context);
    if (body_record(body).move_holds != 0 || body_record(body).move_hold_time != 0.f) // DAT_0032B100.
    {
        return;
    }
    if (runtime_spider_bites(&target, context))
    {
        change_state(0x3ED, context);
    }
}

// Xbox 0x000C0BB0
void update_runtime_spider_strategy(RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    const float delay = fields_of(strategy).web_delay;
    if (0.f < delay)
    {
        const float left = delay - runtime_actor_rate(goals.entry.actor) * runtime_frame_step(goals.clock);
        fields_of(strategy).web_delay = left;
        if (left <= 0.f)
        {
            fields_of(strategy).web_delay = 0.f;
        }
    }
    update_runtime_base_strategy(goals); // 0x0002C130
    bool found = false;
    RuntimeMindTarget* target = nullptr;
    switch (state)
    {
    case 0x3EC:
    {
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        check_runtime_spider_goal_cell(context); // 0x000C0AF0
        const bool reached = !requests_pending(context) && approach_runtime_spider_target(*target, context);
        if (runtime_spider_holds_target(*target, context))
        {
            choose_runtime_spider_attack(*target, context); // 0x000C11F0
            return;
        }
        if (reached)
        {
            change_state(0x40D, context);
        }
        return;
    }
    case 0x3ED:
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (fields_of(strategy).reached == 0)
        {
            check_runtime_spider_goal_cell(context); // 0x000C0AF0
            if (requests_pending(context))
            {
                if (!runtime_spider_holds_target(*target, context))
                {
                    change_state(0x40D, context);
                }
                return;
            }
            const bool reached = approach_runtime_spider_target(*target, context);
            fields_of(strategy).reached = reached ? 1u : 0u;
            if (reached)
            {
                const bool climbing = target != nullptr && runtime_mind_target_climbing(*target, context);
                load_runtime_mind_goal(climbing ? context.goal_attack : context.goal_attack_now,
                    goals.entry.goals); // 0x00141490
            }
        }
        if (!requests_pending(context))
        {
            change_state(0x40D, context);
        }
        if (!runtime_spider_holds_target(*target, context))
        {
            change_state(0x40D, context);
        }
        return;
    case 0x3EF:
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (fields_of(strategy).reached == 0)
        {
            check_runtime_spider_goal_cell(context); // 0x000C0AF0
            if (!requests_pending(context))
            {
                const bool reached = approach_runtime_spider_target(*target, context);
                fields_of(strategy).reached = reached ? 1u : 0u;
                if (reached)
                {
                    clear_runtime_player_mind_requests(*goals.setup.mind);
                    load_runtime_mind_goal(context.goal_web, goals.entry.goals);
                }
            }
        }
        if (!runtime_spider_holds_target(*target, context))
        {
            change_state(0x40D, context);
            return;
        }
        if (requests_pending(context))
        {
            end_runtime_spider_web(*target, context); // 0x000C1290
            return;
        }
        change_state(runtime_spider_bites(target, context) ? 0x3ED : 0x3EC, context);
        return;
    case 0x401:
        target = selected_target(context, found);
        if (found)
        {
            if (runtime_strategy_abandons_target(*target, context.checks) == 0)
            {
                goals.room = nullptr; // Actor +0xCA0.
                change_state(0x3EC, context);
                return;
            }
            if (retreat_runtime_strategy_from_reached_target(context.checks) != 0)
            {
                return;
            }
        }
        check_runtime_spider_goal_cell(context); // 0x000C0AF0
        if (!requests_pending(context))
        {
            roam_runtime_spider(context); // 0x000C08A0
        }
        return;
    case 0x40D:
    {
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        // 0x000415A0, EBX target: the squared distance from the actor.
        const Vec3 position = actor_world_position(goals.entry.actor);
        const Vec3 at = target_point(*target);
        const float dz = position.z - at.z;
        const float dx = position.x - at.x;
        const float dy = position.y - at.y;
        if (dz * dz + dx * dx + dy * dy > 6400.f || runtime_spider_holds_target(*target, context)) // DAT_0032B6C8.
        {
            change_state(0x3EC, context);
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000C0F00
void dispatch_runtime_spider_message(std::span<const u32> message, RuntimeSpiderStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 actor = strategy_base(goals.entry.strategy).actor;
    if (react_runtime_strategy_message(message, context.checks) != 0)
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
        notice_runtime_strategy_hostile_target(0x1A, context.checks);
        return;
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, context.base);
        if (check_runtime_player_knockout(goals) != 0)
        {
            return;
        }
        if (retaliate_runtime_strategy_contact(message, -1, context.checks, context.views) == 0)
        {
            react_runtime_strategy_contact(message, -1.f, context.checks); // DAT_0032B120.
        }
        if (message[2] != actor)
        {
            return;
        }
        fields_of(strategy_of(context)).web_delay = 5.f; // DAT_0032B184.
        change_state(0x3F2, context);
        return;
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

// Xbox 0x000C1040
void enter_runtime_spider_state(u16 state, u16 previous, RuntimeSpiderStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = body_of_actor(context);
    const u32 target_address = get(mind_of(context), mind_field::target_record);
    body_record(body).move_class = 2;
    strategy_base(strategy).attacking = 0;
    strategy_base(strategy).unk_1B = 0;
    switch (state)
    {
    case 0x3EC:
        body_record(body).move_class = 3;
        approach_runtime_spider_target(*goals.target_at(target_address), context); // 0x000C0620
        return;
    case 0x3ED:
        body_record(body).move_class = 4;
        fields_of(strategy).reached = 0u;
        strategy_base(strategy).attacking = 1;
        return;
    case 0x3EF:
        body_record(body).move_class = 6;
        fields_of(strategy).reached = 0u;
        strategy_base(strategy).unk_1B = 1;
        return;
    case 0x40D:
    {
        RuntimeMindGoalContext& mind = goals.entry.goals;
        allocate_runtime_mind_request(mind.anchor, 0x17, mind.sizes); // 0x00029D90, EBX 0x17
        return;
    }
    case 0x401:
        roam_runtime_spider(context); // 0x000C08A0, the tail jump
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state);
        return;
    }
}

// Xbox 0x000B5AF0
void leave_runtime_spider_state(u16 state, RuntimeSpiderStrategy& context)
{
    if (state == 0x3ED || state == 0x3EF)
    {
        detach_runtime_strategy_link(context.base.link); // 0x00112A20, the tail jump
        return;
    }
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_base(strategy_of(context)).pause_count;
    }
}
}
