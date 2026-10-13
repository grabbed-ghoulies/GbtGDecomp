#include "actor_sound_tick.h"
#include "../runtime/link_exchange.h"

namespace gbtg
{
namespace
{
float squared_listener_distance(const RuntimeActorSound& sound, const Vec3& listener)
{
    const float z = listener.z - sound.position.z;
    const float y = listener.y - sound.position.y;
    const float x = listener.x - sound.position.x;
    return (z * z + y * y) + x * x;
}
}

// Xbox 0x0012A510
void update_active_actor_sound_voices(RuntimeActorSoundGroup& group,
    RuntimeActorSoundTick& context)
{
    auto* link = group.active.next;
    while (link != nullptr)
    {
        auto& sound = *static_cast<RuntimeActorSound*>(link);
        auto* next = sound.next;
        auto& initial_voice = context.voices.voices[i16(sound.previous_bone)];
        const u32 state = initial_voice.state;
        if (initial_voice.cue == nullptr || (state != 2 && state != 3 && state != 5 && state != 6 && state != 7))
        {
            if (sound.owner != nullptr)
            {
                context.message(*sound.owner, 0x8A, sound);
            }
            auto& voice = context.voices.voices[i16(sound.previous_bone)];
            if (voice.cue == nullptr)
            {
                voice.state = 10;
            }
            else
            {
                switch (voice.state)
                {
                case 2:
                case 3:
                    voice.state = 4;
                    break;
                case 5:
                case 6:
                case 7:
                    voice.state = 9;
                    stop_runtime_sound_cue(voice.cue->runtime, 0, context.voices.core.stopped_count,
                        context.voices.core.sound);
                    break;
                default:
                    break;
                }
            }
            sound.previous_bone = 0xFFFF;
            sound.enabled = 1;
            if (sound.previous != nullptr)
            {
                sound.previous->next = sound.next;
            }
            if (sound.next != nullptr)
            {
                sound.next->previous = sound.previous;
            }
            sound.previous = nullptr;
            sound.next = nullptr;
        }
        else
        {
            if (sound.owner != nullptr)
            {
                context.message(*sound.owner, 0x8B, sound);
            }
            auto& position_voice = context.voices.voices[i16(sound.previous_bone)];
            if (position_voice.source != nullptr)
            {
                set_audio_core_source_position(*position_voice.source, sound.position, context.voices.core.sources);
            }
            if (sound.owner != nullptr)
            {
                context.message(*sound.owner, 0x8C, sound);
            }
            auto& direction_voice = context.voices.voices[i16(sound.previous_bone)];
            if (direction_voice.source != nullptr)
            {
                set_audio_core_source_cone(*direction_voice.source, sound.direction, context.voices.core.sources);
            }
            sound.enabled = 0;
        }
        link = next;
    }
}

// Xbox 0x0012A6E0
void sort_pending_actor_sounds(RuntimeActorSoundGroup& group, const Vec3& listener)
{
    auto* link = group.pending.next;
    if (link != nullptr)
    {
        auto& first = *static_cast<RuntimeActorSound*>(link);
        first.elapsed = squared_listener_distance(first, listener);
        auto* next = first.next;
        while (next != nullptr)
        {
            auto& sound = *static_cast<RuntimeActorSound*>(next);
            auto* previous = sound.previous;
            next = sound.next;
            sound.elapsed = squared_listener_distance(sound, listener);
            while (previous->previous != nullptr && !(sound.elapsed >= static_cast<RuntimeActorSound*>(previous)->elapsed))
            {
                auto* before = previous->previous;
                exchange_runtime_links<RuntimeSoundLink>(sound, *previous);
                previous = before;
            }
        }
    }
}

// Xbox 0x0012A150
void tick_runtime_actor_sounds(RuntimeActorSoundQueues& queues, RuntimeActorSoundTick& context)
{
    for (u32 index = 0; index < 2; ++index)
    {
        auto& group = queues.groups[index];
        update_active_actor_sound_voices(group, context);
        sort_pending_actor_sounds(group, context.listener);
        const std::function<i16(RuntimeActorSound&)> start = [&](RuntimeActorSound& sound)
        {
            const i16 selected = allocate_audio_event_voice(1, context.voices);
            if (selected != -1)
            {
                auto& voice = context.voices.voices[selected];
                voice.stop = 0;
                if (voice.source != nullptr)
                {
                    set_audio_core_source_position(*voice.source, sound.position, context.voices.core.sources);
                }
                if (voice.source != nullptr)
                {
                    set_audio_core_source_cone(*voice.source, sound.direction, context.voices.core.sources);
                }
                prepare_audio_event_voice(selected, sound.name.data(), context.voices);
            }
            return selected;
        };
        const std::function<void(Actor&, RuntimeActorSound&)> started = [&](Actor& actor, RuntimeActorSound& sound)
        {
            context.message(actor, 0x89, sound);
        };
        dispatch_pending_actor_sounds(group, start, started);
    }
}
}
