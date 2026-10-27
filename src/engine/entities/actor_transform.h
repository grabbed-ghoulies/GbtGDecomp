#pragma once

#include "entity_types.h"

namespace gbtg
{
// namespace

Vec3 actor_world_position(const Actor& actor);
long double runtime_actor_distance(const Actor& first,
    const Actor& second); // Xbox 0x00041490
float runtime_actor_squared_distance(const Actor& first,
    const Actor& second); // Xbox 0x000414F0
// The record's point (+0x20..+0x28) lies nearer the actor than `radius`.
bool runtime_actor_point_within(const Actor& actor, std::span<const u8> record,
    float radius); // Xbox 0x000416C0, ESI actor, EBX record

// DAT_0050DD80

Vec3 actor_world_euler(const Actor& actor);

// Xbox 0x00041730
void actor_set_world_position(Actor& actor, const Vec3& point); // Xbox 0x00041730
void actor_set_uniform_scale(Actor& actor, float scale); // Xbox 0x00041B90

} // namespace gbtg
