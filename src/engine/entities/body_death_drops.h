#pragma once
#include "body_action_selection.h"
#include "actor_template_creation.h"
#include <functional>
#include <vector>

namespace gbtg
{
struct RuntimeBodyDeathDrops
{
    Actor& actor;
    std::span<u8> actor_bytes;
    std::span<ExecutableResourcePool> resources;
    ObjectFactoryRegistry& factory;
    std::function<u32(FactoryObject)> keep; // The scene keeps a created object; its address, or 0 for none.
    std::function<void(u32 actor, std::span<const u32>)> message; // The created actor's vtable +0xC.
    const Vec3& default_position;
    const Vec3& default_euler;
    u32& random;
    float occupancy_initial; // Native view of retained stack scratch in occupancy query.
};
bool take_runtime_body_drop_names(std::span<u8> actor,
    char* name, char* variant); // Xbox 0x000286B0
void spawn_runtime_body_death_drop(RuntimeBodyDeathDrops& context); // Xbox 0x00135080
}
