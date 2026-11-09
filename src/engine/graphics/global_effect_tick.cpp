#include "global_effect_tick.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x000ED5B0
void detach_actor_from_global_effects(void* actor,
    ActorEffectSlots& nodes)
{
    for (size_t index = 0; index < nodes.size(); ++index)
    {
        if (nodes[index] != nullptr && nodes[index]->binding_actor == actor)
        {
            auto& effect = *nodes[index];
            effect.binding_actor = nullptr;
            const u32 null = 0;
            const float stopped = -1.0f;
            std::memcpy(effect.bytes.data() + 0xC0, &null, 4);
            std::memcpy(effect.bytes.data() + 8, &stopped, 4);
        }
    }
}

// Xbox 0x000ED450
void release_global_actor_effects(ActorEffectSlots& nodes, RuntimeModelSoundRelease& release,
    RuntimeEffectDebrisStorage& debris, RuntimeEffectTrailStorage& trails, AnimationResourceRegistry& registry)
{
    for (ActorEffectNode*& node : nodes)
    {
        if (node != nullptr)
        {
            release_actor_effect_node(*node, release); // 0x000ED2E0, EAX the node
            node = nullptr;
        }
    }
    release_effect_debris(debris, registry); // 0x000EC3A0
    release_effect_trails(trails, registry); // 0x000EBBF0
}

// Xbox 0x000ED480
void tick_global_actor_effects(RuntimeGlobalEffectTick& context)
{
    for (size_t index = 0; index < context.nodes.size(); ++index)
    {
        if (context.nodes[index] != nullptr)
        {
            const float step = runtime_frame_step(context.clock);
            if (!tick_actor_effect(*context.nodes[index], step, context.effects))
            {
                release_actor_effect_node(*context.nodes[index], context.release);
                context.nodes[index] = nullptr;
            }
        }
    }
    if (context.debris.particles.data() != nullptr)
    {
        size_t index = 0;
        do
        {
            EffectDebrisRecord& particle = context.debris.particles[index];
            if (particle.next_free == -2)
            {
                tick_effect_debris(particle, context.debris, context.clock);
            }
            ++index;
        }
        while (index < kEffectDebrisCount);
    }
    if (context.trails.particles.data() != nullptr)
    {
        size_t index = 0;
        do
        {
            EffectTrailRecord& particle = context.trails.particles[index];
            if (particle.next_free == -2)
            {
                tick_effect_trail(particle, context.trails, context.clock);
            }
            ++index;
        }
        while (index < kEffectTrailCount);
    }
}
}
