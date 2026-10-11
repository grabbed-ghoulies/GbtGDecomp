#pragma once
#include "player_mind_targets.h"

namespace gbtg
{
bool runtime_mind_target_is_movable(const RuntimeMindTarget& target); // Xbox 0x00141580
bool runtime_mind_target_is_eligible(std::span<const u8> mind,
    std::span<const u8> actor_bytes, const Actor& actor, u32 actor_address,
    const RuntimeMindTarget& target, bool adjust_for_actor_state); // Xbox 0x00029C80
}
