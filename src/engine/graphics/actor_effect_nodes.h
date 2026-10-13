#pragma once

#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"
#include "gbtg/model_runtime.h"

namespace gbtg
{
struct ActorEffectNode
{
    std::span<u8> bytes;
    std::span<const u8> script; // native resource pointer at +0x134
    std::span<const u8> texture; // +0x144
    void* binding_actor; // +0xC0
    i32 binding_joint; // +0xD8
    RuntimeActorSound sound; // native sound slot at +0x2DC
    std::unique_ptr<RuntimeModelInstance> model; // +0x158
};
struct ActorEffectModelContext
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    const RuntimeModelResourceViews& views;
    RuntimeModelDeferredPool& deferred;
    u32& fence;
    std::span<const RuntimeModelNodeType> types;
    RuntimeShaderRegistrationState& shaders;
    RuntimeShaderAnimationScratch& scratch;
    const std::function<void(u8*)>& heap_free;
};
extern std::array<ActorEffectNode*, 150> g_actor_effect_nodes; // DAT_00506930
extern i32 g_last_actor_effect; // DAT_0042A434

void initialize_actor_effect_free_chain(ActorEffectNode& effect); // Xbox 0x000ED170
ActorEffectNode* create_actor_effect_node(const char* name, u16 count,
    const Vec3& position, const Vec3& euler, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x000ED1C0
i32 allocate_actor_effect_slot(i32 category, const char* name, u16 count,
    const Vec3& position, const Vec3& euler, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x000ED550
void reset_actor_effect_to_debris(ActorEffectNode& effect,
    ActorEffectModelContext& context); // Xbox 0x000ED6E0
void recycle_actor_effect_particles(ActorEffectNode& effect); // Xbox 0x000EF790
void set_actor_effect_movement(ActorEffectNode& effect, u32 mode, const Vec3& velocity,
    const Vec3& minimum, const Vec3& maximum, i32 relative); // Xbox 0x000EF7F0
void set_actor_effect_script(ActorEffectNode& effect, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x000ED120
bool retain_last_actor_effect(float duration); // Xbox 0x0013E750
float actor_effect_event_duration(i32 enabled, std::span<const u8> event,
    const ActorAnimationState& animation); // Xbox 0x0003A9A0
void set_actor_effect_model(ActorEffectNode& effect, const char* name,
    ActorEffectModelContext& context); // Xbox 0x000ED8C0
u32 set_actor_effect_texture(ActorEffectNode& effect, const char* name, float rate,
    float spread, u32 setting, i32 mode, i32 enabled, i32 blend,
    ActorEffectModelContext& context); // Xbox 0x000ED730
}
