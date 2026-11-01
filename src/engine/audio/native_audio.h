#pragma once

#include "bank_lookup.h"
#include "bank_streams.h"
#include "core_cue.h"
#include "core_listener.h"
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
    // 0x0015BB30: CreateFile, GetFileSize, ReadFile and CloseHandle over a named file; the bytes and the file's size.
    std::function<std::vector<u8>(const char*)> load_file;
    // The XACT category volumes and listener reverb the boot sets (DAT_004D9798..): 0x0003A1F0 (ESI the preset,
    // through the resource registry) and 0x0015C1C0 (category, volume).
    std::function<void(i32)> reverb_preset;
    std::function<void(u32, i32)> category_volume;
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
    std::deque<std::vector<u8>> file_images; // 0x0015BB30's audio heap blocks, one per bank loaded from a file.
    AudioCoreListener listener; // DAT_004D97EC, DAT_004D97F0 and the listener pose DAT_004D9820..DAT_004D984C.
    i32 bank_count = 0; // DAT_004E14F0.
    // Host: the name record behind each XACT bank handle, for the cue names the playback boundary resolves.
    std::unordered_map<void*, const AudioBankNames*> handles;
};
extern NativeAudioBanks* g_native_audio; // Set by 0x0003A280.
// The audio core's globals, live from boot for the whole game as in retail: the XACT sound sources and cues, the
// 48 event voices (DAT_004E0CE8), the tracked banks, channels and stream slots (DAT_004DF370, DAT_004E030C,
// DAT_004DFFA0). Scenes, the front end and the transitions all request sounds through these.
struct RuntimeFileCopy;
// DAT_004E14F8: the twelve wavebank stream rows 0x00039DD0 opens (stride 0xC), with the stream ids of its table
// (0x00421A80, stride 0x14, +0xC). A row's +0 is the copy of its in-memory bank to the utility drive while it
// runs (0x00039C50); the XACT banks themselves are the host catalog's.
struct AudioWavebankStreams
{
    std::array<i32, 12> ids{}; // 0x00421A8C
    std::array<RuntimeFileCopy*, 12> jobs{}; // DAT_004E14F8 +0
};
// 0xEA while a stream of this id still has bytes to read, else 0; the bytes left and the total are summed over
// its rows.
u8 runtime_stream_bank_busy(i32 id, i32* remaining, i32* total, const AudioWavebankStreams& streams); // Xbox 0x00039D50
struct NativeAudioRuntime
{
    NativeAudioRuntime() = default;
    NativeAudioRuntime(const NativeAudioRuntime&) = delete;
    NativeAudioRuntime& operator=(const NativeAudioRuntime&) = delete;
    ~NativeAudioRuntime(); // Clears g_native_audio_runtime when it is this one.
    Vec3 origin{}; // DAT_0050DD80, BSS.
    Vec3 default_cone{}; // DAT_0042A840, .data (1, 1, 1).
    Vec3 listener_velocity{}; // DAT_00421C70, the velocity 0x0003A040 passes.
    // The event voices' DAT_005077A8 words in retail layout (position +0, rotation +0xC, forward +0x1C). The host
    // display record has another layout, so the frame's audio tick copies them before 0x000396F0.
    std::array<float, 10> listener_words{};
    AudioCoreSources sources{};
    AudioCoreCues cues{sources};
    AudioEventVoices voices{cues, {}, {}, {}, 0, origin, default_cone};
    AudioBankStreams streams{{}, {}, {}, voices};
    AudioWavebankStreams wavebank_streams{};
};
extern NativeAudioRuntime* g_native_audio_runtime; // Set at boot, after 0x0003A280.
NativeAudioRuntime& native_audio_runtime(); // Raises before boot.
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
