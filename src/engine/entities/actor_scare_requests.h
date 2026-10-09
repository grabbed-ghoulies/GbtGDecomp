#pragma once

#include "entity_types.h"
#include "gbtg/actor_spawn_factory.h"

namespace gbtg
{
struct ActorWorldMessageNode
{
    entity_detail::ListHead link;
    void (*message)(ActorWorldMessageNode&, const void*);
};
struct ActorScareContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    std::vector<FactoryObject>& objects;
    u32& flags;
    u32& player_address; // DAT_00511B40, original actor+0x93C token
    const Vec3& default_euler; // DAT_0050DD74
};
void broadcast_actor_world_message(const void* message); // Xbox 0x00122CE0
long double actor_model_height(const Actor& actor); // Xbox 0x0003DB90
void request_actor_scare_at(const Vec3& position, u32 actor_address, float intensity,
    float scale, i32 broadcast, u32 value, ActorScareContext& context); // Xbox 0x0013B270
void request_actor_scare(Actor& actor, u32 actor_address, float intensity, float scale,
    i32 broadcast, u32 value, ActorScareContext& context); // Xbox 0x0013B3A0
}
