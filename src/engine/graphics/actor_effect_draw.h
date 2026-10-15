#pragma once
#include "actor_effect_nodes.h"
#include "billboard_queue.h"

namespace gbtg
{
// One live particle (node +0x390 + index * 0x150) of a type-0 effect queued as a billboard; type 2 draws through
// 0x0005CA10.
void draw_actor_effect_particle(ActorEffectNode& effect, size_t particle, BillboardQueue& queue); // Xbox 0x000EFEA0
// The effect's live particles (+0x5A == -2) by its draw type (+0x13C); returns how many were drawn.
i32 draw_actor_effect_particles(ActorEffectNode& effect, BillboardQueue& queue); // Xbox 0x000ED390, EAX effect
}
