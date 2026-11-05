#include "effect_notifications.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif

#include <cstring>

namespace gbtg
{
// Xbox 0x0013E610
void emit_positioned_actor_effect(const char* name, const Vec3& position,
    ActorEffectStreamContext& context)
{
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.euler = context.default_euler;
    g_actor_creation_options.flags = 5;
    if (load_actor_effect_stream(name, &g_actor_creation_options, context) != 0)
    {
        const i32 index = g_last_actor_effect;
        g_last_actor_effect = -1;
        if (index != -1)
        {
            effect_header(g_actor_effect_nodes[index]->bytes).flags |= 8;
        }
    }
}

// Xbox 0x000EE560
void notify_actor_effect_particle(const ActorEffectNode& owner, u32 collided,
    const Vec3& position, const u32& frame, u32& last_sound_frame,
    AudioEventVoices& voices, ActorEffectStreamContext& context)
{
    const ActorEffectHeader& header = effect_header(owner.bytes);
    const char* sound = collided == 0 ? header.particle_sound.data() : header.collision_sound.data();
    u32 now = frame;
#if !defined(GBTG_RETAIL_GAME_ONLY)
    now = rare_replay_retail_frame(frame); // Rare Replay: the spacing is in retail ticks.
#endif
    if (*sound != '\0' && now - last_sound_frame > 5)
    {
        request_positioned_audio_event_voice(sound, 1, position, voices);
        last_sound_frame = now;
    }
    u32 prefix;
    std::memcpy(&prefix, header.collision_effect.data(), sizeof(prefix));
    if (prefix == 0x5F646961u)
    {
        emit_positioned_actor_effect(
            header.collision_effect.data(), position, context);
    }
}
}
