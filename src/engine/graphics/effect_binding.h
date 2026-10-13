#pragma once
#include "actor_effect_nodes.h"
#include "gbtg/entity.h"

namespace gbtg
{
void sample_actor_effect_shape(const Actor* actor, u32 mask, const u8*& cached,
    Vec3& position, float& radius); // Xbox 0x000EC830
void sample_actor_effect_locator(const Actor* actor, i16 kind, const u8*& cached,
    Vec3& position, Vec3& euler); // Xbox 0x000EC8E0
void sample_actor_effect_joint(const Actor* actor, i32 joint, Vec3& position,
    Vec3& euler); // Xbox 0x000EC9D0
void update_actor_effect_size(ActorEffectNode& effect,
    const std::function<u32(const u8*)>& address); // Xbox 0x000EEA00
void prepare_actor_effect_binding(ActorEffectNode& effect, float step,
    const std::function<u32(const u8*)>& address); // Xbox 0x000EEB60
}
