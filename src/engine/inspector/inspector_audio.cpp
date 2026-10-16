#if defined(GBTG_INSPECTOR_APP)
#include "inspector_audio.h"
#include "../audio/pcm_backend.h"
#include "gbtg/game.h"

namespace gbtg
{
InspectorAudio::InspectorAudio(Game& game, NativeAudioBanks& banks)
{
    const auto memory = native_audio_core_memory();
    sources_.memory = memory;
    sources_.memory.allocate = [this, memory](size_t size)
    {
        void* allocation = memory.allocate(size);
        allocations_.insert(allocation);
        return allocation;
    };
    sources_.memory.release = [this, memory](void* allocation)
    {
        allocations_.erase(allocation);
        memory.release(allocation);
    };
    sources_.boundaries = native_audio_core_source_boundaries();
    sources_.unit_scale = 1.f;
    cues_.boundaries = native_audio_core_cue_boundaries(game, [&banks](void* handle, u32 index)
    {
        static std::string name;
        name = native_audio_cue_name(banks, handle, index);
        return name.c_str();
    });
    cues_.sound = game.native_audio->cues.sound;
    for (i32 index = 0; index < banks.bank_count; ++index)
    {
        auto* bank = banks.banks[size_t(index)];
        if (bank == nullptr)
        {
            continue;
        }
        const auto names = banks.handles.find(bank->runtime.handle);
        if (names == banks.handles.end())
        {
            continue;
        }
        u32 cue_index = 0;
        for (auto* cue = names->second->cues; cue != nullptr; cue = cue->next, ++cue_index)
        {
            sounds.push_back({names->second->name.data(), native_audio_cue_name(banks, bank->runtime.handle, cue_index), bank, cue_index});
        }
    }
}

InspectorAudio::~InspectorAudio()
{
    stop();
    // Native preparation may fail before returning its allocated wrapper. Reclaim
    // only this tooling context's remaining heap allocations after its handles go.
    const auto memory = native_audio_core_memory();
    for (void* allocation : allocations_)
    {
        memory.release(allocation);
    }
}

void InspectorAudio::stop()
{
    suspend_pcm_audio();
    if (cue_ != nullptr)
    {
        void* handle = cue_->runtime.handle;
        free_audio_core_cue(cue_, cues_);
        release_pcm_inspection_preview(handle);
    }
    if (source_ != nullptr)
    {
        destroy_audio_core_source(source_, sources_);
    }
    const auto memory = native_audio_core_memory();
    for (void* allocation : allocations_)
    {
        memory.release(allocation);
    }
    allocations_.clear();
    playing = false;
    paused = false;
    status = "Stopped";
    seconds = 0.f;
}

void InspectorAudio::play(size_t index)
{
    stop();
    error.clear();
    current.clear();
    try
    {
        if (index >= sounds.size())
        {
            throw Error("select a sound cue first");
        }
        selected = int(index);
        const auto& sound = sounds[index];
        current = sound.name;
        create_audio_core_source(5, source_, sources_);
        if (source_ == nullptr)
        {
            throw Error("native sound source creation failed");
        }
        prepare_audio_core_cue(*sound.native_bank, sound.cue_index, *source_, cue_, nullptr, nullptr, cues_);
        if (cue_ == nullptr || cue_->runtime.handle == nullptr)
        {
            throw Error("native cue preparation failed: " + sound.name);
        }
        begin_pcm_inspection_preview(cue_->runtime.handle, volume);
        if (play_audio_core_cue(*cue_, cues_) != 0)
        {
            throw Error("native cue playback failed: " + sound.name);
        }
        playing = true;
        status = "Playing";
    }
    catch (const std::exception& failure)
    {
        stop();
        error = current.empty() ? failure.what() : current + ": " + failure.what();
        status = "Failed";
    }
}

void InspectorAudio::pause()
{
    if (cue_ == nullptr || !playing)
    {
        return;
    }
    const bool next = !paused;
    if (cues_.sound.pause_cue(cue_->bank->runtime.handle, cue_->runtime.handle, next) < 0)
    {
        error = "could not pause/resume the native preview cue";
        return;
    }
    paused = next;
    status = paused ? "Paused" : "Playing";
}

void InspectorAudio::set_volume(float value)
{
    volume = value;
    if (cue_ != nullptr)
    {
        volume_pcm_inspection_preview(cue_->runtime.handle, volume);
    }
}

void InspectorAudio::update()
{
    if (cue_ != nullptr && playing && !pcm_inspection_preview_pending(cue_->runtime.handle, seconds))
    {
        const float elapsed = seconds;
        stop();
        seconds = elapsed;
        status = "Finished";
    }
}
}
#endif
