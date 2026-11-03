#include "vampire_chicken_strategy.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "body_message_helpers.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "strategy_target_selection.h"
#include "player_fields.h"
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = vampire_chicken_strategy_field;

RuntimePlayerStrategyGoals& goals_of(RuntimeVampireChickenStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeVampireChickenStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeVampireChickenStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeVampireChickenStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeVampireChickenStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeVampireChickenStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeVampireChickenStrategy& context)
{
    const std::span<u8> mind = mind_of(context);
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = get(mind, mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

RuntimeMindTarget* select_target(bool& found, RuntimeVampireChickenStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}

bool join_queue(const RuntimeMindTarget* target, RuntimeVampireChickenStrategy& context)
{
    return join_runtime_strategy_target_queue(context.shared.base.link, target == nullptr ? nullptr : target->record,
        &goals_of(context).entry.actor, context.shared.queue); // 0x0002B910, EDI the target
}

bool attacking(RuntimeVampireChickenStrategy& context)
{
    const u16 state = read_u16(strategy_of(context), 0xE);
    return state > 0x3EC && state < 0x3F0; // 0x000B7120
}

u32 own_address(RuntimeVampireChickenStrategy& context)
{
    return read_u32(strategy_of(context), 8);
}

// The body +0x3C names, through the actor it belongs to.
RuntimeStrategyBodyView held_body(RuntimeVampireChickenStrategy& context, const char* caller)
{
    if (context.held_actor == nullptr)
    {
        throw Error(std::string(caller) + " reads a held body with no native actor behind it");
    }
    RuntimeStrategyBodyView body = context.shared.views.body_of(*context.held_actor);
    if (body.address != get(strategy_of(context), field::held))
    {
        throw Error(std::string(caller) + " holds a body its native actor no longer has");
    }
    return body;
}

// Xbox 0x000C2D90, ECX strategy: the held body lets go when this actor still holds it (+0x50).
void release_held(RuntimeVampireChickenStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::held) == 0)
    {
        return;
    }
    RuntimeStrategyBodyView body = held_body(context, "0x000C2D90");
    if (read_u32(body.bytes, 0x50) != own_address(context))
    {
        return;
    }
    const u32 zero = 0;
    std::memcpy(body.bytes.data() + 0x50, &zero, 4);
    set(strategy, field::held, 0u);
    context.held_actor = nullptr;
}

// Xbox 0x0001B0D0, EAX body: a free, standing body with +0x58 set takes `holder` at +0x50.
bool claim_body(RuntimeStrategyBodyView& body, u32 holder)
{
    if (read_u32(body.bytes, 0x50) != 0)
    {
        return false;
    }
    if (body.bits == nullptr)
    {
        throw Error("0x0001B0D0 reads the bit block of a body with none");
    }
    if ((body.bits->bytes[3] & 8) != 0 || read_u32(body.bytes, 0x58) == 0)
    {
        return false;
    }
    std::memcpy(body.bytes.data() + 0x50, &holder, 4);
    return true;
}

// Xbox 0x000C25E0, EAX strategy: a queued, standing target is grabbed (0x3ED) or knocked down (0x3EE).
bool try_attack(RuntimeVampireChickenStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
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
        throw Error("0x000C25E0 reads the bit block of a target with no body");
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
    if (!join_queue(target, context)) // 0x0002B910
    {
        return false;
    }
    const std::span<const u8> attributes = context.shared.checks.attributes; // Actor +0x964.
    // UCOMISS; JP takes the knockdown for a nonzero (or unordered) +0xC.
    const bool knockdown_only = attributes.data() != nullptr && !(read_f32(attributes, 0xC) == 0.f);
    if (get(strategy, field::provoked) == 0 && !knockdown_only)
    {
        RuntimeStrategyBodyView body = context.shared.views.body_of(target->actor);
        if (!claim_body(body, own_address(context))) // 0x0001B0D0, EAX target body
        {
            return false;
        }
        goals.room = nullptr; // Actor +0xCA0.
        set(strategy, field::held, body.address);
        context.held_actor = &target->actor;
        change_state(0x3ED, context);
        return true;
    }
    goals.room = nullptr; // Actor +0xCA0.
    change_state(0x3EE, context);
    return true;
}

// Xbox 0x000C2580, ESI strategy, EBX actor: attack a queued target, else wander.
void tick_idle(RuntimeVampireChickenStrategy& context)
{
    bool found = false;
    RuntimeMindTarget* target = select_target(found, context);
    if (found && runtime_strategy_abandons_target(*target, context.shared.checks) != 0 // 0x00138410, EDX target
        && retreat_runtime_strategy_from_reached_target(context.shared.checks) != 0) // 0x001383A0
    {
        return;
    }
    if (try_attack(context)) // 0x000C25E0
    {
        return;
    }
    if (!requests_pending(context))
    {
        load_goal(context.goal_wander, context);
    }
}

// Xbox 0x000C2740, EAX actor, ESI strategy: the grab (0x3ED) ends in 0x3F2.
void tick_grab(RuntimeVampireChickenStrategy& context)
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
        change_state(0x3F2, context);
    }
}

// Xbox 0x000C2820, ESI strategy: the knockdown (0x3EE) ends in 0x401.
void tick_knockdown(RuntimeVampireChickenStrategy& context)
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
}

// Xbox 0x000C2930
bool initialize_runtime_vampire_chicken_strategy(std::span<u8> strategy)
{
    set(strategy, field::parameters, 0x003034E8u);
    return true;
}

// Xbox 0x000C2950
void update_runtime_vampire_chicken_strategy(RuntimeVampireChickenStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const u16 state = get(strategy, strategy_field::state);
    const RuntimeMindTarget* target = mind_target(context);
    u32 provoked = 0;
    if (target != nullptr)
    {
        provoked = get(actor, player_field::countdown_56) > 0.f ||
                           get(actor, player_field::provoked) != 0 // DAT_0032B100.
                           || !runtime_strategy_target_watching_player(*target)
                       ? 1u
                       : 0u; // 0x001414F0
    }
    else
    {
        provoked = get(actor, player_field::countdown_56) > 0.f ? 1u : u32(get(actor, player_field::provoked));
    }
    set(strategy, field::provoked, provoked);
    update_runtime_base_strategy(goals); // 0x0002C130
    switch (state)
    {
    case 0x3ED:
        tick_grab(context); // 0x000C2740
        return;
    case 0x3EE:
        tick_knockdown(context); // 0x000C2820
        return;
    case 0x3FD:
        // 0x000C2530: the emergence ends with its requests.
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    case 0x401:
        tick_idle(context); // 0x000C2580
        return;
    default:
        return;
    }
}

// Xbox 0x000C2A40
void dispatch_runtime_vampire_chicken_message(std::span<const u32> message, RuntimeVampireChickenStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeSpiderStrategy& shared = context.shared;
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = own_address(context);
    react_runtime_strategy_message(message, shared.checks); // 0x001384F0; its result is not read.
    switch (message[0])
    {
    case 0x2C:
        change_state(0x3F2, context);
        return;
    case 0x31:
        react_runtime_strategy_watched_actor(message, shared.checks, shared.views); // 0x001397B0
        if (get(strategy, field::held) != 0
            && message[1] == read_u32(held_body(context, "0x000C2A40").bytes, 8)) // The held body's +8 actor.
        {
            release_held(context); // The tail jump to 0x000C2D90.
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x1C, shared.checks); // 0x0013A4F0
        return;
    case 0x3A:
    {
        const u32 zero = 0;
        std::memcpy(mind_of(context).data() + 0x10, &zero, 4); // Mind +0x10.
        return;
    }
    case 0x6E:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        retaliate_runtime_strategy_contact(message, -1, shared.checks, shared.views); // 0x00139390
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_contact(message, 0.5f, shared.checks); // 0x001395C0, DAT_0032B0F4
        }
        if (message[1] == actor && runtime_strategy_reactions_enabled(goals))
        {
            change_state(0x401, context);
        }
        return;
    case 0x7C:
        if (message[1] == actor)
        {
            release_held(context); // The tail jump to 0x000C2D90.
        }
        return;
    case 0x80:
        release_held(context); // The tail jump to 0x000C2D90.
        return;
    case 0x82:
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_cower(shared.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (!attacking(context)) // 0x000B7120
        {
            react_runtime_strategy_startle(shared.checks); // The tail jump to 0x001399A0.
        }
        return;
    default:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000C2C70
void enter_runtime_vampire_chicken_state(u16 state, u16 previous, RuntimeVampireChickenStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = goals.entry.body; // Actor +0x95C.
    body[0x328] = 2;
    switch (state)
    {
    case 0x3ED:
        // 0x000C26E0.
        load_goal(context.goal_grab, context);
        body[0x328] = 4;
        strategy[0x1A] = 1;
        return;
    case 0x3EE:
        // 0x000C27F0.
        load_goal(context.goal_knockdown, context);
        body[0x328] = 5;
        strategy[0x1A] = 1;
        return;
    case 0x3FD:
        load_goal(context.goal_emerge, context);
        return;
    case 0x401:
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000C2D10
void leave_runtime_vampire_chicken_state(u16 state, RuntimeVampireChickenStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
        // 0x000C2710.
        release_held(context); // The same release as 0x000C2D90.
        detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
        strategy[0x1A] = 0;
        return;
    case 0x3EE:
        detach_runtime_strategy_link(context.shared.base.link); // 0x00112A20, EAX strategy +0x20
        strategy[0x1A] = 0;
        return;
    case 0x3FD:
    case 0x401:
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
