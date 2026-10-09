#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
bool actor_accepts_spawn_class(const Actor& candidate, const Actor& requester); // Xbox 0x00146080
bool actor_spawn_owner_unavailable(const Actor& actor); // Xbox 0x001460C0
bool actor_has_spawn_model(const Actor& actor); // Xbox 0x00145470
bool actor_matches_spawn_filter(const Actor& candidate, const Actor* requester, u8 flags); // Xbox 0x00129C50
bool actor_can_supply_ghoulybox(const Actor& actor); // Xbox 0x00146150
}
