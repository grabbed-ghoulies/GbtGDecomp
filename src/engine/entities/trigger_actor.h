#pragma once
#include "area_actor.h"
#include "ghouly_requests.h"
#include "ghouly_spawn_record.h"
#include "../scene/scene_marker_handlers.h"
#include "../graphics/actor_effect_stream.h"

namespace gbtg
{
struct RuntimeTriggerActorContext
{
    std::span<u8> bytes;
    Actor& actor;
    GhoulyController& controller;
    u32& random;
    float occupancy_initial;
    std::function<void(u32, std::span<u32>)> message; // Native actor vtable +0xC mapping.
    GhoulySpawnRecord*& load; // Actor +0xB44, native pointer view.
    const i32& blocked_players; // DAT_004E1C34 for 0x0013E980.
    std::function<void(std::span<u32>)> self_message; // This actor's actual vtable +0xC.
};
void copy_runtime_trigger_attributes(u8* destination, const u8* source); // Xbox 0x000280D0
void runtime_trigger_storage_sizes(ObjectSizes& sizes); // Xbox 0x00069600
bool initialize_runtime_trigger_actor(RuntimeTriggerActorContext& context,
    std::span<const u8> parameters); // Xbox 0x00069660
bool find_runtime_trigger_spawn_slot(std::span<u8> bytes, i32& slot); // Xbox 0x00069910
i32 query_runtime_trigger_pose(RuntimeTriggerActorContext& context, i32 select,
    Vec3& position, Vec3& euler); // Xbox 0x00069E00
bool query_runtime_trigger_spawn_pose(RuntimeTriggerActorContext& context,
    Vec3& source_position, Vec3& source_euler, Vec3& position,
    Vec3& euler); // Xbox 0x000697D0
void destroy_runtime_trigger_actor(RuntimeTriggerActorContext& context); // Xbox 0x000697A0
void update_runtime_trigger_actor(RuntimeTriggerActorContext& context); // Xbox 0x00069D20
struct RuntimeTriggerMessages
{
    RuntimeTriggerActorContext& trigger;
    ActorBaseContext& base;
    RuntimeMarkerObjects& objects;
    ActorEffectStreamContext& effects;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<u32(const void*)> address_of;
    std::function<Actor&(FactoryObject&)> actor_of;
    std::function<FactoryObject*(FactoryObject)> keep; // Native scene ownership, not a creation callee.
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message;
    // Required pointer-width adapter into the translated 0x0003EF70.
};
void dispatch_runtime_trigger_message(RuntimeTriggerMessages& context,
    std::span<u32> message); // Xbox 0x00069960
}
