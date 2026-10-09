#include "event_audio.h"
#include "../graphics/actor_effect_stream.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0004C3B0
void start_cutscene_voice_event(const u8* event, std::span<u8> event_state, AudioEventVoices& voices)
{
    g_actor_creation_options.flags = 0;
    const i16 index = allocate_audio_event_voice(1, voices);
    if (index != -1)
    {
        voices.voices[index].stop = 0;
        position_audio_event_voice_ahead(index, 5.f, voices); // DAT_0032B184
        prepare_audio_event_voice(index, reinterpret_cast<const char*>(event + 0x14), voices);
    }
    i32 slot;
    std::memcpy(&slot, event + 0x10, sizeof(slot));
    std::memcpy(event_state.data() + slot * 2, &index, sizeof(index));
}

// Xbox 0x0004C4A0
void dispatch_cutscene_voice_event(const u8* event, std::span<u8> event_state,
    u32 phase, AudioEventVoices& voices)
{
    if (phase == 0)
    {
        start_cutscene_voice_event(event, event_state, voices);
    }
    else if (phase == 1)
    {
        i32 slot;
        std::memcpy(&slot, event + 0x10, sizeof(slot));
        i16 index;
        std::memcpy(&index, event_state.data() + slot * 2, sizeof(index));
        if (index != -1)
        {
            auto& voice = voices.voices[index];
            RuntimeSoundVoice runtime{voice.state, voice.cue == nullptr ? nullptr : &voice.cue->runtime};
            stop_runtime_sound_voice(runtime, 0, voices.core.stopped_count, voices.core.sound);
            voice.state = runtime.state;
            index = -1;
            std::memcpy(event_state.data() + slot * 2, &index, sizeof(index));
        }
    }
}

// Xbox 0x0004D460
void release_cutscene_stream(std::span<u8> event_state, const char* name, AudioBankStreams& audio)
{
    auto* current = reinterpret_cast<char*>(event_state.data() + 0x86);
    if (equal_sound_names(current, name))
    {
        release_audio_stream(name, audio);
        current[0] = 0;
    }
}

// Xbox 0x0004D490
void release_cutscene_bank(std::span<u8> event_state, const char* name, AudioBankStreams& audio)
{
    auto* current = reinterpret_cast<char*>(event_state.data() + 6);
    if (equal_sound_names(current, name) && name[0] != 0)
    {
        release_tracked_audio_bank(name, audio);
        current[0] = 0;
    }
}

// Xbox 0x0004D4C0
void replace_cutscene_stream(std::span<u8> event_state, const char* name, AudioBankStreams& audio)
{
    auto* current = reinterpret_cast<char*>(event_state.data() + 0x86);
    if (current[0] != 0)
    {
        if (equal_sound_names(current, current))
        {
            release_audio_stream(current, audio);
            current[0] = 0;
        }
    }
    if (name[0] != 0)
    {
        request_audio_stream(name, 0x12, audio);
    }
    std::strcpy(current, name);
}

// Xbox 0x0004D510
void replace_cutscene_bank(std::span<u8> event_state, const char* name, AudioBankStreams& audio)
{
    auto* current = reinterpret_cast<char*>(event_state.data() + 6);
    if (current[0] != 0)
    {
        if (equal_sound_names(current, current) && current[0] != 0)
        {
            release_tracked_audio_bank(current, audio);
            current[0] = 0;
        }
    }
    if (name[0] != 0)
    {
        acquire_tracked_audio_bank(name, 7, audio);
    }
    std::strcpy(current, name);
}

// Xbox 0x0004C4F0
void complete_cutscene_dialogue_event(RuntimeCutsceneInstance& instance,
    const u8* event, AudioBankStreams& audio)
{
    const auto* bank = reinterpret_cast<const char*>(event + 0x50);
    auto* current = reinterpret_cast<char*>(instance.event_state.data() + 6);
    if (!equal_sound_names(bank, "XACT_SOUNDBANK_GMISC_NOSOUND")) // DAT_00421960
    {
        if (equal_sound_names(current, bank) && bank[0] != 0)
        {
            release_tracked_audio_bank(bank, audio);
            current[0] = 0;
        }
    }
    const auto* stream = reinterpret_cast<const char*>(event + 0xD0);
    if (stream[0] != 0)
    {
        if (!equal_sound_names(stream, "XACT_SOUNDBANK_GMISC_NOSOUND"))
        {
            replace_cutscene_stream(instance.event_state, stream, audio);
        }
    }
}
}
