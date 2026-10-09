#include "bank_lookup.h"

#include <cstring>

namespace gbtg
{
AudioBankNames* g_audio_bank_names = nullptr;
namespace
{
char bank_scratch[128]; // DAT_004E0C50
char cue_scratch[128]; // DAT_004E0BD0
}

// Xbox 0x00038050
bool is_audio_bank_resource(const char* name)
{
    return std::strncmp(name, "XACT_SOUNDBANK_", 15) == 0;
}

// Xbox 0x000391E0
AudioCueName* find_audio_cue_name(AudioBankNames& bank, const char* name, i32& index)
{
    auto* cue = bank.cues;
    i32 cursor = 0;
    while (true)
    {
        if (cue == nullptr)
        {
            return nullptr;
        }
        const char* candidate = cue->name.data();
        const char* requested = name;
        u8 first;
        u8 second;
        while (true)
        {
            first = u8(*candidate);
            if (first > 0x20)
            {
                first &= 0xDF;
            }
            second = u8(*requested);
            if (second > 0x20)
            {
                second &= 0xDF;
            }
            if (first != second)
            {
                break;
            }
            ++candidate;
            ++requested;
            if (first == 0)
            {
                index = cursor;
                return cue;
            }
        }
        cue = cue->next;
        ++cursor;
    }
}

// Xbox 0x00039260
AudioBankNames* find_audio_bank_name(const char* name, i32& index)
{
    auto* bank = g_audio_bank_names;
    i32 cursor = 0;
    while (true)
    {
        if (bank == nullptr)
        {
            return nullptr;
        }
        const char* candidate = bank->name.data();
        const char* requested = name;
        i32 remaining = 14;
        while (true)
        {
            const u8 first = u8(*candidate) & 0xDF;
            const u8 second = u8(*requested) & 0xDF;
            ++candidate;
            ++requested;
            if (first != second)
            {
                break;
            }
            if (first == 0 || --remaining == 0)
            {
                index = cursor;
                return bank;
            }
        }
        bank = bank->next;
        ++cursor;
    }
}

// Xbox 0x000393E0
bool resolve_audio_bank_cue(const char* name, i32* bank_index, i32* cue_index)
{
    const char* cursor = name + 15;
    char* bank_name = bank_scratch;
    char* cue_name = cue_scratch;
    while (*cursor != '_')
    {
        *bank_name++ = *cursor++;
    }
    ++cursor;
    *bank_name = '\0';
    while (*cursor != '\0')
    {
        *cue_name++ = *cursor++;
    }
    *cue_name = '\0';
    i32 found_bank;
    i32 found_cue;
    auto* bank = find_audio_bank_name(bank_scratch, found_bank);
    if (bank != nullptr && find_audio_cue_name(*bank, cue_scratch, found_cue) != nullptr)
    {
        if (bank_index != nullptr)
        {
            *bank_index = found_bank;
        }
        if (cue_index != nullptr)
        {
            *cue_index = found_cue;
        }
        return true;
    }
    return false;
}
}
