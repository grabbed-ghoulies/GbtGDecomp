#pragma once
#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
void initialize_runtime_body_death_effect(std::span<u8, 0x55C> parameters,
    const Actor& actor, u32 actor_address, float first, float lifetime,
    const char* default_name); // Xbox 0x00068770
}
