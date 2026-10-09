#pragma once

#include "core_source.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
struct AudioCoreBank
{
    RuntimeSoundBank runtime; // +18
    AudioCoreLink cues; // +24
};
struct AudioCoreCue : AudioCoreHeader
{
    void* user_data; // +14
    AudioCoreBank* bank; // +18
    AudioCoreSource* source; // +1C
    u32 index; // +20
    RuntimeSoundCue runtime; // handle +24, flags +34
    u32 state; // +28
    u32 (*callback)(void*, AudioCoreCue&, u32); // +2C
    void* callback_context; // native owner of the retail callback
};
struct AudioCoreCueBoundaries
{
    std::function<i32(void*, u32, void*&)> prepare; // IXACTSoundBank_PrepareEx
    std::function<i32(void*, void*, void*&)> play; // IXACTSoundBank_PlayEx
};
struct AudioCoreCues
{
    AudioCoreSources& sources;
    AudioCoreCueBoundaries boundaries;
    RuntimeSoundBoundaries sound;
    u32 started_count; // DAT_004D9B70
    u32 stopped_count; // DAT_004D9B74
};
void prepare_audio_core_cue(AudioCoreBank& bank, u32 index, AudioCoreSource& source,
    AudioCoreCue*& output, u32 (*callback)(void*, AudioCoreCue&, u32),
    void* callback_context, AudioCoreCues& context); // Xbox 0x0015CBF0
void set_audio_core_cue_user_data(AudioCoreCue& cue, void* data); // Xbox 0x0015CDB0
void* audio_core_cue_user_data(const AudioCoreCue& cue); // Xbox 0x0015CDC0
u32 play_audio_core_cue(AudioCoreCue& cue, AudioCoreCues& context); // Xbox 0x0015CCC0
void free_audio_core_cue(AudioCoreCue*& cue, AudioCoreCues& context); // Xbox 0x0015CE90
}
