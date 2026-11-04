#include "vampire_strategy.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "body_message_helpers.h"
#include "entity.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_targets.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../graphics/actor_effect_stream.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
namespace
{
namespace field = vampire_strategy_field;

constexpr u32 kRecordAddress = 0x003034D0;
// The record at 0x003034D0: +0 the dot past which a hit lands from behind, +4 the knocked walk's spread in degrees,
// +8 the coffin's attack delay, +0xC the open time, +0x10 the cheer chance, +0x14 the follow-up chance.
constexpr float kBehindDot = 0.4f; // 0x003034D0 +0.
constexpr float kKnockedSpread = 30.f; // 0x003034D0 +4.
constexpr float kAttackDelay = 1.f; // 0x003034D0 +8.
constexpr float kOpenTime = 5.f; // 0x003034D0 +0xC.
constexpr float kCheerChance = 1.f; // 0x003034D0 +0x10.
constexpr float kFollowupChance = 0.5f; // 0x003034D0 +0x14.
constexpr float kEpsilon = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4, 0.001.

RuntimePlayerStrategyGoals& goals_of(RuntimeVampireStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeVampireStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x95C.
std::span<u8> body_of_actor(RuntimeVampireStrategy& context)
{
    return goals_of(context).entry.body;
}

EntityBitBlock& body_bits(RuntimeVampireStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string("vampire strategy ") + caller + " reads a body with no bit block");
    }
    return *bits;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeVampireStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeVampireStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

RuntimeMindTarget* selected_target(RuntimeVampireStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeVampireStrategy& context)
{
    const u32 target = get(goals_of(context).entry.events.mind, mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

void load_goal(const char* goal, RuntimeVampireStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

float roll(RuntimeVampireStrategy& context)
{
    u32& random = goals_of(context).random;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// Actor +0x958 is set.
bool mind_present(RuntimeVampireStrategy& context)
{
    return !goals_of(context).entry.events.mind.empty();
}

// A timer run down by the actor's rate and the frame step, held at zero once it is spent.
template <u32 Offset>
void run_down(RuntimeVampireStrategy& context, const RetailField<float, Offset>& timer)
{
    const std::span<u8> strategy = strategy_of(context);
    const float left = get(strategy, timer);
    if (0.f < left)
    {
        const float next = left - runtime_actor_rate(goals_of(context).entry.actor)
            * runtime_frame_step(goals_of(context).clock);
        set(strategy, timer, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive timer.
        {
            set(strategy, timer, 0.f);
        }
    }
}

// Xbox 0x000C1EE0, EDX actor: the open coffin closes with no target, the target held by another, or the open
// time spent.
bool open_ends(RuntimeVampireStrategy& context)
{
    if (!mind_present(context))
    {
        return true;
    }
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return true;
    }
    if (target->bytes[0x14] == 0)
    {
        const u32 holder = read_u32(context.views.body_of(target->actor).bytes, 0x718);
        if (holder != 0 && holder != strategy_base(strategy_of(context)).actor)
        {
            return true;
        }
    }
    return !(kEpsilon <= get(strategy_of(context), field::open_time)); // COMISS; JBE returns 0.
}

// Xbox 0x000C1CE0, EAX strategy, ECX stage
void set_knockdown_stage(u32 stage, RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::knockdown_stage) == stage)
    {
        return;
    }
    set(strategy, field::knockdown_stage, stage);
    if (stage == 1)
    {
        load_goal(context.goal_knockdown, context);
    }
    else if (stage == 2)
    {
        const float draw = roll(context);
        load_goal(kCheerChance < draw ? context.goal_close : context.goal_open_cheer, context); // COMISS; JC.
    }
}

// The knocked stages of 0x000C1940 / 0x000C1920 (the stage switch inlined at 0x000C19C0).
void set_knocked_stage(u32 stage, RuntimeVampireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::knocked_stage) == stage)
    {
        return;
    }
    set(strategy, field::knocked_stage, stage);
    if (stage == 1)
    {
        context.knock_back(); // 0x00020690, actor +0x95C
        return;
    }
    if (stage == 3)
    {
        load_goal(context.goal_look, context);
        return;
    }
    if (stage != 2)
    {
        return;
    }
    // Walk 100 units off, away from the way he faces, turned by up to the record's spread either side.
    const Vec3 position = actor_world_position(goals.entry.actor); // 0x000412A0
    const float yaw = actor_world_euler(goals.entry.actor).y; // 0x00041820
    const float low = 0.f - kKnockedSpread;
    const float heading = ((kKnockedSpread - low) * roll(context) + low)
        * std::bit_cast<float>(0x3C8EFA35u) + yaw + std::bit_cast<float>(0x40490FDBu); // DAT_0032B0FC, DAT_0032B148
    Vec3 direction;
    runtime_horizontal_direction(direction, heading, 100.f); // 0x001060A0
    const Vec3 point{position.x + direction.x, position.y + direction.y, position.z + direction.z};
    RuntimeMindGoalContext& mind = goals.entry.goals;
    RuntimePlayerMindRequest* walk = allocate_runtime_mind_request(mind.anchor, 0x26, mind.sizes); // 0x00029D90
    const float speed = 5.f; // DAT_0032B184.
    std::memcpy(walk->bytes.data() + 0x1C, &speed, 4);
    std::memcpy(walk->bytes.data() + 0x10, &point, 12);
}

// Xbox 0x000C1B70, EAX actor, ECX strategy
void update_knockback(RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    bool found = false;
    selected_target(context, found); // Its result is not read.
    switch (get(strategy, field::knockback_stage))
    {
    case 1:
        if (requests_pending(context))
        {
            return;
        }
        if (get(strategy, field::struck) != 0 && actor_event_chance(goals_of(context).random, kFollowupChance))
        {
            // 0x00019200, XMM1 the record's +0x14.
            if (get(strategy, field::knockback_stage) != 2)
            {
                set(strategy, field::knockback_stage, 2u);
                load_goal(context.goal_knockback_followup, context);
            }
            return;
        }
        if (get(strategy, field::knockback_stage) == 3)
        {
            return;
        }
        set(strategy, field::knockback_stage, 3u);
        load_goal(context.goal_close_knockback, context);
        return;
    case 2:
        if (requests_pending(context))
        {
            return;
        }
        set(strategy, field::knockback_stage, 3u);
        load_goal(context.goal_close_knockback, context);
        return;
    case 3:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000C1C80, EAX actor, ECX strategy
void update_knockdown(RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    bool found = false;
    selected_target(context, found); // Its result is not read.
    const u32 stage = get(strategy, field::knockdown_stage);
    if (stage == 1)
    {
        if (!requests_pending(context))
        {
            set_knockdown_stage(2, context); // 0x000C1CE0
        }
    }
    else if (stage == 2 && !requests_pending(context))
    {
        change_state(0x401, context);
    }
}

// Xbox 0x000C1DC0, EAX actor, ECX strategy
void update_open(RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    bool found = false;
    selected_target(context, found); // Its result is not read.
    const u32 stage = get(strategy, field::open_stage);
    if (stage == 1)
    {
        run_down(context, field::open_time);
        if ((body_bits(context, "0x000C1DC0").bytes[3] & 4) != 0)
        {
            if (get(strategy, field::open_stage) != 2)
            {
                set(strategy, field::open_stage, 2u);
            }
            return;
        }
        if (open_ends(context) && get(strategy, field::open_stage) != 3) // 0x000C1EE0
        {
            set(strategy, field::open_stage, 3u);
            clear_runtime_player_mind_requests(*goals_of(context).setup.mind); // 0x00029E80
            load_goal(context.goal_close, context);
        }
        return;
    }
    if (stage > 1 && stage <= 3 && !requests_pending(context))
    {
        change_state(0x401, context);
    }
}

// Xbox 0x000C1940, ECX strategy, EDX actor
void update_knocked(RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (get(strategy, field::knocked_stage))
    {
    case 1:
    {
        const std::span<const u8> body = body_of_actor(context);
        if (get(body, body_field::move_holds) == 0 &&
            get(body, body_field::move_hold_time) == 0.f) // DAT_0032B100; LAHF; JP.
        {
            set_knocked_stage(2, context);
        }
        return;
    }
    case 2:
        if (!requests_pending(context))
        {
            set(strategy, field::knocked_stage, 3u);
            load_goal(context.goal_look, context);
        }
        return;
    case 3:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000C1530, EBX strategy, stack actor
void update_waiting(RuntimeVampireStrategy& context)
{
    bool found = false;
    RuntimeMindTarget* target = selected_target(context, found);
    if (found)
    {
        if (target == nullptr)
        {
            throw Error("vampire strategy 0x000C1530 checks a null target record");
        }
        if (runtime_strategy_abandons_target(*target, context.checks) != 0 // 0x00138410
            && retreat_runtime_strategy_from_reached_target(context.checks) != 0) // 0x001383A0
        {
            return;
        }
    }
    run_down(context, field::delay);
    if (runtime_vampire_reaches_target(context)) // 0x000C17C0
    {
        choose_runtime_vampire_attack(context); // 0x000C1890
    }
    if (!requests_pending(context))
    {
        load_goal(context.goal_wander, context);
        return;
    }
    if ((body_bits(context, "0x000C1530").bytes[2] & 0x20) != 0 && runtime_vampire_hit_from_behind(context))
    {
        change_state(0x406, context);
    }
}
}

// Xbox 0x000C1FE0
void vampire_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0x18;
}

// Xbox 0x000C2050
bool initialize_runtime_vampire_strategy(std::span<u8> strategy)
{
    set(strategy, field::record, kRecordAddress);
    return true;
}

// Xbox 0x000C17C0, EAX strategy
bool runtime_vampire_reaches_target(RuntimeVampireStrategy& context)
{
    if (get(strategy_of(context), field::delay) > kEpsilon)
    {
        return false;
    }
    if (!mind_present(context))
    {
        return false;
    }
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    if (runtime_strategy_abandons_target(*target, context.checks) != 0) // 0x00138410
    {
        return false;
    }
    const RuntimeStrategyBodyView body = context.views.body_of(target->actor); // Target +0x10 actor's +0x95C.
    if (read_u16(body.bytes, 0x94) == 0)
    {
        return false;
    }
    if (body.bits == nullptr)
    {
        throw Error("0x000C17C0 reads the bit block of a target with no body");
    }
    if ((body.bits->bytes[4] & 0x10) != 0)
    {
        return false;
    }
    if (runtime_body_bit_set(*body.bits, 0x25) || runtime_body_bit_set(*body.bits, 0x1F)
        || runtime_body_bit_set(*body.bits, 0x1B)) // 0x0001EF10
    {
        return false;
    }
    if (!join_runtime_strategy_target_queue(context.base.link, target->record, &goals_of(context).entry.actor,
            context.queue)) // 0x0002B910
    {
        return false;
    }
    // 0x001381C0 / 0x001381F0: the target is held (+0x14 clear, its body's +0x718 set), and then by him.
    if (target->bytes[0x14] == 0 && read_u32(body.bytes, 0x718) != 0)
    {
        return read_u32(body.bytes, 0x718) == strategy_base(strategy_of(context)).actor;
    }
    return true;
}

// Xbox 0x000C1890, EAX strategy, EDX actor
void choose_runtime_vampire_attack(RuntimeVampireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    if (!mind_present(context) || get(goals.entry.events.mind, mind_field::target_record) == 0)
    {
        return;
    }
    goals.room = nullptr; // Actor +0xCA0.
    if (roll_runtime_actor_attack(context.checks.attributes, goals.random)) // 0x000196F0
    {
        change_state(0x3EF, context); // The inlined vtable +0x18, 0x00029E80 and +0x14.
        return;
    }
    change_state(roll_runtime_actor_knockdown(context.checks.attributes, goals.random) ? 0x3EE : 0x3ED,
        context); // 0x00019750
}

// Xbox 0x000C1640, EDI strategy, ESI body +0x1CC
bool runtime_vampire_hit_from_behind(RuntimeVampireStrategy& context)
{
    const std::span<const u8> actor = goals_of(context).entry.actor_bytes; // Body +8.
    float ax = read_f32(actor, 0x3C);
    float ay = 0.f;
    float az = read_f32(actor, 0x44);
    if (ax == 0.f && az == 0.f) // UCOMISS; LAHF; JNP.
    {
        return true;
    }
    float square = az * az + ax * ax;
    if (square != 0.f)
    {
        const long double scale = engine_reciprocal_sqrt(square); // 0x001534C0, x87.
        ax = float(ax * scale);
        ay = float(0.L * scale); // DAT_0032B100.
        az = float(az * scale);
    }
    const std::span<const u8> body = body_of_actor(context);
    float bx = read_f32(body, 0x1D8); // +0x1CC +0xC.
    float by = 0.f;
    float bz = read_f32(body, 0x1E0); // +0x1CC +0x14.
    if (bx == 0.f && bz == 0.f)
    {
        return true;
    }
    square = bz * bz + bx * bx;
    if (square != 0.f)
    {
        const long double scale = engine_reciprocal_sqrt(square);
        bx = float(bx * scale);
        by = float(0.L * scale);
        bz = float(bz * scale);
    }
    const float dot = bz * az + by * ay + bx * ax;
    return 0.f - dot > kBehindDot; // COMISS the record's +0; JBE returns 0.
}

// Xbox 0x000C2070
void update_runtime_vampire_strategy(RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    set(strategy, field::shut, 0u);
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3ED:
        update_knockback(context); // 0x000C1B70
        return;
    case 0x3EE:
        update_knockdown(context); // 0x000C1C80
        return;
    case 0x3EF:
        update_open(context); // 0x000C1DC0
        return;
    case 0x401:
        update_waiting(context); // 0x000C1530
        return;
    case 0x406:
        update_knocked(context); // 0x000C1940
        return;
    case 0x416:
        // Xbox 0x000C1F90: back into the wait once the requests are done.
        if (!requests_pending(context))
        {
            change_state(0x401, context); // The inlined vtable +0x18, 0x00029E80 and +0x14.
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000C2140
void dispatch_runtime_vampire_message(std::span<const u32> message, RuntimeVampireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    switch (message[0])
    {
    case 0x1A:
        set(strategy, field::shut, 1u);
        change_state(0x416, context);
        break;
    case 0x2C:
        change_state(0x3F2, context);
        break;
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        break;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x1B, context.checks); // 0x0013A4F0
        break;
    case 0x6E:
    {
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            break;
        }
        if (message[2] == actor)
        {
            set(strategy, field::struck, 1u);
        }
        if (message[1] != actor || strategy_base(strategy).pause_count != 0)
        {
            break;
        }
        const std::span<const u8> actor_bytes = goals.entry.actor_bytes;
        const u32 kind =
            get(actor_bytes, player_field::mode) == 2 || get(actor_bytes, player_field::case_link) != 0 ? 1u : 0u;
        if (goals.resume.references[kind] != 0 || goals.resume.decay_enabled[goals.resume.decay_index] == 0)
        {
            break; // DAT_0050E764, DAT_0030F144[DAT_0051248C].
        }
        // The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
        const RuntimePlayerGoalRing* ring = goals.entry.ring;
        if (ring != nullptr
            && (ring->tail == ring->capacity ? read_u16(goals.entry.address_zero, 0)
                                             : read_u16(ring->rows, size_t(ring->stride) * ring->head))
                != 0)
        {
            break;
        }
        if (get(strategy, field::shut) == 0)
        {
            change_state(0x401, context);
        }
        break;
    }
    case 0x82:
        react_runtime_strategy_cower(context.checks); // 0x001398A0
        break;
    case 0x84:
        react_runtime_strategy_startle(context.checks); // 0x001399A0
        break;
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
}

// Xbox 0x000C2350
void enter_runtime_vampire_state(u16 state, u16 previous, RuntimeVampireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of_actor(context);
    switch (state)
    {
    case 0x3ED:
        // Xbox 0x000C1B30, ESI strategy
        if (get(strategy, field::knockback_stage) != 1)
        {
            set(strategy, field::knockback_stage, 1u);
            set(strategy, field::struck, 0u);
            load_goal(context.goal_knockback, context);
        }
        strategy_base(strategy).attacking = 1;
        body[0x328] = 4;
        return;
    case 0x3EE:
        // Xbox 0x000C1C40, EAX strategy
        strategy_base(strategy).attacking = 1;
        body[0x328] = 5;
        if (get(strategy, field::knockdown_stage) != 1)
        {
            set(strategy, field::knockdown_stage, 1u);
            load_goal(context.goal_knockdown, context);
        }
        return;
    case 0x3EF:
        // Xbox 0x000C1D70, ESI strategy
        if (get(strategy, field::open_stage) != 1)
        {
            set(strategy, field::open_stage, 1u);
            set(strategy, field::open_time, kOpenTime);
            load_goal(context.goal_special, context);
        }
        strategy_base(strategy).attacking = 1;
        body[0x328] = 6;
        return;
    case 0x401:
        set(strategy, field::delay, kAttackDelay);
        body[0x328] = 2;
        return;
    case 0x406:
        // Xbox 0x000C1920, EAX strategy
        set_knocked_stage(1, context);
        return;
    case 0x416:
    {
        // Xbox 0x000C1F40, EAX strategy
        load_goal(context.goal_waiting, context);
        body_bits(context, "0x000C1F40").bytes[8] |= 1;
        constexpr char open[] = "opencoffin"; // 0x0032431C, copied to body +0x288.
        std::memcpy(body.data() + 0x288, open, sizeof(open));
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000C2420
void leave_runtime_vampire_state(u16 state, RuntimeVampireStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_base(strategy).pause_count;
    }
    switch (state)
    {
    case 0x3ED:
        if (get(strategy, field::knockback_stage) != 0)
        {
            set(strategy, field::knockback_stage, 0u);
        }
        detach_runtime_strategy_link(context.base.link); // 0x00112A20, EAX strategy +0x20.
        strategy_base(strategy).attacking = 0;
        return;
    case 0x3EE:
        if (get(strategy, field::knockdown_stage) != 0)
        {
            set(strategy, field::knockdown_stage, 0u);
        }
        detach_runtime_strategy_link(context.base.link);
        strategy_base(strategy).attacking = 0;
        return;
    case 0x3EF:
        if (get(strategy, field::open_stage) != 0)
        {
            set(strategy, field::open_stage, 0u);
        }
        detach_runtime_strategy_link(context.base.link);
        strategy_base(strategy).attacking = 0;
        return;
    case 0x406:
        if (get(strategy, field::knocked_stage) != 0)
        {
            set(strategy, field::knocked_stage, 0u);
        }
        return;
    case 0x416:
    {
        body_bits(context, "0x000C2420").bytes[8] &= 0xFE;
        body_of_actor(context)[0x288] = 0;
        return;
    }
    default:
        return;
    }
}
}
