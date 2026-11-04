#include "mummy_cursed_strategy.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_model_anchors.h"
#include "actor_transform.h"
#include "body_animation_commands.h"
#include "body_damage_motion.h"
#include "body_damage_notices.h"
#include "body_jump_on_state.h"
#include "body_look_for_weapon_state.h"
#include "body_message_helpers.h"
#include "body_action_ids.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "player_strategy_goals.h"
#include "../graphics/actor_effect_nodes.h"
#include "../graphics/actor_effect_stream.h"
#include "../navigation/navigation.h"
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
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
namespace field = mummy_cursed_strategy_field;

constexpr float kMinusOne = -1.f; // DAT_0032B120.
constexpr float kHalf = 0.5f; // DAT_0032B0F4.
constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kScareRadius = 1000.f; // 0x447A0000.

RuntimePlayerStrategyGoals& goals_of(RuntimeMummyCursedStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeMummyCursedStrategy& context)
{
    return goals_of(context).entry.strategy;
}

Actor& actor_of(RuntimeMummyCursedStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeMummyCursedStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeMummyCursedStrategy& context)
{
    return goals_of(context).entry.body;
}

u32 actor_address(RuntimeMummyCursedStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    return goals.address_of(goals.entry.actor_bytes.data());
}

u32 strategy_address(RuntimeMummyCursedStrategy& context)
{
    return goals_of(context).address_of(strategy_of(context).data());
}

EntityBitBlock& body_bits(RuntimeMummyCursedStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string(caller) + " reads the cursed mummy's body +0x15C bit block of a body with none");
    }
    return *bits;
}

float parameter(RuntimeMummyCursedStrategy& context, size_t offset)
{
    return read_f32(context.parameters, offset);
}

const char* name_at(RuntimeMummyCursedStrategy& context, size_t offset)
{
    return reinterpret_cast<const char*>(strategy_of(context).data() + offset);
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeMummyCursedStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

void clear_requests(RuntimeMummyCursedStrategy& context)
{
    clear_runtime_player_mind_requests(*goals_of(context).setup.mind); // 0x00029E80, EBX mind
}

// The inlined 0x0002B8D0 sequence: vtable +0x18 with the old state, the state, 0x00029E80 and vtable +0x14.
void change_state(u16 state, RuntimeMummyCursedStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeMummyCursedStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> mind = mind_of(context);
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = get(mind, mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

Actor& actor_at(u32 address, RuntimeMummyCursedStrategy& context, const char* caller)
{
    Actor* actor = address == 0 ? nullptr : context.shared.views.actor_at(address);
    if (actor == nullptr)
    {
        throw Error(std::string(caller) + " reads the cursed mummy's actor " + std::to_string(address)
            + " with no scene actor behind it");
    }
    return *actor;
}

float roll(RuntimeMummyCursedStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - kOne;
}

// Xbox 0x000BC720, EAX the block: a time between the block's +0x18 and +0x1C.
float phase_three_time(RuntimeMummyCursedStrategy& context)
{
    const float low = parameter(context, 0x18);
    const float high = parameter(context, 0x1C);
    return (high - low) * roll(context) + low;
}

// Xbox 0x000BC6D0, EAX the block: a time between the block's +0x20 and +0x24.
float phase_four_time(RuntimeMummyCursedStrategy& context)
{
    const float low = parameter(context, 0x20);
    const float high = parameter(context, 0x24);
    return (high - low) * roll(context) + low;
}

// Xbox 0x000B7120, EAX strategy: an attack state (0x3ED..0x3EF).
bool attacking(RuntimeMummyCursedStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    return state > 0x3EC && state < 0x3F0;
}

// The actor's player kind (actor +0xCDC 2, or +0x93C set) is not blocked (DAT_0050E764) and decay runs.
bool decay_running(RuntimeMummyCursedStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const u32 kind = get(actor, player_field::mode) == 2 || get(actor, player_field::case_link) != 0 ? 1u : 0u;
    return goals.resume.references[kind] == 0 && goals.resume.decay_enabled[goals.resume.decay_index] != 0;
}

// An object's vtable +0xC with message 5 (its end), when there is one.
void end_object(u32 object, RuntimeMummyCursedStrategy& context)
{
    const std::array<u32, 2> message{5, 0};
    context.object_message(object, message);
}

// 0x0003BE20 or 0x0013B7E0 writing their tokens (0x0013B8B0) into the strategy block at `offset`.
template <typename Dispatch>
void with_events(size_t offset, RuntimeMummyCursedStrategy& context, Dispatch dispatch)
{
    const std::span<u8> saved = context.callouts.command_attachments;
    context.callouts.command_attachments = strategy_of(context).subspan(offset, 0x24);
    dispatch(strategy_address(context) + u32(offset));
    context.callouts.command_attachments = saved;
}

// 0x0003BE20 with 0x0013B8B0 over the actor's callout records (+0x8C).
void dispatch_callouts(i32 id, size_t offset, RuntimeMummyCursedStrategy& context)
{
    with_events(offset, context,
        [&](u32 output)
        {
            dispatch_actor_attachment_records(context.resource.resource.data(), id, record_runtime_body_attachment_event,
                nullptr, context.callouts, output); // 0x0003BE20
        });
}

// 0x0013B7E0 with 0x46 into +0x158.
void request_grab_callout(RuntimeMummyCursedStrategy& context)
{
    with_events(field::grab_events.offset, context,
        [&](u32 output)
        {
            request_runtime_player_attachment_events(context.resource.resource.data(), context.callouts, 0x46,
                output); // 0x0013B7E0
        });
}

// The inlined 0x0013B810 / 0x0003CFD0 loop over a token block: each token's attachment removed, the count zeroed.
void remove_events(size_t offset, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> events = strategy_of(context).subspan(offset, 0x24);
    i32 index = 0;
    while (index < std::bit_cast<i32>(read_u32(events, 0)))
    {
        remove_runtime_body_attachment(u16(read_u32(events, size_t(index + 1) * 4)), context.lists); // 0x0003CFD0
        ++index;
    }
    const u32 none = 0;
    std::memcpy(events.data(), &none, 4);
}

// Xbox 0x000BD650, EAX strategy: the victim's body bit block byte 7 loses bit 0x20.
void release_victim_hold(RuntimeMummyCursedStrategy& context)
{
    const u32 victim = get(strategy_of(context), field::victim);
    if (victim == 0 || context.body_at(victim).data() == nullptr)
    {
        return;
    }
    EntityBitBlock& bits = context.body_bits_at(victim);
    bits.bytes[7] = u8(bits.bytes[7] & 0xDF);
}

// The inlined end of a phase: phase 2 requests callout 0x46 once, phase 4 lets the victim go (0x000BD650) and drops
// the callout's attachments (0x0013B810), phase 5 forgets the whack.
void end_phase(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 phase = get(strategy, field::phase);
    if (phase == 2)
    {
        if (get(strategy, field::grab_callout) == 0)
        {
            request_grab_callout(context); // 0x0013B7E0, ECX actor, EDX +0x158
            set(strategy, field::grab_callout, 1u);
        }
    }
    else if (phase == 4)
    {
        release_victim_hold(context); // 0x000BD650
        clear_runtime_player_attachment_events(strategy.subspan(field::grab_events.offset, 0x24),
            context.lists); // 0x0013B810, EBX +0x158
        set(strategy, field::grab_callout, 0u);
    }
    else if (phase == 5)
    {
        set(strategy, field::whacked, 0u);
    }
}

// Xbox 0x000BE070, EAX strategy: a rider claim (+0x180) on +0x184 whose body still holds this mummy (+0x718) ends.
void release_mount(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::riding) == 0 || get(strategy, field::mount) == 0)
    {
        return;
    }
    const std::span<u8> body = context.body_at(get(strategy, field::mount));
    const u32 holder = read_u32(body, 0x718);
    if (holder != 0 && holder == strategy_base(strategy).actor)
    {
        const u32 none = 0;
        std::memcpy(body.data() + 0x718, &none, 4);
        set(strategy, field::riding, 0u);
        set(strategy, field::mount, 0u);
    }
}

// Xbox 0x000BCAA0, ESI strategy: the deathcurse object (+0x2A0) ends.
void end_deathcurse(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::deathcurse) != 0)
    {
        end_object(get(strategy, field::deathcurse), context);
        set(strategy, field::deathcurse, 0u);
    }
}

// Xbox 0x000BC770, ESI strategy: the curse ends: the form (+0x190), the meter while it shows this actor, both curse
// objects and both callout blocks.
void end_curse(u32 actor, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::curse_form) != 0)
    {
        set(strategy, field::curse_form, 0u);
    }
    if (context.meter_shown() && context.meter_actor() == actor) // DAT_00545A18, DAT_00545A20
    {
        context.stop_meter(); // 0x0012E6C0
    }
    if (get(strategy, field::curse_effect) != 0)
    {
        end_object(get(strategy, field::curse_effect), context);
        set(strategy, field::curse_effect, 0u);
    }
    if (get(strategy, field::deathcurse) != 0)
    {
        end_object(get(strategy, field::deathcurse), context);
        set(strategy, field::deathcurse, 0u);
    }
    remove_events(field::curse_events.offset, context); // 0x0003CFD0 over +0x2A8, EAX actor
    remove_events(field::spawn_events.offset, context); // 0x0003CFD0 over +0x2CC, EAX actor
}

// Xbox 0x000BC910, EAX form: form 1 drops the deathcurse and, with the block's +0x3C clear, spawns the object named at
// +0x198 at the actor's model binding 4 and bursts it; form 2 makes the effect named at +0x3C, shows the baddie meter
// against the cursed player, scares the room and sets the body bit block byte 0 bit 0x20.
void set_curse_form(u32 form, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (form == get(strategy, field::curse_form))
    {
        return;
    }
    set(strategy, field::curse_form, form);
    Actor& actor = actor_of(context);
    const u32 address = actor_address(context);
    if (form == 1)
    {
        end_deathcurse(context); // 0x000BCAA0
        clear_runtime_player_attachment_events(strategy.subspan(field::curse_events.offset, 0x24),
            context.lists); // 0x0013B810, EBX +0x2A4
        dispatch_callouts(0x43, field::spawn_events.offset, context); // 0x0003BE20, actor +0x8C
        if (read_u32(context.parameters, 0x3C) != 0)
        {
            return;
        }
        if (actor.model == nullptr)
        {
            throw Error("0x00052F80 reads the cursed mummy's model data through address zero");
        }
        if (!actor_model_record_bit(actor.model->cpu, 0x10)) // 0x00052F80, EDX the model data
        {
            return;
        }
        context.model_nodes(0x10, 1); // 0x00040D50, BL 1
        const u8* record = find_actor_model_binding(actor, 4); // 0x000408D0, EDI 4
        if (record == nullptr)
        {
            throw Error("0x000BC910 places the cursed mummy's spawn at a model binding 4 it does not have");
        }
        ModelBoneBinding binding;
        std::memcpy(&binding.kind, record, 2);
        std::memcpy(&binding.joint, record + 2, 2);
        std::memcpy(binding.matrix.data(), record + 4, 48);
        const u32 spawn = context.create_patched(name_at(context, field::spawn_name.offset),
            [&](std::span<u8> clone)
            {
                Vec3 position;
                Vec3 euler;
                actor_binding_world_position(actor, binding, position); // 0x00040A60, ESI clone +8
                actor_binding_world_euler(actor, binding, euler); // 0x00040AA0, EBX clone +0x14
                std::memcpy(clone.data() + 8, &position, 12);
                std::memcpy(clone.data() + 0x14, &euler, 12);
            }); // 0x00107720, 0x00107520, 0x000663E0
        set(strategy, field::spawn, spawn);
        context.burst(spawn, kMinusOne, name_at(context, field::burst_name.offset)); // 0x0013B1F0, ESI +0x218
        context.activate(spawn); // 0x00112CC0, ESI +0x298
        return;
    }
    if (form != 2)
    {
        return;
    }
    set(strategy, field::curse_effect, context.create_effect(name_at(context, field::curse_effect_name.offset),
        address)); // 0x00107720, 0x00107520, 0x000663E0
    context.start_meter(address, get(strategy, field::cursed), parameter(context, 0x38)); // 0x0012E5E0
    context.scare_near(kScareRadius); // 0x0013B660, ESI actor
    EntityBitBlock& bits = body_bits(context, "0x000BC910");
    bits.bytes[0] = u8(bits.bytes[0] | 0x20);
}

// Xbox 0x000BC870, EAX strategy: with no baddie meter shown the curse takes form 2, else it ends.
void land_curse(u32 actor, RuntimeMummyCursedStrategy& context)
{
    if (!context.meter_shown()) // DAT_00545A18
    {
        set_curse_form(2, context); // 0x000BC910
        return;
    }
    end_curse(actor, context); // 0x000BC770
}

// Xbox 0x000BC8A0, ECX actor, EDX strategy: decay running, a cursed player and the body bit block byte 0 bit 4 set,
// the curse takes form 1.
void check_curse(RuntimeMummyCursedStrategy& context)
{
    if (!decay_running(context) || get(strategy_of(context), field::cursed) == 0)
    {
        return;
    }
    if ((body_bits(context, "0x000BC8A0").bytes[0] & 4) == 0)
    {
        return;
    }
    set_curse_form(1, context); // 0x000BC910, EAX 1
}

// Xbox 0x000BCD40: the mind's target point (+0x20) lies nearer than the block's +0.
bool target_in_reach(RuntimeMummyCursedStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const long double dx = static_cast<long double>(position.x) - get(target->bytes, mind_target_field::shape_position).x;
    const long double dy = static_cast<long double>(position.y) - get(target->bytes, mind_target_field::shape_position).y;
    const long double dz = static_cast<long double>(position.z) - get(target->bytes, mind_target_field::shape_position).z;
    return static_cast<long double>(parameter(context, 0)) > std::sqrt(dz * dz + (dy * dy + dx * dx));
}

// Xbox 0x000BCB80, ESI strategy: a target in reach is evaded ("evade").
bool evade_target(RuntimeMummyCursedStrategy& context)
{
    if (!target_in_reach(context)) // 0x000BCD40
    {
        return false;
    }
    const std::span<u8> strategy = strategy_of(context);
    body_of(context)[0x328] = get(strategy, field::curse_landed) != 0 ? 3 : 2;
    load_goal(context.goal_evade, context);
    set(strategy, field::evading, 0u);
    return true;
}

// Xbox 0x000BCBE0, EAX strategy: an uncursed mummy whose target has energy, is not bit 3.3 and is not abandoned
// queues on it; then it curses (0x3EF) a watched player by its aggression, or grabs (0x3EE) a target it reaches with a
// clear path and claims as a rider. Otherwise it leaves the queue.
bool engage_target(RuntimeMummyCursedStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::curse_landed) != 0)
    {
        return false;
    }
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    Actor& actor = actor_of(context);
    const u32 address = actor_address(context);
    const RuntimeStrategyBodyView body = context.shared.views.body_of(target->actor); // Target actor +0x95C.
    if (body.bytes.data() == nullptr || body.bits == nullptr)
    {
        throw Error("0x000BCBE0 reads the body of a cursed mummy target with none");
    }
    if (read_u16(body.bytes, 0x94) == 0 || (body.bits->bytes[3] & 8) != 0)
    {
        return false;
    }
    if (runtime_strategy_abandons_target(*target, context.shared.checks) != 0) // 0x00138410, EDX target
    {
        return false;
    }
    if (!join_runtime_strategy_target_queue(context.shared.base.link, target->record, &actor,
            context.shared.queue)) // 0x0002B910, EBX strategy, EDI target
    {
        return false;
    }
    const std::span<const u8> attributes = context.shared.checks.attributes; // Actor +0x964.
    const float aggression = attributes.data() != nullptr ? read_f32(attributes, 8) : 0.f;
    if (actor_event_chance(goals.random, aggression) // 0x00019200, ECX DAT_0054DD7C
        && runtime_strategy_target_watching_player(*target)) // 0x001414F0, EAX target
    {
        goals.room = nullptr; // Actor +0x954 -> +8 -> +0xCA0.
        change_state(0x3EF, context);
        return true;
    }
    const Vec3 point = actor_world_position(target->actor); // 0x000412A0, ESI the target's actor
    i32 last_cell = 0;
    if (navigation_path_clear(actor_world_position(actor), point, 1, 0, 0, 0, nullptr, &last_cell,
            i16(read_u16(goals.entry.actor_bytes, 6)))) // 0x0012C970, EAX actor
    {
        const u32 mount = get(target->bytes, mind_target_field::actor);
        RuntimeJumpOnPeer peer{&target->actor, mount, target->actor_bytes, body.bytes,
            const_cast<EntityBitBlock*>(body.bits)};
        if (claim_runtime_rider(&peer, address)) // 0x00138140, EAX the target's actor
        {
            set(strategy, field::mount, mount);
            set(strategy, field::riding, 1u);
            goals.room = nullptr; // Actor +0x954 -> +8 -> +0xCA0.
            change_state(0x3EE, context);
            return true;
        }
    }
    detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
    return false;
}

// Xbox 0x000BCAE0, EAX actor, ECX strategy: holding, the target is reselected; one not engaged with is evaded once
// near, otherwise the mummy wanders ("randomworriedwalk" once the curse landed, else "randomwalk").
void update_holding(RuntimeMummyCursedStrategy& context)
{
    RuntimeMindTarget* selected = nullptr;
    select_runtime_strategy_target(selected, 0, goals_of(context)); // 0x00139AA0
    if (engage_target(context)) // 0x000BCBE0
    {
        return;
    }
    const std::span<u8> strategy = strategy_of(context);
    if (!requests_pending(context) && !evade_target(context)) // 0x000BCB80
    {
        body_of(context)[0x328] = 3;
        load_goal(get(strategy, field::curse_landed) != 0 ? context.goal_random_worried_walk : context.goal_random_walk,
            context);
        set(strategy, field::evading, 1u);
        return;
    }
    if (get(strategy, field::evading) != 0)
    {
        evade_target(context); // 0x000BCB80
    }
}

// Xbox 0x000BD450, ESI strategy: the grab's phase changes, ending the one it leaves.
void set_phase(u32 phase, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::phase) == phase)
    {
        return;
    }
    end_phase(context);
    set(strategy, field::phase, phase);
    switch (phase)
    {
    case 1:
    {
        set(strategy, field::unk_14C, 0u);
        set(strategy, field::pulling, 0u);
        set(strategy, field::whacked, 0u);
        const Actor& victim = actor_at(get(strategy, field::victim), context, "0x000BD504");
        if (parameter(context, 8) > runtime_actor_distance(actor_of(context), victim)) // 0x00041490
        {
            load_goal(context.goal_evade_knockdown, context);
            return;
        }
        if (runtime_actor_distance(actor_of(context), victim) > parameter(context, 0xC)) // 0x00041490
        {
            load_goal(context.goal_come_into_range_knockdown, context);
        }
        return;
    }
    case 2:
        load_goal(context.goal_slab_whack_prepare, context);
        return;
    case 3:
        set(strategy, field::phase_timer, phase_three_time(context)); // 0x000BC720
        return;
    case 4:
    {
        set(strategy, field::pulling, 1u);
        const u32 victim = get(strategy, field::victim);
        if (victim != 0)
        {
            set_runtime_body_bit(context.body_bits_at(victim), 0x3D); // 0x0001EFD0, EAX the victim's body
        }
        set(strategy, field::phase_timer, phase_four_time(context)); // 0x000BC6D0
        return;
    }
    case 5:
        set(strategy, field::pulling, 0u);
        if (get(strategy, field::whacked) == 0)
        {
            const std::array<u32, 2> message{0x5B, 0};
            context.object_message(get(strategy, field::victim), message); // The victim's vtable +0xC.
            set(strategy, field::whacked, 1u);
            load_goal(context.goal_slab_whack, context);
        }
        return;
    case 6:
        set(strategy, field::pulling, 0u);
        load_goal(context.goal_scared, context);
        return;
    case 7:
        clear_requests(context);
        load_goal(context.goal_slab_whack_sneaky, context);
        return;
    default:
        return;
    }
}

// Xbox 0x000BD750, EAX strategy: while pulling and the runtime flag 0x20000 is clear, the victim has energy, is not
// bit 3.3 or 4.4 nor body bits 0x25 / 0x1F, and its body timer is idle (0x00024C20).
bool victim_pulled(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::pulling) == 0 || (context.runtime_flags & 0x20000) != 0)
    {
        return false;
    }
    const u32 victim = get(strategy, field::victim);
    const std::span<u8> body = context.body_at(victim);
    if (get(body, body_field::energy) == 0)
    {
        return false;
    }
    const EntityBitBlock& bits = context.body_bits_at(victim);
    if ((bits.bytes[3] & 8) != 0 || (bits.bytes[4] & 0x10) != 0)
    {
        return false;
    }
    if (runtime_body_bit_set(bits, 0x25) || runtime_body_bit_set(bits, 0x1F)) // 0x0001EF10, EAX the body
    {
        return false;
    }
    return runtime_body_timer_is_idle(body); // 0x00024C20
}

// Xbox 0x000BD680, EAX actor, EBX strategy: without a clear path to the victim, or once the phase timer has run out,
// the pull ends; otherwise it ends past the block's +0x34.
bool pull_ends(const Vec3& victim, float distance, RuntimeMummyCursedStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    i32 last_cell = 0;
    if (!navigation_path_clear(actor_world_position(actor), victim, 1, 0, 0, 0, nullptr, &last_cell,
            i16(read_u16(goals.entry.actor_bytes, 6)))) // 0x0012CC50, EAX the actor's position (0x000412A0)
    {
        return true;
    }
    const float step = runtime_frame_step(goals.clock);
    const float rate = runtime_actor_rate(actor);
    const float timer = get(strategy, field::phase_timer);
    if (timer > 0.f)
    {
        const float left = timer - rate * step;
        set(strategy, field::phase_timer, left);
        if (!(0.f < left)) // COMISS; JC keeps a positive time.
        {
            set(strategy, field::phase_timer, 0.f);
            return true;
        }
    }
    return distance > parameter(context, 0x34);
}

// Xbox 0x000BCFB0: the grab's phases.
void update_grab(RuntimeMummyCursedStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const std::span<u8> body = body_of(context);
    {
        const Actor& victim = actor_at(get(strategy, field::victim), context, "0x000BCFDC");
        const float distance = float(runtime_actor_distance(actor, victim)); // 0x00041490
        set_runtime_attachment_effect_reach(strategy.subspan(field::grab_events.offset, 0x24), distance,
            context.lists); // 0x0013B840, EDI +0x158, EBX actor
    }
    if (victim_pulled(context)) // 0x000BD750
    {
        const u32 victim_address = get(strategy, field::victim);
        Actor& victim = actor_at(victim_address, context, "0x000BD006");
        const std::span<u8> victim_body = context.body_at(victim_address);
        const float x = read_f32(victim_body, 0x18);
        const float y = read_f32(victim_body, 0x1C);
        const float z = read_f32(victim_body, 0x20);
        const float pull = -parameter(context, 0x28);
        const float angle = runtime_actor_angle_toward(victim, actor); // 0x00041900, EAX the victim
        const float moved_x = float(retail_sin(angle) * pull + x); // 0x00153320
        const float moved_z = float(retail_cos(angle) * pull + z); // 0x00153230
        std::memcpy(victim_body.data() + 0x18, &moved_x, 4);
        std::memcpy(victim_body.data() + 0x1C, &y, 4);
        std::memcpy(victim_body.data() + 0x20, &moved_z, 4);
    }
    const u32 phase = get(strategy, field::phase);
    const auto face_victim = [&]()
    {
        face_runtime_body_actor(body, actor, actor_at(get(strategy, field::victim), context, "0x001345B0"));
    };
    // LAB 0x000BD28F: to phase 8, ending the current one.
    const auto finish = [&](u32 current)
    {
        if (current == 8)
        {
            return;
        }
        end_phase(context);
        set(strategy, field::phase, 8u);
    };
    switch (phase)
    {
    case 1:
        if (!requests_pending(context))
        {
            set_phase(2, context); // 0x000BD450
        }
        return;
    case 2:
        face_victim(); // 0x001345B0, ESI the victim
        if (!requests_pending(context))
        {
            set_phase(3, context);
        }
        if (get(strategy, field::grab_callout) == 0
            && runtime_body_bit_set(body_bits(context, "0x000BD0FE"), 0x17)) // 0x0001EF10, ECX 0x17
        {
            request_grab_callout(context); // 0x0013B7E0, ECX actor, EDX +0x158
            set(strategy, field::grab_callout, 1u);
        }
        return;
    case 3:
    {
        face_victim(); // 0x001345B0
        float timer = get(strategy, field::phase_timer);
        const bool done = count_down_runtime_strategy_timer(timer, goals); // 0x0002B9F0, ECX +0x148
        set(strategy, field::phase_timer, timer);
        if (done)
        {
            set_phase(4, context);
        }
        return;
    }
    case 4:
    {
        const Actor& victim = actor_at(get(strategy, field::victim), context, "0x000BD167");
        const float distance = float(runtime_actor_distance(actor, victim)); // 0x00041490
        const Vec3 point = actor_world_position(victim); // 0x000412A0
        face_victim(); // 0x001345B0
        if (parameter(context, 0x2C) > distance)
        {
            set_phase(5, context);
            return;
        }
        if (!pull_ends(point, distance, context) || get(strategy, field::phase) == 6) // 0x000BD680
        {
            return;
        }
        end_phase(context);
        set(strategy, field::phase, 6u);
        set(strategy, field::pulling, 0u);
        load_goal(context.goal_scared, context);
        return;
    }
    case 5:
    case 7:
        if (requests_pending(context))
        {
            return;
        }
        finish(phase);
        return;
    case 6:
    {
        const RuntimeMindTarget* target = mind_target(context);
        const long double distance = runtime_target_distance(actor, target == nullptr ? std::span<const u8>()
                                                                                       : target->bytes); // 0x00041560
        if (static_cast<long double>(parameter(context, 0x30)) > distance)
        {
            if (get(strategy, field::phase) == 7)
            {
                return;
            }
            end_phase(context);
            set(strategy, field::phase, 7u);
            clear_requests(context);
            load_goal(context.goal_slab_whack_sneaky, context);
            return;
        }
        if (requests_pending(context))
        {
            return;
        }
        finish(get(strategy, field::phase));
        return;
    }
    case 8:
        if (!requests_pending(context))
        {
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000BCDC0, EAX strategy: the grab starts on the mind target's actor: phase 1 for a player target with a body
// (+0xCE0) that is not marked (+0x14), else phase 5. Without a target the mummy holds (0x401).
void start_grab(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        change_state(0x401, context); // The inlined 0x0002B8D0.
        return;
    }
    set(strategy, field::victim, get(target->bytes, mind_target_field::actor));
    const std::span<const u8> victim = target->actor_bytes;
    const bool player = target->bytes[0x14] == 0 && read_u32(victim, 0xCE0) != 0
        && (read_u16(victim, 0xCDC) == 2 || read_u32(victim, 0x93C) != 0);
    set_phase(player ? 1u : 5u, context); // 0x000BD450
    body_of(context)[0x328] = 5;
    strategy_base(strategy).attacking = 1;
}

// Xbox 0x000BCE80, ESI strategy: the grab ends: its phase, the victim's hold, the rider claim, the queue and the
// callout's attachments.
void end_grab(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::phase) != 0)
    {
        end_phase(context);
        set(strategy, field::phase, 0u);
    }
    release_victim_hold(context); // The inlined 0x000BD650.
    set(strategy, field::whacked, 0u);
    if (get(strategy, field::mount) != 0)
    {
        const std::span<u8> body = context.body_at(get(strategy, field::mount));
        const u32 holder = read_u32(body, 0x718);
        if (holder != 0 && holder == strategy_base(strategy).actor)
        {
            const u32 none = 0;
            std::memcpy(body.data() + 0x718, &none, 4);
            set(strategy, field::riding, 0u);
            set(strategy, field::mount, 0u);
        }
    }
    detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
    strategy_base(strategy).attacking = 0;
    remove_events(field::grab_events.offset, context); // 0x0003CFD0 over +0x15C, EAX actor
    set(strategy, field::grab_callout, 0u);
}

// Xbox 0x000BD8B0, ECX strategy, EAX step: the curse's steps. Step 1 takes the mind target's actor as the cursed
// player (else holds) and closes in or backs off by the block's +0x10 / +0x14; step 2 casts ("deathcurse", callout
// 0x42 into +0x2A4 and the object named at +0xBC).
void set_curse_step(u32 step, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (step == get(strategy, field::curse_step))
    {
        return;
    }
    set(strategy, field::curse_step, step);
    const u32 address = actor_address(context);
    if (step == 1)
    {
        const RuntimeMindTarget* target = mind_target(context);
        if (target == nullptr)
        {
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
            return;
        }
        set(strategy, field::cursed, get(target->bytes, mind_target_field::actor));
        if (get(target->actor_bytes, player_field::mode) != 2)
        {
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
            set(strategy, field::cursed, 0u);
            return;
        }
        const Actor& cursed = actor_at(get(strategy, field::cursed), context, "0x000BD99F");
        if (parameter(context, 0x10) > runtime_actor_distance(actor_of(context), cursed)) // 0x00041490
        {
            load_goal(context.goal_evade_special, context);
            return;
        }
        if (runtime_actor_distance(actor_of(context), cursed) > parameter(context, 0x14)) // 0x00041490
        {
            load_goal(context.goal_come_into_range_special, context);
        }
        return;
    }
    if (step != 2)
    {
        return;
    }
    load_goal(context.goal_death_curse, context);
    dispatch_callouts(0x42, field::curse_events.offset, context); // 0x0003BE20, mind +8 -> +0x8C
    set(strategy, field::deathcurse, context.create_effect(name_at(context, field::deathcurse_name.offset),
        address)); // 0x00107720, 0x00107520, 0x000663E0
}

// Xbox 0x000BD830, ECX strategy, EDX actor: the curse runs: step 1 casts once the requests are done, step 2 faces
// the cursed player until the body bit block byte 3 bit 1, step 3 holds (0x401) once the requests are done.
void update_curse(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (get(strategy, field::curse_step))
    {
    case 1:
        if (!requests_pending(context))
        {
            set_curse_step(2, context); // The tail jump to 0x000BD8B0, EAX 2.
        }
        return;
    case 2:
        if ((body_bits(context, "0x000BD85D").bytes[3] & 1) != 0)
        {
            set(strategy, field::curse_step, 3u);
            return;
        }
        face_runtime_body_actor(body_of(context), actor_of(context),
            actor_at(get(strategy, field::cursed), context, "0x000BD880")); // 0x001345B0, ESI +0x188
        return;
    case 3:
        if (!requests_pending(context))
        {
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000BD7D0, ESI strategy: the curse's state ends: the queue, the attack byte, callout 0x42's attachments and
// the step.
void end_curse_state(RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20
    strategy_base(strategy).attacking = 0;
    remove_events(field::curse_events.offset, context); // 0x0003CFD0 over +0x2A8
    if (get(strategy, field::curse_step) != 0)
    {
        set(strategy, field::curse_step, 0u);
    }
}
}

// Xbox 0x0013B840, EDI block, EBX actor, XMM0 distance
void set_runtime_attachment_effect_reach(std::span<const u8> events, float distance, RuntimeBodyAttachmentLists& lists)
{
    const i32 count = std::bit_cast<i32>(read_u32(events, 0));
    for (i32 index = 0; index < count; ++index)
    {
        const u16 token = u16(read_u32(events, size_t(index + 1) * 4));
        for (ActorAttachmentNode* node = lists.second; node != nullptr; node = node->next) // Actor +0x1B0.
        {
            if (read_u16(node->bytes, 0x18) != token)
            {
                continue;
            }
            ActorEffectNode* effect = lists.timed[read_u32(node->bytes, 8)]; // DAT_00506930.
            if (effect != nullptr)
            {
                const float reach = distance == kMinusOne ? kMinusOne : distance * distance; // UCOMISS, DAT_0032B120
                std::memcpy(effect->bytes.data() + 0xBC, &reach, 4);
            }
            break;
        }
    }
}

// Xbox 0x000BDA00
void mummy_cursed_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x218;
}

// Xbox 0x000BDA70
bool initialize_runtime_mummy_cursed_strategy(std::span<const u8> parameters, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    set(strategy, field::parameters, context.parameters_address);
    set(strategy, field::curse_landed, 0u);
    set(strategy, field::cursed, 0u);
    set(strategy, field::riding, 0u);
    set(strategy, field::mount, 0u);
    set(strategy, field::victim, 0u);
    if (get(strategy, field::curse_form) != 0)
    {
        set(strategy, field::curse_form, 0u);
    }
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
    copy(field::curse_effect_name.offset, 0x98);
    copy(field::deathcurse_name.offset, 0x18);
    copy(field::burst_name.offset, 0x118);
    copy(field::spawn_name.offset, 0x198);
    return true;
}

// Xbox 0x000BDB30
void destroy_runtime_mummy_cursed_strategy(RuntimeMummyCursedStrategy& context)
{
    release_mount(context); // The inlined 0x000BE070.
    end_curse(strategy_base(strategy_of(context)).actor, context); // 0x000BC770
}

// Xbox 0x000BDB80
void update_runtime_mummy_cursed_strategy(RuntimeMummyCursedStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    check_curse(context); // 0x000BC8A0, ECX actor, EDX strategy
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3EE:
        update_grab(context); // 0x000BCFB0
        return;
    case 0x3EF:
        update_curse(context); // The tail jump to 0x000BD830.
        return;
    case 0x401:
        update_holding(context); // The tail jump to 0x000BCAE0.
        return;
    default:
        return;
    }
}

// Xbox 0x000BDBE0
void dispatch_runtime_mummy_cursed_message(std::span<const u32> message, RuntimeMummyCursedStrategy& context)
{
    RuntimeSpiderStrategy& shared = context.shared;
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
    switch (message[0])
    {
    case 0x2E:
        if (message[1] == actor)
        {
            const u32 cursed = get(strategy, field::cursed);
            const u16 energy = read_u16(context.body_at(cursed), 0x94); // The cursed player's body +0x94.
            const std::array<u32, 3> curse{0x2F, cursed, actor};
            context.object_message(cursed, curse); // The cursed player's vtable +0xC.
            notify_runtime_body_damage(context.game, cursed, actor, energy, 0, 0, 0, 0, 0); // 0x00024A00
            end_curse(actor, context); // 0x000BC770
        }
        return;
    case 0x31:
        if (react_runtime_strategy_watched_actor(message, shared.checks, shared.views) != 0) // 0x001397B0
        {
            release_mount(context); // 0x000BE070
            return;
        }
        if (message[1] != actor)
        {
            return;
        }
        release_mount(context); // 0x000BE070
        end_curse(actor, context); // 0x000BC770
        if (get(strategy, field::curse_landed) != 0)
        {
            const std::array<u32, 2> lifted{0x2C, 0};
            context.object_message(get(strategy, field::cursed), lifted); // The cursed player's vtable +0xC.
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x17, shared.checks); // 0x0013A4F0
        return;
    case 0x49:
        if (message[6] == get(strategy, field::cursed)
            && read_u32(context.actor_bytes_at(message[1]), 0xD8C) == actor)
        {
            land_curse(actor, context); // 0x000BC870
        }
        return;
    case 0x5F:
    {
        const u32 record = context.shared.views.first_live_attachment_at(get(strategy, field::cursed)); // Actor +0x8F8.
        set(strategy, field::curse_landed, 1u);
        std::memcpy(context.actor_bytes_at(message[1]).data() + 0xE50, &record, 4);
        return;
    }
    case 0x6E:
    {
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            return;
        }
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_contact(message, kHalf, shared.checks); // 0x001395C0, XMM1 DAT_0032B0F4
        }
        if (get(strategy, field::riding) == 0
            && !runtime_body_bit_set(body_bits(context, "0x000BDD34"), 5)) // 0x0001EF10, ECX 5
        {
            retaliate_runtime_strategy_contact(message, -1, shared.checks, shared.views); // 0x00139390
        }
        if (message[1] != actor)
        {
            if (runtime_message_from_mind_target(message, actor, mind_of(context), goals.target_at)) // 0x001408E0
            {
                react_runtime_strategy_contact(message, kOne, shared.checks); // 0x001395C0, XMM1 DAT_0032B0F8
                set(strategy, field::unk_14C, 1u);
            }
            return;
        }
        if (strategy_base(strategy).pause_count != 0 || !decay_running(context))
        {
            return;
        }
        // The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
        const RuntimePlayerGoalRing* ring = goals.entry.ring;
        if (ring != nullptr
            && (ring->tail == ring->capacity ? read_u16(goals.entry.address_zero, 0)
                                             : read_u16(ring->rows, size_t(ring->stride) * ring->head))
                != 0)
        {
            return;
        }
        change_state(0x401, context);
        return;
    }
    case 0x7C:
        if (message[1] == get(strategy, field::victim))
        {
            set(strategy, field::victim, 0u);
            release_mount(context); // 0x000BE070
        }
        if (message[1] == get(strategy, field::cursed))
        {
            set(strategy, field::cursed, 0u);
        }
        return;
    case 0x80:
        release_mount(context); // 0x000BE070
        end_curse(actor, context); // 0x000BC770
        return;
    case 0x82:
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_cower(shared.checks); // 0x001398A0
        }
        return;
    case 0x84:
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_startle(shared.checks); // 0x001399A0
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000BDF90
void enter_runtime_mummy_cursed_state(u16 state, RuntimeMummyCursedStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    body_of(context)[0x328] = 2;
    switch (state)
    {
    case 0x3EE:
        start_grab(context); // The tail jump to 0x000BCDC0.
        return;
    case 0x3EF:
        set_curse_step(1, context); // 0x000BD8B0, EAX 1
        body_of(context)[0x328] = 6;
        strategy_base(strategy).attacking = 1;
        return;
    case 0x401:
        set(strategy, field::evading, 0u);
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000BE010
void leave_runtime_mummy_cursed_state(u16 state, RuntimeMummyCursedStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EE:
        end_grab(context); // 0x000BCE80
        return;
    case 0x3EF:
        end_curse_state(context); // 0x000BD7D0
        return;
    case 0x401:
        set(strategy, field::evading, 0u);
        return;
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
