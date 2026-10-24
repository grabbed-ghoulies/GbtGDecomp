#include "imp_ninja_strategy.h"
#include "actor_transform.h"
#include "body_message_helpers.h"
#include "player_attributes.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/anim.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = imp_ninja_strategy_field;

RuntimePlayerStrategyGoals& goals_of(RuntimeImpNinjaStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeImpNinjaStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeImpNinjaStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeImpNinjaStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeImpNinjaStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeImpNinjaStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

float parameter(RuntimeImpNinjaStrategy& context, size_t offset)
{
    return read_f32(context.parameters, offset);
}

float roll(RuntimeImpNinjaStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// Actor +0x964 +offset, or 0 without the attribute resource.
float attribute(RuntimeImpNinjaStrategy& context, size_t offset)
{
    const std::span<const u8> attributes = context.shared.checks.attributes;
    return attributes.data() != nullptr ? read_f32(attributes, offset) : 0.f;
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> mind = mind_of(context);
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = read_u32(mind, 0x10);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

RuntimeMindTarget* select_target(bool& found, RuntimeImpNinjaStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

bool join_queue(const RuntimeMindTarget* target, RuntimeImpNinjaStrategy& context)
{
    return join_runtime_strategy_target_queue(context.shared.base.link, target == nullptr ? nullptr : target->record,
        &goals_of(context).entry.actor, context.shared.queue); // 0x0002B910, EDI the target
}

EntityBitBlock& body_bits(RuntimeImpNinjaStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string(caller) + " reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

// Xbox 0x001414F0, EAX target: a player (+0xCDC 2, or +0x93C set) with +0xCE0 set, and the record's +0x14 clear.
bool target_is_watching_player(const RuntimeMindTarget& target)
{
    if (target.bytes[0x14] != 0)
    {
        return false;
    }
    const std::span<const u8> actor = target.actor_bytes;
    if (read_u32(actor, 0xCE0) == 0)
    {
        return false;
    }
    return read_u16(actor, 0xCDC) == 2 || read_u32(actor, 0x93C) != 0;
}

// Xbox 0x000B7360, EBX strategy: the inner force's effect, made once from the template named at +0x3C.
void make_inner_force_effect(RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_made) != 0)
    {
        return;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const char* name = reinterpret_cast<const char*>(strategy.data() + field::effect_name.offset);
    const u32 actor = goals.address_of(goals.entry.actor_bytes.data());
    set(strategy, field::effect, context.create_effect(name, actor)); // 0x00107720, 0x00107520, 0x000663E0
    set(strategy, field::effect_made, 1u);
}

// Xbox 0x000B73C0, ESI strategy: message 5 ends the effect.
void end_inner_force_effect(RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_made) == 0)
    {
        return;
    }
    const std::array<u32, 2> message{5, 0};
    context.object_message(get(strategy, field::effect), message); // Effect vtable +0xC.
    set(strategy, field::effect, 0u);
    set(strategy, field::effect_made, 0u);
}

// Xbox 0x000B7750, ESI strategy: the psych-up (0x3ED), then the kick (0x3EE).
void tick_psych_up(RuntimeImpNinjaStrategy& context)
{
    bool found = false;
    RuntimeMindTarget* target = select_target(found, context);
    if (!found)
    {
        return;
    }
    if (!join_queue(target, context))
    {
        change_state(0x401, context);
    }
    if (!requests_pending(context))
    {
        change_state(0x3EE, context);
    }
}

// Xbox 0x000B7860, ESI strategy: the kick (0x3EE) ends in 0x401.
void tick_kick(RuntimeImpNinjaStrategy& context)
{
    bool found = false;
    RuntimeMindTarget* target = select_target(found, context);
    if (!found)
    {
        return;
    }
    if (!join_queue(target, context))
    {
        change_state(0x401, context);
    }
    if (!requests_pending(context))
    {
        change_state(0x401, context);
    }
}

// Xbox 0x000B71D0, ECX strategy, EAX actor: the inner force (0x3EF); its first ending makes the imp big.
void tick_inner_force(RuntimeImpNinjaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    EntityBitBlock& bits = body_bits(context, "0x000B71D0");
    if ((bits.bytes[0] & 4) != 0)
    {
        make_inner_force_effect(context); // 0x000B7360
        bits.bytes[0] &= 0xFB;
    }
    if (requests_pending(context))
    {
        return;
    }
    if (get(strategy, field::transformed) != 0)
    {
        change_state(0x401, context);
        return;
    }
    Actor& actor = goals.entry.actor;
    const std::span<u8> actor_bytes = goals.entry.actor_bytes;
    const std::span<u8> body = goals.entry.body; // Actor +0x95C.
    set(strategy, field::transformed, 1u);
    load_goal(context.goal_inner_force_out, context);
    actor_set_uniform_scale(actor, actor.radius_y * 1.7f); // The inlined 0x00041B90, DAT_0032B59C.
    actor_refresh_transform(actor); // 0x00040DE0
    const std::array<u32, 1> message{0x4A};
    context.actor_message(message); // Actor vtable +0xC.
    bits.bytes[7] |= 0x80;
    if (read_u16(actor_bytes, 0xCF4) != 0)
    {
        const u16 zero = 0;
        std::memcpy(actor_bytes.data() + 0xCF4, &zero, 2);
    }
    const u16 row = find_runtime_player_tag_index(0x1C, context.tag_rows, context.tag_names); // 0x00133A50
    std::memcpy(actor_bytes.data() + 0xCF4, &row, 2);
    actor.damage_class = row; // Native alias of the actor +0xCF4 word.
    const u16 tag = 0x1C;
    std::memcpy(actor_bytes.data() + 0xCF6, &tag, 2);
    if (context.damage_tables == nullptr)
    {
        throw Error("0x000B71D0 refills the big imp's health before the scene's damage tables are set");
    }
    const u16 health = u16(runtime_actor_damage_value(1, actor.damage_class, *context.damage_tables)); // 0x00028DA0
    std::memcpy(body.data() + 0x94, &health, 2);
    std::memcpy(body.data() + 0x96, &health, 2);
    end_inner_force_effect(context); // 0x000B73C0
}

// Xbox 0x000B74A0, EAX actor, ECX strategy: the target lives, stands, is no ghost and queues the actor.
bool target_open_to_attack(RuntimeImpNinjaStrategy& context)
{
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    if (target->body.energy == 0) // Target body +0x94.
    {
        return false;
    }
    const EntityBitBlock* bits = target->body.bits; // Target body +0x15C.
    if (bits == nullptr)
    {
        throw Error("0x000B74A0 reads the bit block of a target with no body");
    }
    if ((bits->bytes[3] & 8) != 0 || (bits->bytes[4] & 0x10) != 0 || (bits->bytes[4] & 0x20) != 0)
    {
        return false;
    }
    if (runtime_body_bit_set(*bits, 0x1F)) // 0x0001EF10
    {
        return false;
    }
    if (runtime_strategy_abandons_target(*target, context.shared.checks) != 0) // 0x00138410, EDX target
    {
        return false;
    }
    return join_queue(target, context); // 0x0002B910
}

// Xbox 0x000B7600, EAX strategy: the attack against the queued target.
void choose_attack(RuntimeImpNinjaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return;
    }
    goals.room = nullptr; // Actor +0xCA0.
    if (get(strategy, field::unk_C0) != 0)
    {
        const std::span<const u8> target_body = context.shared.views.body_of(target->actor).bytes;
        if (std::bit_cast<i32>(read_u32(target_body, 0x738)) <= 1)
        {
            change_state(0x3ED, context);
            set(strategy, field::spin_kick, 1u);
            return;
        }
        if (get(strategy, field::transformed) != 0
            || actor_event_chance(goals.random, attribute(context, 0xC))) // 0x00019200
        {
            change_state(0x3EE, context);
            return;
        }
        change_state(0x3ED, context);
        return;
    }
    if (get(strategy, field::transformed) != 0)
    {
        change_state(0x3EE, context);
        return;
    }
    if (actor_event_chance(goals.random, attribute(context, 8))) // 0x00019200
    {
        change_state(0x3EF, context);
        return;
    }
    change_state(actor_event_chance(goals.random, attribute(context, 0xC)) ? 0x3EE : 0x3ED, context); // 0x00019200
}

// Xbox 0x000B7520, EAX strategy: meditate (5%) when no target is near, else move or walk at random.
void choose_idle(RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::transformed) == 0 && !(parameter(context, 0xC) < roll(context))
        && !runtime_imp_ninja_target_near(context)) // 0x000B7FE0
    {
        change_state(0x412, context);
        return;
    }
    load_goal(!(parameter(context, 8) < roll(context)) ? context.goal_random_move : context.goal_random_walk,
        context);
}

// Xbox 0x000B7410, ESI strategy, EBX actor.
void tick_idle(RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    bool found = false;
    RuntimeMindTarget* target = select_target(found, context);
    if (found && runtime_strategy_abandons_target(*target, context.shared.checks) != 0 // 0x00138410, EDX target
        && retreat_runtime_strategy_from_reached_target(context.shared.checks) != 0) // 0x001383A0
    {
        return;
    }
    switch (get(strategy, field::idle_step))
    {
    case 1:
        if ((body_bits(context, "0x000B7410").bytes[6] & 1) != 0)
        {
            set(strategy, field::idle_step, 2u);
        }
        return;
    case 2:
        if (target_open_to_attack(context)) // 0x000B74A0
        {
            choose_attack(context); // 0x000B7600
            return;
        }
        if (!requests_pending(context))
        {
            choose_idle(context); // 0x000B7520
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000B79B0, EAX strategy: meditation (0x412) until its time is out or a target comes near, then out of it.
void tick_meditation(RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (get(strategy, field::meditation_step))
    {
    case 1:
    {
        float meditation = get(strategy, field::meditation);
        const bool done = count_down_runtime_strategy_timer(meditation, goals_of(context)); // Inlined 0x0002B9F0.
        set(strategy, field::meditation, meditation);
        if (!done && !runtime_imp_ninja_target_near(context)) // 0x000B7FE0
        {
            return;
        }
        if (get(strategy, field::meditation_step) == 2)
        {
            return;
        }
        set(strategy, field::meditation_step, 2u);
        load_goal(context.goal_meditate_out, context);
        return;
    }
    case 2:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}
}

// Xbox 0x000B7A90
void imp_ninja_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x98;
}

// Xbox 0x000B7B00
bool initialize_runtime_imp_ninja_strategy(std::span<u8> strategy, std::span<const u8> parameters)
{
    set(strategy, field::parameters, 0x00303198u);
    set(strategy, field::spin_kick, 0u);
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(strategy.data() + field::effect_name.offset, name, std::strlen(name) + 1);
    return true;
}

// Xbox 0x000B7FE0, ECX strategy
bool runtime_imp_ninja_target_near(RuntimeImpNinjaStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const float reach = parameter(context, 0);
    const Vec3 position = actor_world_position(goals_of(context).entry.actor); // 0x000412A0
    const float dx = position.x - read_f32(target->bytes, 0x20);
    const float dz = position.z - read_f32(target->bytes, 0x28);
    const float dy = position.y - read_f32(target->bytes, 0x24);
    return dx * dx + dz * dz + dy * dy < reach * reach;
}

// Xbox 0x000B7B40
void update_runtime_imp_ninja_strategy(RuntimeImpNinjaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const u16 state = read_u16(strategy, 0xE);
    const bool provoked = read_f32(actor, 0x978) > 0.f || actor[0x969] != 0; // DAT_0032B100.
    const RuntimeMindTarget* target = mind_target(context);
    u32 open = 0;
    if (target != nullptr)
    {
        open = provoked || !target_is_watching_player(*target) ? 1u : 0u; // 0x001414F0
    }
    else
    {
        open = read_f32(actor, 0x978) > 0.f ? 1u : u32(actor[0x969]);
    }
    set(strategy, field::unk_C0, open);
    update_runtime_base_strategy(goals); // 0x0002C130
    switch (state)
    {
    case 0x3ED:
        tick_psych_up(context); // 0x000B7750
        return;
    case 0x3EE:
        tick_kick(context); // 0x000B7860
        return;
    case 0x3EF:
        tick_inner_force(context); // The tail jump to 0x000B71D0.
        return;
    case 0x401:
        tick_idle(context); // 0x000B7410
        return;
    case 0x412:
        tick_meditation(context); // 0x000B79B0
        return;
    default:
        return;
    }
}

// Xbox 0x000B7C50
void dispatch_runtime_imp_ninja_message(std::span<const u32> message, RuntimeImpNinjaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeSpiderStrategy& shared = context.shared;
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = read_u32(strategy, 8);
    const u16 state = read_u16(strategy, 0xE);
    const bool attacking = state >= 0x3ED && state <= 0x3EF; // 0x000B8080 for 0x6E.
    react_runtime_strategy_message(message, shared.checks); // 0x001384F0; its result is not read.
    switch (message[0])
    {
    case 0x31:
        if (!attacking)
        {
            react_runtime_strategy_watched_actor(message, shared.checks, shared.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x13, shared.checks); // 0x0013A4F0
        return;
    case 0x6E:
    {
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        retaliate_runtime_strategy_contact(message, 2, shared.checks, shared.views); // 0x00139390
        if (!attacking)
        {
            react_runtime_strategy_contact(message, 0.5f, shared.checks); // 0x001395C0, DAT_0032B0F4
        }
        bool settle = false;
        if (message[1] == actor && strategy[0x1C] == 0)
        {
            const std::span<const u8> actor_bytes = goals.entry.actor_bytes;
            const u32 kind = read_u16(actor_bytes, 0xCDC) == 2 || read_u32(actor_bytes, 0x93C) != 0 ? 1u : 0u;
            if (goals.resume.references[kind] == 0 && goals.resume.decay_enabled[goals.resume.decay_index] != 0)
            {
                // The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
                const RuntimePlayerGoalRing* ring = goals.entry.ring;
                settle = ring == nullptr
                    || (ring->tail == ring->capacity ? read_u16(goals.entry.address_zero, 0)
                                                     : read_u16(ring->rows, size_t(ring->stride) * ring->head))
                        == 0;
            }
        }
        if (!settle)
        {
            if (!runtime_message_lands_from_mind_target(message, mind_of(context), goals.target_at)) // 0x00140910
            {
                return;
            }
            if (state == 0x3EF)
            {
                return;
            }
        }
        change_state(0x401, context);
        return;
    }
    case 0x82:
        if (!attacking)
        {
            react_runtime_strategy_cower(shared.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (!attacking)
        {
            react_runtime_strategy_startle(shared.checks); // The tail jump to 0x001399A0.
        }
        return;
    default:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B7E60
void enter_runtime_imp_ninja_state(u16 state, u16 previous, RuntimeImpNinjaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = goals.entry.body; // Actor +0x95C.
    body[0x328] = 3;
    switch (state)
    {
    case 0x3ED:
        // The tail jump to 0x000B7710.
        body[0x328] = 4;
        strategy[0x1A] = 1;
        load_goal(get(strategy, field::unk_C0) == 0 ? context.goal_psycheup : context.goal_psycheup_traitor, context);
        return;
    case 0x3EE:
        // The tail jump to 0x000B7800.
        body[0x328] = 5;
        strategy[0x1A] = 1;
        if (get(strategy, field::spin_kick) != 0)
        {
            load_goal(context.goal_spin_kick, context);
            return;
        }
        load_goal(get(strategy, field::transformed) != 0 ? context.goal_jump_kick_big : context.goal_jump_kick, context);
        return;
    case 0x3EF:
        // 0x000B7140.
        load_goal(context.goal_inner_force, context);
        set(strategy, field::effect_made, 0u);
        body[0x328] = 6;
        strategy[0x1A] = 1;
        return;
    case 0x401:
        set(strategy, field::spin_kick, 0u);
        set(strategy, field::idle_step, 1u);
        return;
    case 0x412:
        // 0x000B7920.
        if (get(strategy, field::meditation_step) != 1)
        {
            set(strategy, field::meditation_step, 1u);
            load_goal(context.goal_meditate, context);
            const float shortest = parameter(context, 0x10);
            const float longest = parameter(context, 0x14);
            set(strategy, field::meditation, (longest - shortest) * roll(context) + shortest);
        }
        strategy[0x1A] = 1;
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // The tail jump to 0x0002BAF0.
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000B7F20
void leave_runtime_imp_ninja_state(u16 state, RuntimeImpNinjaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
    case 0x3EE:
        detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
        strategy[0x1A] = 0;
        return;
    case 0x3EF:
        // 0x000B7180.
        detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
        strategy[0x1A] = 0;
        end_inner_force_effect(context); // The same release as 0x000B73C0.
        return;
    case 0x401:
        set(strategy, field::idle_step, 0u);
        return;
    case 0x412:
        if (get(strategy, field::meditation_step) != 0)
        {
            set(strategy, field::meditation_step, 0u);
        }
        strategy[0x1A] = 0;
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            strategy[0x1C] = u8(strategy[0x1C] - 1);
        }
        return;
    }
}
}
