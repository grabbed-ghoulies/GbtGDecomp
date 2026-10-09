#include "core_cue.h"

namespace gbtg
{
// Xbox 0x0015CBF0
void prepare_audio_core_cue(AudioCoreBank& bank, u32 index, AudioCoreSource& source,
    AudioCoreCue*& output, u32 (*callback)(void*, AudioCoreCue&, u32),
    void* callback_context, AudioCoreCues& context)
{
    auto* cue = static_cast<AudioCoreCue*>(allocate_audio_core_object(0x4C,
        sizeof(AudioCoreCue), "Cue", 4, "audioSoundBankGetCue", 0x16C, 0,
        context.sources.memory));
    audio_core_source_valid(&source, context.sources);
    cue->index = index;
    cue->bank = &bank;
    cue->runtime.bank = &bank.runtime;
    cue->source = &source;
    cue->callback = callback;
    cue->callback_context = callback_context;
    cue->state = 4;
    if (callback != nullptr)
    {
        callback(callback_context, *cue, 4);
    }
    if (context.boundaries.prepare(bank.runtime.handle, index, cue->runtime.handle) < 0)
    {
        sound_runtime_error(context.sound);
        output = nullptr;
        return;
    }
    append_audio_core_link(bank.cues, *cue);
    output = cue;
}

// Xbox 0x0015CDB0
void set_audio_core_cue_user_data(AudioCoreCue& cue, void* data)
{
    cue.user_data = data;
}

// Xbox 0x0015CDC0
void* audio_core_cue_user_data(const AudioCoreCue& cue)
{
    return cue.user_data;
}

// Xbox 0x0015CCC0
u32 play_audio_core_cue(AudioCoreCue& cue, AudioCoreCues& context)
{
    if ((cue.runtime.flags & 1) != 0)
    {
        sound_runtime_error(context.sound);
        return 1;
    }
    if ((cue.runtime.flags & 2) != 0)
    {
        sound_runtime_error(context.sound);
        return 1;
    }
    audio_core_source_valid(cue.source, context.sources);
    if (context.boundaries.play(cue.bank->runtime.handle, cue.source->handle, cue.runtime.handle) < 0)
    {
        sound_runtime_error(context.sound);
        return 1;
    }
    cue.runtime.flags |= 1;
    ++context.started_count;
    return 0;
}

// Xbox 0x0015CE90
void free_audio_core_cue(AudioCoreCue*& cue, AudioCoreCues& context)
{
    if (cue->runtime.handle != nullptr)
    {
        stop_runtime_sound_cue(cue->runtime, 0, context.stopped_count, context.sound);
    }
    unlink_audio_core_link(*cue);
    AudioCoreHeader* header = cue;
    release_audio_core_object(header, "audioSoundBankFreeCue", 0x26D, 0, context.sources.memory);
    cue = nullptr;
}
}
