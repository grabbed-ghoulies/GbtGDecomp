#include "generic_strategy.h"
#include <cstddef>
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_shape_bounds.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "body_fields.h"
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
GenericStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<GenericStrategyState>(strategy);
}

const GenericStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<GenericStrategyState>(strategy);
}


constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kChaseRange = 40.f; // DAT_0032B1B8.
constexpr float kChaseSlack = 30.f; // DAT_0032B108.
constexpr float kArmedReach = 30.f; // DAT_0032B108: a class target this near keeps a hit ghoulie from arming.
constexpr float kCloseInSquared = 6400.f; // DAT_0032B6C8, 80 squared.
constexpr float kGunRange = 5.f; // DAT_0032B184.

RuntimePlayerStrategyGoals& goals_of(RuntimeGenericStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeGenericStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeGenericStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeGenericStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeGenericStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Body +0x48, the held weapon.
RuntimeBodyAttachedActor* weapon_of(RuntimeGenericStrategy& context)
{
    return goals_of(context).actions.attached;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeGenericStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeGenericStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeGenericStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimePlayerMindRequest& allocate_request(u16 kind, RuntimeGenericStrategy& context)
{
    RuntimeMindGoalContext& mind = goals_of(context).entry.goals;
    return *allocate_runtime_mind_request(mind.anchor, kind, mind.sizes); // 0x00029D90, EBX kind
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    if (offset + 4 > bytes.size())
    {
        throw Error("generic strategy mind request field lies outside the request");
    }
    std::memcpy(bytes.data() + offset, &value, 4);
}

bool join_queue(RuntimeMindTarget* target, RuntimeGenericStrategy& context)
{
    return join_runtime_strategy_target_queue(context.base.link, target == nullptr ? nullptr : target->record,
        &actor_of(context), context.queue); // 0x0002B910, EBX strategy, EDI target
}

const RuntimeMindTarget& require_target(const RuntimeMindTarget* target, const char* reader)
{
    if (target == nullptr)
    {
        throw Error(std::string(reader) + " reads the generic strategy's target record through address zero");
    }
    return *target;
}

// Xbox 0x000ABA40, ESI strategy: the inlined 0x0002B8D0 into 0x3EE.
void start_attack(RuntimeGenericStrategy& context)
{
    change_state(0x3EE, context);
}

// Xbox 0x000AB900, EDI strategy: once a second (+0x44) a ghoulie whose body is flagged (+0x6E bit 1) and has a target
// forces a reselection, keeping its old target when none is found; otherwise the target is reselected, forced while
// +0x3C is set. A new target starts an attack while +0x40 is set. With no target and `reset` the ghoulie holds (0x401).
bool select_target(RuntimeMindTarget*& output, bool reset, RuntimeGenericStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<const u8> mind = mind_of(context);
    const u32 prior = get(mind, mind_field::target_record);
    float timer = fields_of(strategy).reselect_timer - runtime_frame_step(goals.clock);
    fields_of(strategy).reselect_timer = timer;
    bool elapsed = false;
    if (!(0.f < timer)) // COMISS; JC keeps a positive time.
    {
        fields_of(strategy).reselect_timer = timer + kOne;
        elapsed = true;
    }
    bool changed = false;
    bool settled = false;
    if ((body_of(context)[offsetof(BodyRecord, tag)] & 1) != 0 && prior != 0 && elapsed)
    {
        select_runtime_strategy_target(output, 1, goals); // 0x00139AA0
        if (output != nullptr)
        {
            changed = output->address != prior;
            settled = true;
        }
        else
        {
            set_runtime_mind_target(goals.entry.events, prior); // 0x0002A030, ESI mind, EAX prior
            set_runtime_mind_alert_event(goals.entry.events, prior); // 0x0002A080, EAX mind, ESI prior
        }
    }
    if (!settled)
    {
        const u32 current = get(mind, mind_field::target_record);
        select_runtime_strategy_target(output, fields_of(strategy).reselect != 0 ? 1u : 0u, goals); // 0x00139AA0
        changed = current == 0 && output != nullptr;
    }
    if (changed && fields_of(strategy).attack_new != 0)
    {
        start_attack(context); // 0x000ABA40
    }
    fields_of(strategy).attack_new = 0u;
    fields_of(strategy).reselect = 0u;
    if (output != nullptr)
    {
        return true;
    }
    if (reset)
    {
        change_state(0x401, context); // The inlined 0x0002B8D0.
    }
    return false;
}

// Xbox 0x00133C10, EAX body, EDX gun: the gun's damage value (0x00028DA0) for its type (+0xDAC) against the body's
// actor (+8) is above zero; 0xC and 0xD read key 0x22, 0xE 0x20, 0xF 0x21 and 0x10 0x23, any other type is false.
bool gun_damages_actor(const RuntimeBodyAttachedActor& gun, const Actor& actor,
    const RuntimeBodyDamageTables& tables)
{
    u8 key = 0;
    switch (gun.bytes[0xDAC])
    {
    case 0xC:
    case 0xD:
        key = 0x22;
        break;
    case 0xE:
        key = 0x20;
        break;
    case 0xF:
        key = 0x21;
        break;
    case 0x10:
        key = 0x23;
        break;
    default:
        return false;
    }
    return runtime_actor_damage_value(key, actor.damage_class, tables) > 0;
}

// Xbox 0x0013A630, EAX actor, stack record: the distance from the actor to the target record's point (+0x20) less
// the reach of the body's +0x78 and +0x888 scaled by the actor's +0x38, a held kind-1 weapon's sweep radius
// (0x0003D9A0) and the record's +0x38; never below zero.
float target_gap(std::span<const u8> record, RuntimeGenericStrategy& context)
{
    const Actor& actor = actor_of(context);
    const std::span<u8> body = body_of(context);
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float dx = position.x - read_f32(record, 0x20);
    const float dy = position.y - read_f32(record, 0x24);
    const float dz = position.z - read_f32(record, 0x28);
    const float scale = actor.radius_xz; // Body +8, +0x38.
    const float distance = float(std::sqrt(double(dx) * dx + double(dy) * dy + double(dz) * dz));
    float reach = body_record(body).reach * scale;
    float weapon_radius = 0.f;
    const RuntimeBodyAttachedActor* weapon = weapon_of(context);
    const u8 kind = weapon == nullptr ? 0 : weapon->bytes[0x93D];
    if (kind == 1)
    {
        if (weapon->actor == nullptr)
        {
            throw Error("generic strategy: 0x0013A630 reads a held weapon with no native actor");
        }
        if (weapon->actor->model != nullptr && !weapon->actor->model->cpu.empty())
        {
            weapon_radius = float(actor_model_sweep_radius(*weapon->actor)); // 0x0003D9A0, EDI weapon
        }
    }
    reach += weapon_radius;
    const float gap = distance - (body_record(body).radius_scale * scale + reach + read_f32(record, 0x38));
    return 0.f >= gap ? 0.f : gap;
}

// Xbox 0x000ABB10, EDX gun, ESI record, stack actor: a live gun (+0x93C) with a target record not flagged (+0x14)
// that the gun damages (0x00133C10) and that lies at least DAT_0032B184 beyond reach (0x0013A630).
bool gun_target_in_range(const RuntimeBodyAttachedActor& gun, u32 record, RuntimeGenericStrategy& context)
{
    if (gun.bytes[0x93C] == 0)
    {
        return false;
    }
    const RuntimeMindTarget* target = goals_of(context).target_at(record);
    const std::span<const u8> bytes = require_target(target, "0x000ABB1A").bytes;
    if (bytes[0x14] != 0)
    {
        return false;
    }
    if (context.damage_tables == nullptr)
    {
        throw Error("generic strategy: 0x00133C10 reads 0x00028DA0's tables before the scene has them");
    }
    if (!gun_damages_actor(gun, target->actor, *context.damage_tables)) // 0x00133C10
    {
        return false;
    }
    return !(kGunRange > target_gap(bytes, context)); // 0x0013A630
}

// Xbox 0x000ABA80, EAX strategy: the attack goal for the held weapon (body +0x48, its kind at +0x93D).
void load_attack_goal(RuntimeGenericStrategy& context)
{
    const RuntimeBodyAttachedActor* weapon = weapon_of(context);
    if (weapon != nullptr)
    {
        const u8 kind = weapon->bytes[0x93D];
        if (kind >= 2 && kind <= 3)
        {
            load_goal(context.goal_attack_projectile, context);
            return;
        }
        if (kind == 4)
        {
            const u32 record = get(goals_of(context).entry.events.mind, mind_field::target_record); // ESI, mind +0x10.
            load_goal(gun_target_in_range(*weapon, record, context) ? context.goal_attack_gun
                : context.goal_attack_gun_secondary, context);
            return;
        }
    }
    load_goal(context.goal_attack, context);
}
}

// Xbox 0x000ABBD0
bool initialize_runtime_generic_strategy(RuntimeGenericStrategy& context)
{
    RuntimeMindGoalContext& goals = goals_of(context).entry.goals;
    const std::span<u8> strategy = strategy_of(context);
    const RuntimeMindGoalRow* row = nullptr;
    if (goals.table != nullptr) // Mind +0x60.
    {
        const char* wildcard = std::strchr(context.goal_get_weapon, '?');
        if (wildcard != nullptr)
        {
            row = choose_runtime_mind_goal(*goals.table, context.goal_get_weapon, wildcard, goals.scratch,
                goals.random); // 0x0002A810
        }
        else
        {
            row = find_runtime_mind_goal(*goals.table, context.goal_get_weapon); // 0x0002A770
        }
    }
    fields_of(strategy).can_arm = row != nullptr && row->bytes.data() + 4 != nullptr ? 1u : 0u;
    return true;
}

// Xbox 0x000ABC50 and 0x000AB420, EAX message
void dispatch_runtime_generic_message(std::span<const u32> message, RuntimeGenericStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
    if (react_runtime_strategy_message(message, context.checks) != 0) // 0x001384F0, EAX strategy, ECX message
    {
        return;
    }
    if (message[0] == 0x31)
    {
        float distance = 0.f; // The strategy argument's slot.
        if (find_runtime_strategy_hostile_target(true, &distance, goals) == nullptr) // 0x00029BA0, EAX 1
        {
            react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        }
        return;
    }
    if (message[0] != 0x6E)
    {
        return;
    }
    if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
    {
        return;
    }
    retaliate_runtime_strategy_contact(message, -1, context.checks, context.views); // 0x00139390
    if (actor != message[2] && actor != message[1])
    {
        return;
    }
    bool arming = false;
    if (message[4] != 0 && fields_of(strategy).can_arm != 0 && weapon_of(context) == nullptr)
    {
        bool near = false;
        if (actor != message[1])
        {
            const RuntimeMindTarget* target = find_runtime_strategy_class_target(true, nullptr, goals); // 0x00029AC0
            near = target != nullptr && runtime_target_distance(actor_of(context), target->bytes) <= kArmedReach;
        }
        if (!near)
        {
            change_state(0x405, context); // 0x0002B8D0, EDI 0x405
            arming = true;
        }
    }
    fields_of(strategy).reselect = 1u;
    fields_of(strategy).attack_new = arming ? 0u : 1u;
}

// Xbox 0x000AB540
void update_runtime_generic_strategy(RuntimeGenericStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    update_runtime_base_strategy(goals); // 0x0002C130
    RuntimeMindTarget* target = nullptr;
    switch (state)
    {
    case 0x3EC:
        if (!select_target(target, true, context)) // 0x000AB900
        {
            return;
        }
        if (join_queue(target, context)) // 0x0002B910
        {
            change_state(0x3EE, context);
        }
        if (!requests_pending(context))
        {
            change_state(0x40D, context);
        }
        return;
    case 0x3EE:
        if (!select_target(target, true, context)) // 0x000AB900
        {
            return;
        }
        if (!requests_pending(context))
        {
            load_attack_goal(context); // 0x000ABA80
        }
        if (!join_queue(target, context)) // 0x0002B910
        {
            change_state(0x40D, context);
        }
        return;
    case 0x401:
        if (select_target(target, false, context)) // 0x000AB900
        {
            if (runtime_strategy_abandons_target(require_target(target, "0x000AB63E"), context.checks)
                == 0) // 0x00138410, EDX target
            {
                goals.room = nullptr; // Actor +0xCA0.
                start_attack(context); // 0x000ABA40
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
    case 0x405:
        if (weapon_of(context) != nullptr)
        {
            select_target(target, false, context); // 0x000AB900
        }
        else
        {
            const u32 none = 0;
            std::memcpy(mind_of(context).data() + 0xC, &none, 4); // Mind +0xC.
        }
        if (!requests_pending(context))
        {
            change_state(target != nullptr ? 0x3EC : 0x401, context);
        }
        return;
    case 0x40D:
        if (!select_target(target, true, context)) // 0x000AB900
        {
            return;
        }
        if (runtime_squared_target_distance(actor_of(context), require_target(target, "0x000AB6F6").bytes)
            > kCloseInSquared) // 0x000415A0, ESI actor, EBX target
        {
            change_state(0x3EC, context);
        }
        if (join_queue(target, context)) // 0x0002B910
        {
            change_state(0x3EC, context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000AB770
void enter_runtime_generic_state(u16 state, RuntimeGenericStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    body_record(body).move_class = 2;
    strategy_base(strategy).attacking = 0;
    switch (state)
    {
    case 0x3EC:
    {
        body_record(body).move_class = 3;
        RuntimePlayerMindRequest& chase = allocate_request(0x2F, context); // 0x00029D90, EBX 0x2F
        put_float(chase.bytes, 0x10, kOne);
        put_float(chase.bytes, 0x14, kOne);
        put_float(chase.bytes, 0x1C, kChaseSlack);
        put_float(chase.bytes, 0x18, kChaseRange);
        return;
    }
    case 0x3EE:
        body_record(body).move_class = 5;
        strategy_base(strategy).attacking = context.attack_resource.data() != nullptr ? 1 : 0; // Body +0x5C.
        load_attack_goal(context); // The tail jump to 0x000ABA80.
        return;
    case 0x401:
        load_goal(context.goal_far, context);
        return;
    case 0x405:
        body_record(body).move_class = 3;
        load_goal(context.goal_get_weapon, context);
        return;
    case 0x40D:
        allocate_request(0x17, context); // 0x00029D90, EBX 0x17
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000AB8C0
void leave_runtime_generic_state(u16 state, RuntimeGenericStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EE:
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20, EAX strategy +0x20.
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
