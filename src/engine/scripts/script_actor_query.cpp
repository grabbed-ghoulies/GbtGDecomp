#include "script_actor_query.h"
#include "../entities/actor_spawn_eligibility.h"
#include "../entities/actor_transform.h"
#include "../entities/player_mind_events.h"
#include "../navigation/navigation.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x001292D0
Actor* find_runtime_script_player(u32 player_link,
    const std::unordered_map<u32, Actor*>& addresses)
{
    if (player_link != 0)
    {
        return addresses.at(player_link - 0x93C);
    }
    return nullptr;
}

// Xbox 0x00127BC0, type 0x6F
int wait_runtime_script_player(u32 player_link,
    const std::unordered_map<u32, Actor*>& addresses,
    const RuntimeScriptActorQueryContext& context)
{
    const Actor* actor = find_runtime_script_player(player_link, addresses);
    if (actor == nullptr ||
        (script_actor_current_mask(*actor, context) != 0 && actor->mask_loading == 0))
    {
        return 0;
    }
    return 2;
}

// Xbox 0x00028D20
bool script_actor_has_active_marker(const RuntimeScriptActorQueryView& candidate)
{
    if (candidate.marker != nullptr
        && (candidate.actor.mask_mode == 2 || candidate.actor.mask_flag != 0))
    {
        return true;
    }
    return false;
}
// Xbox 0x00028D00
bool script_actor_is_active(const Actor& actor)
{
    if (actor.mask_mode == 2)
    {
        return true;
    }
    return actor.mask_flag != 0;
}
// Xbox 0x00028C10
Actor* script_actor_reference_source(const RuntimeScriptActorQueryView& candidate)
{
    if (candidate.reference != nullptr)
    {
        return candidate.reference->source;
    }
    return nullptr;
}
// Xbox 0x0003B910
bool script_actor_has_ancestor(Actor* actor, Actor* ancestor)
{
    while (true)
    {
        if (actor == nullptr)
        {
            return false;
        }
        if (actor == ancestor)
        {
            return true;
        }
        ActorBind* bind = actor->bind;
        i32 mode;
        if (bind == nullptr)
        {
            mode = 0;
        }
        else if (bind->parent == nullptr)
        {
            if (actor->link == nullptr || actor->link->head == nullptr)
            {
                engine_heap_free(bind);
                actor->bind = nullptr;
                mode = 0;
            }
            else
            {
                mode = 2;
            }
        }
        else
        {
            mode = 1;
        }
        if (mode == 1)
        {
            actor = bind->parent;
        }
        else
        {
            if (mode != 2)
            {
                return false;
            }
            actor = actor->link->owner;
        }
    }
}
// Xbox 0x0001EF10
bool script_mind_test_bit(const RuntimeScriptMindQuery& mind, u32 bit)
{
    return (mind.bits->bytes[bit >> 3] & (1u << (bit & 7))) != 0;
}
// Xbox 0x0001D440
bool script_mind_is_idle(const RuntimeScriptMindQuery& mind, i32 allow_permitted)
{
    if (mind.suspended == 0
        && (mind.active == 0 || (allow_permitted != 0 && mind.permitted != 0))
        && !(mind.timer > 0.f)
        && mind.state == 0 && mind.state_time == 0.f
        && (mind.bits->bytes[3] & 8) == 0)
    {
        if (!script_mind_test_bit(mind, 0x2C))
        {
            return !script_mind_test_bit(mind, 0x2A);
        }
    }
    return false;
}
// Xbox 0x00028360
u16 script_actor_current_mask(const Actor& actor,
    const RuntimeScriptActorQueryContext& context)
{
    const EntityMaskSpan* span = actor.mask_span;
    if (span == nullptr)
    {
        return 0;
    }
    u16 value;
    if (span->end == span->mark)
    {
        std::memcpy(&value, context.address_zero, sizeof(value));
        return value;
    }
    const u32 offset = static_cast<u32>(span->index) * span->stride;
    std::memcpy(&value, span->data + offset, sizeof(value));
    return value;
}
// Xbox 0x00129420
u32 matches_runtime_script_actor_query(RuntimeScriptActorQueryView& candidate,
    Actor* requester, u32 flags, const RuntimeScriptActorQueryContext& context)
{
    Actor& actor = candidate.actor;
    if (&actor == requester)
    {
        return 0;
    }
    RuntimeScriptMindQuery* mind = candidate.mind;
    if ((flags & 0x20000) != 0 && (mind->bits->bytes[7] & 0x10) != 0)
    {
        return 0;
    }
    if ((flags & 0x40) != 0 && mind->energy == 0 && mind->maximum_energy != 0)
    {
        return 0;
    }
    if ((flags & 4) != 0 && (mind->bits->bytes[3] & 0x20) == 0)
    {
        return 0;
    }
    if ((flags & 2) != 0 && mind->blocked != 0)
    {
        return 0;
    }
    if ((flags & 8) != 0 && script_actor_has_active_marker(candidate))
    {
        return 0;
    }
    if ((flags & 0x80) != 0 && mind->case_actor == nullptr)
    {
        return 0;
    }
    if ((flags & 0x100) != 0 && requester != nullptr
        && script_actor_has_ancestor(&actor, requester))
    {
        return 0;
    }
    if ((flags & 0x200) != 0 && actor.mask_mode != 1
        && (actor.mask_mode != 5 || (flags & 1) == 0))
    {
        return 0;
    }
    if ((flags & 0x400) != 0
        && (requester == nullptr || candidate.reference == nullptr
            || candidate.reference->binding == nullptr || candidate.reference->binding->owner != requester))
    {
        return 0;
    }
    if ((flags & 0x800) != 0 && !script_mind_is_idle(*mind, 0) && mind->active == 0)
    {
        return 0;
    }
    if ((flags & 0x1000) != 0 && !script_actor_is_active(actor))
    {
        return 0;
    }
    if ((flags & 0x2000) != 0 && !script_actor_has_active_marker(candidate))
    {
        return 0;
    }
    if ((flags & 0x8000) != 0 && mind->case_actor != nullptr
        && actor_spawn_owner_unavailable(*mind->case_actor))
    {
        return 0;
    }
    if ((flags & 0x10000) != 0 && script_mind_test_bit(*mind, 0x28))
    {
        return 0;
    }
    if ((flags & 0x40000) != 0 && requester != nullptr
        && script_actor_reference_source(candidate) != requester)
    {
        return 0;
    }
    if ((flags & 0x80000) != 0 && script_actor_current_mask(actor, context) != 0)
    {
        return 0;
    }
    if ((flags & 0x100000) == 0 || (mind->node != nullptr && mind->node->active != nullptr))
    {
        return 1;
    }
    return 0;
}
// Xbox 0x001291A0
i32 count_runtime_script_actor_tags(u32 flags, i16 tag, RuntimeScriptActorPredicate predicate,
    RuntimeScriptActorQueryContext& context)
{
    i32 count = 0;
    for (scene_detail::RangeLink* link = context.first; link != nullptr; link = link->next)
    {
        auto& candidate = context.views.at(link->host);
        if (matches_runtime_script_actor_query(candidate, nullptr, flags, context) != 0
            && candidate.tag == static_cast<u16>(tag)
            && (predicate == nullptr || predicate(nullptr, &candidate.actor) != 0))
        {
            ++count;
        }
    }
    return count;
}

// Xbox 0x001295E0
bool runtime_script_actor_path_open(u32 flags, const Actor* requester, const Actor& candidate)
{
    if ((flags & 0x4000) != 0 && requester != nullptr)
    {
        const Vec3 destination = actor_world_position(candidate);
        const Vec3 source = actor_world_position(*requester);
        i32 last_cell;
        if (!entity_detail::navigation_path_clear(source, destination, 0, 0, 0, 0, nullptr, &last_cell,
                i16(requester->class_key)))
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00128DF0
Actor* find_nearest_runtime_script_actor(const Vec3* point, Actor& requester, i16 tag, float radius,
    float* distance, u32 flags, RuntimeScriptActorPredicate predicate, const RuntimeScriptActorQueryContext& context)
{
    Actor* found = nullptr;
    Vec3 origin;
    if (point == nullptr)
    {
        origin = actor_world_position(requester);
    }
    else
    {
        origin = *point;
    }
    float best = radius * radius;
    if (!(radius > 0.0f))
    {
        best = std::bit_cast<float>(0x7F7FFFFFu); // DAT_0032B310.
    }
    for (scene_detail::RangeLink* link = context.first; link != nullptr; link = link->next)
    {
        auto& candidate = context.views.at(link->host);
        if ((tag == 0 || i16(candidate.tag) == tag)
            && matches_runtime_script_actor_query(candidate, &requester, flags, context) != 0)
        {
            const Vec3 position = actor_world_position(candidate.actor);
            const float dy = origin.y - position.y;
            const float dx = origin.x - position.x;
            const float dz = origin.z - position.z;
            const float measured = dz * dz + dy * dy + dx * dx;
            if (best > measured && runtime_script_actor_path_open(flags, &requester, candidate.actor)
                && (predicate == nullptr || predicate(&requester, &candidate.actor) != 0))
            {
                found = &candidate.actor;
                best = measured;
            }
        }
    }
    if (found != nullptr && distance != nullptr)
    {
        *distance = float(retail_square_root(best));
    }
    return found;
}

// Xbox 0x00128F70
Actor* find_nearest_runtime_script_actor_in_angle(Actor& requester, float heading,
    float half_angle, float radius, float* angle, float* distance, u32 flags,
    RuntimeScriptActorPredicate predicate, const RuntimeScriptActorQueryContext& context)
{
    Actor* found = nullptr;
    const Vec3 origin = actor_world_position(requester);
    if (!(radius > 0.f))
    {
        radius = std::bit_cast<float>(0x47C35000u); // DAT_0032B4AC.
    }
    float best = radius * radius;
    const float pi = std::bit_cast<float>(0x40490FDBu);
    const float turn = std::bit_cast<float>(0x40C90FDBu);
    float picked_angle = heading + pi;
    for (scene_detail::RangeLink* link = context.first; link != nullptr; link = link->next)
    {
        auto& candidate = context.views.at(link->host);
        if (matches_runtime_script_actor_query(candidate, &requester, flags, context) != 0)
        {
            const Vec3 position = actor_world_position(candidate.actor);
            const float dy = origin.y - position.y;
            const float dx = origin.x - position.x;
            const float dz = origin.z - position.z;
            const float measured = (dz * dz + dy * dy) + dx * dx;
            const Vec3 direction{position.x - origin.x, position.y - origin.y,
                position.z - origin.z};
            float candidate_angle;
            runtime_direction_angle(direction, candidate_angle);
            const long double remainder = animation_remainder(candidate_angle - heading, turn);
            float relative = float(remainder);
            if (static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)) > remainder)
            {
                relative += turn;
            }
            if (relative >= pi)
            {
                relative -= turn;
            }
            const float magnitude = float(animation_absolute(relative));
            if (best > measured && half_angle > magnitude
                && runtime_script_actor_path_open(flags, &requester, candidate.actor)
                && (predicate == nullptr || predicate(&requester, &candidate.actor) != 0))
            {
                found = &candidate.actor;
                best = measured;
                picked_angle = candidate_angle;
            }
        }
    }
    if (found != nullptr)
    {
        if (angle != nullptr)
        {
            *angle = picked_angle;
        }
        if (distance != nullptr)
        {
            *distance = float(retail_square_root(best));
        }
    }
    return found;
}
}
