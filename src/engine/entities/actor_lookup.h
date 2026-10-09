#pragma once

#include "gbtg/anim.h"

#include "../scene/scene_types.h"

namespace gbtg
{
bool spawn_resource_under_class_limit(std::span<ExecutableResourcePool> resources,
    const u32* class_limits, const char* name); // Xbox 0x0013E860
scene_detail::PulseNode* find_actor_by_group(u16 caller_key); // Xbox 0x00129F00
scene_detail::PulseNode* find_actor_by_id(u16 key); // Xbox 0x00129ED0
}
