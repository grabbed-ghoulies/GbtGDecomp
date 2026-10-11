#pragma once
#include "actor_clock.h"
#include <array>

namespace gbtg
{
bool separate_runtime_player_world_contacts(std::span<const u8> body, Actor& actor,
    const EntityMaskHost& mask, const Vec3& stationary); // Xbox 0x000755A0
void separate_runtime_player_body_contacts(std::span<u8>& body, Actor& actor,
    EntityBitBlock& bits, const EntityMaskHost& mask, i32 floor,
    const Vec3& stationary); // Xbox 0x000744F0
// Body +0x340: a u32 index followed by 0x50 Vec3 samples.
i32 record_runtime_body_motion(std::span<u8> history, const Vec3& delta, const Actor& owner,
    const RuntimeActorClock& clock); // Xbox 0x00025A30, ECX history, EDX delta, EAX owner

// Retail 0x58-byte collision sphere in the body +0x710 array.
struct RuntimeBodyCollisionSphere
{
    std::array<u8, 0x54> contact{}; // 0x000FCD70 copy of the first segment record; +0x26 marks a hit.
    const EntityShapeNode* node = nullptr; // +0x54.
};
struct RuntimeBodyCollision
{
    std::span<u8> body; // Position +0x18, bytes +0x714..+0x717.
    Actor& owner; // Body +8.
    const EntityBitBlock* bits; // Body +0x15C.
    RuntimeBodyCollisionSphere* spheres; // Body +0x710.
    const EntityMaskHost& mask; // Body +8 fields read by 0x0001FE00.
    const RuntimeActorClock& clock;
};
u8 push_runtime_body_sphere(RuntimeBodyCollision& context, RuntimeBodyCollisionSphere& sphere,
    const float* world); // Xbox 0x00026100
void push_runtime_body_spheres(RuntimeBodyCollision& context); // Xbox 0x00025F00, ESI body
bool sweep_runtime_player_body_floor(std::span<u8> body, Actor& actor,
    EntityBitBlock& bits, const EntityMaskHost& mask, i32 floor,
    const Vec3& start, const Vec3& base, Vec3& candidate,
    float radius, float height); // Xbox 0x00075350
}
