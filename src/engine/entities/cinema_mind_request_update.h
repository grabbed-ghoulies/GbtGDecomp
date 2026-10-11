#pragma once
#include "cinema_mind_movement.h"
#include "cinema_mind_control.h"
#include "player_mind_requests.h"

namespace gbtg
{
struct RuntimeCinemaMindRequestUpdate
{
    RuntimeCinemaMindMovement& movement;
    RuntimeCinemaMindSpawnMovement& spawn;
    RuntimeParentMindRequests& parent;
    RuntimeMindChainRequestSelection& chains;
    std::span<u8> body;
    std::span<u8> input;
    const RuntimeInputReleaseContext& input_context;
    const std::span<const u8>& sound_resource; // Aliases lifecycle resource acquired during construction.
    RuntimeActorSoundQueues& sounds;
    const RuntimeBodyDamageTables& damage_tables;
    std::span<const float> mode_scales;
    RuntimeBodyActionSelection& actions;
    std::function<u16()> effect_action_key; // Actual body vtable +0x38.
    std::function<u16()> contact_action_key; // Actual body vtable +0x3C.
    std::function<u16(RuntimeBodyAttachedActor&)> attach; // Actual body vtable +0x5C returns the action key.
};
i32 update_runtime_cinema_mind_request(RuntimeCinemaMindRequestUpdate& context,
    std::span<u8> request); // Xbox 0x0007C0E0
i32 update_runtime_cinema_mind_facing_request(std::span<const u8> mind,
    std::span<u8> body, const Actor& actor, std::span<const u8> target,
    std::span<const u8> request); // Xbox 0x0007C0E0, facing cases 0x25..0x27.
}
