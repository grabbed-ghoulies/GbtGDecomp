#pragma once

#include "core_cue.h"

namespace gbtg
{
struct AudioEventVoice
{
    u32 state; // +0
    u32 stop; // +4
    u32 retained_8;
    u32 word_c;
    u32 retained_10;
    u32 retained_14;
    u32 mode; // +18
    AudioCoreSource* source; // +1C
    AudioCoreCue* cue; // +20
};
struct AudioEventVoices
{
    AudioCoreCues& core;
    std::array<AudioEventVoice, 48> voices; // DAT_004E0CE8
    std::span<AudioCoreBank* const> banks; // DAT_004E13B0
    std::span<const std::array<u32, 2>> reverb_curves; // DAT_00421A38
    u32 reverb_index; // DAT_004E13A8
    const Vec3& origin; // DAT_0050DD80
    const Vec3& default_cone; // DAT_0042A840
    const float* listener = nullptr; // DAT_005077A8, position +0 and forward +7
};
void initialize_event_voice_reverb(AudioCoreSource& source, AudioEventVoices& context); // Xbox 0x00039620
void release_audio_event_voice(AudioEventVoice& voice, AudioEventVoices& context); // Xbox 0x00039660
i16 allocate_audio_event_voice(u32 mode, AudioEventVoices& context); // Xbox 0x00039830
bool prepare_audio_event_voice(i16 index, const char* name, AudioEventVoices& context); // Xbox 0x00039A20
i16 request_audio_event_voice(const char* name, u32 stop, AudioEventVoices& context); // Xbox 0x000395E0
void notify_audio_event_voice(AudioEventVoice* voice, u32 state, AudioEventVoices& context); // Xbox 0x000398E0
u32 audio_event_cue_callback(void* context, AudioCoreCue& cue, u32 state); // Xbox 0x00039A00
void tick_audio_event_voices(AudioEventVoices& context); // Xbox 0x000396F0
void pause_audio_event_voice(i16 index, i32 paused, AudioEventVoices& context); // Xbox 0x00039AA0
void position_audio_event_voice_ahead(i16 index, float distance,
    AudioEventVoices& context); // Xbox 0x00039B30
i16 request_spatial_audio_event_voice(const char* name, u32 stop, float distance,
    AudioEventVoices& context); // Xbox 0x00039510
}
