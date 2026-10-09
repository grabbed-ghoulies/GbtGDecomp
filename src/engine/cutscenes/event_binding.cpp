#include "event_binding.h"

namespace gbtg
{
// Xbox 0x0004C7B0
RuntimeActorCreationOptions& cutscene_actor_effect_options(const Actor& actor, void* binding)
{
    g_actor_creation_options.flags = 0;
    const Vec3 position = actor_world_position(actor);
    const Vec3 euler = actor_world_euler(actor);
    g_actor_creation_options.user = binding;
    g_actor_creation_options.position = position;
    g_actor_creation_options.euler = euler;
    g_actor_creation_options.flags |= 0xD;
    return g_actor_creation_options;
}
}
