#include "event_pause.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0004C420
void stop_cutscene_event_voices(std::span<u8> event_state,
    AudioEventVoices& voices)
{
    i32 slot = 0;
    do
    {
        i16 index;
        std::memcpy(&index, event_state.data() + slot * 2, sizeof(index));
        if (index != -1)
        {
            auto& voice = voices.voices[index];
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
                    stop_runtime_sound_cue(voice.cue->runtime, 0,
                        voices.core.stopped_count, voices.core.sound);
                    break;
                }
            }
            index = -1;
            std::memcpy(event_state.data() + slot * 2, &index, sizeof(index));
        }
        ++slot;
    }
    while (slot < 3);
}

// Xbox 0x0004E640
void pause_cutscene_events(RuntimeCutsceneInstance& instance, i32 paused,
    CutsceneEventPauseContext& context)
{
    if (paused == 0)
    {
        --instance.pending[2]; // +158, reference count with retail word wraparound
    }
    else
    {
        ++instance.pending[2];
    }
    std::int8_t index = 0;
    if (0 < instance.actor_count)
    {
        do
        {
            pause_cutscene_actor(context.actors, index, paused, context.animations);
            index = static_cast<std::int8_t>(static_cast<u8>(index) + 1);
        }
        while (static_cast<i32>(index) < instance.actor_count);
    }
    if ((instance.flags & 0x10) == 0)
    {
        i32 slot = 0;
        do
        {
            i16 voice;
            std::memcpy(&voice, instance.event_state.data() + slot * 2, sizeof(voice));
            if (voice != -1)
            {
                pause_audio_event_voice(voice, paused, context.voices);
            }
            ++slot;
        }
        while (slot < 3);
    }
    if (instance.pending[4] != 0 && paused == 0 && instance.pending[2] == 0)
    {
        context.callbacks.at(instance.pending[4])(instance, instance.pending[5], paused);
    }
}
}
