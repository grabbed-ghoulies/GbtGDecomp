#include "haunted_picture_strategy.h"
#include "body_fields.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_live_attachments.h"
#include "actor_transform.h"
#include "imp_fire_strategy.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../navigation/mind_navigation_angles.h"
#include "../navigation/player_avoidance.h"
#include "../scripts/script_vm.h"
#include "gbtg/anim.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
HauntedPictureStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<HauntedPictureStrategyState>(strategy);
}

const HauntedPictureStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<HauntedPictureStrategyState>(strategy);
}

namespace parameter = haunted_picture_parameter;

constexpr u32 kMovementAddress = 0x00302E80; // The movement table B0680 passes.

RuntimePlayerStrategyGoals& goals_of(RuntimeHauntedPictureStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeHauntedPictureStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of(RuntimeHauntedPictureStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeHauntedPictureStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeHauntedPictureStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0 (and the same sequence through the vtable in 0x000B1350).
void change_state(u16 state, RuntimeHauntedPictureStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeHauntedPictureStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimeMindTarget* select_target(bool& found, RuntimeHauntedPictureStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

const RuntimeMindTarget& require_target(const RuntimeMindTarget* target, const char* reader)
{
    if (target == nullptr)
    {
        throw Error(std::string(reader) + " reads the haunted picture's target record through address zero");
    }
    return *target;
}

// Each of the actor's live attachment records (+0x8F8) holds `step` more (0x00042090 / 0x000420B0).
void adjust_live_holds(i32 step, RuntimeHauntedPictureStrategy& context)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

// The wait before the next attack: 0.45 to 0.55 s off DAT_0054DD7C.
float roll_attack_wait(RuntimeHauntedPictureStrategy& context)
{
    u32& random = goals_of(context).random;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    return unit * std::bit_cast<float>(0x3E4CCCCEu) * .5f + std::bit_cast<float>(0x3EE66666u); // DAT_0032B5D4,
                                                                                                   // B0F4, B4F8.
}

// The phase steps on by the actor's step, wrapped at 2.2 (DAT_0032B5A4).
float step_phase(RuntimeHauntedPictureStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    float phase = runtime_actor_step(actor_of(context), goals_of(context).clock) + fields_of(strategy).phase;
    fields_of(strategy).phase = phase;
    const float period = std::bit_cast<float>(0x400CCCCDu);
    if (period < phase)
    {
        do
        {
            phase -= period;
        } while (period < phase);
        fields_of(strategy).phase = phase;
    }
    return fields_of(strategy).phase;
}

// value held within [0, 2 pi] (DAT_0032B298).
float clamp_angle(float value)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    if (value > tau)
    {
        return tau;
    }
    float angle = 0.f;
    if (!(0.f > value))
    {
        angle = value;
    }
    return angle;
}

// Xbox 0x000B0AC0, EBX target, ESI actor: how far the target stands between the table's near (+0x1C) and far
// (+0x20) rings, held to [0, 1].
float runtime_haunted_picture_range_fraction(std::span<const u8> target, RuntimeHauntedPictureStrategy& context)
{
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const float dx = position.x - mind_target_record(target).shape.position.x;
    const float dy = position.y - mind_target_record(target).shape.position.y;
    const float dz = position.z - mind_target_record(target).shape.position.z;
    const float nearest = read_f32(context.movement, 0x1C);
    const float farthest = read_f32(context.movement, 0x20);
    if (farthest == nearest)
    {
        return 1.f; // DAT_0032B0F8.
    }
    const long double distance = std::sqrt((static_cast<long double>(dz) * dz + static_cast<long double>(dx) * dx)
        + static_cast<long double>(dy) * dy);
    const float fraction = float((distance - nearest) / (static_cast<long double>(farthest) - nearest));
    if (fraction > 1.f)
    {
        return 1.f;
    }
    if (0.f > fraction)
    {
        return 0.f;
    }
    return fraction;
}

// Xbox 0x000B09C0, EAX target, ECX actor: a heading toward or past the target by the range fraction (0x000B40E0),
// the nearer of it and its reflection to the actor's heading, spread by the table's +0x24 degrees.
float select_runtime_haunted_picture_heading(std::span<const u8> target, RuntimeHauntedPictureStrategy& context)
{
    const Actor& actor = actor_of(context);
    const float fraction = runtime_haunted_picture_range_fraction(target, context); // 0x000B0AC0
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 direction{mind_target_record(target).shape.position.x - position.x,
        mind_target_record(target).shape.position.y - position.y,
        mind_target_record(target).shape.position.z - position.z};
    // The output reuses the caller's argument slot, which holds the movement table's address.
    float angle = std::bit_cast<float>(kMovementAddress);
    runtime_direction_angle(direction, angle); // 0x00105F90
    const float heading = blend_runtime_imp_fire_heading(fraction, angle); // 0x000B40E0
    const float reflected = reflect_runtime_angle(angle, heading); // 0x00100D00
    const float chosen = nearest_runtime_angle(actor.euler.y, heading, reflected); // 0x00100B80, actor +0x18
    float scale;
    if (fraction > .5f) // DAT_0032B0F4.
    {
        scale = (1.f - fraction) * 2.f; // DAT_0032B110.
    }
    else
    {
        scale = fraction * 2.f;
    }
    const float width = read_f32(context.movement, 0x24) * scale * std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    return randomize_runtime_navigation_heading(chosen, width, goals_of(context).random); // 0x00100C70
}

// Xbox 0x000B0890: a free point on the table's ring (+8, within 0.5) along the chosen heading, moved to with a sway
// of sin(phase / +0x2C) times +0x30 (0x0013A000).
void circle_runtime_haunted_picture_target(const RuntimeMindTarget& target, float phase,
    RuntimeHauntedPictureStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> movement = context.movement;
    const float heading = select_runtime_haunted_picture_heading(target.bytes, context); // 0x000B09C0
    const Vec3 origin = actor_world_position(actor_of(context)); // 0x000412A0
    const float ring = read_f32(movement, 8);
    Vec3 point{};
    RuntimeNavigationHeadingSearch search{origin, i32(mind_target_record(target.bytes).floor), ring - .5f, ring
        + .5f, nullptr,
        goals.entry.allowed, &point, 0, 0x1C, i16(read_u16(goals.entry.actor_bytes, 6)), goals.random,
        context.occupancy_initial};
    if (!search_runtime_navigation_headings(search, heading, std::bit_cast<float>(0x3E9C61AAu), 10)) // 0x0013AA50
    {
        return;
    }
    const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const float period = read_f32(movement, 0x2C);
    const float angle = period == 0.f ? tau : clamp_angle(phase / period * tau);
    const float amplitude = read_f32(movement, 0x30);
    const float sway = float(retail_sin(angle) * static_cast<long double>(amplitude)); // 0x00153320
    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
    request_runtime_strategy_sway(point, movement.subspan(4), sway, amplitude, goals); // 0x0013A000, ESI table +4
}

// Xbox 0x000B0680, EAX strategy
void approach_runtime_haunted_picture_target(const RuntimeMindTarget& target, RuntimeHauntedPictureStrategy& context)
{
    const float phase = step_phase(context);
    circle_runtime_haunted_picture_target(target, phase, context); // 0x000B0890
}

// Xbox 0x000B0720, EAX mind, ESI strategy: with no request pending it asks for a random destination; it bobs
// (request 0xF) by sin(phase / 2.2) times 8.
void wander_runtime_haunted_picture(RuntimeHauntedPictureStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const float phase = step_phase(context);
    const bool pending = requests_pending(context); // Mind +0x58, read before the clear.
    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80
    if (!pending)
    {
        load_goal(context.goal_random_dest, context);
    }
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    const float angle = clamp_angle(phase * std::bit_cast<float>(0x3EE8BA2Eu) * tau); // DAT_0032B5A0.
    const float lift = float(retail_sin(angle) * 8.0L); // 0x00153320, DAT_0032B26C
    RuntimeMindGoalContext& mind = goals.entry.goals;
    RuntimePlayerMindRequest& bob = *allocate_runtime_mind_request(mind.anchor, 0xF, mind.sizes); // 0x00029D90
    set(bob.bytes, vertical_position_request::approach, 30.f); // DAT_0032B108.
    set(bob.bytes, vertical_position_request::speed_low, 1.f); // DAT_0032B0F8.
    set(bob.bytes, vertical_position_request::speed_high, 1.f);
    set(bob.bytes, vertical_position_request::tolerance, 1.f);
    set(bob.bytes, vertical_position_request::height, 10.f); // DAT_0032B10C.
    set(bob.bytes, vertical_position_request::vertical_speed, .25f); // DAT_0032B104.
    set(bob.bytes, vertical_position_request::speed_low, 1.f);
    set(bob.bytes, vertical_position_request::speed_high, 1.f);
    set(bob.bytes, vertical_position_request::radius, 10.f);
    set(bob.bytes, vertical_position_request::height, lift);
    set(bob.bytes, vertical_position_request::approach, 0.f);
    set(bob.bytes, vertical_position_request::tolerance, 8.f); // DAT_0032B26C.
    set(bob.bytes, vertical_position_request::vertical_speed, .25f);
}

// Xbox 0x000B1320, EAX strategy, EDI target: while the attack wait runs the picture leaves its target's queue,
// else it joins it (0x0002B910).
bool runtime_haunted_picture_attack_ready(RuntimeMindTarget* target, RuntimeHauntedPictureStrategy& context)
{
    if (fields_of(strategy_of(context)).attack_wait > 0.f) // DAT_0032B100.
    {
        detach_runtime_strategy_link(context.base.link); // 0x00112A20, EAX strategy +0x20
        return false;
    }
    return join_runtime_strategy_target_queue(context.base.link, target == nullptr ? nullptr : target->record,
        &actor_of(context), context.queue); // 0x0002B910
}

// Xbox 0x000B1350, ESI strategy: "attack_special" (0x3EF) when the aggression (actor +0x964 +8) beats a roll, else
// "attack" (0x3ED).
void choose_runtime_haunted_picture_attack(RuntimeHauntedPictureStrategy& context)
{
    const std::span<const u8> attributes = context.checks.attributes;
    const float aggression = attributes.data() == nullptr ? 0.f : read_f32(attributes, 8);
    u32& random = goals_of(context).random;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    change_state(aggression >= roll ? 0x3EF : 0x3ED, context); // Vtable +0x18, the state, 0x00029E80, +0x14.
}

// Xbox 0x000B1420, ECX strategy, EAX body: the picture wakes once. The live holds message 0x7D raised are
// dropped, it resumes in 0x401 and the script hears {0xB4, the actor's tag}.
void wake_runtime_haunted_picture(RuntimeHauntedPictureStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).awake != 0)
    {
        return;
    }
    fields_of(strategy).awake = 1u;
    if (fields_of(strategy).latched != 0)
    {
        adjust_live_holds(-1, context); // The inlined 0x000420B0.
        fields_of(strategy).latched = 0u;
    }
    strategy_base(strategy).unk_18 = 0x401;
    strategy_base(strategy).unk_1B = 1;
    const std::array<u32, 2> event{0xB4, read_u16(goals.entry.actor_bytes, 6)};
    script_queue_event(goals.game, {reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810, EDI
    restore_runtime_player_script_overrides(context.self, context.overrides); // 0x00138030, ESI actor
}
}

// Xbox 0x000B0B90
void haunted_picture_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(HauntedPictureStrategyState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000B0C00
void update_runtime_haunted_picture_strategy(RuntimeHauntedPictureStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    const float wait = fields_of(strategy).attack_wait;
    if (wait > 0.f)
    {
        const float left = wait - runtime_actor_step(actor_of(context), goals.clock);
        fields_of(strategy).attack_wait = left;
        if (0.f >= left)
        {
            fields_of(strategy).attack_wait = 0.f;
        }
    }
    bool found = false;
    switch (state)
    {
    case 0x3ED:
    case 0x3EF:
        select_target(found, context);
        if (found && !requests_pending(context))
        {
            change_state(0x40C, context);
        }
        return;
    case 0x401:
    {
        wander_runtime_haunted_picture(context); // 0x000B0720
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        if (runtime_strategy_abandons_target(require_target(target, "0x000B0E02"), context.checks) != 0) // 0x00138410
        {
            retreat_runtime_strategy_from_reached_target(context.checks); // The tail jump to 0x001383A0.
            return;
        }
        change_state(0x40C, context);
        return;
    }
    case 0x40C:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            return;
        }
        approach_runtime_haunted_picture_target(require_target(target, "0x000B0680"), context); // 0x000B0680
        if (runtime_haunted_picture_attack_ready(target, context)) // 0x000B1320
        {
            choose_runtime_haunted_picture_attack(context); // 0x000B1350
        }
        if (fields_of(strategy).attack_wait == 0.f) // DAT_0032B100.
        {
            fields_of(strategy).attack_wait = roll_attack_wait(context);
        }
        return;
    }
    case 0x413:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (target == nullptr)
        {
            return;
        }
        const float distance = runtime_squared_target_distance(actor_of(context), target->bytes); // 0x000415A0
        if (2500.f >= distance || read_u16(goals.entry.actor_bytes, 0x1FC) == 0) // DAT_0032B40C.
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
        return;
    }
    case 0x417:
        select_target(found, context);
        if (requests_pending(context))
        {
            return;
        }
        change_state(runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0 ? 0x401
                                                                                                          : 0x3E8,
            context); // 0x0002B7F0
        return;
    default:
        return;
    }
}

// Xbox 0x000B0EA0
void dispatch_runtime_haunted_picture_message(std::span<const u32> message, RuntimeHauntedPictureStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (react_runtime_strategy_message(message, context.checks) != 0) // 0x001384F0, EAX strategy, ECX message
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
    const u32 actor = strategy_base(strategy).actor;
    // LAB 0x000B105E: a target found wakes it.
    const auto wake_on = [&](bool woken)
    {
        if (woken)
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
    };
    // LAB 0x000B100F.
    const auto fight_back = [&]()
    {
        if (retaliate_runtime_strategy_contact(message, -1, context.checks, context.views) == 0) // 0x00139390
        {
            react_runtime_strategy_contact(message, -1.f, context.checks); // 0x001395C0, DAT_0032B120
        }
    };
    switch (message[0])
    {
    case 1:
    {
        const u16 state = strategy_base(strategy).state;
        if (state == 0x3E9)
        {
            if (fields_of(strategy).awake != 0)
            {
                return;
            }
        }
        else if (state != 0x413)
        {
            dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
            return;
        }
        RuntimeMindTarget* target = nullptr;
        wake_on(select_runtime_strategy_hostile_target(target, 0, goals)); // 0x00139C70, EDX 0
        return;
    }
    case 0x26:
    case 0x27:
        if (strategy_base(strategy).state == 0x413 || strategy_base(strategy).state == 0x417)
        {
            return;
        }
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    case 0x30:
        wake_runtime_haunted_picture(context); // 0x000B1420
        return;
    case 0x31:
        if (fields_of(strategy).awake != 0)
        {
            react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0xD, context.checks); // 0x0013A4F0, ESI strategy
        return;
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        if (message[1] == actor && fields_of(strategy).awake == 0 && strategy_base(strategy).state == 0x413)
        {
            const std::span<const u8> mind = goals.entry.events.mind; // Actor +0x958.
            if (mind_record(mind).target_record != 0)
            {
                goals.room = nullptr; // Actor +0xCA0.
            }
            wake_runtime_haunted_picture(context); // 0x000B1420
            change_state(runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0 ? 0x401
                                                                                                              : 0x3E8,
                context); // 0x0002B7F0
            return;
        }
        if (fields_of(strategy).awake == 0)
        {
            return;
        }
        fight_back();
        return;
    case 0x70:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (strategy_base(strategy).state != 0x413)
        {
            return;
        }
        wake_on(adopt_runtime_strategy_message_attacker(message, 1, context.checks, context.views) != 0); // 0x00139550
        return;
    case 0x7D:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        adjust_live_holds(1, context); // 0x00042090, EAX actor
        fields_of(strategy).latched = 1u;
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B1150
void enter_runtime_haunted_picture_state(u16 state, u16 previous, RuntimeHauntedPictureStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    body_record(body).move_class = 2;
    strategy_base(strategy).attacking = 0;
    strategy_base(strategy).unk_1B = 0;
    switch (state)
    {
    case 0x3ED:
        body_record(body).move_class = 4;
        strategy_base(strategy).attacking = 1;
        load_goal(context.goal_attack, context);
        return;
    case 0x3EF:
        body_record(body).move_class = 6;
        strategy_base(strategy).attacking = 1;
        load_goal(context.goal_attack_special, context);
        return;
    case 0x401:
        return;
    case 0x40C:
        body_record(body).move_class = 3;
        fields_of(strategy).attack_wait = roll_attack_wait(context);
        return;
    case 0x413:
        load_goal(context.goal_sleep, context);
        return;
    case 0x417:
        wake_runtime_haunted_picture(context); // 0x000B1420
        load_goal(context.goal_wakeup, context);
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000B12D0
void leave_runtime_haunted_picture_state(u16 state, RuntimeHauntedPictureStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
    case 0x3EF:
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20, EAX strategy +0x20.
        return;
    case 0x417:
        wake_runtime_haunted_picture(context); // The tail jump to 0x000B1420.
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
