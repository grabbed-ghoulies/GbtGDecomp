#include "hunchback_strategy.h"
#include "gbtg/retail_field.h"
#include <cstddef>
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_live_attachments.h"
#include "body_contacts.h"
#include "body_message_helpers.h"
#include "body_resources.h"
#include "body_timers.h"
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "player_mind_goals.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "gbtg/actor_runtime.h"
#include "body_fields.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
HunchbackStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<HunchbackStrategyState>(strategy);
}

const HunchbackStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<HunchbackStrategyState>(strategy);
}


void put_u32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeHunchbackStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeHunchbackStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeHunchbackStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeHunchbackStrategy& context)
{
    return goals_of(context).entry.actor;
}

EntityBitBlock& body_bits(RuntimeHunchbackStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string(caller) + " reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeHunchbackStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeHunchbackStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeHunchbackStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

// 0x00029E80 over the mind's requests.
void clear_requests(RuntimeHunchbackStrategy& context)
{
    clear_runtime_player_mind_requests(*goals_of(context).setup.mind);
}

const HunchbackParameters& parameters_of(RuntimeHunchbackStrategy& context)
{
    return retail_view<HunchbackParameters>(context.parameters);
}

float roll(RuntimeHunchbackStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// EAX the range: least (+0xC) plus a roll of the span to most (+0x10).
// Xbox 0x000B2B70
float roll_between(RuntimeHunchbackStrategy& context, const HunchbackRange& range)
{
    return (range.most - range.least) * roll(context) + range.least;
}

RuntimeMindTarget* selected_target(RuntimeHunchbackStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

// The target's point lies beyond `reach` of the actor (0x000B2CD0 at +8, 0x000B3300 and the inlined 0x40C test at +0).
bool target_beyond(const RuntimeMindTarget& target, float reach, RuntimeHunchbackStrategy& context)
{
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const float dx = position.x - mind_target_record(target.bytes).shape.position.x;
    const float dz = position.z - mind_target_record(target.bytes).shape.position.z;
    const float dy = position.y - mind_target_record(target.bytes).shape.position.y;
    return reach * reach < dx * dx + dz * dz + dy * dy;
}

// Each live attachment gains a hold and body +0x6D counts it (retail's step is 1).
// Xbox 0x0001D760
void hold_live_attachments(i32 step, RuntimeHunchbackStrategy& context)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
    const std::span<u8> body = body_of(context);
    body_record(body).unk_6D = u8(body_record(body).unk_6D + step);
}

// "Evade" unless a request is pending (the mind's +0x58); 0x000B2BC0 inlines the same.
// Xbox 0x000B2D70
void evade_goal(RuntimeHunchbackStrategy& context)
{
    if (!requests_pending(context))
    {
        load_goal(context.goal_evade, context);
    }
}

// Message 0x41's 0x0013AD60: the named actor's callout records (+0x8C) at the contact point.
void contact_callout(std::span<const u32> message, i32 id, RuntimeHunchbackStrategy& context)
{
    const Vec3 point{std::bit_cast<float>(message[7]), std::bit_cast<float>(message[8]),
        std::bit_cast<float>(message[9])};
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = point;
    g_actor_creation_options.flags = 1;
    if (message[5] == 0xFFFFFF74u)
    {
        return;
    }
    context.actor_callouts(message[5], u32(id), &g_actor_creation_options);
}
}

// Xbox 0x000B3A70
void hunchback_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(HunchbackStrategyState);
    output.parameters = 0x98;
}

// Xbox 0x000B3AE0
bool initialize_runtime_hunchback_strategy(std::span<u8> strategy, std::span<const u8> parameters, Actor& actor)
{
    fields_of(strategy).parameters = 0x00428770u;
    clear_actor_shader_bits(actor, 0, 4); // Model +0x16C.
    clear_actor_shader_bits(actor, 3, 4); // Model +0x178.
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(fields_of(strategy).charge_effect.data(), name, std::strlen(name) + 1);
    return true;
}

// Xbox 0x000B3700, ECX strategy
void recover_runtime_hunchback(RuntimeHunchbackStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 phase = fields_of(strategy).special_phase;
    if (2 < phase && phase < 5 && phase != 5)
    {
        fields_of(strategy).special_phase = 5u;
        fields_of(strategy).special_time = roll_between(context, parameters_of(context).recovery_time);
    }
}

// Xbox 0x000B3F10, EAX actor, EDX strategy
float runtime_hunchback_stun_time(RuntimeHunchbackStrategy& context)
{
    const std::span<const u8> body = body_of(context);
    const u16 most = body_record(body).max_energy;
    if (most != 0)
    {
        const float health = float(i32(body_record(body).energy)) / float(i32(most));
        if (0.25f > health) // DAT_0032B104.
        {
            return parameters_of(context).stun_time_quarter;
        }
        if (0.5f > health) // DAT_0032B0F4.
        {
            return parameters_of(context).stun_time_half;
        }
        if (0.75f > health) // DAT_0032B2AC.
        {
            return parameters_of(context).stun_time_three_quarters;
        }
    }
    return parameters_of(context).stun_time_healthy;
}

// Xbox 0x000B2F20, ECX strategy, EAX phase
void set_runtime_hunchback_idle_phase(u32 phase, RuntimeHunchbackStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).idle_phase == phase)
    {
        return;
    }
    fields_of(strategy).idle_phase = phase;
    switch (phase)
    {
    case 1:
        load_goal(context.goal_into_far, context);
        return;
    case 2:
    {
        fields_of(strategy).taunt_delay = 0.05f; // DAT_0032B250.
        fields_of(strategy).taunts = 0;
        const i32 least = i32(parameters_of(context).taunt_limit_least);
        const i32 most = i32(parameters_of(context).taunt_limit_most);
        fields_of(strategy).taunt_limit = i32(float(most - least) * roll(context) + float(least)); // CVTTSS2SI
        return;
    }
    case 3:
        load_goal(context.goal_random_run, context);
        return;
    default:
        return;
    }
}

// Xbox 0x000B3780, EAX strategy, ECX phase
void set_runtime_hunchback_special_phase(u32 phase, RuntimeHunchbackStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).special_phase == phase)
    {
        return;
    }
    fields_of(strategy).special_phase = phase;
    const std::span<u8> body = body_of(context);
    Actor& actor = actor_of(context);
    switch (phase)
    {
    case 1:
        goals_of(context).room = nullptr; // Actor +0xCA0.
        load_goal(context.goal_prepare, context);
        fields_of(strategy).armed = 0u;
        clear_runtime_body_bit(body_bits(context, "0x000B3780"), 0x2B); // The tail jump to 0x0001F030.
        return;
    case 3:
    {
        load_goal(context.goal_prepare_armed, context);
        // 0x0001F800, ECX 0xFF, EDX 3, XMM4 1, ESI body: the hands' contact shapes.
        if (context.collision != nullptr)
        {
            activate_runtime_body_contact_shapes(*context.collision, actor, 3, 0xFF, 1.f); // 0x0013F420
           body_record(body).contact_count = 0;
        }
        if (context.attack_resource.data() == nullptr)
        {
            throw Error("0x000B3780 reads the body +0x5C attack tables through address zero");
        }
        const u8* row = find_runtime_attachment_first_request(
            *context.attack_tables.at(context.attack_resource.data()), 0xFD); // 0x0001C560, EBX 0xFD
       body_record(body).hit_record = row == nullptr ? 0u : goals_of(context).address_of(row);
       body_record(body).unk_88 = 0;
        body_record(body).unk_6A = u8(body_record(body).unk_6A + 1);
        hold_live_attachments(1, context); // 0x0001D760, ECX body
       body_record(body).unk_74C = 1;
        increment_runtime_body_latches(body, actor, context.latches); // 0x0001FC40, ECX body
        const char* effect = fields_of(strategy).charge_effect.data();
        fields_of(strategy).effect = context.create_effect(effect, strategy_base(strategy).actor); // 0x0003DF40
        const float time = roll_between(context, parameters_of(context).charge_time);
        strategy_base(strategy).attacking = 1;
        fields_of(strategy).armed = 1u;
        fields_of(strategy).special_time = time;
        fields_of(strategy).saved_334 = read_u32(body, offsetof(BodyRecord, ease_rate));
        return;
    }
    case 4:
        load_goal(context.goal_special, context);
        fields_of(strategy).special_time = roll_between(context, parameters_of(context).special_time);
        body_record(body).ease_rate = parameters_of(context).special_ease_rate;
        return;
    case 5:
        fields_of(strategy).special_time = roll_between(context, parameters_of(context).recovery_time);
        return;
    case 6:
        clear_requests(context); // 0x00029E80, EBX mind
        load_goal(context.goal_finish, context);
        disarm_runtime_hunchback(context); // 0x000B39D0
        return;
    default:
        return;
    }
}

// Xbox 0x000B39D0, EDI strategy
void disarm_runtime_hunchback(RuntimeHunchbackStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    if (context.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*context.collision, 3); // 0x0013F4D0, EAX 3
    }
    body_record(body).unk_6A = u8(body_record(body).unk_6A - 1);
   body_record(body).hit_record = 0;
   body_record(body).unk_88 = 0;
    hold_live_attachments(-1, context);
   body_record(body).unk_74C = 0;
    decrement_runtime_body_latches(body, actor_of(context), context.latches); // 0x0001FD20
    const u32 effect = fields_of(strategy).effect;
    if (effect == 0)
    {
        throw Error("0x000B39D0 messages a charge effect the strategy never made (+0xE4 is 0)");
    }
    const std::array<u32, 2> expire{5, 0};
    context.object_message(effect, expire); // Effect vtable +0xC.
    fields_of(strategy).effect = 0u;
    put_u32(body, offsetof(BodyRecord, ease_rate), fields_of(strategy).saved_334);
    strategy_base(strategy).attacking = 0;
    fields_of(strategy).armed = 0u;
}

// Xbox 0x000B3B40
void update_runtime_hunchback_strategy(RuntimeHunchbackStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    bool found;
    RuntimeMindTarget* target = nullptr;
    switch (state)
    {
    case 0x3EF:
    {
        // The tail jump to 0x000B3530.
        if (context.collision != nullptr && context.collision->overrides != nullptr)
        {
            expire_runtime_body_contact_overrides(context.collision->overrides, 0.5f); // 0x0013EB60, DAT_0032B0F4
        }
        const auto finish = [&]()
        {
            // The inlined 0x000B3780 to phase 6.
            if (fields_of(strategy).special_phase == 6)
            {
                return;
            }
            fields_of(strategy).special_phase = 6u;
            clear_requests(context);
            load_goal(context.goal_finish, context);
            disarm_runtime_hunchback(context);
        };
        const auto time_up = [&]()
        {
            float time = fields_of(strategy).special_time;
            count_down_runtime_strategy_timer(time, goals); // 0x0002B9F0, ECX +0xD8
            fields_of(strategy).special_time = time;
            return fields_of(strategy).special_time == 0.f;
        };
        switch (fields_of(strategy).special_phase)
        {
        case 1:
            if ((body_bits(context, "0x000B3530").bytes[5] & 8) != 0)
            {
                set_runtime_hunchback_special_phase(3, context); // The tail jump to 0x000B3780.
            }
            return;
        case 3:
            if (!time_up())
            {
                return;
            }
            if (fields_of(strategy).interrupted == 0)
            {
                set_runtime_hunchback_special_phase(4, context);
                return;
            }
            finish();
            return;
        case 4:
            if (fields_of(strategy).interrupted != 0)
            {
                clear_requests(context);
                finish();
                return;
            }
            if (!requests_pending(context))
            {
                load_goal(context.goal_special, context);
            }
            if (time_up())
            {
                finish();
            }
            return;
        case 5:
            if (time_up())
            {
                finish();
            }
            return;
        case 6:
            if (!requests_pending(context))
            {
                change_state(0x401, context);
            }
            return;
        default:
            return;
        }
    }
    case 0x3FE:
        // The tail jump to 0x000B2BF0.
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (target == nullptr)
        {
            change_state(0x401, context);
            return;
        }
        if (target_beyond(*target, parameters_of(context).far_reach, context)) // 0x000B2CD0
        {
            change_state(0x401, context);
            return;
        }
        if (runtime_mind_target_within(parameters_of(context).attack_reach, goals)) // 0x000BABF0
        {
            change_state(roll_runtime_actor_attack(context.checks.attributes, goals.random) ? 0x3EF : 0x40C,
                context); // 0x000196F0
            return;
        }
        evade_goal(context); // 0x000B2D70
        return;
    case 0x401:
    {
        // The tail jump to 0x000B2DD0.
        selected_target(context, found);
        if (runtime_mind_target_within_second(parameters_of(context).close_reach, goals)) // 0x000B5C50
        {
            change_state(0x3FE, context);
            return;
        }
        const auto taunt = [&]()
        {
            float delay = fields_of(strategy).taunt_delay;
            const bool due = count_down_runtime_strategy_timer(delay, goals); // 0x0002B9F0, ECX +0xBC
            fields_of(strategy).taunt_delay = delay;
            if (due && fields_of(strategy).taunts < fields_of(strategy).taunt_limit)
            {
                load_goal(context.goal_taunt, context);
                fields_of(strategy).taunts = fields_of(strategy).taunts + 1;
                fields_of(strategy).taunt_delay = roll_between(context, parameters_of(context).taunt_delay); // 0x000B2B70, EAX the block
            }
        };
        switch (fields_of(strategy).idle_phase)
        {
        case 1:
            if (requests_pending(context))
            {
                taunt();
                return;
            }
            set_runtime_hunchback_idle_phase(2, context);
            [[fallthrough]];
        case 2:
            if (!requests_pending(context) && fields_of(strategy).taunts >= fields_of(strategy).taunt_limit)
            {
                if (fields_of(strategy).idle_phase != 3)
                {
                    fields_of(strategy).idle_phase = 3u;
                    load_goal(context.goal_random_run, context);
                }
                return;
            }
            taunt();
            return;
        case 3:
            if (!requests_pending(context))
            {
                set_runtime_hunchback_idle_phase(2, context);
            }
            return;
        default:
            return;
        }
    }
    case 0x409:
    {
        // 0x000B3220.
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        const EntityBitBlock& bits = body_bits(context, "0x000B3220");
        switch (fields_of(strategy).dizzy_step)
        {
        case 1:
            if ((bits.bytes[6] & 1) != 0)
            {
                fields_of(strategy).dizzy_step = 2u;
            }
            return;
        case 2:
            if (target == nullptr)
            {
                throw Error("0x000B3300 measures the reach to a target the mind does not hold");
            }
            if (target_beyond(*target, parameters_of(context).attack_reach, context) && fields_of(strategy).dizzy_step != 3)
            {
                fields_of(strategy).dizzy_step = 3u;
                const std::array<float, 2> push{1.f, 0.f}; // DAT_0032B0F8.
                set_runtime_input_axes(context.axes, push, context.keyboard); // 0x00108FC0, EAX actor +0x960
            }
            return;
        case 3:
            if ((bits.bytes[4] & 1) != 0 || (bits.bytes[6] & 1) != 0)
            {
                change_state(0x401, context);
            }
            return;
        default:
            return;
        }
    }
    case 0x40C:
        // The tail jump to 0x000B33B0.
        target = selected_target(context, found);
        if (!found)
        {
            return;
        }
        if (target == nullptr)
        {
            throw Error("0x000B33B0 measures the reach to a target the mind does not hold");
        }
        if (target_beyond(*target, parameters_of(context).attack_reach, context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000B3BF0
void dispatch_runtime_hunchback_message(std::span<const u32> message, RuntimeHunchbackStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    const u32 actor = strategy_base(strategy).actor;
    switch (message[0])
    {
    case 0x82:
        if (state != 0x3EF)
        {
            react_runtime_strategy_cower(context.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (state != 0x3EF)
        {
            react_runtime_strategy_startle(context.checks); // The tail jump to 0x001399A0.
        }
        return;
    case 0x36:
        if (state != 0x3EF)
        {
            notice_runtime_strategy_hostile_target(0xF, context.checks); // 0x0013A4F0
        }
        return;
    case 0x31:
    {
        const std::span<const u8> mind = goals.entry.events.mind;
        const u32 target = mind.empty() ? 0u : mind_record(mind).target_record;
        if (target != 0 && read_u32(goals.target_at(target)->bytes, 0x10) == message[1])
        {
            recover_runtime_hunchback(context); // The tail jump to 0x000B3700.
        }
        return;
    }
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        retaliate_runtime_strategy_contact(message, -1, context.checks, context.views); // 0x00139390
        if (runtime_message_lands_on_actor(message, actor)) // 0x001408A0
        {
            change_state(0x409, context);
            return;
        }
        if (runtime_message_from_mind_target(message, actor, goals.entry.events.mind, goals.target_at)) // 0x001408E0
        {
            recover_runtime_hunchback(context); // The tail jump to 0x000B3700.
        }
        return;
    case 0x41:
        contact_callout(message, 0x34, context); // 0x0013AD60, ECX message +0x14, EAX message +0x1C
        return;
    case 0x6F:
    {
        const float time = runtime_hunchback_stun_time(context); // 0x000B3F10
        if (time == 0.f)
        {
            return;
        }
        const std::span<u8> body = body_of(context);
        const float held = body_record(body).move_hold_time;
        const float longest = time >= held ? time : held;
        body_record(body).move_hold_time = longest;
        return;
    }
    case 0x64:
        if (state == 0x3EF)
        {
            fields_of(strategy).interrupted = 1u;
            return;
        }
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B3DD0
void enter_runtime_hunchback_state(u16 state, u16 previous, RuntimeHunchbackStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    switch (state)
    {
    case 0x3EF:
        // 0x000B3490, ESI strategy.
        fields_of(strategy).armed = 0u;
        fields_of(strategy).interrupted = 0u;
        if (fields_of(strategy).special_phase != 1)
        {
            fields_of(strategy).special_phase = 1u;
            goals.room = nullptr; // Actor +0xCA0.
            load_goal(context.goal_prepare, context);
            fields_of(strategy).armed = 0u;
            clear_runtime_body_bit(body_bits(context, "0x000B3490"), 0x2B); // 0x0001F030
        }
        body_record(body).move_class = 6;
        return;
    case 0x3FE:
        // 0x000B2BC0, ESI strategy.
        evade_goal(context);
        body_record(body).move_class = 3;
        return;
    case 0x401:
        // 0x000B2D90, ESI strategy.
        if (fields_of(strategy).idle_phase != 1)
        {
            fields_of(strategy).idle_phase = 1u;
            load_goal(context.goal_into_far, context);
        }
        body_record(body).move_class = 2;
        return;
    case 0x409:
    {
        // The tail jump to 0x000B2FF0, EAX strategy.
        if (fields_of(strategy).dizzy_step != 1)
        {
            fields_of(strategy).dizzy_step = 1u;
        }
        body_bits(context, "0x000B2FF0").bytes[1] |= 2;
        Actor& actor = actor_of(context);
        clear_actor_shader_bits(actor, 0, 2); // Model +0x16C.
        set_actor_shader_bits(actor, 0, 4);
        set_actor_shader_bits(actor, 3, 4); // Model +0x178.
        increment_runtime_body_latch_mask(body, 1, actor, context.latches); // The inlined 0x0001F3F0, mask 1
        return;
    }
    case 0x40C:
        // 0x000B3380, ESI strategy.
        load_goal(context.goal_into_block, context);
        body_record(body).move_class = 3;
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // The tail jump to 0x0002BAF0.
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000B3E70
void leave_runtime_hunchback_state(u16 state, RuntimeHunchbackStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EF:
        // The tail jump to 0x000B3500, EAX strategy.
        if (fields_of(strategy).armed != 0)
        {
            disarm_runtime_hunchback(context); // 0x000B39D0
        }
        if (fields_of(strategy).special_phase != 0)
        {
            fields_of(strategy).special_phase = 0u;
        }
        return;
    case 0x3FE:
    case 0x40C:
        return;
    case 0x401:
        if (fields_of(strategy).idle_phase != 0)
        {
            fields_of(strategy).idle_phase = 0u;
        }
        return;
    case 0x409:
    {
        // 0x000B3110, EDI strategy.
        body_bits(context, "0x000B3110").bytes[1] &= 0xFD;
        Actor& actor = actor_of(context);
        set_actor_shader_bits(actor, 0, 2); // Model +0x16C.
        clear_actor_shader_bits(actor, 0, 4);
        clear_actor_shader_bits(actor, 3, 4); // Model +0x178.
        decrement_runtime_body_latch_mask(body_of(context), 1, actor, context.latches); // The inlined 0x0001F2F0
        if (fields_of(strategy).dizzy_step != 0)
        {
            fields_of(strategy).dizzy_step = 0u;
        }
        return;
    }
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            --strategy_base(strategy).pause_count;
        }
        return;
    }
}
}
