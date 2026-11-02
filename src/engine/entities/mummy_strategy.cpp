#include "mummy_strategy.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_transform.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_feedback.h"
#include "player_mind_goals.h"
#include "player_mind_targets.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../navigation/spawn_candidates.h"
#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
namespace
{
namespace field = mummy_strategy_field;

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeMummyStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeMummyStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeMummyStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of_actor(RuntimeMummyStrategy& context)
{
    return goals_of(context).entry.body;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeMummyStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeMummyStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

RuntimeMindTarget* selected_target(RuntimeMummyStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeMummyStrategy& context)
{
    const u32 target = get(mind_of(context), mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

const RuntimeMindTarget& require_target(const RuntimeMindTarget* target, const char* where)
{
    if (target == nullptr)
    {
        throw Error(std::string("mummy strategy ") + where + " reads a null target record");
    }
    return *target;
}

void load_goal(const char* goal, RuntimeMummyStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

float roll(RuntimeMummyStrategy& context)
{
    u32& random = goals_of(context).random;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// 1.5 + 2.5 * random (DAT_0032B210, DAT_0032B20C).
float ring_delay(RuntimeMummyStrategy& context)
{
    return roll(context) * 2.5f + 1.5f;
}
}

// Xbox 0x000BBC00
void mummy_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x18;
}

// Xbox 0x000BBC70
bool initialize_runtime_mummy_strategy(std::span<u8> strategy, std::span<const u8> actor)
{
    if (read_f32(actor, 0x978) > 0.f || actor[0x969] != 0) // DAT_0032B100.
    {
        set(strategy, field::provoked, 1u);
    }
    return true;
}

// Xbox 0x000BBAB0, EAX strategy, ECX target
void approach_runtime_mummy_ring(const RuntimeMindTarget* target, RuntimeMummyStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const RuntimeMindTarget& ring_target = require_target(target, "0x000BBAB0");
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const float radius = read_f32(ring_target.bytes, 0x38);
    Vec3 point = actor_world_position(goals.entry.actor); // 0x000412A0
    const float reach = goals.entry.actor.radius_xz * read_f32(body_of_actor(context), 0x888); // Body +8 actor +0x38.
    const Vec3 center{read_f32(ring_target.bytes, 0x20), read_f32(ring_target.bytes, 0x24),
        read_f32(ring_target.bytes, 0x28)};
    const i32 floor = ring_target.bytes[0x19];
    const i16 group = i16(read_u16(actor, 6));
    // 0x00111420, EAX 0xC, ESI the point: two then six reaches out (DAT_0032B110, DAT_0032B218).
    if (!select_navigation_spawn_position(floor, nullptr, center, reach + radius, reach * 2.f + radius, nullptr,
            goals.entry.allowed, 0xC, group, point, goals.random, context.checks.occupancy_initial)
        && !select_navigation_spawn_position(floor, nullptr, center, reach + radius, reach * 6.f + radius, nullptr,
            goals.entry.allowed, 0xC, group, point, goals.random, context.checks.occupancy_initial))
    {
        return;
    }
    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
    RuntimeMindGoalContext& mind = goals.entry.goals;
    RuntimePlayerMindRequest* move = allocate_runtime_mind_request(mind.anchor, 0x2E, mind.sizes); // 0x00029D90
    put_float(move->bytes, 0x1C, 1.f); // DAT_0032B0F8.
    put_float(move->bytes, 0x20, 1.f);
    put_float(move->bytes, 0x28, 30.f); // DAT_0032B108.
    std::memcpy(move->bytes.data() + 0x10, &point, 12);
    put_float(move->bytes, 0x28, 0.f);
}

// Xbox 0x000BB8E0, ESI strategy, ECX target, EDX mind
void update_runtime_mummy_ring(const RuntimeMindTarget* target, RuntimeMummyStrategy& context)
{
    if (target == nullptr)
    {
        change_state(0x416, context);
        return;
    }
    const std::span<u8> strategy = strategy_of(context);
    const float delay = get(strategy, field::ring_delay);
    bool expired = false;
    if (delay > 0.f)
    {
        const float left = delay - runtime_actor_rate(goals_of(context).entry.actor)
            * runtime_frame_step(goals_of(context).clock);
        set(strategy, field::ring_delay, left);
        if (!(0.f < left)) // COMISS; JC keeps a positive delay.
        {
            set(strategy, field::ring_delay, 0.f);
            expired = true;
        }
    }
    if (!expired && requests_pending(context))
    {
        return;
    }
    set(strategy, field::ring_delay, ring_delay(context));
    approach_runtime_mummy_ring(target, context); // 0x000BBAB0
}

// Xbox 0x000BB9F0, EAX strategy
void start_runtime_mummy_ring(const RuntimeMindTarget* target, RuntimeMummyStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    set(strategy, field::ring_delay, ring_delay(context));
    set(strategy, field::side, roll(context) <= 0.5f ? 1 : -1); // DAT_0032B0F4; COMISS; JC.
    approach_runtime_mummy_ring(target, context); // 0x000BBAB0
}

// Xbox 0x000BC560, EAX strategy, EDI target
bool runtime_mummy_engages(const RuntimeMindTarget* target, RuntimeMummyStrategy& context)
{
    if (get(strategy_of(context), field::provoked) == 0)
    {
        return false;
    }
    return join_runtime_strategy_target_queue(context.base.link, target == nullptr ? nullptr : target->record,
        &goals_of(context).entry.actor, context.queue); // 0x0002B910
}

// Xbox 0x000BC580, EAX strategy
void choose_runtime_mummy_attack(RuntimeMummyStrategy& context)
{
    if (read_u32(body_of_actor(context), 0x48) != 0)
    {
        change_state(0x3EE, context);
        return;
    }
    const RuntimeMindTarget& target = require_target(mind_target(context), "0x000BC580");
    if (target.bytes[0x14] == 0)
    {
        const std::span<const u8> body = context.views.body_of(target.actor).bytes; // Target +0x10 actor's +0x95C.
        if (const RuntimeScriptLookupTable* table = context.actions_of(body); table != nullptr) // Body +0x73C.
        {
            u32 parameters = 0;
            if (find_runtime_script_table_row(table, 0x10, parameters) != nullptr // 0x00027510
                && roll_runtime_actor_attack(context.checks.attributes, goals_of(context).random)) // 0x000196F0
            {
                change_state(0x3EF, context);
                return;
            }
        }
    }
    const float aggression = context.checks.attributes.data() == nullptr ? 0.f
        : read_f32(context.checks.attributes, 0xC); // Actor +0x964 -> +0xC.
    if (aggression >= roll(context) && get(strategy_of(context), strategy_field::state) != 0x3EE) // COMISS; JC
    {
        change_state(0x3EE, context);
        return;
    }
    change_state(0x3ED, context);
}

// Xbox 0x000BBCB0
void update_runtime_mummy_strategy(RuntimeMummyStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = get(strategy, strategy_field::state);
    update_runtime_base_strategy(goals); // 0x0002C130
    bool found = false;
    switch (state)
    {
    case 0x3EC:
    {
        RuntimeMindTarget* target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (runtime_mummy_engages(target, context)) // 0x000BC560
        {
            choose_runtime_mummy_attack(context); // 0x000BC580
        }
        if (!requests_pending(context))
        {
            change_state(0x40D, context);
        }
        return;
    }
    case 0x3ED:
    {
        RuntimeMindTarget* target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (!requests_pending(context))
        {
            const RuntimeMindTarget& acting = require_target(target, "0x000BBCB0 state 0x3ED");
            if (runtime_mind_target_requires_action(acting, context.views.body_of(acting.actor).bytes)) // 0x001413C0
            {
                change_state(0x3EE, context);
            }
            else
            {
                choose_runtime_mummy_attack(context); // 0x000BC580
            }
        }
        if (!runtime_mummy_engages(target, context)) // 0x000BC560
        {
            change_state(0x40D, context);
        }
        return;
    }
    case 0x3EE:
    {
        RuntimeMindTarget* target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (!requests_pending(context))
        {
            choose_runtime_mummy_attack(context); // 0x000BC580
        }
        if (!runtime_mummy_engages(target, context)) // 0x000BC560
        {
            change_state(0x40D, context);
        }
        return;
    }
    case 0x3EF:
        selected_target(context, found);
        if (found && !requests_pending(context) && get(strategy, field::held) == 0)
        {
            change_state(0x40D, context);
        }
        return;
    case 0x3F5:
    {
        RuntimeMindTarget* target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        update_runtime_mummy_ring(target, context); // 0x000BB8E0
        if (runtime_mummy_engages(target, context)) // 0x000BC560
        {
            choose_runtime_mummy_attack(context); // The tail jump to 0x000BC580.
        }
        return;
    }
    case 0x401:
    {
        RuntimeMindTarget* target = selected_target(context, found);
        if (found)
        {
            if (runtime_strategy_abandons_target(require_target(target, "0x000BBCB0 state 0x401"),
                    context.checks) == 0) // 0x00138410
            {
                goals.room = nullptr; // Actor +0xCA0.
                if (get(strategy, field::provoked) == 0)
                {
                    change_state(0x3F5, context);
                    return;
                }
                choose_runtime_mummy_attack(context); // The tail jump to 0x000BC580.
                return;
            }
            if (retreat_runtime_strategy_from_reached_target(context.checks) != 0) // 0x001383A0
            {
                return;
            }
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_far, context);
        }
        return;
    }
    case 0x40D:
    {
        RuntimeMindTarget* target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        const RuntimeMindTarget& chased = require_target(target, "0x000BBCB0 state 0x40D");
        if (runtime_squared_target_distance(goals.entry.actor, chased.bytes) > 6400.f) // 0x000415A0, DAT_0032B6C8
        {
            change_state(get(strategy, field::provoked) != 0 ? 0x3EC : 0x3F5, context);
        }
        if (runtime_mummy_engages(target, context)) // 0x000BC560
        {
            choose_runtime_mummy_attack(context); // The tail jump to 0x000BC580.
        }
        return;
    }
    case 0x411:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000BBFA0
void dispatch_runtime_mummy_message(std::span<const u32> message, RuntimeMummyStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = read_u32(strategy, 8);
    if (react_runtime_strategy_message(message, context.checks) != 0 || message[0] - 1 > 0x83) // 0x001384F0
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
    switch (message[0])
    {
    case 1:
        set(strategy, field::provoked, 1u);
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x16, context.checks); // 0x0013A4F0
        return;
    case 0x5F:
        set(strategy, field::held, message[1]);
        return;
    case 0x68:
    {
        const RuntimeMindTarget* target = runtime_strategy_reactions_enabled(goals) ? mind_target(context) : nullptr;
        if (target != nullptr && message[1] == read_u32(target->bytes, 0x10))
        {
            change_state(0x3EE, context);
            set(strategy, field::unk_3C, 1u);
            body_of_actor(context)[0x328] = 6;
        }
        return;
    }
    case 0x69:
    {
        const RuntimeMindTarget* target = mind_target(context);
        if (target != nullptr && message[1] == read_u32(target->bytes, 0x10)
            && runtime_strategy_reactions_enabled(goals))
        {
            change_state(0x411, context);
        }
        return;
    }
    case 0x6E:
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            return;
        }
        const std::span<const u8> mind = mind_of(context);
        if (message[1] == actor)
        {
            const u32 retaliated = retaliate_runtime_strategy_contact(message, -1, context.checks,
                context.views); // 0x00139390
            if (retaliated != 0
                || runtime_message_source_targets_mind_target(message, actor, mind, context.views)) // 0x00140A00
            {
                set(strategy, field::provoked, 1u);
            }
            if (retaliated == 0)
            {
                react_runtime_strategy_contact(message, -1.f, context.checks); // 0x001395C0, DAT_0032B120
            }
        }
        if (runtime_message_from_mind_target(message, actor, mind, goals.target_at)) // 0x001408E0
        {
            if (message[4] == 0)
            {
                const RuntimeStrategyBodyView body = context.views.body_of(*context.views.actor_at(message[1]));
                if (runtime_body_target_state_flagged(body.bytes, *body.bits)) // 0x00134620
                {
                    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80
                }
            }
            else if (get(strategy, field::unk_3C) != 0
                && runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0) // 0x0002B7F0
            {
                change_state(0x3F2, context);
            }
        }
        if (runtime_message_names_mind_target_actor(message, mind, goals.target_at) // 0x001408C0
            && !runtime_message_from_mind_target(message, actor, mind, goals.target_at)) // 0x001408E0
        {
            set(strategy, field::unk_3C, 0u);
        }
        return;
    }
    case 0x7C:
        clear_runtime_player_mind_reference(strategy, 0x48, message[1]); // The tail jump to 0x00140A40.
        return;
    case 0x82:
        react_runtime_strategy_cower(context.checks); // The tail jump to 0x001398A0.
        return;
    case 0x84:
        react_runtime_strategy_startle(context.checks); // The tail jump to 0x001399A0.
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000BC350
void enter_runtime_mummy_state(u16 state, u16 previous, RuntimeMummyStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of_actor(context);
    const u32 target = get(mind_of(context), mind_field::target_record);
    body[0x328] = 2;
    strategy[0x1A] = 0;
    switch (state)
    {
    case 0x3EC:
        body[0x328] = 3;
        append_runtime_range_request(goals.entry.goals.anchor, goals.entry.goals.sizes, 40.f); // 0x00136AC0
        return;
    case 0x3ED:
        body[0x328] = 4;
        strategy[0x1A] = 1;
        load_goal(context.goal_attack, context);
        return;
    case 0x3EE:
        set(strategy, field::unk_3C, 0u);
        body[0x328] = 5;
        strategy[0x1A] = 1;
        if (read_u32(body, 0x48) == 0)
        {
            load_goal(context.goal_ankh_getout, context);
        }
        load_goal(context.goal_ankh_whack, context);
        return;
    case 0x3EF:
        body[0x328] = 6;
        strategy[0x1A] = 1;
        load_goal(context.goal_bandage, context);
        return;
    case 0x3F4:
        body[0x328] = 5;
        break;
    case 0x3F5:
        body[0x328] = 3;
        start_runtime_mummy_ring(target == 0 ? nullptr : goals.target_at(target), context); // 0x000BB9F0
        return;
    case 0x401:
        load_goal(context.goal_far, context);
        return;
    case 0x40D:
        allocate_runtime_mind_request(goals.entry.goals.anchor, 0x17, goals.entry.goals.sizes); // 0x00029D90
        return;
    case 0x411:
        load_goal(context.goal_scared, context);
        return;
    default:
        break;
    }
    enter_runtime_base_strategy_state(goals.entry, state); // The tail jump to 0x0002BAF0.
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
}

// Xbox 0x000BC520
void leave_runtime_mummy_state(u16 state, RuntimeMummyStrategy& context)
{
    if (state >= 0x3ED && state <= 0x3EE)
    {
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20, EAX strategy +0x20.
        return;
    }
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        const std::span<u8> strategy = strategy_of(context);
        strategy[0x1C] = u8(strategy[0x1C] - 1);
    }
}
}
