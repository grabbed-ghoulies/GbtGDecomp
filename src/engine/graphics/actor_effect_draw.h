#pragma once
#include "actor_effect_nodes.h"
#include "billboard_queue.h"
#include "scene_model_draw.h"

namespace gbtg
{
struct RuntimeDisplaySetupContext;
// One live particle (node +0x390 + index * 0x150) of a type-0 effect queued as a billboard.
void draw_actor_effect_particle(ActorEffectNode& effect, size_t particle, BillboardQueue& queue); // Xbox 0x000EFEA0
// One live particle of a type-2 effect drawn as a world sprite quad (0x0005CF90).
void draw_actor_effect_sprite(ActorEffectNode& effect, size_t particle,
    RuntimeDisplaySetupContext& display); // Xbox 0x000F0010, ESI particle
// The effect's model (+0x158) at the particle's position, euler and scale, tinted by its colour, through 0x000F9080.
// `model_cpu` is the model's CPU resource address.
void draw_actor_effect_model(ActorEffectNode& effect, size_t particle, u32 model_cpu,
    RuntimeDisplaySetupContext& display, SceneModelDrawContext& models); // Xbox 0x000F0160, EAX particle
// The model at a world position, euler (y, x, z) and scale, built on DAT_0050DD70 from the identity.
void draw_runtime_model_at(RuntimeModelInstance& model, u32 model_cpu, const Vec3* position, const Vec3* euler,
    const Vec3* scale, u32 kind, RuntimeDisplaySetupContext& display,
    SceneModelDrawContext& models); // Xbox 0x000F8EA0, EAX position, EDI euler, ESI scale, EBX kind
// The effect's live particles (+0x5A == -2) by its draw type (+0x13C); returns how many were drawn. Type 2 draws
// between the world sprite state (0x0005CF20, 0x0005DFF0) and the projection rebind; type 4 draws the effect's model.
i32 draw_actor_effect_particles(ActorEffectNode& effect, BillboardQueue& queue,
    RuntimeDisplaySetupContext& display, SceneModelDrawContext& models,
    u32 model_cpu); // Xbox 0x000ED390, EAX effect
}
