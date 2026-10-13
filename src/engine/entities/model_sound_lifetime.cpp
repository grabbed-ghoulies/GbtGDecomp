#include "model_sound_lifetime.h"
#include "gbtg/anim.h"

#include <memory>

namespace gbtg
{
// Xbox 0x000ED2E0
void release_actor_effect_node(ActorEffectNode& effect, RuntimeModelSoundRelease& services)
{
    release_actor_sound(effect.sound, services.sounds);
    if (effect.texture.data() != nullptr)
    {
        release_animation_resource(services.registry, effect.texture);
        effect.texture = {};
    }
    if (effect.model != nullptr)
    {
        const RuntimeModelAsset* source = defer_runtime_model_release(*effect.model,
            services.deferred, services.fence, services.registry, services.shaders, services.heap_free);
        release_animation_resource(services.registry, source->cpu);
        services.deferred.records.back().owner = std::move(effect.model);
    }
    if (effect.script.data() != nullptr)
    {
        release_animation_resource(services.registry, effect.script);
        effect.script = {};
    }
    std::destroy_at(&effect);
    services.heap_free(reinterpret_cast<u8*>(&effect));
}
}
