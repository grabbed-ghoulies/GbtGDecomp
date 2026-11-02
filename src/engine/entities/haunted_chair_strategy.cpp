#include "haunted_chair_strategy.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "gbtg/anim.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = haunted_chair_strategy_field;

constexpr float kAttackRange = std::bit_cast<float>(0x47C35000u); // 100000, the target queue's reach.
constexpr float kAhead = 10.f; // 0x41200000.

RuntimePlayerStrategyGoals& goals_of(RuntimeHauntedChairStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeHauntedChairStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeHauntedChairStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeHauntedChairStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeHauntedChairStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

// The body (actor +0x95C) neither turning (+0x30) nor easing (+0x34).
bool body_settled(RuntimeHauntedChairStrategy& context)
{
    const std::span<const u8> body = goals_of(context).entry.body;
    return read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f; // DAT_0032B100.
}

bool selected(RuntimeHauntedChairStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    return select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
}

bool target_attackable(RuntimeHauntedChairStrategy& context)
{
    return runtime_strategy_target_attackable(kAttackRange, context.base.link, context.checks,
        context.queue); // 0x0013A330
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeHauntedChairStrategy& context)
{
    const u32 target = get(mind_of(context), mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

// The goal point (mind +0x70): the target's actor, else 10 ahead of the chair (0x00137EF0).
Vec3 goal_point(RuntimeHauntedChairStrategy& context)
{
    RuntimeMindTarget* target = mind_target(context);
    Vec3 point;
    if (target != nullptr)
    {
        point = actor_world_position(target->actor); // 0x000412A0, ESI target +0x10
    }
    else
    {
        runtime_point_ahead_of_actor(goals_of(context).entry.actor, kAhead, point); // 0x00137EF0
    }
    return point;
}

void set_goal_point(const Vec3& point, RuntimeHauntedChairStrategy& context)
{
    std::memcpy(mind_of(context).data() + 0x70, &point, 12);
}

void load_goal(const char* goal, RuntimeHauntedChairStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

// Xbox 0x000AE140, EAX strategy: 0x3EF (the swing-kick) when the actor's aggression (+0x964 -> +8, 0 without one) is
// at least a random fraction, else 0x3EE.
u16 pick_attack_state(RuntimeHauntedChairStrategy& context)
{
    const float aggression = context.attributes.data() == nullptr ? 0.f : read_f32(context.attributes, 8);
    u32& random = goals_of(context).random;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    return aggression >= roll ? u16(0x3EF) : u16(0x3EE); // COMISS; JNC
}

// Xbox 0x000AE1A0, ESI strategy: the attack 0x000AE140 picks while a target is attackable, else back to holding
// (0x401).
void choose_attack(RuntimeHauntedChairStrategy& context)
{
    if (target_attackable(context))
    {
        change_state(pick_attack_state(context), context); // 0x000AE140
        return;
    }
    change_state(0x401, context);
}

// Xbox 0x000AE360, EAX strategy, ECX phase.
void set_phase(u32 phase, RuntimeHauntedChairStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 previous = get(strategy, field::phase);
    set(strategy, field::phase, phase);
    Actor& actor = goals_of(context).entry.actor;
    const std::span<const u8> parameters = context.parameters;
    if (phase == 1)
    {
        if (previous != 1)
        {
            const float low = read_f32(parameters, 0x24);
            const float high = read_f32(parameters, 0x28);
            u32& random = goals_of(context).random;
            random = random * 0x19660Du + 0x3C6EF35Fu;
            const float roll = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
            set(strategy, field::hold, (high - low) * roll + low);
        }
        request_runtime_strategy_roam(parameters.subspan(4), context.checks); // 0x0013A0A0, EAX block +4, EBX actor
        return;
    }
    if (phase != 2)
    {
        return;
    }
    Vec3 point;
    if (!select_runtime_strategy_roam_position(point, actor.euler.y, read_f32(parameters, 0x20),
            read_f32(parameters, 0x1C), context.checks)) // 0x00138310, EAX actor, XMM0 block +0x1C
    {
        runtime_point_ahead_of_actor(actor, kAhead, point); // 0x00137EF0
    }
    set_goal_point(point, context);
    load_goal(context.goal_slide, context);
}

// Xbox 0x000AE280, ESI strategy: the hold counts down; with no request pending, an attackable target starts an attack,
// else the chair holds again (time left) or slides.
void update_hold(RuntimeHauntedChairStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = goals_of(context).entry.actor;
    const float hold = get(strategy, field::hold);
    if (0.f < hold)
    {
        const float left = hold - runtime_actor_rate(actor) * runtime_frame_step(goals_of(context).clock);
        set(strategy, field::hold, left);
        if (!(0.f < left)) // COMISS; JC keeps a positive time.
        {
            set(strategy, field::hold, 0.f);
        }
    }
    if (requests_pending(context))
    {
        return;
    }
    if (target_attackable(context)) // 0x0013A330
    {
        change_state(pick_attack_state(context), context); // 0x000AE140
        return;
    }
    set_phase(get(strategy, field::hold) > 0.f ? 1u : 2u, context); // DAT_0032B100, the tail jump to 0x000AE360
}

// Xbox 0x000AE520, EAX actor, ECX strategy: one more slide at the target.
void slide_at_target(RuntimeHauntedChairStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    set(strategy, field::slides, get(strategy, field::slides) + 1);
    set_goal_point(goal_point(context), context);
    load_goal(context.goal_slide_at_target, context);
}

// Xbox 0x000AE460: the goal point follows the target; once settled with no request pending, slide at it again while
// slides remain, else choose the next attack.
void update_slide(RuntimeHauntedChairStrategy& context)
{
    set_goal_point(goal_point(context), context);
    if (!body_settled(context) || requests_pending(context))
    {
        return;
    }
    if (mind_target(context) != nullptr
        && get(strategy_of(context), field::slides) < std::bit_cast<i32>(read_u32(context.parameters, 0x34)))
    {
        slide_at_target(context); // 0x000AE520
        return;
    }
    choose_attack(context); // 0x000AE1A0
}

// Xbox 0x000AE6E0, EAX strategy.
void update_settled_attack(RuntimeHauntedChairStrategy& context)
{
    if (body_settled(context) && !requests_pending(context))
    {
        choose_attack(context); // 0x000AE1A0
    }
}

// Xbox 0x000AE230, ECX strategy, EAX actor.
void update_holding(RuntimeHauntedChairStrategy& context)
{
    if (!body_settled(context))
    {
        return;
    }
    const u32 phase = get(strategy_of(context), field::phase);
    if (phase == 1)
    {
        update_hold(context); // 0x000AE280
    }
    else if (phase == 2 && !requests_pending(context))
    {
        choose_attack(context); // 0x000AE1A0
    }
}

// Xbox 0x000AE660, EAX strategy, ECX actor: the mind's target lies within the block's reach (+0).
bool target_within_reach(RuntimeHauntedChairStrategy& context)
{
    const float reach = read_f32(context.parameters, 0);
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const Vec3 position = actor_world_position(goals_of(context).entry.actor); // 0x000412A0
    const long double dx = static_cast<long double>(position.x) - get(target->bytes, mind_target_field::shape_position).x;
    const long double dy = static_cast<long double>(position.y) - get(target->bytes, mind_target_field::shape_position).y;
    const long double dz = static_cast<long double>(position.z) - get(target->bytes, mind_target_field::shape_position).z;
    return static_cast<long double>(reach) > std::sqrt((dz * dz + dx * dx) + dy * dy);
}

// Xbox 0x000AE5C0, ECX actor, ESI strategy: asleep, the chair wakes (0x417) while its player kind is not blocked
// (DAT_0050E764) and decay runs, once a marked chair (+0x1FC) has its target within reach.
void update_asleep(RuntimeHauntedChairStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const u32 kind = read_u16(actor, 0xCDC) == 2 || read_u32(actor, 0x93C) != 0 ? 1u : 0u;
    if (goals.resume.references[kind] != 0 || goals.resume.decay_enabled[goals.resume.decay_index] == 0)
    {
        return;
    }
    if (read_u16(actor, 0x1FC) != 0 && !target_within_reach(context)) // 0x000AE660
    {
        return;
    }
    change_state(0x417, context);
}
}

// Xbox 0x000ABB60
void haunted_chair_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x18;
}

// Xbox 0x000AE770
bool initialize_runtime_haunted_chair_strategy(std::span<u8> strategy)
{
    set(strategy, field::parameters, 0x00428734u);
    set(strategy, field::hold, 0.f);
    return true;
}

// Xbox 0x00137EF0, EAX actor, EBX output
void runtime_point_ahead_of_actor(const Actor& actor, float distance, Vec3& output)
{
    const float heading = actor.euler.y; // Actor +0x18.
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float across = float(retail_sin(heading) * distance); // 0x00153320
    const long double along = retail_cos(heading) * distance; // 0x00153230
    output.y = position.y;
    output.x = float(static_cast<long double>(position.x) + across);
    output.z = float(static_cast<long double>(position.z) + along);
}

// Xbox 0x000AE790
void update_runtime_haunted_chair_strategy(RuntimeHauntedChairStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = read_u16(strategy, 0xE);
    update_runtime_base_strategy(goals); // 0x0002C130
    switch (state)
    {
    case 0x3EE:
        if (selected(context))
        {
            update_slide(context); // 0x000AE460
        }
        return;
    case 0x3EF:
        if (selected(context))
        {
            update_settled_attack(context); // 0x000AE6E0
        }
        return;
    case 0x401:
        selected(context);
        if (state == read_u16(strategy, 0xE))
        {
            update_holding(context); // 0x000AE230
        }
        return;
    case 0x413:
        selected(context);
        if (state == read_u16(strategy, 0xE))
        {
            update_asleep(context); // 0x000AE5C0
        }
        return;
    case 0x417:
        selected(context);
        if (state == read_u16(strategy, 0xE))
        {
            update_settled_attack(context); // 0x000AE6E0
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000AE8B0
void dispatch_runtime_haunted_chair_message(std::span<const u32> message, RuntimeHauntedChairStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    switch (message[0])
    {
    case 0x26:
    case 0x27:
        if (read_u16(strategy, 0xE) == 0x413)
        {
            return;
        }
        break;
    case 0x36:
        notice_runtime_strategy_hostile_target(0xB, context.checks); // 0x0013A4F0, ESI strategy
        break;
    case 0x6E:
    {
        const bool own = message[1] == read_u32(strategy, 8);
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            break;
        }
        if (!own)
        {
            react_runtime_strategy_contact(message, -1.f, context.checks); // 0x001395C0, DAT_0032B120
            break;
        }
        const u16 state = read_u16(strategy, 0xE);
        const bool landed = runtime_message_lands_on_actor(message, read_u32(strategy, 8)); // 0x001408A0
        clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
        if (landed)
        {
            strategy[0x1A] = 0;
        }
        if (state == 0x413)
        {
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
        }
        break;
    }
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
}

// Xbox 0x000AEA10
void enter_runtime_haunted_chair_state(u16 state, u16 previous, RuntimeHauntedChairStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    goals.entry.body[0x328] = 2;
    switch (state)
    {
    case 0x3EE:
        goals.entry.body[0x328] = 5;
        strategy[0x1A] = 1;
        set(strategy, field::slides, 0);
        slide_at_target(context); // The tail jump to 0x000AE520.
        return;
    case 0x3EF:
        goals.entry.body[0x328] = 6;
        strategy[0x1A] = 1;
        load_goal(context.goal_swing_kick, context);
        return;
    case 0x401:
        set(strategy, field::phase, 0u);
        set_phase(1, context); // The tail jump to 0x000AE360, ECX 1.
        return;
    case 0x413:
        // 0x000AE5A0, ESI strategy.
        load_goal(context.goal_mimic, context);
        strategy[0x1C] = u8(strategy[0x1C] + 1);
        return;
    case 0x417:
        load_goal(context.goal_wakeup, context);
        {
            const u16 next = 0x401;
            std::memcpy(strategy.data() + 0x18, &next, 2);
        }
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000AEB30
void leave_runtime_haunted_chair_state(u16 state, u16 next, RuntimeHauntedChairStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (state >= 0x3EE && state <= 0x3EF && (next < 0x3EE || next > 0x3EF))
    {
        detach_runtime_strategy_link(context.base.link); // 0x00112A20, EAX strategy +0x20
    }
    switch (state)
    {
    case 0x3EE:
    case 0x3EF:
        strategy[0x1A] = 0;
        return;
    case 0x401:
        set(strategy, field::phase, 0u);
        return;
    case 0x417:
        return;
    case 0x413:
    case 0x3FC:
    case 0x3FF:
    case 0x415:
        strategy[0x1C] = u8(strategy[0x1C] - 1);
        return;
    default:
        return;
    }
}
}
