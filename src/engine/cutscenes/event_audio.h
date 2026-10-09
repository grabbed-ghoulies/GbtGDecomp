#pragma once
#include "instance_construction.h"
#include "../audio/bank_streams.h"

namespace gbtg
{
void release_cutscene_stream(std::span<u8> event_state, const char* name,
    AudioBankStreams& audio); // Xbox 0x0004D460
void release_cutscene_bank(std::span<u8> event_state, const char* name,
    AudioBankStreams& audio); // Xbox 0x0004D490
void replace_cutscene_stream(std::span<u8> event_state, const char* name,
    AudioBankStreams& audio); // Xbox 0x0004D4C0
void replace_cutscene_bank(std::span<u8> event_state, const char* name,
    AudioBankStreams& audio); // Xbox 0x0004D510
void complete_cutscene_dialogue_event(RuntimeCutsceneInstance& instance,
    const u8* event, AudioBankStreams& audio); // Xbox 0x0004C4F0
void start_cutscene_voice_event(const u8* event, std::span<u8> event_state,
    AudioEventVoices& voices); // Xbox 0x0004C3B0
void dispatch_cutscene_voice_event(const u8* event, std::span<u8> event_state,
    u32 phase, AudioEventVoices& voices); // Xbox 0x0004C4A0
}
