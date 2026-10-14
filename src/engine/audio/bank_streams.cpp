#include "bank_streams.h"
#include "../graphics/actor_effect_stream.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00038210
void initialize_tracked_audio_bank(AudioTrackedBank& bank, i32 id, const char* name)
{
    bank.id = id;
    bank.pending = 0;
    bank.references = 1;
    bank.silence = equal_sound_names("XACT_SILENCE", name); // DAT_002FD0C0
    bank.retained_90 = 0;
    bank.held = 0;
    std::strcpy(bank.name.data(), name);
}

// Xbox 0x000383C0
AudioTrackedBank* allocate_tracked_audio_bank(i32 id, const char* name, AudioBankStreams& context)
{
    for (auto& bank : context.banks)
    {
        if (bank.id == 0)
        {
            initialize_tracked_audio_bank(bank, id, name);
            return &bank;
        }
    }
    return nullptr;
}

// Xbox 0x00038BF0
void initialize_tracked_audio_channels(AudioBankStreams& context)
{
    for (auto& channel : context.channels)
    {
        channel.voice = -1;
        channel.name[0] = 0;
        channel.owned = 0;
    }
}

// Xbox 0x00038BB0
void retire_tracked_audio_channel(AudioTrackedChannel& channel, AudioBankStreams& context)
{
    channel.name[0] = 0;
    if (channel.voice != -1)
    {
        auto& voice = context.voices.voices[channel.voice];
        RuntimeSoundVoice view{voice.state, voice.cue == nullptr ? nullptr : &voice.cue->runtime};
        stop_runtime_sound_voice(view, 0, context.voices.core.stopped_count, context.voices.core.sound);
        voice.state = view.state;
        channel.voice = -1;
        channel.mode = 0;
    }
    channel.owned = 0;
}

// Xbox 0x00038D10
std::int8_t acquire_tracked_audio_channel(const char* name, i32* existing, AudioBankStreams& context)
{
    i32 found;
    auto* channel = find_tracked_audio_channel(name, found, context);
    if (channel == nullptr)
    {
        return -1;
    }
    if (found == 0)
    {
        const auto voice = static_cast<std::int8_t>(allocate_audio_event_voice(0, context.voices));
        if (voice == -1)
        {
            return -1;
        }
        channel->voice = voice;
        std::strcpy(channel->name.data(), name);
        channel->mode = 1;
    }
    if (channel->voice == -1)
    {
        retire_tracked_audio_channel(*channel, context);
        return -1;
    }
    if (existing != nullptr)
    {
        *existing = found;
    }
    return static_cast<std::int8_t>(channel - context.channels.data());
}

// Xbox 0x000382C0
void start_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context)
{
    bank.retained_90 = 1;
    if (bank.silence == 0)
    {
        i32 existing;
        const auto channel = acquire_tracked_audio_channel(bank.name.data(), &existing, context);
        std::memcpy(&bank.channel, &channel, 1); // Only the byte at +84 is written.
        if (channel != -1)
        {
            resume_tracked_audio_channel(channel, context);
        }
    }
}

// Xbox 0x00038E60
void pause_tracked_audio_channel(std::int8_t index, AudioBankStreams& context)
{
    auto& channel = context.channels[index];
    if (channel.mode != 1)
    {
        if (channel.mode == 2)
        {
            auto& voice = context.voices.voices[channel.voice];
            if (voice.cue != nullptr)
            {
                switch (voice.state)
                {
                case 2:
                    channel.mode = 3;
                    voice.state = 3;
                    return;
                case 5:
                case 6:
                case 7:
                    voice.state = 7;
                    pause_runtime_sound_cue(voice.cue->runtime, 1, context.voices.core.sound);
                    break;
                }
            }
        }
        channel.mode = 3;
    }
}

// Xbox 0x00038300
void pause_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context)
{
    bank.pending = 1;
    if (bank.silence == 0 && bank.held == 0)
    {
        i32 found;
        auto* channel = find_tracked_audio_channel(bank.name.data(), found, context);
        if (found != 0)
        {
            pause_tracked_audio_channel(static_cast<std::int8_t>(channel - context.channels.data()), context);
        }
    }
}

// Xbox 0x00038530
bool tracked_audio_bank_has_priority(AudioTrackedBank& bank, AudioBankStreams& context)
{
    i32 index = 0;
    do
    {
        if (context.banks[index].id != 0)
        {
            auto* candidate = &context.banks[index];
            if (candidate == nullptr)
            {
                return true;
            }
            while (candidate == &bank || candidate->id == 0x12 || candidate->id <= bank.id)
            {
                candidate = next_tracked_audio_bank(candidate, context);
                if (candidate == nullptr)
                {
                    return true;
                }
            }
            return false;
        }
        ++index;
    }
    while (index < 20);
    return true;
}

// Xbox 0x00038AD0
void pause_lower_priority_audio_banks(AudioTrackedBank& bank, AudioBankStreams& context)
{
    i32 index = 0;
    do
    {
        if (context.banks[index].id != 0)
        {
            auto* candidate = &context.banks[index];
            while (candidate != nullptr)
            {
                if (candidate != &bank && candidate->id != 0x12 && candidate->id < bank.id)
                {
                    pause_tracked_audio_bank(*candidate, context);
                }
                candidate = next_tracked_audio_bank(candidate, context);
            }
            return;
        }
        ++index;
    }
    while (index < 20);
}

// Xbox 0x00038910
void acquire_tracked_audio_bank(const char* name, i32 id, AudioBankStreams& context)
{
    auto* existing = find_tracked_audio_bank(name, context);
    if (existing != nullptr)
    {
        ++existing->references;
        return;
    }
    auto& bank = *allocate_tracked_audio_bank(id, name, context);
    if (bank.id == 0x12)
    {
        start_tracked_audio_bank(bank, context);
        return;
    }
    if (tracked_audio_bank_has_priority(bank, context))
    {
        pause_lower_priority_audio_banks(bank, context);
        start_tracked_audio_bank(bank, context);
        return;
    }
    start_tracked_audio_bank(bank, context);
    pause_tracked_audio_bank(bank, context);
}

// Xbox 0x00038400
AudioTrackedBank* next_tracked_audio_bank(AudioTrackedBank* bank, AudioBankStreams& context)
{
    do
    {
        ++bank;
        if (bank == context.banks.data() + 20)
        {
            return nullptr;
        }
    }
    while (bank->id == 0);
    return bank;
}

// Xbox 0x00038460
AudioTrackedBank* find_tracked_audio_bank(const char* name, AudioBankStreams& context)
{
    i32 index = 0;
    do
    {
        if (context.banks[index].id != 0)
        {
            auto* bank = &context.banks[index];
            if (bank == nullptr)
            {
                return nullptr;
            }
            while (std::strcmp(bank->name.data(), name) != 0)
            {
                bank = next_tracked_audio_bank(bank, context);
                if (bank == nullptr)
                {
                    return nullptr;
                }
            }
            return bank;
        }
        ++index;
    }
    while (index < 20);
    return nullptr;
}

// Xbox 0x00038590
AudioTrackedBank* select_resume_audio_bank(AudioTrackedBank& removed, AudioBankStreams& context)
{
    AudioTrackedBank* selected = nullptr;
    i32 index = 0;
    do
    {
        if (context.banks[index].id != 0)
        {
            auto* bank = &context.banks[index];
            if (bank == nullptr)
            {
                return nullptr;
            }
            do
            {
                if (bank != &removed && bank->id != 0x12)
                {
                    if (removed.id < bank->id)
                    {
                        return nullptr;
                    }
                    if (selected == nullptr || selected->id < bank->id)
                    {
                        selected = bank;
                    }
                }
                bank = next_tracked_audio_bank(bank, context);
            }
            while (bank != nullptr);
            return selected;
        }
        ++index;
    }
    while (index < 20);
    return nullptr;
}

// Xbox 0x00038B30
AudioTrackedChannel* find_tracked_audio_channel(const char* name, i32& found, AudioBankStreams& context)
{
    AudioTrackedChannel* free_channel = nullptr;
    for (auto& channel : context.channels)
    {
        if (channel.voice != -1)
        {
            if (std::strcmp(channel.name.data(), name) == 0)
            {
                found = 1;
                return &channel;
            }
        }
        else
        {
            free_channel = &channel;
        }
    }
    found = 0;
    return free_channel;
}

// Xbox 0x00038DA0
void resume_tracked_audio_channel(std::int8_t index, AudioBankStreams& context)
{
    auto& channel = context.channels[index];
    if (channel.mode == 1)
    {
        prepare_audio_event_voice(channel.voice, channel.name.data(), context.voices);
    }
    else if (channel.mode == 3 && channel.voice != -1)
    {
        auto& voice = context.voices.voices[channel.voice];
        if (voice.cue != nullptr)
        {
            switch (voice.state)
            {
            case 3:
                channel.mode = 2;
                voice.state = 2;
                return;
            case 5:
            case 6:
            case 7:
                voice.state = 6;
                pause_runtime_sound_cue(voice.cue->runtime, 0, context.voices.core.sound);
                channel.mode = 2;
                return;
            }
        }
    }
    channel.mode = 2;
}

// Xbox 0x00038CC0
void release_tracked_audio_channel(std::int8_t index, AudioBankStreams& context)
{
    auto& channel = context.channels[index];
    const i16 voice_index = channel.voice;
    channel.name[0] = 0;
    if (voice_index != -1)
    {
        auto& voice = context.voices.voices[voice_index];
        RuntimeSoundVoice view{voice.state, voice.cue == nullptr ? nullptr : &voice.cue->runtime};
        stop_runtime_sound_voice(view, 0, context.voices.core.stopped_count, context.voices.core.sound);
        voice.state = view.state;
        channel.voice = -1;
        channel.mode = 0;
    }
    channel.owned = 0;
}

// Xbox 0x00038360
void resume_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context)
{
    bank.pending = 0;
    if (bank.silence == 0 && bank.held == 0)
    {
        i32 found;
        auto* channel = find_tracked_audio_channel(bank.name.data(), found, context);
        if (found != 0)
        {
            resume_tracked_audio_channel(static_cast<std::int8_t>(channel - context.channels.data()), context);
        }
    }
}

// Xbox 0x00038260
void detach_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context)
{
    if (bank.held == 0 && bank.silence == 0)
    {
        i32 found;
        auto* channel = find_tracked_audio_channel(bank.name.data(), found, context);
        if (found != 0)
        {
            release_tracked_audio_channel(static_cast<std::int8_t>(channel - context.channels.data()), context);
        }
    }
}

// Xbox 0x00038970
void release_tracked_audio_bank(const char* name, AudioBankStreams& context)
{
    auto& bank = *find_tracked_audio_bank(name, context);
    --bank.references;
    if (bank.references < 1)
    {
        if (bank.id != 0x12)
        {
            auto* resumed = select_resume_audio_bank(bank, context);
            if (resumed != nullptr)
            {
                resume_tracked_audio_bank(*resumed, context);
            }
        }
        detach_tracked_audio_bank(bank, context);
        bank.id = 0;
    }
}

// Xbox 0x00038810
void release_audio_stream_slot(i32 index, AudioBankStreams& context)
{
    auto& slot = context.streams[index];
    slot.id = -1;
    if (slot.state == 2)
    {
        auto& bank = *find_tracked_audio_bank(slot.name.data(), context);
        --bank.references;
        if (bank.references < 1)
        {
            if (bank.id != 0x12)
            {
                auto* resumed = select_resume_audio_bank(bank, context);
                if (resumed != nullptr)
                {
                    resume_tracked_audio_bank(*resumed, context);
                }
            }
            detach_tracked_audio_bank(bank, context);
            bank.id = 0;
        }
    }
    slot.name[0] = 0;
}

// Xbox 0x00038780
void release_audio_stream(const char* name, AudioBankStreams& context)
{
    i32 index = 0;
    while (std::strcmp(context.streams[index].name.data(), name) != 0)
    {
        ++index;
        if (index >= 6)
        {
            return;
        }
    }
    release_audio_stream_slot(index, context);
}

// Xbox 0x00038720
void request_audio_stream(const char* name, i32 id, AudioBankStreams& context)
{
    i32 index = 0;
    do
    {
        auto& slot = context.streams[index];
        if (slot.name[0] == 0)
        {
            std::strcpy(slot.name.data(), name);
            slot.state = 1;
            slot.id = id;
            return;
        }
        ++index;
    }
    while (index < 6);
}

// Xbox 0x000389C0
i32 runtime_audio_bank_state(const char* name, AudioBankStreams& context)
{
    const AudioTrackedBank* bank = find_tracked_audio_bank(name, context);
    if (bank == nullptr)
    {
        return 1;
    }
    if (bank->silence != 0)
    {
        return 3;
    }
    if (bank->held != 0 || std::int8_t(bank->channel) == -1)
    {
        return 2;
    }
    return runtime_audio_channel_idle(std::int8_t(bank->channel), context) == 0 ? 3 : 2;
}

// Xbox 0x00038EF0
u32 runtime_audio_channel_idle(std::int8_t index, AudioBankStreams& context)
{
    const AudioTrackedChannel& channel = context.channels.at(size_t(index));
    if (channel.mode == 3)
    {
        return 0;
    }
    const AudioEventVoice& voice = context.voices.voices.at(size_t(channel.voice));
    if (voice.cue != nullptr)
    {
        const u32 state = voice.state;
        if (state == 2 || state == 3 || state == 5 || state == 6 || state == 7)
        {
            return 0;
        }
    }
    return 1;
}

// Xbox 0x00038640
void tick_audio_stream_slots(float step, i32 gate, AudioBankStreams& context)
{
    for (i32 index = 0; index < 6; ++index)
    {
        auto& slot = context.streams[size_t(index)];
        if (slot.name[0] == 0)
        {
            continue;
        }
        if (slot.state == 2)
        {
            // 0x000389C0, inlined.
            i32 state;
            const AudioTrackedBank* bank = find_tracked_audio_bank(slot.name.data(), context);
            if (bank == nullptr)
            {
                state = 1;
            }
            else if (bank->silence == 0)
            {
                state = 2;
                if (bank->held == 0 && std::int8_t(bank->channel) != -1
                    && runtime_audio_channel_idle(std::int8_t(bank->channel), context) == 0)
                {
                    state = 3;
                }
            }
            else
            {
                state = 3;
            }
            if (state == 2)
            {
                release_audio_stream_slot(index, context); // 0x00038810
            }
            else if (state == 3)
            {
                slot.time = slot.time + step;
            }
        }
        else if (gate == 0)
        {
            acquire_tracked_audio_bank(slot.name.data(), slot.id, context); // 0x00038910
            slot.state = 2;
            slot.time = 0.f;
        }
    }
}

// Xbox 0x00038C20
void release_idle_tracked_audio_channels(AudioBankStreams& context)
{
    for (auto& channel : context.channels)
    {
        const i16 index = channel.voice;
        if (index == -1)
        {
            continue;
        }
        auto& voice = context.voices.voices[size_t(index)];
        const u32 state = voice.state;
        const bool sounding = state == 2 || state == 3 || state == 5 || state == 6 || state == 7;
        if ((voice.cue == nullptr || !sounding) && channel.owned != 0)
        {
            channel.name[0] = 0;
            RuntimeSoundVoice view{voice.state, voice.cue == nullptr ? nullptr : &voice.cue->runtime};
            stop_runtime_sound_voice(view, 0, context.voices.core.stopped_count, context.voices.core.sound); // 0x00039690
            voice.state = view.state;
            channel.voice = -1;
            channel.mode = 0;
            channel.owned = 0;
        }
    }
    context.level = 1.f;
}
}
