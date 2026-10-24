#include "strategy_target_selection.h"
#include "mind_target_policy.h"
#include "actor_spawn_eligibility.h"
#include "actor_transform.h"
#include "actor_shape_bounds.h"
#include "player_children.h"
#include "player_mind_targets.h"
#include "body_request_selection.h"
#include "../navigation/player_avoidance.h"
#include "../scripts/script_conditions.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

std::span<u8> mind_of(RuntimePlayerStrategyGoals& context)
{
    return context.entry.events.mind;
}

std::span<const u8> strategy_of(RuntimePlayerStrategyGoals& context)
{
    return context.entry.strategy;
}

// Actor +0x978 above zero (ordered) or the +0x969 byte.
bool actor_hostile(std::span<const u8> actor)
{
    return read_f32(actor, 0x978) > 0.f || actor[0x969] != 0;
}

// Actor +0x97E/+0x97C class filters; each word above one applies while hostile.
u16 hostile_class(std::span<const u8> actor, size_t offset)
{
    if (!actor_hostile(actor))
    {
        return 0;
    }
    const u16 value = read_u16(actor, offset);
    return value > 1 ? value : 0;
}

u16 front_goal_kind(const RuntimePlayerGoalRing* ring, std::span<const u8> address_zero)
{
    if (ring->tail == ring->capacity)
    {
        return read_u16(address_zero, 0);
    }
    return read_u16(ring->rows, size_t(ring->head) * ring->stride);
}

// The inlined resume gate: strategy +0x1C clear, 0x0002B7F0 and an empty or idle goal ring.
// 0x0002B840, EAX strategy, ECX force.
bool reactions_enabled(RuntimePlayerStrategyGoals& context, u32 force = 0)
{
    if (force == 0 && strategy_of(context)[0x1C] != 0)
    {
        return false;
    }
    if (runtime_player_strategy_decay_enabled(context.entry.actor_bytes, context.resume) == 0)
    {
        return false;
    }
    const RuntimePlayerGoalRing* ring = context.entry.ring;
    return ring == nullptr || front_goal_kind(ring, context.entry.address_zero) == 0;
}

bool body_settled(std::span<const u8> body)
{
    return read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f;
}

bool roll(u32& random, float chance)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return chance >= std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}

void change_state(u16 state, RuntimePlayerStrategyGoals& context)
{
    change_runtime_player_strategy_state(context.strategy, state, context.setup);
}

// Shared tail of 0x00139AA0 and 0x00139C70 from the target store at +0x10.
bool store_selected_target(RuntimeMindTarget* target, RuntimeMindTarget*& output,
    RuntimePlayerStrategyGoals& context)
{
    RuntimeMindEvents& events = context.entry.events;
    std::span<u8> mind = events.mind;
    put(mind, 0x10, target != nullptr ? target->address : 0);
    if (target == nullptr)
    {
        for (LiveScript* node = events.anchor.next; node != nullptr; node = node->next)
        {
            if (u16(node->type) == 3)
            {
                script_release_event(node);
                break;
            }
        }
        put(mind, 0xC, 0);
        output = nullptr;
        return false;
    }
    refresh_runtime_mind_target_event(events);
    return false;
}

bool keep_selected_target(RuntimeMindTarget* target, RuntimeMindTarget*& output,
    RuntimePlayerStrategyGoals& context)
{
    RuntimeMindEvent* event = allocate_runtime_mind_event(context.entry.events, 3, 0.5f);
    put(event->bytes, 0x20, target->address);
    if (target->bytes[0x14] == 0)
    {
        put(mind_of(context), 0xC, read_u32(target->bytes, 0x10));
    }
    output = target;
    return true;
}

// Sends {100, 0} through the actual actor vtable +0xC.
void report_lost_target(RuntimePlayerStrategyGoals& context)
{
    const std::array<u32, 2> message{100, 0};
    context.entry.events.actor_message(message);
}
}

// Xbox 0x00029AC0
RuntimeMindTarget* find_runtime_strategy_class_target(bool class_filter, float* distance,
    RuntimePlayerStrategyGoals& context)
{
    std::span<const u8> mind = mind_of(context);
    std::span<const u8> actor = context.entry.actor_bytes;
    u32 flags = read_u32(mind, 0x64);
    u16 exclude = 0;
    u16 include = 0;
    if (class_filter)
    {
        if (actor_hostile(actor))
        {
            flags = (flags & ~4u) | 0x10u;
        }
        exclude = hostile_class(actor, 0x97E);
        include = hostile_class(actor, 0x97C);
    }
    float found_distance;
    RuntimeMindTarget* target = find_runtime_mind_target(context.entry.actor, context.entry.allowed, include,
        exclude, &found_distance, read_u32(mind, 0x68), flags, context.entry.targets);
    if (target != nullptr)
    {
        const float range = read_f32(mind, 0x6C);
        if (range > 0.f && found_distance > range)
        {
            return nullptr;
        }
        if (distance != nullptr)
        {
            *distance = found_distance;
        }
    }
    return target;
}

// Xbox 0x00029BA0
RuntimeMindTarget* find_runtime_strategy_hostile_target(bool hostile_filter, float* distance,
    RuntimePlayerStrategyGoals& context)
{
    std::span<const u8> mind = mind_of(context);
    std::span<const u8> actor = context.entry.actor_bytes;
    u32 flags = read_u32(mind, 0x64);
    if (hostile_filter && actor_hostile(actor))
    {
        flags = (flags & ~4u) | 0x10u;
    }
    flags = (flags & ~0x20u) | 8u;
    u16 exclude = 0;
    u16 include = 0;
    if (hostile_filter)
    {
        exclude = hostile_class(actor, 0x97E);
        include = hostile_class(actor, 0x97C);
    }
    float found_distance;
    RuntimeMindTarget* target = find_runtime_mind_target(context.entry.actor, context.entry.allowed, include,
        exclude, &found_distance, read_u32(mind, 0x68), flags, context.entry.targets);
    if (target != nullptr && distance != nullptr)
    {
        *distance = found_distance;
    }
    return target;
}

// Xbox 0x00139AA0
bool select_runtime_strategy_target(RuntimeMindTarget*& output, u32 force, RuntimePlayerStrategyGoals& context)
{
    std::span<u8> mind = mind_of(context);
    std::span<const u8> actor = context.entry.actor_bytes;
    RuntimeMindTarget* current = read_u32(mind, 0x10) != 0 ? context.target_at(read_u32(mind, 0x10)) : nullptr;
    if (force == 0 && current != nullptr)
    {
        if (runtime_mind_target_occupancy_differs(*current, read_u16(actor, 6), context.entry.allowed))
        {
            force = 1;
        }
        if (actor_hostile(actor)
            && !runtime_mind_target_is_eligible(mind, actor, context.entry.actor,
                context.address_of(actor.data()), *current, true))
        {
            force = 1;
        }
    }
    if (current != nullptr && force == 0)
    {
        return keep_selected_target(current, output, context);
    }
    RuntimeMindTarget* target = find_runtime_strategy_class_target(true, nullptr, context);
    if (target == nullptr)
    {
        bool searched = false;
        if (actor_hostile(actor) && find_runtime_strategy_hostile_target(true, nullptr, context) == nullptr)
        {
            float distance;
            RuntimeMindTarget* found = find_runtime_mind_target(context.entry.actor, context.entry.allowed, 0, 0,
                &distance, read_u32(mind, 0x68), read_u32(mind, 0x64), context.entry.targets);
            const float range = read_f32(mind, 0x6C);
            if (found != nullptr && range > 0.f && distance > range)
            {
                target = nullptr;
            }
            else
            {
                target = found;
                searched = found != nullptr;
            }
        }
        if (!searched && read_u32(mind, 0x10) != 0)
        {
            report_lost_target(context);
        }
    }
    store_selected_target(target, output, context);
    if (target == nullptr)
    {
        return false;
    }
    return keep_selected_target(target, output, context);
}

// Xbox 0x00139C70
bool select_runtime_strategy_hostile_target(RuntimeMindTarget*& output, u32 force,
    RuntimePlayerStrategyGoals& context)
{
    std::span<u8> mind = mind_of(context);
    std::span<const u8> actor = context.entry.actor_bytes;
    RuntimeMindTarget* current = read_u32(mind, 0x10) != 0 ? context.target_at(read_u32(mind, 0x10)) : nullptr;
    if (force == 0 && current != nullptr
        && runtime_mind_target_occupancy_differs(*current, read_u16(actor, 6), context.entry.allowed))
    {
        force = 1;
    }
    if (current != nullptr && force == 0)
    {
        return keep_selected_target(current, output, context);
    }
    RuntimeMindTarget* target = find_runtime_strategy_hostile_target(true, nullptr, context);
    if (target == nullptr)
    {
        bool searched = false;
        if (actor_hostile(actor))
        {
            float distance;
            RuntimeMindTarget* found = find_runtime_mind_target(context.entry.actor, context.entry.allowed, 0, 0,
                &distance, read_u32(mind, 0x68), read_u32(mind, 0x64), context.entry.targets);
            const float range = read_f32(mind, 0x6C);
            if (found != nullptr && range > 0.f && distance > range)
            {
                target = nullptr;
            }
            else
            {
                target = found;
                searched = found != nullptr;
            }
        }
        if (!searched && read_u32(mind, 0x10) != 0)
        {
            report_lost_target(context);
        }
    }
    store_selected_target(target, output, context);
    if (target == nullptr)
    {
        return false;
    }
    return keep_selected_target(target, output, context);
}

// Xbox 0x00141450
Actor* runtime_mind_target_case_actor(const RuntimeMindTarget& target)
{
    if (target.bytes[0x14] != 0)
    {
        return nullptr;
    }
    return target.body.case_actor;
}

// Xbox 0x00141470
bool runtime_mind_target_case_unavailable(const RuntimeMindTarget& target)
{
    if (target.bytes[0x14] == 0 && target.body.case_actor != nullptr)
    {
        return actor_spawn_owner_unavailable(target.actor);
    }
    return false;
}

// Xbox 0x000196B0
u32 runtime_actor_attack_limit(std::span<const u8> attributes)
{
    if (attributes.data() != nullptr)
    {
        return read_u32(attributes, 0x18);
    }
    return 0;
}

// Xbox 0x000196F0
bool roll_runtime_actor_attack(std::span<const u8> attributes, u32& random)
{
    const float chance = attributes.data() == nullptr ? 0.f : read_f32(attributes, 8);
    return roll(random, chance);
}

// Xbox 0x00019750
bool roll_runtime_actor_knockdown(std::span<const u8> attributes, u32& random)
{
    const float chance = attributes.data() == nullptr ? 0.f : read_f32(attributes, 0xC);
    return roll(random, chance);
}

// Xbox 0x00129280
i32 count_runtime_energy_actors(const RuntimeActiveActorLink* first, std::span<const u8> address_zero)
{
    i32 count = 0;
    for (const RuntimeActiveActorLink* link = first; link != nullptr; link = link->next)
    {
        const RuntimePlayerGoalRing* ring = *link->goals;
        if (ring != nullptr && front_goal_kind(ring, address_zero) != 0)
        {
            continue;
        }
        if (link->body->energy != 0)
        {
            ++count;
        }
    }
    return count;
}

// Xbox 0x00138410
u32 runtime_strategy_abandons_target(const RuntimeMindTarget& target, RuntimeStrategyTargetChecks& context)
{
    std::span<const u8> actor = context.goals.entry.actor_bytes;
    const bool disabled = target.bytes[0x14] != 0;
    if (!disabled && ((target.body.bits->bytes[5] >> 4) & 1) != 0)
    {
        return 0;
    }
    if (actor[0x96C] != 0 && !disabled && target.body.case_actor != nullptr)
    {
        return 0;
    }
    if (actor[0x96D] != 0 || read_f32(actor, 0x974) > 0.f)
    {
        return 1;
    }
    if (actor[0x96F] != 0 && runtime_mind_target_case_actor(target) != nullptr)
    {
        return 1;
    }
    if (actor[0x970] != 0 && runtime_mind_target_case_unavailable(target))
    {
        return 1;
    }
    if (actor[0x96E] != 0)
    {
        const i32 limit = i32(runtime_actor_attack_limit(context.attributes));
        if (count_runtime_energy_actors(context.active, context.goals.entry.address_zero) < limit)
        {
            return 1;
        }
    }
    if (context.attributes.data() == nullptr || read_u32(context.attributes, 0x2C) == 0)
    {
        return 0;
    }
    return context.difficulty == 3 ? 0 : 1;
}

// Xbox 0x001409C0
bool runtime_message_names_mind_target(std::span<const u32> message, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at)
{
    if (message[4] == 0 || mind.data() == nullptr || read_u32(mind, 0x10) == 0)
    {
        return false;
    }
    const RuntimeMindTarget* target = target_at(read_u32(mind, 0x10));
    if (target->bytes[0x14] != 0)
    {
        return false;
    }
    return message[2] == read_u32(target->bytes, 0x10);
}

// Xbox 0x00140980
bool runtime_message_hits_mind_target(std::span<const u32> message, u32 actor, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at)
{
    if (mind.data() == nullptr || read_u32(mind, 0x10) == 0)
    {
        return false;
    }
    const RuntimeMindTarget* target = target_at(read_u32(mind, 0x10));
    return read_u32(target->bytes, 0x10) == message[1] && message[4] != 0 && message[2] != actor;
}

// Xbox 0x0013A540
bool runtime_strategy_target_within(float range, const RuntimeMindTarget& target, const Actor& actor)
{
    if (range == 100000.f) // DAT_0032B4AC.
    {
        return true;
    }
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI actor.
    const long double dx = static_cast<long double>(position.x) - read_f32(target.bytes, 0x20);
    const long double dy = static_cast<long double>(position.y) - read_f32(target.bytes, 0x24);
    const long double dz = static_cast<long double>(position.z) - read_f32(target.bytes, 0x28);
    const long double distance = std::sqrt(dy * dy + dx * dx + dz * dz);
    return static_cast<long double>(range) > distance;
}

// Xbox 0x000F4E10
void keep_nearer_tutorial_actor(RuntimeTutorialNotice& notice, const Actor& actor, const Actor* player)
{
    if (player == nullptr)
    {
        return;
    }
    const float candidate = float(runtime_actor_distance(*player, actor));
    if (runtime_actor_distance(*player, *notice.actor) > static_cast<long double>(candidate))
    {
        notice.actor = &actor;
    }
}

// Xbox 0x000F4870
void queue_runtime_tutorial_notice(const Actor& actor, u32 id, RuntimeTutorialNotices& context)
{
    if ((context.seen[id >> 3] & u8(1u << (id & 7))) != 0)
    {
        return;
    }
    if (context.pending == 0)
    {
        context.first = &actor;
        context.first_id = id;
        context.pending = 1;
        return;
    }
    if (context.first->class_key == actor.class_key)
    {
        return;
    }
    for (i32 index = 0; index < context.count; ++index)
    {
        if (context.queued[size_t(index)].actor->class_key == actor.class_key)
        {
            keep_nearer_tutorial_actor(context.queued[size_t(index)], actor, context.player);
            return;
        }
    }
    if (context.count == 4)
    {
        return;
    }
    context.queued[size_t(context.count)].actor = &actor;
    context.queued[size_t(context.count)].id = id;
    ++context.count;
}

// Xbox 0x0013A430
u32 notice_runtime_strategy_target(u32 id, RuntimeStrategyTargetChecks& context)
{
    std::span<const u8> body = context.goals.entry.body;
    if (read_u16(body, 0x94) == 0 && read_u16(body, 0x96) != 0)
    {
        return 0;
    }
    const RuntimePlayerGoalRing* ring = context.goals.entry.ring;
    if (ring != nullptr && front_goal_kind(ring, context.goals.entry.address_zero) != 0)
    {
        return 0;
    }
    queue_runtime_tutorial_notice(context.goals.entry.actor, id, context.notices);
    return 1;
}

// Xbox 0x0013A4F0
u32 notice_runtime_strategy_hostile_target(u32 id, RuntimeStrategyTargetChecks& context)
{
    std::span<u8> mind = mind_of(context.goals);
    const u32 previous = read_u32(mind, 0x10);
    RuntimeMindTarget* target;
    if (!select_runtime_strategy_hostile_target(target, 0, context.goals))
    {
        return 0;
    }
    const u32 noticed = notice_runtime_strategy_target(id, context);
    if (noticed == 0 && previous == 0)
    {
        put(mind, 0x10, previous);
    }
    return noticed;
}

// Xbox 0x00138700
u32 resume_runtime_strategy_for_target(std::span<const u32> message, RuntimeStrategyTargetChecks& context)
{
    std::span<const u8> mind = mind_of(context.goals);
    if (read_u32(mind, 0x10) == 0)
    {
        return 0;
    }
    const RuntimeMindTarget* target = context.goals.target_at(read_u32(mind, 0x10));
    std::span<const u8> strategy = strategy_of(context.goals);
    if (read_u32(target->bytes, 0x10) != message[1] || read_u16(strategy, 0xE) != 0x3F4)
    {
        return 0;
    }
    change_state(read_u16(strategy, 0x18), context.goals);
    return 1;
}

// Xbox 0x00138740
u32 retreat_runtime_strategy_from_target(std::span<const u32> message, RuntimeStrategyTargetChecks& context)
{
    std::span<const u8> mind = mind_of(context.goals);
    if (read_u32(mind, 0x10) == 0)
    {
        return 0;
    }
    const RuntimeMindTarget* target = context.goals.target_at(read_u32(mind, 0x10));
    if (read_u32(target->bytes, 0x10) != message[1] || runtime_strategy_abandons_target(*target, context) == 0
        || read_u16(strategy_of(context.goals), 0xE) == 0x3F4)
    {
        return 0;
    }
    change_state(0x3F4, context.goals);
    return 1;
}

// Xbox 0x00138790
u32 review_runtime_strategy_target(RuntimeStrategyTargetChecks& context)
{
    std::span<const u8> mind = mind_of(context.goals);
    if (read_u32(mind, 0x10) == 0
        || runtime_player_can_resume_strategy(context.goals.entry.actor_bytes, context.goals.resume) == 0)
    {
        return 0;
    }
    const RuntimeMindTarget* target = context.goals.target_at(read_u32(mind, 0x10));
    const u32 abandon = runtime_strategy_abandons_target(*target, context);
    std::span<const u8> strategy = strategy_of(context.goals);
    if (read_u16(strategy, 0xE) == 0x3F4)
    {
        if (abandon != 0)
        {
            return 0;
        }
        change_state(read_u16(strategy, 0x18), context.goals);
        return 1;
    }
    if (abandon == 0)
    {
        return 0;
    }
    change_state(0x3F4, context.goals);
    return 1;
}

// Xbox 0x001384F0
u32 react_runtime_strategy_message(std::span<const u32> message, RuntimeStrategyTargetChecks& context)
{
    switch (message[0])
    {
    case 0x55:
    case 0x71:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x81:
    case 0x83:
        if (!reactions_enabled(context.goals))
        {
            return 0;
        }
        return review_runtime_strategy_target(context);
    case 0x73:
        if (!reactions_enabled(context.goals))
        {
            return 0;
        }
        return resume_runtime_strategy_for_target(message, context);
    case 0x74:
        if (!reactions_enabled(context.goals))
        {
            return 0;
        }
        return retreat_runtime_strategy_from_target(message, context);
    default:
        return 0;
    }
}

// Xbox 0x001395C0
u32 react_runtime_strategy_contact(std::span<const u32> message, float chance, RuntimeStrategyTargetChecks& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    if (!reactions_enabled(goals))
    {
        return 0;
    }
    if (0.f > chance)
    {
        chance = 0.5f; // DAT_0032B0F4.
    }
    const bool settled = body_settled(goals.entry.body);
    std::span<const u8> mind = mind_of(goals);
    const u32 actor_address = goals.address_of(goals.entry.actor_bytes.data());
    const RuntimeMindTarget* target = mind.data() != nullptr && read_u32(mind, 0x10) != 0
        ? goals.target_at(read_u32(mind, 0x10)) : nullptr;
    if (target != nullptr && read_u32(target->bytes, 0x10) == message[1] && message[4] != 0)
    {
        if (message[2] != actor_address && !settled)
        {
            return 0;
        }
        if (!roll(goals.random, chance))
        {
            return 0;
        }
        change_state(0x3F2, goals);
        return 1;
    }
    if (message[1] == actor_address || !runtime_message_names_mind_target(message, mind, goals.target_at)
        || !settled || !actor_event_chance(goals.random, chance))
    {
        return 0;
    }
    change_state(0x418, goals);
    return 1;
}

// Xbox 0x001398A0
u32 react_runtime_strategy_cower(RuntimeStrategyTargetChecks& context)
{
    if (!reactions_enabled(context.goals))
    {
        return 0;
    }
    if (!body_settled(context.goals.entry.body) || !roll(context.goals.random, 1.f))
    {
        return 0;
    }
    change_state(0x418, context.goals);
    return 1;
}

// Xbox 0x001399A0
u32 react_runtime_strategy_startle(RuntimeStrategyTargetChecks& context)
{
    if (!reactions_enabled(context.goals))
    {
        return 0;
    }
    if (!body_settled(context.goals.entry.body) || !roll(context.goals.random, 1.f))
    {
        return 0;
    }
    change_state(0x3F2, context.goals);
    return 1;
}

// Xbox 0x001383A0
u32 retreat_runtime_strategy_from_reached_target(RuntimeStrategyTargetChecks& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    RuntimeMindTarget* target;
    select_runtime_strategy_target(target, 0, goals);
    if (target == nullptr)
    {
        return 0;
    }
    const RuntimePlayerAttachmentEligibility body{goals.entry.body, *goals.body_bits, goals.entry.actor, nullptr};
    if (runtime_body_target_surface_distance(goals.entry.actor, body, target->bytes) > 50.f) // DAT_0032B1E0.
    {
        return 0;
    }
    if (runtime_strategy_abandons_target(*target, context) == 0)
    {
        return 0;
    }
    goals.room = nullptr; // Actor +0xCA0.
    change_state(0x3F4, goals);
    return 1;
}

// Xbox 0x00139FA0
RuntimePlayerMindRequest* request_runtime_strategy_position(const Vec3& position, std::span<const u8> settings,
    RuntimePlayerStrategyGoals& context)
{
    RuntimeMindGoalContext& goals = context.entry.goals;
    RuntimePlayerMindRequest* request = allocate_runtime_mind_request(goals.anchor, 0x2E, goals.sizes);
    std::span<u8> bytes = request->bytes;
    put_float(bytes, 0x1C, 1.f); // DAT_0032B0F8.
    put_float(bytes, 0x20, 1.f);
    put_float(bytes, 0x28, 30.f); // DAT_0032B108.
    put_float(bytes, 0x10, position.x);
    put_float(bytes, 0x14, position.y);
    put_float(bytes, 0x18, position.z);
    put(bytes, 0x24, read_u32(settings, 8));
    put(bytes, 0x28, read_u32(settings, 0xC));
    put(bytes, 0x1C, read_u32(settings, 0x10));
    put(bytes, 0x20, read_u32(settings, 0x14));
    return request;
}

// Xbox 0x00138310
bool select_runtime_strategy_roam_position(Vec3& output, float heading, float range, float width,
    RuntimeStrategyTargetChecks& context)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    std::span<const u8> actor = goals.entry.actor_bytes;
    const float selected = randomize_runtime_navigation_heading(heading, width, goals.random);
    const Vec3 position = actor_world_position(goals.entry.actor);
    RuntimeNavigationHeadingSearch search{position, i32(read_u32(actor, 0xCEC)), range - 0.5f, range + 0.5f,
        goals.room, goals.entry.allowed, &output, 1, 0x1C, i16(read_u16(actor, 6)), goals.random,
        context.occupancy_initial};
    return search_runtime_navigation_headings(search, selected, std::bit_cast<float>(0x3E9C61AAu), 10);
}

// Xbox 0x0013A0A0
RuntimePlayerMindRequest* request_runtime_strategy_roam(std::span<const u8> settings,
    RuntimeStrategyTargetChecks& context)
{
    Vec3 position;
    if (!select_runtime_strategy_roam_position(position, read_f32(context.goals.entry.actor_bytes, 0x18),
        read_f32(settings, 4), read_f32(settings, 0), context))
    {
        return nullptr;
    }
    return request_runtime_strategy_position(position, settings, context.goals);
}

// Xbox 0x001372A0
i32 runtime_mind_contact_count(const RuntimeMindEvents& events, u32 attacker)
{
    const u32 owner = read_u32(events.mind, 8);
    for (LiveScript* node = events.anchor.next; node != nullptr; node = node->next)
    {
        if (u16(node->type) != 1)
        {
            continue;
        }
        const auto& event = *static_cast<RuntimeMindEvent*>(node);
        if (read_u32(event.bytes, 0x20) == owner && read_u32(event.bytes, 0x24) == attacker)
        {
            return std::bit_cast<i32>(read_u32(event.bytes, 0x28));
        }
    }
    return 0;
}

// Xbox 0x001372F0
bool runtime_mind_watches_actor(const RuntimeMindEvents& events, u32 actor)
{
    for (LiveScript* node = events.anchor.next; node != nullptr; node = node->next)
    {
        if (u16(node->type) == 4 && read_u32(static_cast<RuntimeMindEvent*>(node)->bytes, 0x20) == actor)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x001408A0
bool runtime_message_lands_on_actor(std::span<const u32> message, u32 actor)
{
    return message[1] == actor && message[4] != 0;
}

// Xbox 0x001408C0, EAX actor, ECX message
bool runtime_message_names_mind_target_actor(std::span<const u32> message, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at)
{
    if (mind.data() == nullptr || read_u32(mind, 0x10) == 0)
    {
        return false;
    }
    return read_u32(target_at(read_u32(mind, 0x10))->bytes, 0x10) == message[1];
}

// Xbox 0x001408E0, ECX message, EDX actor
bool runtime_message_from_mind_target(std::span<const u32> message, u32 actor, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at)
{
    return runtime_message_names_mind_target_actor(message, mind, target_at) && message[2] == actor;
}

// Xbox 0x00140910, EAX actor, ECX message
bool runtime_message_lands_from_mind_target(std::span<const u32> message, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at)
{
    return runtime_message_names_mind_target_actor(message, mind, target_at) && message[4] != 0;
}

// Xbox 0x00140A00, EAX message, ECX actor
bool runtime_message_source_targets_mind_target(std::span<const u32> message, u32 actor, std::span<const u8> mind,
    RuntimeStrategyActorViews& views)
{
    if (message[1] != actor || message[2] == 0)
    {
        return false;
    }
    const u32 source_target = read_u32(views.actor_bytes_at(message[2]), 0x8F8);
    if (source_target == 0)
    {
        return false;
    }
    const u32 target = mind.data() == nullptr ? 0 : read_u32(mind, 0x10);
    return target == source_target;
}

// Xbox 0x00041560, ESI actor, EBX target
long double runtime_target_distance(const Actor& actor, std::span<const u8> target)
{
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const long double dx = static_cast<long double>(position.x) - read_f32(target, 0x20);
    const long double dy = static_cast<long double>(position.y) - read_f32(target, 0x24);
    const long double dz = static_cast<long double>(position.z) - read_f32(target, 0x28);
    return std::sqrt(dz * dz + dx * dx + dy * dy);
}

// Xbox 0x0002B9F0, EAX strategy, ECX timer
bool count_down_runtime_strategy_timer(float& timer, RuntimePlayerStrategyGoals& context)
{
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.entry.actor);
    if (!(timer > 0.f))
    {
        return false;
    }
    timer = timer - rate * step;
    if (0.f < timer)
    {
        return false;
    }
    timer = 0.f;
    return true;
}

// Xbox 0x000BABF0, EAX actor
bool runtime_mind_target_within(float reach, RuntimePlayerStrategyGoals& context)
{
    const std::span<const u8> mind = context.entry.events.mind;
    const u32 target = mind.empty() ? 0u : read_u32(mind, 0x10);
    if (target == 0)
    {
        return false;
    }
    const std::span<const u8> record = context.target_at(target)->bytes;
    const Vec3 position = actor_world_position(context.entry.actor); // 0x000412A0
    const float dx = position.x - read_f32(record, 0x20);
    const float dz = position.z - read_f32(record, 0x28);
    const float dy = position.y - read_f32(record, 0x24);
    return dx * dx + dz * dz + dy * dy < reach * reach;
}

// Xbox 0x000B5C50, EAX actor
bool runtime_mind_target_within_second(float reach, RuntimePlayerStrategyGoals& context)
{
    return runtime_mind_target_within(reach, context);
}

// Xbox 0x0002BA60
bool count_down_runtime_strategy_live_timer(float& timer, RuntimePlayerStrategyGoals& context)
{
    if (context.resume.references[1] != 0 || context.resume.decay_enabled[context.resume.decay_index] == 0)
    {
        return false;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.entry.actor);
    if (!(timer > 0.f))
    {
        return false;
    }
    timer = timer - rate * step;
    if (0.f < timer)
    {
        return false;
    }
    timer = 0.f;
    return true;
}

// Xbox 0x00139550, EAX strategy, EBX message, ECX force
u32 adopt_runtime_strategy_message_attacker(std::span<const u32> message, u32 force,
    RuntimeStrategyTargetChecks& context, RuntimeStrategyActorViews& views)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    RuntimeMindEvents& events = goals.entry.events;
    const u32 actor = read_u32(goals.entry.strategy, 8);
    if (!reactions_enabled(goals, force) || message[1] != actor || message[2] == 0) // 0x0002B840
    {
        return 0;
    }
    const u32 record = read_u32(views.actor_bytes_at(message[2]), 0x8F8);
    if (record == 0)
    {
        return 0;
    }
    RuntimeMindTarget* target = goals.target_at(record);
    std::span<const u8> actor_bytes = goals.entry.actor_bytes;
    if (target->bytes[0x18] != 0
        || !runtime_mind_target_is_eligible(events.mind, actor_bytes, goals.entry.actor,
            goals.address_of(actor_bytes.data()), *target, true)) // 0x00029C80, EDX 1
    {
        return 0;
    }
    set_runtime_mind_target(events, record); // 0x0002A030
    put(events.mind, 0xC, message[2]);
    return 1;
}

// Xbox 0x000415A0, ESI actor, EBX target
float runtime_squared_target_distance(const Actor& actor, std::span<const u8> target)
{
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float dz = position.z - read_f32(target, 0x28);
    const float dx = position.x - read_f32(target, 0x20);
    const float dy = position.y - read_f32(target, 0x24);
    return (dz * dz + dx * dx) + dy * dy;
}

bool runtime_strategy_reactions_enabled(RuntimePlayerStrategyGoals& context)
{
    return reactions_enabled(context);
}

// Xbox 0x001363E0
float runtime_actor_body_gap(const Actor& first, const Actor& second, RuntimeStrategyActorViews& views)
{
    const std::span<const u8> first_body = views.body_of(first).bytes;
    const std::span<const u8> second_body = views.body_of(second).bytes;
    const float distance = float(runtime_actor_distance(first, second));
    const float first_scale = first.radius_xz;
    const float reach = read_f32(second_body, 0x78) * second.radius_xz;
    float held_radius = 0.f;
    const u32 held = read_u32(second_body, 0x48);
    if (held != 0 && views.actor_bytes_at(held)[0x93D] == 1)
    {
        const Actor& weapon = *views.actor_at(held);
        if (weapon.model != nullptr && weapon.model->cpu.data() != nullptr)
        {
            held_radius = float(actor_model_sweep_radius(weapon));
        }
    }
    const float radii = read_f32(second_body, 0x888) * second.radius_xz + read_f32(first_body, 0x888) * first_scale
        + (held_radius + reach);
    const float gap = distance - radii;
    if (0.f < gap)
    {
        return gap;
    }
    return 0.f;
}

// Xbox 0x00139390
u32 retaliate_runtime_strategy_contact(std::span<const u32> message, i32 minimum, RuntimeStrategyTargetChecks& context,
    RuntimeStrategyActorViews& views)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    RuntimeMindEvents& events = goals.entry.events;
    const u32 actor = read_u32(goals.entry.strategy, 8);
    if ((views.story_flags & 0x100) != 0 || message[1] != actor)
    {
        return 0;
    }
    const u32 attacker = message[2];
    if (attacker == 0)
    {
        return 0;
    }
    const std::span<const u8> attacker_body = views.body_of(*views.actor_at(attacker)).bytes;
    if (std::bit_cast<i32>(read_u32(attacker_body, 0x738)) < minimum || !reactions_enabled(goals))
    {
        return 0;
    }
    const i32 contacts = runtime_mind_contact_count(events, attacker);
    const bool landed = runtime_message_lands_on_actor(message, actor);
    const u32 current = read_u32(events.mind, 0x10);
    clear_runtime_player_mind_requests(*goals.setup.mind);
    if (current != 0 && !landed && contacts <= 0)
    {
        return 0;
    }
    const u32 record = read_u32(views.actor_bytes_at(attacker), 0x8F8);
    if (record == 0)
    {
        return 0;
    }
    RuntimeMindTarget* target = goals.target_at(record);
    if (target->bytes[0x18] != 0 || runtime_strategy_abandons_target(*target, context) != 0)
    {
        return 0;
    }
    set_runtime_mind_target(events, record);
    put(events.mind, 0xC, attacker);
    return 1;
}

// Xbox 0x00139740
u32 runtime_strategy_target_hit_lands(std::span<const u32> message, RuntimeStrategyTargetChecks& context,
    RuntimeStrategyActorViews& views)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    const std::span<const u8> mind = goals.entry.events.mind;
    const u32 actor = read_u32(goals.entry.strategy, 8);
    if (mind.data() == nullptr || read_u32(mind, 0x10) == 0)
    {
        return 0;
    }
    const RuntimeMindTarget* target = goals.target_at(read_u32(mind, 0x10));
    if (read_u32(target->bytes, 0x10) != message[1] || message[2] != actor || message[4] == 0)
    {
        return 0;
    }
    const RuntimeStrategyBodyView body = views.body_of(*views.actor_at(message[1]));
    if ((body.bits->bytes[3] & 2) != 0)
    {
        return 1;
    }
    if ((body.bits->bytes[2] & 2) != 0)
    {
        return runtime_body_target_state_active(body.bytes) ? 1 : 0;
    }
    return 0;
}

// Xbox 0x001397B0
u32 react_runtime_strategy_watched_actor(std::span<const u32> message, RuntimeStrategyTargetChecks& context,
    RuntimeStrategyActorViews& views)
{
    RuntimePlayerStrategyGoals& goals = context.goals;
    const RuntimeMindEvents& events = goals.entry.events;
    if (!reactions_enabled(goals) || read_u32(events.mind, 0xC) != message[1]
        || !runtime_mind_watches_actor(events, message[1]))
    {
        return 0;
    }
    Actor* nearest = find_nearest_runtime_script_actor(nullptr, goals.entry.actor, 0, 0.f, nullptr, 0xC00, nullptr,
        views.query);
    if (nearest != nullptr && 40.f > runtime_actor_body_gap(goals.entry.actor, *nearest, views)) // DAT_0032B1B8.
    {
        return 0;
    }
    change_state(0x3F2, goals);
    return 1;
}

// Xbox 0x001414F0, EAX target
bool runtime_strategy_target_watching_player(const RuntimeMindTarget& target)
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
}
