#include "actor_spawn_eligibility.h"

#include <cstdint>

namespace gbtg
{
// Xbox 0x00146150
bool actor_can_supply_ghoulybox(const Actor& actor)
{
    const ActorLink* link = actor.link;
    if ((link == nullptr || link->head == nullptr ||
        reinterpret_cast<uintptr_t>(link) == 0x918) &&
        (u8(actor.mask_flag) != 0 && actor.spawn_blocked == 0))
    {
        return actor.spawn_point_count != 0;
    }
    return false;
}
// Xbox 0x00146080
bool actor_accepts_spawn_class(const Actor& candidate, const Actor& requester)
{
    i32 index = 0;
    if (candidate.accepted_class_count > 0)
    {
        const ActorAcceptedClass* slot = candidate.accepted_classes;
        while (slot->key != requester.class_key)
        {
            ++index;
            ++slot;
            if (candidate.accepted_class_count <= index)
            {
                return false;
            }
        }
        if (index != -1)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x001460C0
bool actor_spawn_owner_unavailable(const Actor& actor)
{
    const u8 state = u8(u32(actor.mask_flag) >> 24); // +0x93F shares the +0x93C dword.
    if (state != 1 && (state != 2 ||
        (actor.spawn_owner != nullptr &&
            (actor.spawn_owner->mask_mode == 2 || actor.spawn_owner->mask_flag != 0))))
    {
        return false;
    }
    return true;
}

// Xbox 0x00145470
bool actor_has_spawn_model(const Actor& actor)
{
    if (actor.spawn_model_blocked != 0)
    {
        return false;
    }
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 metadata = read_u32(cpu, 0x10);
        const u32 flag = read_u32(cpu, metadata + 0x10);
        if (flag != 0)
        {
            return cpu[flag] != 0;
        }
    }
    return false;
}

// Xbox 0x00129C50
bool actor_matches_spawn_filter(const Actor& candidate, const Actor* requester, u8 flags)
{
    if (requester != nullptr && (flags & 1) != 0 &&
        !actor_accepts_spawn_class(candidate, *requester))
    {
        return false;
    }
    const ActorLink* link = candidate.link;
    if (((flags & 2) != 0 && link != nullptr && link->head != nullptr &&
            reinterpret_cast<uintptr_t>(link) != 0x918) ||
        ((flags & 4) != 0 && u8(candidate.mask_flag) == 0))
    {
        return false;
    }
    if ((flags & 0x10) != 0 && !actor_can_supply_ghoulybox(candidate))
    {
        return false;
    }
    if ((flags & 0x20) != 0 && actor_spawn_owner_unavailable(candidate))
    {
        return false;
    }
    if ((flags & 0x40) != 0 && !actor_has_spawn_model(candidate))
    {
        return false;
    }
    return true;
}

}
