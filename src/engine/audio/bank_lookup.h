#pragma once

#include "gbtg/types.h"

namespace gbtg
{
struct AudioCueName
{
    AudioCueName* previous;
    AudioCueName* next;
    std::array<char, 128> name;
};
struct AudioBankNames
{
    AudioCueName* previous_cue;
    AudioCueName* cues;
    AudioBankNames* next;
    std::array<char, 128> name;
};
extern AudioBankNames* g_audio_bank_names; // DAT_004E0CDC
bool is_audio_bank_resource(const char* name); // Xbox 0x00038050
AudioCueName* find_audio_cue_name(AudioBankNames& bank, const char* name, i32& index); // Xbox 0x000391E0
AudioBankNames* find_audio_bank_name(const char* name, i32& index); // Xbox 0x00039260
bool resolve_audio_bank_cue(const char* name, i32* bank_index, i32* cue_index); // Xbox 0x000393E0
}
