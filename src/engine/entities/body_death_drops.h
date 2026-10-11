#pragma once
#include "body_action_selection.h"
#include "actor_template_creation.h"
#include <vector>

namespace gbtg
{
struct RuntimeBodyDeathDrops
{
    Actor& actor;
    std::span<u8> actor_bytes;
    std::span<ExecutableResourcePool> resources;
    ObjectFactoryRegistry& factory;
    std::vector<FactoryObject>& objects;
    const std::unordered_map<const u8*, RuntimeBodyAttachedActor*>& created;
    const Vec3& default_position;
    const Vec3& default_euler;
    u32& random;
    float occupancy_initial; // Native view of retained stack scratch in occupancy query.
};
bool take_runtime_body_drop_names(std::span<u8> actor,
    char* name, char* variant); // Xbox 0x000286B0
void spawn_runtime_body_death_drop(RuntimeBodyDeathDrops& context); // Xbox 0x00135080
}
