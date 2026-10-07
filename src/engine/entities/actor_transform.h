#pragma once

#include "entity_types.h"

namespace gbtg
{
// namespace

Vec3 actor_world_position(const Actor& actor);

// DAT_0050DD80

Vec3 actor_world_euler(const Actor& actor);

// Xbox 0x00041730
void actor_set_world_position(Actor& actor, const Vec3& point); // Xbox 0x00041730

} // namespace gbtg
