#include "event_voices.h"
#include "bank_lookup.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00039AA0
void pause_audio_event_voice(i16 index, i32 paused, AudioEventVoices& context)
{
    auto& voice = context.voices[index];
    if (voice.cue != nullptr)
    {
        switch (voice.state)
        {
        case 2:
            if (paused != 0)
            {
                voice.state = 3;
                return;
            }
            break;
        case 3:
            if (paused == 0)
            {
                voice.state = 2;
                return;
            }
            break;
        case 5:
        case 6:
        case 7:
            voice.state = (paused != 0) + 6;
            pause_runtime_sound_cue(voice.cue->runtime, static_cast<u32>(paused), context.core.sound);
            break;
        }
    }
}

// Xbox 0x00039620
void initialize_event_voice_reverb(AudioCoreSource& source, AudioEventVoices& context)
{
    const auto selected = context.reverb_curves[context.reverb_index];
    const auto defaults = context.reverb_curves[0];
    const std::array<u32, 9> values{0, 0, 0, 0, 0, selected[0], selected[1], defaults[0], defaults[1]};
    set_audio_core_source_reverb(source, values, context.core.sources);
}

// Xbox 0x00039660
void release_audio_event_voice(AudioEventVoice& voice, AudioEventVoices& context)
{
    if (voice.state != 0)
    {
        if (voice.source == nullptr)
        {
            voice.state = 0;
        }
        else
        {
            destroy_audio_core_source(voice.source, context.core.sources);
            if (voice.source == nullptr)
            {
                voice.state = 0;
            }
        }
    }
}

// Xbox 0x00039830
i16 allocate_audio_event_voice(u32 mode, AudioEventVoices& context)
{
    for (i16 index = 0; index < 48; ++index)
    {
        AudioEventVoice& voice = context.voices[index];
        if (voice.state == 0)
        {
            voice.state = 0;
            voice.stop = 0;
            voice.word_c = 0;
            create_audio_core_source(mode, voice.source, context.core.sources);
            if (voice.source == nullptr)
            {
                release_audio_event_voice(voice, context);
                return -1;
            }
            voice.mode = mode;
            if (mode == 1)
            {
                set_audio_core_source_position(*voice.source, context.origin, context.core.sources);
                set_audio_core_source_velocity(*voice.source, context.origin, context.core.sources);
                set_audio_core_source_cone(*voice.source, context.default_cone, context.core.sources);
                initialize_event_voice_reverb(*voice.source, context);
            }
            voice.state = 1;
            return index;
        }
    }
    return -1;
}

// Xbox 0x00039A20
bool prepare_audio_event_voice(i16 index, const char* name, AudioEventVoices& context)
{
    AudioEventVoice& voice = context.voices[index];
    i32 bank;
    i32 cue;
    if (resolve_audio_bank_cue(name, &bank, &cue))
    {
        prepare_audio_core_cue(*context.banks[bank], static_cast<u32>(cue), *voice.source,
            voice.cue, audio_event_cue_callback, &context, context.core);
        if (voice.cue != nullptr)
        {
            set_audio_core_cue_user_data(*voice.cue, &voice);
            voice.state = 2;
            return true;
        }
        voice.state = 8;
    }
    return false;
}

// Xbox 0x00039B30
void position_audio_event_voice_ahead(i16 index, float distance, AudioEventVoices& context)
{
    auto& voice = context.voices[index];
    if (voice.source != nullptr)
    {
        voice.retained_14 = std::bit_cast<u32>(distance);
        voice.word_c = 1;
        const float* listener = context.listener;
        const Vec3 position{listener[7] * distance + listener[0],
            listener[8] * distance + listener[1], listener[9] * distance + listener[2]};
        set_audio_core_source_position(*voice.source, position, context.core.sources);
    }
}

// Xbox 0x00039510
i16 request_spatial_audio_event_voice(const char* name, u32 stop, float distance,
    AudioEventVoices& context)
{
    if (distance == 0.f)
    {
        distance = 5.f; // DAT_0032B184
    }
    const i16 index = allocate_audio_event_voice(1, context);
    if (index != -1)
    {
        context.voices[index].stop = stop;
        position_audio_event_voice_ahead(index, distance, context);
        prepare_audio_event_voice(index, name, context);
    }
    return index;
}

// Xbox 0x00039580
i16 request_positioned_audio_event_voice(const char* name, u32 stop,
    const Vec3& position, AudioEventVoices& context)
{
    const i16 index = allocate_audio_event_voice(1, context);
    if (index != -1)
    {
        AudioEventVoice& voice = context.voices[index];
        voice.stop = stop;
        if (voice.source != nullptr)
        {
            set_audio_core_source_position(*voice.source, position, context.core.sources);
        }
        prepare_audio_event_voice(index, name, context);
    }
    return index;
}

// Xbox 0x000395E0
i16 request_audio_event_voice(const char* name, u32 stop, AudioEventVoices& context)
{
    const i16 index = allocate_audio_event_voice(0, context);
    if (index != -1)
    {
        context.voices[index].stop = stop;
        prepare_audio_event_voice(index, name, context);
    }
    return index;
}

// Xbox 0x000398E0
void notify_audio_event_voice(AudioEventVoice* voice, u32 state, AudioEventVoices& context)
{
    if (state == 0)
    {
        voice->state = 6;
    }
    else if (state == 1)
    {
        if (voice->cue != nullptr)
        {
            free_audio_core_cue(voice->cue, context.core);
        }
        switch (voice->state)
        {
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
            voice->state = 8;
            return;
        case 9:
            voice->state = 10;
            return;
        default:
            break;
        }
    }
    else if (state == 3)
    {
        play_audio_core_cue(*voice->cue, context.core);
        const u32 previous = voice->state;
        voice->state = 5;
        if (previous == 3)
        {
            // The Xbox computes this slot from (voice - DAT_004E0CE8)/0x24.
            const i16 index = static_cast<i16>(voice - context.voices.data());
            AudioEventVoice& selected = context.voices[index];
            if (selected.cue != nullptr)
            {
                switch (selected.state)
                {
                case 2:
                    selected.state = 3;
                    return;
                case 5:
                case 6:
                case 7:
                    selected.state = 7;
                    pause_runtime_sound_cue(selected.cue->runtime, 1, context.core.sound);
                    return;
                default:
                    break;
                }
            }
        }
        else if (previous == 4)
        {
            RuntimeSoundVoice runtime{voice->state, &voice->cue->runtime};
            stop_runtime_sound_voice(runtime, 1, context.core.stopped_count, context.core.sound);
            voice->state = runtime.state;
            return;
        }
    }
}

// Xbox 0x00039A00
u32 audio_event_cue_callback(void* owner, AudioCoreCue& cue, u32 state)
{
    auto& context = *static_cast<AudioEventVoices*>(owner);
    auto* voice = static_cast<AudioEventVoice*>(audio_core_cue_user_data(cue));
    notify_audio_event_voice(voice, state, context);
    return 0;
}

// Xbox 0x000396F0
void tick_audio_event_voices(AudioEventVoices& context)
{
    for (AudioEventVoice& voice : context.voices)
    {
        const u32 state = voice.state;
        if (state == 6)
        {
            if (voice.mode == 1)
            {
                set_audio_core_source_cone(*voice.source, context.default_cone, context.core.sources);
                initialize_event_voice_reverb(*voice.source, context);
                if (voice.word_c != 0)
                {
                    const float ahead = std::bit_cast<float>(voice.retained_14);
                    const float* listener = context.listener;
                    const Vec3 position{listener[7] * ahead + listener[0],
                        listener[8] * ahead + listener[1], listener[9] * ahead + listener[2]};
                    set_audio_core_source_position(*voice.source, position, context.core.sources);
                }
            }
        }
        else if (state == 8)
        {
            if (voice.stop != 0)
            {
                release_audio_event_voice(voice, context);
            }
        }
        else if (state == 10)
        {
            release_audio_event_voice(voice, context);
        }
    }
}
// 0x00039690 (stop_runtime_sound_voice) over the event voice DAT_004E0CE8[index].
void stop_audio_event_voice(i16 index, i32 immediate, AudioEventVoices& context)
{
    auto& voice = context.voices[size_t(index)];
    RuntimeSoundVoice view{voice.state, voice.cue == nullptr ? nullptr : &voice.cue->runtime};
    stop_runtime_sound_voice(view, immediate, context.core.stopped_count, context.core.sound);
    voice.state = view.state;
}
}
