#pragma once

#include "actor_effect_nodes.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct ActorEffectBinding
{
    i32 mode;
    Actor* actor;
    i32 index;
};
extern ActorEffectBinding g_actor_effect_binding; // DAT_0050690C
struct ActorEffectStreamContext
{
    ActorEffectModelContext& models;
    RuntimeActorSoundQueues& sounds;
    u32& random_state; // DAT_0054DD7C
    Vec3& default_position; // DAT_0050DD80
    Vec3& default_euler; // DAT_0050DD74
};
bool resource_has_type(const char* name, const char* type); // Xbox 0x00035870
bool equal_sound_names(const char* name, const char* expected); // Xbox 0x000380D0
u32 actor_effect_joint(const Actor& actor, i32 kind); // Xbox 0x00040AF0
void set_actor_effect_sound(ActorEffectNode& effect, const char* name,
    RuntimeActorSoundQueues& queues); // Xbox 0x000EF870
u32 interpret_actor_effect_nodes(ActorEffectNode* effect, const u8*& cursor,
    const RuntimeActorCreationOptions* options, ActorEffectStreamContext& context); // Xbox 0x000ED9F0
u32 interpret_named_actor_effect(ActorEffectNode* effect, const char* name,
    const RuntimeActorCreationOptions* options, ActorEffectStreamContext& context); // Xbox 0x000ED990
u32 interpret_new_actor_effect(const u8*& cursor, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context); // Xbox 0x000ED970
u32 reject_unused_effect_nodes(); // Xbox 0x00055A90
u32 dispatch_actor_effect_node(const u8*& cursor, i32& more,
    const RuntimeActorCreationOptions* options, ActorEffectStreamContext& context); // Xbox 0x00053060
u32 run_actor_effect_stream(const u8*& cursor, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context); // Xbox 0x00053140
u32 load_actor_effect_stream(const char* name, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context); // Xbox 0x00053170
bool actor_event_chance(u32& random_state, float chance); // Xbox 0x00019200
}
