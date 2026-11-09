#pragma once
#include "event_voices.h"

namespace gbtg
{
struct AudioTrackedBank
{
    i32 id; // +0, stride 0x9C
    std::array<char, 128> name; // +4
    i32 channel; // +84
    i32 references; // +88
    i32 silence; // +8C
    u32 retained_90;
    u32 pending; // +94
    i32 held; // +98
};
struct AudioTrackedChannel
{
    i16 voice; // +0, stride 0x8C
    std::array<char, 130> name; // +2
    i32 mode; // +84
    i32 owned; // +88
};
struct AudioStreamSlot
{
    std::array<char, 128> name; // DAT_004DFFA0, stride 0x80
    i32 state; // DAT_004E02A0
    i32 id; // DAT_004E02B8
    float time; // DAT_004E02D0
};
struct AudioBankStreams
{
    std::array<AudioTrackedBank, 20> banks; // DAT_004DF370
    std::array<AudioTrackedChannel, 16> channels; // DAT_004E030C
    std::array<AudioStreamSlot, 6> streams;
    AudioEventVoices& voices;
    float level = 0.f; // DAT_004E0BCC.
};
void initialize_tracked_audio_channels(AudioBankStreams& context); // Xbox 0x00038BF0
// The six stream slots lose their names and states and take id -1; the slot gate (DAT_004E02E8) opens.
void initialize_audio_stream_slots(AudioBankStreams& context, i32& gate); // Xbox 0x00038600
AudioTrackedBank* next_tracked_audio_bank(AudioTrackedBank* bank,
    AudioBankStreams& context); // Xbox 0x00038400
AudioTrackedBank* find_tracked_audio_bank(const char* name, AudioBankStreams& context); // Xbox 0x00038460
AudioTrackedBank* find_tracked_audio_bank_by_id(i32 id, AudioBankStreams& context); // Xbox 0x000384F0, ESI id
AudioTrackedBank* select_resume_audio_bank(AudioTrackedBank& removed,
    AudioBankStreams& context); // Xbox 0x00038590
AudioTrackedChannel* find_tracked_audio_channel(const char* name, i32& found,
    AudioBankStreams& context); // Xbox 0x00038B30
void resume_tracked_audio_channel(std::int8_t index, AudioBankStreams& context); // Xbox 0x00038DA0
void release_tracked_audio_channel(std::int8_t index, AudioBankStreams& context); // Xbox 0x00038CC0
void resume_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context); // Xbox 0x00038360
void detach_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context); // Xbox 0x00038260
void release_tracked_audio_bank(const char* name, AudioBankStreams& context); // Xbox 0x00038970
void release_audio_stream_slot(i32 index, AudioBankStreams& context); // Xbox 0x00038810
void release_audio_stream(const char* name, AudioBankStreams& context); // Xbox 0x00038780
void request_audio_stream(const char* name, i32 id, AudioBankStreams& context); // Xbox 0x00038720
void restart_audio_stream(const char* name, i32 id, AudioBankStreams& context); // Xbox 0x00038880, EBX name
void initialize_tracked_audio_bank(AudioTrackedBank& bank, i32 id,
    const char* name); // Xbox 0x00038210
AudioTrackedBank* allocate_tracked_audio_bank(i32 id, const char* name,
    AudioBankStreams& context); // Xbox 0x000383C0
void retire_tracked_audio_channel(AudioTrackedChannel& channel,
    AudioBankStreams& context); // Xbox 0x00038BB0
std::int8_t acquire_tracked_audio_channel(const char* name, i32* existing,
    AudioBankStreams& context); // Xbox 0x00038D10
void start_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context); // Xbox 0x000382C0
void pause_tracked_audio_channel(std::int8_t index, AudioBankStreams& context); // Xbox 0x00038E60
void pause_tracked_audio_bank(AudioTrackedBank& bank, AudioBankStreams& context); // Xbox 0x00038300
bool tracked_audio_bank_has_priority(AudioTrackedBank& bank,
    AudioBankStreams& context); // Xbox 0x00038530
void pause_lower_priority_audio_banks(AudioTrackedBank& bank,
    AudioBankStreams& context); // Xbox 0x00038AD0
// EBX priority: every tracked bank below it (bar id 0x12) gives up its channel (0x00038260) and is held.
void hold_lower_priority_audio_banks(i32 priority, AudioBankStreams& context); // Xbox 0x00038A10
void acquire_tracked_audio_bank(const char* name, i32 id,
    AudioBankStreams& context); // Xbox 0x00038910
// 1 untracked, 2 finished or held, 3 still sounding.
i32 runtime_audio_bank_state(const char* name, AudioBankStreams& context); // Xbox 0x000389C0, EAX name
u32 runtime_audio_channel_idle(std::int8_t index, AudioBankStreams& context); // Xbox 0x00038EF0, AL index
// The six queued stream slots: a requested slot acquires its bank once DAT_004E02E8 is clear; a playing slot is
// released when its bank finishes and otherwise times its play by `step` (DAT_005096C0).
void tick_audio_stream_slots(float step, i32 gate, AudioBankStreams& context); // Xbox 0x00038640
// Owned channels whose voice has no cue or has stopped give the voice up; DAT_004E0BCC becomes 1.
void release_idle_tracked_audio_channels(AudioBankStreams& context); // Xbox 0x00038C20
}
