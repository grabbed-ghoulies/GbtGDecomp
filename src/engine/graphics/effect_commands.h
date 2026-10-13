#pragma once
#include "actor_effect_nodes.h"

namespace gbtg
{
bool execute_actor_effect_commands(std::span<u8> particle, ActorEffectNode& owner, u32& cursor,
    const std::function<std::span<const u8>(u32)>& resolve, u32& random,
    const i32& pal, const i32& override_pal); // Xbox 0x000F0390
}
