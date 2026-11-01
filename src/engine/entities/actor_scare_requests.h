#pragma once

#include "entity_types.h"
#include "gbtg/actor_spawn_factory.h"
#include <functional>

namespace gbtg
{
// Vtable +0xC of each DAT_00511B38 actor (its +0x934 node), the next node read before the call. Bound by the native
// scene.
extern std::function<void(std::span<const u32>)> g_marked_actor_message;
struct ActorScareContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    std::vector<FactoryObject>& objects;
    u32& flags;
    std::function<u32()> player_address; // DAT_00511B40, the first case node (the player's actor +0x93C), or 0
    const Vec3& default_euler; // DAT_0050DD74
};
void broadcast_actor_world_message(std::span<const u32> message); // Xbox 0x00122CE0, EDI the message
long double actor_model_height(const Actor& actor); // Xbox 0x0003DB90
void request_actor_scare_at(const Vec3& position, u32 actor_address, float intensity,
    float scale, i32 broadcast, u32 value, ActorScareContext& context); // Xbox 0x0013B270
void request_actor_scare(Actor& actor, u32 actor_address, float intensity, float scale,
    i32 broadcast, u32 value, ActorScareContext& context); // Xbox 0x0013B3A0
}
