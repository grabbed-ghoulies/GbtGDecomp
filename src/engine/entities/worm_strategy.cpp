#include "worm_strategy.h"
#include <cstddef>
#include "body_fields.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_transform.h"
#include "body_animation_commands.h"
#include "body_attachment_requests.h"
#include "body_message_helpers.h"
#include "mind_target_policy.h"
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_targets.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../scripts/script_actor_query.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
namespace
{
WormStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<WormStrategyState>(strategy);
}

const WormStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<WormStrategyState>(strategy);
}


constexpr float kTimerStart = 5.f; // DAT_0032B184.
constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kEmergedScale = 1.25f; // DAT_0032B114.

RuntimePlayerStrategyGoals& goals_of(RuntimeWormStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeWormStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x95C.
std::span<u8> body_of_actor(RuntimeWormStrategy& context)
{
    return goals_of(context).entry.body;
}

EntityBitBlock& body_bits(RuntimeWormStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string("worm strategy ") + caller + " reads a body with no bit block");
    }
    return *bits;
}

// Body +0x15C +8 byte 2 bit 3: actorBodyFlagId 0x13.
bool emerged(RuntimeWormStrategy& context, const char* caller)
{
    return (body_bits(context, caller).bytes[2] & 8) != 0;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeWormStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeWormStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeWormStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeWormStrategy& context)
{
    const u32 target = mind_record(goals_of(context).entry.events.mind).target_record;
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

// Body +0xE0 / +0xDC and the +0xA4..+0xA6 bytes: the body's rest timer.
void set_body_timer(std::span<u8> body, float value)
{
    const u32 one = 1;
    body_record(body).unk_E0 = one;
   body_record(body).unk_DC = value;
    body_record(body).overlay_state = 0;
    body_record(body).overlay_enabled = 0;
    body_record(body).overlay_alpha = 0;
}

// The old mode's object goes (0x0003DE30) and its mark is undone: mode 1's bit, mode 2's latches.
void end_effect_mode(RuntimeWormStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 effect = fields_of(strategy).effect;
    if (effect != 0)
    {
        context.release_frame_object(goals.entry.actor, effect); // 0x0003DE30, ECX actor, ESI effect
        fields_of(strategy).effect = 0u;
    }
    const u32 mode = fields_of(strategy).effect_mode;
    if (mode == 1)
    {
        EntityBitBlock& bits = body_bits(context, "0x000C4C70");
        bits.bytes[0] = u8(bits.bytes[0] & 0x7F);
    }
    else if (mode == 2)
    {
        decrement_runtime_body_latches(body_of_actor(context), goals.cleanup.body_owner, context.latches); // 0x0001FD20
    }
}
}

// Xbox 0x000C4560
void worm_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(WormStrategyState);
    output.parameters = 0x118;
}

// Xbox 0x000C45D0
bool initialize_runtime_worm_strategy(std::span<u8> strategy, std::span<const u8> parameters)
{
    fields_of(strategy).timer = kTimerStart;
    const auto copy = [&](size_t to, size_t from)
    {
        size_t index = 0;
        char character;
        do
        {
            character = char(parameters[from + index]);
            strategy[to + index] = u8(character);
            ++index;
        } while (character != 0);
    };
    copy(offsetof(WormStrategyState, emerge_effect_name), 0x18);
    copy(offsetof(WormStrategyState, hit_effect_name), 0x98);
    return true;
}

// Xbox 0x000C4620
void destroy_runtime_worm_strategy(RuntimeWormStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).effect_mode == 0)
    {
        return;
    }
    end_effect_mode(context);
    fields_of(strategy).effect_mode = 0u;
}

// Xbox 0x000C4C70
void set_runtime_worm_effect_mode(u32 mode, RuntimeWormStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (mode == fields_of(strategy).effect_mode)
    {
        return;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 actor = strategy_base(strategy).actor;
    end_effect_mode(context);
    fields_of(strategy).effect_mode = mode;
    if (mode == 1)
    {
        fields_of(strategy).effect = context.create_effect(fields_of(strategy_of(context)).emerge_effect_name.data(), actor); // 0x00107720, 0x00107520, 0x000663E0
        EntityBitBlock& bits = body_bits(context, "0x000C4C70");
        bits.bytes[0] = u8(bits.bytes[0] | 0x80);
    }
    else if (mode == 2)
    {
        fields_of(strategy).effect = context.create_effect(fields_of(strategy_of(context)).hit_effect_name.data(), actor); // 0x00107720, 0x00107520, 0x000663E0
        increment_runtime_body_latches(body_of_actor(context), goals.cleanup.body_owner,
            context.latches); // The tail jump to 0x0001FC40, ECX body.
    }
}

// Xbox 0x000C4D60
bool run_down_runtime_worm_timer(const RuntimeMindTarget* target, RuntimeWormStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    float shown = fields_of(strategy).timer + kOne;
    if (!(kTimerStart > shown)) // COMISS; JA keeps the sum.
    {
        shown = kTimerStart;
    }
    set_body_timer(body_of_actor(context), shown);
    if (target != nullptr && !runtime_mind_target_is_movable(*target)) // 0x00141580, EAX target
    {
        return false;
    }
    const float step = runtime_frame_step(goals.clock);
    const float rate = runtime_actor_rate(goals.entry.actor);
    const float left = fields_of(strategy).timer;
    if (!(left > 0.f))
    {
        return false;
    }
    const float next = left - rate * step;
    fields_of(strategy).timer = next;
    if (0.f < next) // COMISS; JC keeps a positive timer.
    {
        return false;
    }
    fields_of(strategy).timer = 0.f;
    change_state(0x400, context); // The inlined vtable +0x18, 0x00029E80 and +0x14.
    return true;
}

// Xbox 0x000C4690
void update_runtime_worm_strategy(RuntimeWormStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u16 state = strategy_base(strategy_of(context)).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    switch (state)
    {
    case 0x3EF:
        if (emerged(context, "0x000C4690"))
        {
            if (run_down_runtime_worm_timer(mind_target(context), context)) // EAX mind +0x10, ECX body
            {
                return;
            }
            if (!requests_pending(context))
            {
                load_goal(context.goal_attack, context);
            }
            return;
        }
        if (!requests_pending(context))
        {
            change_state(fields_of(strategy_of(context)).effect_mode != 0 ? 0x3EF : 0x401, context);
        }
        return;
    case 0x3FE:
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_target(target, 0, goals) && !requests_pending(context)) // 0x00139AA0
        {
            change_state(0x401, context);
        }
        return;
    }
    case 0x401:
    {
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_target(target, 0, goals)) // 0x00139AA0
        {
            if (target == nullptr)
            {
                throw Error("worm strategy 0x000C4690 passes 0x00138410 a selected target at address zero");
            }
            if (runtime_strategy_abandons_target(*target, context.checks) != 0) // 0x00138410, EDX target
            {
                if (retreat_runtime_strategy_from_reached_target(context.checks) != 0) // 0x001383A0
                {
                    return;
                }
            }
            else if (claim_runtime_strategy_target_queue(context.base.link, target->record, &goals.entry.actor,
                    context.queue)) // 0x0002B970, EBX target, EDI strategy
            {
                goals.room = nullptr; // Actor +0xCA0.
                change_state(0x3EF, context);
                return;
            }
        }
        if (emerged(context, "0x000C4690") && run_down_runtime_worm_timer(target, context)) // EAX target, ECX body
        {
            return;
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_far, context);
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000C4820
void dispatch_runtime_worm_message(std::span<const u32> message, RuntimeWormStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    if (react_runtime_strategy_message(message, context.checks) != 0) // 0x001384F0, EAX strategy, ECX message
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
    switch (message[0])
    {
    case 7:
    {
        // An animation event {7, position, slot, event, phase}: kind 0x3A naming actorBodyFlagId 0x13.
        const std::span<const u8> event = context.event_at(message[3]);
        if (read_u16(event, 0xC) != 0x3A)
        {
            return;
        }
        if (context.flag_names.size() != 0x41)
        {
            throw Error("worm strategy: 0x0001EF70 needs the 0x41 actorBodyFlagId names");
        }
        const char* name = reinterpret_cast<const char*>(event.data() + 0x10);
        if (runtime_body_flag_name_index(name, std::span<const char* const, 0x41>(context.flag_names.data(), 0x41))
            != 0x13) // 0x0001EF70, EBX name
        {
            return;
        }
        actor_set_uniform_scale(goals.entry.actor, kEmergedScale); // 0x00041B90, ECX actor
        set_runtime_worm_effect_mode(1, context); // 0x000C4C70
        raise_runtime_body_request_tag(body_of_actor(context), context.live_attachments, 8); // 0x0001D830
        strategy_base(strategy).unk_18 = u16(0x3EF);
        const std::array<u32, 1> notice{0x4A};
        goals.entry.events.actor_message(notice); // Actor vtable +0xC.
        return;
    }
    case 0x14:
        if (fields_of(strategy).effect == message[1])
        {
            fields_of(strategy).effect = 0u;
        }
        return;
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x1E, context.checks); // 0x0013A4F0
        return;
    case 0x6E:
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        const std::span<const u8> actor_bytes = goals.entry.actor_bytes;
        if (player_record(actor_bytes).unk_CE4 != 0 && player_record(actor_bytes).unk_96D != 0)
        {
            reset_runtime_player_attribute_timer(goals.cleanup.actor, 0); // 0x00019660, EAX actor
        }
        if (message[1] != actor)
        {
            return;
        }
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            return;
        }
        if (runtime_body_bit_set(body_bits(context, "0x000C4820"), 0x13)) // 0x0001EF10, EAX body
        {
            return;
        }
        if (mind_record(goals.entry.events.mind).target_record != 0 && strategy_base(strategy).pause_count == 0)
        {
            const u32 kind =
                player_record(actor_bytes).mode == 2 || player_record(actor_bytes).case_link != 0 ? 1u : 0u;
            if (goals.resume.references[kind] == 0 && goals.resume.decay_enabled[goals.resume.decay_index] != 0)
            {
                // DAT_0050E764, DAT_0030F144[DAT_0051248C]. The first goal kind of the actor +0x980 ring; an empty
                // ring reads the word at address zero.
                const RuntimePlayerGoalRing* ring = goals.entry.ring;
                const bool queued = ring != nullptr
                    && (ring->tail == ring->capacity ? read_u16(goals.entry.address_zero, 0)
                                                     : read_u16(ring->rows, size_t(ring->stride) * ring->head))
                        != 0;
                if (!queued && body_record(body_of_actor(context)).energy != 0)
                {
                    if (strategy_base(strategy).state != 0x3EF)
                    {
                        set_runtime_worm_effect_mode(2, context); // 0x000C4C70
                        change_state(0x3EF, context);
                        strategy_base(strategy).unk_18 = u16(0x3EF);
                        return;
                    }
                    if (fields_of(strategy).effect != 0)
                    {
                        return;
                    }
                    set_runtime_worm_effect_mode(2, context); // 0x000C4C70
                    strategy_base(strategy).unk_18 = u16(0x3EF);
                    return;
                }
            }
        }
        if (script_actor_current_mask(goals.entry.actor, context.base.query) == 0) // 0x00028360, EAX actor
        {
            return;
        }
        take_runtime_strategy_hit_sender(message, context.checks, context.views); // 0x001394D0; its result is not read.
        if (fields_of(strategy).effect != 0)
        {
            return;
        }
        set_runtime_worm_effect_mode(2, context); // 0x000C4C70
        strategy_base(strategy).after_wait_state = u16(0x3EF);
        strategy_base(strategy).unk_18 = u16(0x3EF);
        return;
    }
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000C4B20
void enter_runtime_worm_state(u16 state, u16 previous, RuntimeWormStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of_actor(context);
    const std::span<u8> mind = goals.entry.events.mind;
    body_record(body).move_class = 2;
    strategy_base(strategy).attacking = 0;
    const u32 target = mind_record(mind).target_record;
    switch (state)
    {
    case 0x3EF:
        body_record(body).move_class = 6;
        strategy_base(strategy).attacking = 1;
        if (!emerged(context, "0x000C4B20"))
        {
            load_goal(context.goal_attack_prepare, context);
        }
        load_goal(context.goal_attack, context);
        return;
    case 0x400:
        set_body_timer(body, 0.f);
        ++strategy_base(strategy).pause_count;
        mind_record(mind).action_record = target;
        context.select_request(0x67); // 0x0001D210, EAX key
        return;
    case 0x401:
        load_goal(context.goal_far, context);
        return;
    default:
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000C4C10
void leave_runtime_worm_state(u16 state, RuntimeWormStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EF:
        detach_runtime_strategy_link(context.base.link); // 0x00112A20
        if (emerged(context, "0x000C4C10"))
        {
            strategy_base(strategy).unk_18 = u16(0x3EF);
        }
        return;
    case 0x400:
    case 0x3FC:
    case 0x3FF:
    case 0x415:
        --strategy_base(strategy).pause_count;
        return;
    default:
        return;
    }
}
}
