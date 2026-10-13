#pragma once

#include "effect_collision_response.h"
#include "effect_debris.h"
#include "effect_notifications.h"
#include "../audio/footstep_names.h"

namespace gbtg
{
struct RuntimeEffectCollisionCommands
{
    RuntimeEffectCollision& collision;
    RuntimeEffectDebris& debris;
    AudioEventVoices& voices;
    ActorEffectStreamContext& effects;
    const FootstepSoundNames& footsteps;
    u32& callback_result; // DAT_00506C50.
};
bool execute_effect_collision_commands(std::span<u8> particle,
    const ActorEffectNode& owner, RuntimeEffectCollisionCommands& context); // Xbox 0x000F10F0
void notify_effect_particle_collision(std::span<u8> particle,
    const ActorEffectNode& owner, RuntimeEffectCollisionCommands& context); // Xbox 0x000F18C0
}
