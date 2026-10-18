#pragma once

#include "gbtg/anim.h"

#include "../scene/scene_types.h"
#include "entity_types.h"

#include <functional>
#include <span>

namespace gbtg
{
bool spawn_resource_under_class_limit(std::span<ExecutableResourcePool> resources,
    const u32* class_limits, const char* name); // Xbox 0x0013E860
scene_detail::PulseNode* find_actor_by_group(u16 caller_key); // Xbox 0x00129F00
scene_detail::PulseNode* find_actor_by_id(u16 key); // Xbox 0x00129ED0 (legacy PulseNode view)
// The DAT_00511AA4 object whose +0x1FC word is the key, as its retail address; 0 when the key is 0 or unmatched.
u32 find_runtime_actor_by_id(u16 key, const std::function<u32(const entity_detail::LatchNode&)>& owner,
    const std::function<std::span<u8>(u32)>& bytes_at); // Xbox 0x00129ED0, DX key
}
