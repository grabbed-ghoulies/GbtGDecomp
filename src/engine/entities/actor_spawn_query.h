#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
struct ActorSpawnLink
{
    ActorSpawnLink* previous = nullptr;
    ActorSpawnLink* next = nullptr;
    Actor* actor = nullptr; // native owner of retail +0x934 link
};
bool select_actor_spawn_point(const Actor& actor, const Vec3& query, Vec3& point,
    float& radius, i32& index); // Xbox 0x001476D0
bool actor_spawn_path_clear(const Actor& actor, const Vec3& query, u8 flags); // Xbox 0x00129CD0
Actor* find_nearest_spawn_actor(const Vec3& query, const Actor* requester,
    float* distance, u8 flags); // Xbox 0x00129D70
}
