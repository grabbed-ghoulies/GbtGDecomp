#pragma once

#include "bank_lookup.h"
#include "core_cue.h"
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace gbtg
{
// What 0x0003A280 reaches below the translated code: resource bytes and the XACT engine.
struct NativeAudioBootBoundaries
{
    std::function<std::span<const u8>(const char*)> resource; // 0x00035E90 / 0x00035D90: an AID's bytes, or empty.
    std::function<void(u32)> resource_error; // 0x00032700.
    std::function<void()> create_engine; // XACTEngineCreate and the audio flags (0x0015B960).
    std::function<void(std::span<const u8>)> download_effects; // 0x0015D090, the DSP image 0x00039EF0 loads.
    std::function<void()> register_wavebanks; // 0x00039DD0: the in-memory and streaming XACT wave banks.
    std::function<i32(std::span<const u8>, void*&)> create_sound_bank; // IXACTEngine_CreateSoundBank.
    std::function<void(void*, u32)> register_notification; // IXACTEngine_RegisterNotification (0x0015D0E0).
};
// The audio state 0x0003A280 builds at boot.
struct NativeAudioBanks
{
    AudioCoreMemory memory; // DAT_004D9784 / DAT_004D9788 (0x0015B730).
    std::vector<char> list; // The cue list text; 0x000392D0 terminates its lines in place.
    std::deque<AudioBankNames> bank_names; // DAT_004E0CD0 arena records (0x000390A0).
    std::deque<AudioCueName> cue_names; // DAT_004E0CD0 arena records (0x00039170).
    AudioBankNames* current = nullptr; // DAT_004E0CE4.
    const char* current_name = nullptr; // DAT_004E0CE0.
    AudioCoreLink loaded{}; // DAT_004D9804, the created sound banks.
    u32 loaded_count = 0; // DAT_004D980C.
    u32 notifications = 0; // DAT_004D9794.
    std::array<AudioCoreBank*, 80> banks{}; // DAT_004E13B0.
    i32 bank_count = 0; // DAT_004E14F0.
    // Host: the name record behind each XACT bank handle, for the cue names the playback boundary resolves.
    std::unordered_map<void*, const AudioBankNames*> handles;
};
extern NativeAudioBanks* g_native_audio; // Set by 0x0003A280.
char* read_audio_list_line(char*& cursor, const char* base, i32 size); // Xbox 0x000FC120, EDI size
AudioBankNames* add_audio_bank_name(const char* name, NativeAudioBanks& banks); // Xbox 0x000390A0
void add_audio_cue_name(AudioBankNames& bank, const char* name, NativeAudioBanks& banks); // Xbox 0x00039170
void register_audio_bank_names(const char* list, NativeAudioBanks& banks,
    NativeAudioBootBoundaries& boundaries); // Xbox 0x000392D0, EAX list
void create_audio_sound_bank(std::span<const u8> image, const char* name, AudioCoreBank*& output,
    NativeAudioBanks& banks, NativeAudioBootBoundaries& boundaries); // Xbox 0x0015CB10
void load_audio_sound_banks(const char* prefix, NativeAudioBanks& banks,
    NativeAudioBootBoundaries& boundaries); // Xbox 0x00039F50
void initialize_native_audio(NativeAudioBanks& banks, NativeAudioBootBoundaries& boundaries); // Xbox 0x0003A280
// The XACT cue name ("XACT_SOUNDBANK_<bank>_<cue>") of a bank handle's cue index, for the playback boundary.
std::string native_audio_cue_name(const NativeAudioBanks& banks, void* handle, u32 index);
}
