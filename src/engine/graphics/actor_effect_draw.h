#pragma once
#include "actor_effect_nodes.h"
#include "billboard_queue.h"

namespace gbtg
{
struct RuntimeDisplaySetupContext;
// One live particle (node +0x390 + index * 0x150) of a type-0 effect queued as a billboard.
void draw_actor_effect_particle(ActorEffectNode& effect, size_t particle, BillboardQueue& queue); // Xbox 0x000EFEA0
// One live particle of a type-2 effect drawn as a world sprite quad (0x0005CF90).
void draw_actor_effect_sprite(ActorEffectNode& effect, size_t particle,
    RuntimeDisplaySetupContext& display); // Xbox 0x000F0010, ESI particle
// The effect's live particles (+0x5A == -2) by its draw type (+0x13C); returns how many were drawn. Type 2 draws
// between the world sprite state (0x0005CF20, 0x0005DFF0) and the projection rebind.
i32 draw_actor_effect_particles(ActorEffectNode& effect, BillboardQueue& queue,
    RuntimeDisplaySetupContext& display); // Xbox 0x000ED390, EAX effect
}
