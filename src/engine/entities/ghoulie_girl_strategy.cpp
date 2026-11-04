#include "ghoulie_girl_strategy.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = ghoulie_girl_strategy_field;
namespace parameter = ghoulie_girl_strategy_parameter;

constexpr u32 kRecordAddress = 0x00302B68;
// The record at 0x00302B68: +0 the chance of another knockdown, +4 the wake's seconds, +8 the knockdown wait, +0xC
// what each repeated knockdown adds to it.
constexpr float kAgainChance = 0.25f; // 0x00302B68 +0.
constexpr float kWakeTime = 4.f; // 0x00302B68 +4.
constexpr float kKnockdownTime = 3.f; // 0x00302B68 +8.
constexpr float kKnockdownStep = 1.f; // 0x00302B68 +0xC.
constexpr float kRunOut = 0.001f; // DAT_0032B1A4.
constexpr float kOne = 1.f; // DAT_0032B0F8.

RuntimePlayerStrategyGoals& goals_of(RuntimeGhoulieGirlStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeGhoulieGirlStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeGhoulieGirlStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeGhoulieGirlStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeGhoulieGirlStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeGhoulieGirlStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeGhoulieGirlStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeGhoulieGirlStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeGhoulieGirlStrategy& context)
{
    const std::span<const u8> mind = mind_of(context);
    if (mind.empty())
    {
        return nullptr;
    }
    const u32 address = get(mind, mind_field::target_record);
    if (address == 0)
    {
        return nullptr;
    }
    RuntimeMindTarget* target = goals_of(context).target_at(address);
    if (target == nullptr)
    {
        throw Error("ghoulie girl strategy: the mind's target " + std::to_string(address) + " has no record");
    }
    return target;
}

// The target actor's body (+0x95C).
RuntimeStrategyBodyView target_body(const RuntimeMindTarget& target, RuntimeGhoulieGirlStrategy& context,
    const char* reader)
{
    const RuntimeStrategyBodyView body = context.views.body_of(target.actor);
    if (body.bytes.data() == nullptr || body.bits == nullptr)
    {
        throw Error(std::string(reader) + " reads the ghoulie girl's target body, which has none");
    }
    return body;
}

// Xbox 0x000ABD00 and the head of 0x000ABE80, ESI strategy: the wake's object hears {5, 0} and body latch 0x3F
// drops.
void end_wake(RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::array<u32, 2> message{5, 0};
    context.object_message(get(strategy, field::object), message); // Object vtable +0xC.
    set(strategy, field::object, 0u);
    decrement_runtime_body_latch_mask(body_of(context), 0x3F, actor_of(context), context.latches); // 0x0001F2F0
}

// Xbox 0x000ABE80, EAX mode, ESI strategy
void set_mode(u32 mode, RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::mode) == mode)
    {
        return;
    }
    if (get(strategy, field::mode) == 1)
    {
        end_wake(context);
    }
    set(strategy, field::mode, mode);
    if (mode == 1)
    {
        load_goal(context.goal_wake, context);
        set(strategy, field::timer, kWakeTime); // Record +4.
        const auto name = get(strategy, field::object_name);
        // 0x00107720 clones the named template, +8 becomes the actor, 0x00107520 makes it and 0x000663E0 frees
        // the clone; with no template the object is 0.
        set(strategy, field::object, context.create_object(name.data(), strategy_base(strategy).actor));
        increment_runtime_body_latch_mask(body_of(context), 0x3F, actor_of(context), context.latches); // 0x0001F3F0
        return;
    }
    if (mode == 2)
    {
        load_goal(context.goal_wander, context);
    }
}

// Xbox 0x000ABD00, ESI strategy
void stop_mode(RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::mode) == 0)
    {
        return;
    }
    if (get(strategy, field::mode) == 1)
    {
        end_wake(context);
    }
    set(strategy, field::mode, 0u);
}

// Xbox 0x000ABC60, EAX strategy: holding, she wakes the first time and wanders after.
void enter_hold(RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    body_of(context)[0x328] = 2;
    if (get(strategy, field::woken) == 0)
    {
        set_mode(1, context); // 0x000ABE80, EAX 1
        return;
    }
    set_mode(2, context); // The inlined 0x000ABE80, EAX 2.
}

// Xbox 0x000ABE10, EAX actor, ECX strategy: the mind's target is one she keeps, its body is not held (byte 4 bits
// 0x10 and 0x20, byte 3 bit 0x80) and she joins its queue.
bool can_knock_down(RuntimeGhoulieGirlStrategy& context)
{
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    if (runtime_strategy_abandons_target(*target, context.checks) != 0) // 0x00138410, EAX strategy, EDX target
    {
        return false;
    }
    const EntityBitBlock& bits = *target_body(*target, context, "0x000ABE32").bits; // +0x15C -> +8
    if ((bits.bytes[4] & 0x10) != 0 || (bits.bytes[4] & 0x20) != 0 || (bits.bytes[3] & 0x80) != 0)
    {
        return false;
    }
    if (!join_runtime_strategy_target_queue(context.base.link, target->record, &actor_of(context),
            context.queue)) // 0x0002B910, EBX strategy, EDI target
    {
        return false;
    }
    goals_of(context).room = nullptr; // Actor +0xCA0.
    return true;
}

// Xbox 0x000AC020, EAX actor: the target still has energy (body +0x94) and the record's +0 chance comes up.
bool knock_down_again(RuntimeGhoulieGirlStrategy& context)
{
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    if (read_u16(target_body(*target, context, "0x000AC035").bytes, 0x94) == 0)
    {
        return false;
    }
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - kOne;
    return kAgainChance >= roll; // COMISS; JNC.
}

// Xbox 0x000ABD60, EAX strategy, EBX actor
void update_hold(RuntimeGhoulieGirlStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    if (select_runtime_strategy_target(target, 0, goals_of(context)) // 0x00139AA0
        && runtime_strategy_abandons_target(*target, context.checks) != 0 // 0x00138410, EDX target
        && retreat_runtime_strategy_from_reached_target(context.checks) != 0) // 0x001383A0
    {
        return;
    }
    const std::span<const u8> mind = mind_of(context);
    if (mind.empty() || get(mind, mind_field::target_record) == 0)
    {
        return;
    }
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::mode) == 1)
    {
        float timer = get(strategy, field::timer);
        count_down_runtime_strategy_timer(timer, goals_of(context)); // 0x0002B9F0, ECX +0x44
        set(strategy, field::timer, timer);
        if (kRunOut > get(strategy, field::timer)) // COMISS; JBE keeps the wake.
        {
            set_mode(2, context); // 0x000ABE80, EAX 2
            set(strategy, field::woken, 1u);
        }
        return;
    }
    if (get(strategy, field::mode) != 2)
    {
        return;
    }
    if (can_knock_down(context)) // 0x000ABE10, EAX actor, ECX strategy
    {
        change_state(0x3EE, context); // 0x0002B8D0, EDI 0x3EE
        return;
    }
    if (!requests_pending(context))
    {
        load_goal(context.goal_wander, context);
    }
}

// Xbox 0x000ABF60, ESI strategy
void enter_knockdown(RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    load_goal(context.goal_knockdown, context);
    set(strategy, field::knockdown_time, kKnockdownTime); // Record +8.
    body_of(context)[0x328] = 4;
    strategy_base(strategy).attacking = 1;
}

// Xbox 0x000ABFA0, EAX actor, ESI strategy
void update_knockdown(RuntimeGhoulieGirlStrategy& context)
{
    if (requests_pending(context)) // Mind +0x58.
    {
        return;
    }
    const std::span<u8> strategy = strategy_of(context);
    if (knock_down_again(context)) // 0x000AC020, EAX actor
    {
        load_goal(context.goal_knockdown, context);
        set(strategy, field::knockdown_time, kKnockdownStep + get(strategy, field::knockdown_time)); // Record +0xC.
        return;
    }
    change_state(0x40D, context); // The inlined 0x0002B8D0.
}

// Xbox 0x000AC090, ESI strategy: the inlined 0x0002B9F0 on +0x44, then holding once it runs out.
void update_wait(RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    float timer = get(strategy, field::timer);
    count_down_runtime_strategy_timer(timer, goals_of(context));
    set(strategy, field::timer, timer);
    if (kRunOut > get(strategy, field::timer)) // COMISS
    {
        change_state(0x401, context); // The inlined 0x0002B8D0.
    }
}
}

// Xbox 0x000AC140
void ghoulie_girl_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000AC1B0
bool initialize_runtime_ghoulie_girl_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeGhoulieGirlStrategy& context)
{
    (void)context;
    set(strategy, field::record, kRecordAddress);
    set(strategy, field::woken, 0u);
    const auto source = get(parameters, parameter::object_name);
    std::array<char, 0x80> name{};
    std::strncpy(name.data(), source.data(), name.size() - 1); // The inlined strcpy.
    set(strategy, field::object_name, name);
    set(strategy, field::object, 0u);
    set(strategy, field::knockdown_time, 0.f);
    set(strategy, field::timer, 0.f);
    return true;
}

// Xbox 0x000AC250
void dispatch_runtime_ghoulie_girl_message(std::span<const u32> message, RuntimeGhoulieGirlStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 actor = strategy_base(strategy_of(context)).actor;
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
    switch (message[0])
    {
    case 0x2C:
        change_state(0x3F2, context); // 0x0002B8D0, EDI 0x3F2
        return;
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(9, context.checks); // 0x0013A4F0
        return;
    case 0x6E:
        if (check_runtime_player_knockout(goals) != 0 || message[1] != actor) // 0x0013A490, EBX strategy
        {
            return;
        }
        // Strategy +0x1C clear, the actor's pause group (DAT_0050E764) and decay entry, and an idle goal ring.
        if (runtime_strategy_reactions_enabled(goals))
        {
            change_state(0x401, context); // 0x0002B8D0, EDI 0x401
        }
        return;
    case 0x82:
        react_runtime_strategy_cower(context.checks); // The tail jump to 0x001398A0.
        return;
    case 0x84:
        react_runtime_strategy_startle(context.checks); // The tail jump to 0x001399A0.
        return;
    default:
        return;
    }
}

// Xbox 0x000AC200
void update_runtime_ghoulie_girl_strategy(RuntimeGhoulieGirlStrategy& context)
{
    const u16 state = strategy_base(strategy_of(context)).state;
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3EE:
        update_knockdown(context); // 0x000ABFA0, EAX actor
        return;
    case 0x401:
        update_hold(context); // 0x000ABD60, EAX strategy
        return;
    case 0x40D:
        update_wait(context); // 0x000AC090
        return;
    default:
        return;
    }
}

// Xbox 0x000AC3E0
void enter_runtime_ghoulie_girl_state(u16 state, RuntimeGhoulieGirlStrategy& context)
{
    enter_runtime_base_strategy_state(goals_of(context).entry, state); // 0x0002BAF0
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EE:
        enter_knockdown(context); // 0x000ABF60
        return;
    case 0x401:
        enter_hold(context); // The tail jump to 0x000ABC60.
        return;
    case 0x40D:
        load_goal(context.goal_wake, context);
        // UCOMISS against DAT_0032B100 (0): a set knockdown time is waited out, else the record's +8.
        set(strategy, field::timer,
            get(strategy, field::knockdown_time) != 0.f ? get(strategy, field::knockdown_time) : kKnockdownTime);
        return;
    default:
        return;
    }
}

// Xbox 0x000AC460
void leave_runtime_ghoulie_girl_state(u16 state, RuntimeGhoulieGirlStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_base(strategy).pause_count;
    }
    switch (state)
    {
    case 0x3EE:
        strategy_base(strategy).attacking = 0;
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20, EAX strategy +0x20.
        return;
    case 0x401:
        stop_mode(context); // 0x000ABD00
        return;
    case 0x40D:
        set(strategy, field::knockdown_time, 0.f);
        return;
    default:
        return;
    }
}
}
