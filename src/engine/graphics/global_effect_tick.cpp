#include "global_effect_tick.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x000ED5B0
void detach_actor_from_global_effects(void* actor,
    std::array<ActorEffectNode*, 150>& nodes)
{
    for (size_t index = 0; index < 150; ++index)
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
void release_global_actor_effects(std::array<ActorEffectNode*, 150>& nodes, RuntimeModelSoundRelease& release,
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
    for (size_t index = 0; index < 150; ++index)
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
        size_t offset = 0;
        do
        {
            const auto particle = context.debris.particles.subspan(offset, 0x74);
            if (i32(read_u32(particle, 0x68)) == -2)
            {
                tick_effect_debris(particle, context.debris, context.clock);
            }
            offset += 0x74;
        }
        while (offset < 0x16A8);
    }
    if (context.trails.particles.data() != nullptr)
    {
        size_t offset = 0;
        do
        {
            const auto particle = context.trails.particles.subspan(offset, 0x84);
            if (i32(read_u32(particle, 0x7C)) == -2)
            {
                tick_effect_trail(particle, context.trails, context.clock);
            }
            offset += 0x84;
        }
        while (offset < 0x19C8);
    }
}
}
