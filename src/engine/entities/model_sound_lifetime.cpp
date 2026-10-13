#include "model_sound_lifetime.h"
#include "gbtg/anim.h"

namespace gbtg
{
// Xbox 0x000ED2E0
void release_runtime_model_sound_object(RuntimeModelSoundObject& object,
    RuntimeModelSoundRelease& services)
{
    release_actor_sound(object.sound_2dc, services.sounds);
    if (object.resource_144.data() != nullptr)
    {
        release_animation_resource(services.registry, object.resource_144);
        object.resource_144 = {};
    }
    if (object.model_158 != nullptr)
    {
        const RuntimeModelAsset* source = defer_runtime_model_release(*object.model_158,
            services.deferred, services.fence, services.registry, services.shaders, services.heap_free);
        release_animation_resource(services.registry, source->cpu);
        object.model_158 = nullptr;
    }
    if (object.resource_134.data() != nullptr)
    {
        release_animation_resource(services.registry, object.resource_134);
        object.resource_134 = {};
    }
    services.heap_free(object.allocation);
}
}
