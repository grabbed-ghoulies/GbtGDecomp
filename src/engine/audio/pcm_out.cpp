#include "gbtg/engine_internal.h"

#include "gbtg/bundle.h"
#include "gbtg/log.h"
#include "pcm_backend.h"
#include "../camera/display_view.h"
#include "../scene/executable_resources.h"
#include "../video/native_movies.h"
#include "../video/movie_session.h"
#include "wavebank_files.h"
#include "listener_settings.h"
#include "../scene/dialogue_resource_owner.h"
#include "../scene/scene.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#endif

#include <algorithm>
#include <cerrno>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>

#include "xaudio2_host.h"
#include "mix_recorder.h"

namespace gbtg
{
namespace
{

u32 ru32(std::span<const u8> b, size_t o)
{
    u32 v = 0;
    if (o + 4 <= b.size())
    {
        std::memcpy(&v, b.data() + o, 4);
    }
    return v;
}

u16 ru16(std::span<const u8> b, size_t o)
{
    u16 v = 0;
    if (o + 2 <= b.size())
    {
        std::memcpy(&v, b.data() + o, 2);
    }
    return v;
}

i16 rs16(std::span<const u8> b, size_t o)
{
    return static_cast<i16>(ru16(b, o));
}

float rf32(std::span<const u8> b, size_t o)
{
    float v = 0.f;
    if (o + 4 <= b.size())
    {
        std::memcpy(&v, b.data() + o, 4);
    }
    return v;
}

// A 3D sound's distance settings: the sound bank's 0x28-byte record (table at header +0x10) that XACT applies to
// the sound's voice when the sound entry's flags (+0xB) have bit 0 set (0x001C210A -> 0x001C085E, index at entry
// +0xC): +8 min distance, +0xC max distance, +0x14 rolloff factor, +0x1C mode, +0x1D curve count, +0x1E the curve's
// bytes in 1/255 steps (DAT_0032B154).
struct Sound3D
{
    float min_distance = 0.f;
    float max_distance = 0.f;
    float rolloff = 1.f;
    u8 mode = 0;
    std::vector<float> curve;
};

// A sound bank variation table as XACT keeps it: the header word (count in bits 0..12, the type in bits 13..16, the
// last choice in bits 17..29, bit 30 a pending choice), each entry's weight range (entry +4 / +6) and the shuffle's
// played bits after the entries. XACT updates them in the bank's memory, so they persist from one play to the next.
struct VariationTable
{
    u32 header = 0;
    std::vector<std::pair<u16, u16>> ranges;
    std::vector<u32> played;
};
std::vector<VariationTable> g_variation_tables;
std::map<const u8*, i32> g_variation_table_index; // By the table's address in its loaded bank.

// XACT's random source (0x001BD3A2): RtlRandom (0x0016FCC1) on DAT_001C54F8, seeded with GetTickCount() when XACT
// starts (0x001BAC09), over the 128-word vector at 0x00338750.
u32 g_xact_random_seed = 0;
std::array<u32, 128> g_xact_random_vector{};

u32 xact_rtl_random()
{
    const u32 first = (g_xact_random_seed * 0x7FFFFFEDu + 0x7FFFFFC3u) % 0x7FFFFFFFu;
    const u32 second = (first * 0x7FFFFFEDu + 0x7FFFFFC3u) % 0x7FFFFFFFu;
    g_xact_random_seed = second;
    const u32 value = g_xact_random_vector[second & 0x7F];
    g_xact_random_vector[second & 0x7F] = first;
    return value;
}

// Xbox 0x001BD3A2
u32 xact_random_below(u32 range)
{
    if (range < 2)
    {
        return 0;
    }
    return xact_rtl_random() % range;
}

// Xbox 0x001C2A3B: the entry a variation table plays next. Types 0 (random, not the last), 2 (shuffle), 4 (random)
// draw over the 10000-wide weights; 1 plays in order; 5 plays in order from a random start. Type 3 follows a
// runtime parameter that nothing here sets, so it reads 0. A returned count or more is no entry.
u32 choose_variation(VariationTable& table)
{
    const u32 header = table.header;
    const u32 count = header & 0x1FFF;
    const u32 last = (header >> 17) & 0x1FFF;
    const auto played = [&table](u32 index)
    {
        return index / 32 < table.played.size() && (table.played[index / 32] & (1u << (index & 31))) != 0;
    };
    const auto span = [&table](u32 index)
    {
        return i32(table.ranges[index].second) - i32(table.ranges[index].first);
    };
    u32 chosen = 0xFFFFFFFFu;
    if ((header & 0x40000000u) == 0 || count <= last)
    {
        const u32 type = (header >> 13) & 0xF;
        if (type == 4 || type == 0 || type == 2)
        {
            i32 range = 0x2711;
            if (type == 0)
            {
                if (last < count)
                {
                    range = 10000 - span(last);
                }
            }
            else if (type == 2 && count != 0)
            {
                for (u32 index = 0; index < count; ++index)
                {
                    if (played(index))
                    {
                        range += -1 - span(index);
                    }
                }
                if (range == 0)
                {
                    std::fill(table.played.begin(), table.played.end(), 0u);
                    range = 0x2711;
                }
            }
            u32 draw = xact_random_below(u32(range));
            chosen = 0;
            while (chosen < count)
            {
                if ((type == 0 && chosen == last) || (type == 2 && played(chosen)))
                {
                    draw = draw + 1 + u32(span(chosen));
                }
                if (table.ranges[chosen].first <= draw && draw <= table.ranges[chosen].second)
                {
                    break;
                }
                ++chosen;
            }
            if ((table.header & 0x1E000) == 0x4000 && chosen < count)
            {
                if (table.played.size() <= chosen / 32)
                {
                    table.played.resize(chosen / 32 + 1);
                }
                table.played[chosen / 32] |= 1u << (chosen & 31);
            }
        }
        else if (type == 3)
        {
            u32 matches = 0;
            for (u32 index = 0; index < count; ++index)
            {
                if (table.ranges[index].first == 0)
                {
                    ++matches;
                    chosen = index;
                }
            }
            if (matches > 1)
            {
                u32 pick = xact_random_below(matches);
                for (u32 index = 0; index < count; ++index)
                {
                    if (table.ranges[index].first == 0 && pick-- == 0)
                    {
                        chosen = index;
                        break;
                    }
                }
            }
        }
        else if (type == 5 && count <= last)
        {
            chosen = xact_random_below(count);
        }
        else if (type == 1 || type == 5)
        {
            chosen = last + 1;
            if (count <= chosen)
            {
                chosen = 0;
            }
        }
    }
    else
    {
        table.header &= ~0x40000000u;
        chosen = last;
    }
    table.header = table.header ^ (((chosen << 17) ^ table.header) & 0x3FFE0000u);
    return chosen;
}

i32 variation_table_at(std::span<const u8> blob, size_t at)
{
    const u8* key = blob.data() + at;
    if (const auto found = g_variation_table_index.find(key); found != g_variation_table_index.end())
    {
        return found->second;
    }
    VariationTable table;
    table.header = ru32(blob, at);
    const u32 count = table.header & 0x1FFF;
    for (u32 index = 0; index < count && at + 4 + size_t(index) * 8 + 8 <= blob.size(); ++index)
    {
        table.ranges.emplace_back(ru16(blob, at + 8 + size_t(index) * 8), ru16(blob, at + 10 + size_t(index) * 8));
    }
    const size_t bits = at + 4 + size_t(count) * 8;
    for (u32 word = 0; word < (count + 31) / 32 && bits + size_t(word) * 4 + 4 <= blob.size(); ++word)
    {
        table.played.push_back(ru32(blob, bits + size_t(word) * 4));
    }
    g_variation_tables.push_back(std::move(table));
    const i32 index = i32(g_variation_tables.size()) - 1;
    g_variation_table_index[key] = index;
    return index;
}

struct WaveChoice
{
    std::optional<Sound3D> sound_3d;
    // The variation tables this wave was listed from: the cue's table of sounds and the sound's table of waves,
    // with this wave's entry in each (-1 when there is no table).
    i32 cue_table = -1;
    u32 cue_slot = 0;
    i32 sound_table = -1;
    u32 sound_slot = 0;
    u8 category = 0;
    std::string bank; // WBND internal name, e.g. Gharddrive1
    u16 index = 0;
    u16 weight_lo = 0;
    u16 weight_hi = 10000;
    // The play-wave event's loop count (event header +6): 0 plays once, 0xFFFF loops until the cue stops. The
    // wave bank entry's loop region (+0x10/+0x14) is what repeats.
    u16 loop_count = 0;
};

#if GBTG_HAS_XAUDIO2

struct WaveFile
{
    std::filesystem::path path;
    u32 count = 0;
};

struct PcmVoice
{
    u8 category = 0;
    u64 handle = 0;
    IXAudio2SourceVoice* voice = nullptr;
    std::vector<u8> pcm;
    std::string cue;
    bool held = false; // Stopped by the dialog hold. Start resumes the buffer.
    // A native audio core cue (IXACTSoundBank_Prepare). Only the native core stops and releases it; the host
    // cue paths below leave it alone.
    bool native = false;
#if defined(GBTG_INSPECTOR_APP)
    float duration = 0.f; // Decoded PCM duration for host inspection diagnostics.
#endif
    // The virtual audio clock (pcm_use_virtual_audio_clock): how long the voice plays, how far it has played in game
    // time, and whether it is started.
    double virtual_length = 0.0;
    double virtual_position = 0.0;
    bool virtual_started = false;
};

IXAudio2* g_xa = nullptr;
IXAudio2MasteringVoice* g_master = nullptr;
float g_master_volume = 1.f;
std::array<float, 256> g_category_volumes = []()
{
    std::array<float, 256> volumes;
    volumes.fill(1.f);
    return volumes;
}();
bool g_listener_effect = false;
// The Xbox listener reverb (I3DL2) is a wet send beside each voice's dry path, so it lives on its own submix.
IXAudio2SubmixVoice* g_reverb = nullptr;
// XACT's 3D engine: the listener and each 3D (mono) sound source, mixed into the master channels by X3DAudio.
X3DAUDIO_HANDLE g_x3d{};
bool g_x3d_ready = false;
X3DAUDIO_LISTENER g_x3d_listener{};
UINT32 g_master_channels = 0;
// A sound source's I3DL2 settings as IXACTSoundSource_SetI3DL2Source stores them (0x001C4A63, source +0xC4..+0xE4)
// over the reset values of 0x001C414B: all zero but the occlusion LF ratio, 0.25.
struct SourceI3DL2
{
    i32 direct = 0; // +0xC4
    i32 direct_hf = 0; // +0xC8
    i32 room = 0; // +0xCC
    i32 room_hf = 0; // +0xD0
    float room_rolloff = 0.f; // +0xD4
    i32 obstruction_hf = 0; // +0xD8
    float obstruction_lf_ratio = 0.f; // +0xDC
    i32 occlusion_hf = 0; // +0xE0
    float occlusion_lf_ratio = 0.25f; // +0xE4
};
struct SpatialSource
{
    X3DAUDIO_VECTOR position{};
    SourceI3DL2 i3dl2;
    std::array<float, 8> reverb_send{}; // The send's matrix into the reverb as routed, before the room level.
};
// The listener's I3DL2 HF reference (IXACTEngine_SetI3dl2Listener word 11), which the source calculation reads
// (0x001C67F7 -> 0x001CE923). DirectSound's default until the game sets a listener.
float g_listener_hf_reference = 5000.f;
// The mix's sample rate, for the one-pole filters retail computes at 48000 Hz.
UINT32 g_mix_sample_rate = 48000;
std::unordered_map<IXAudio2Voice*, SpatialSource> g_spatial_sources;
// Owned cue voices routed into a sound-source submix by IXACTSoundBank_PlayEx.
std::map<IXAudio2SourceVoice*, IXAudio2Voice*> g_source_routes;
// Each owned cue voice's 3D record, from its sound entry (absent for a sound without the 3D flag).
std::unordered_map<IXAudio2SourceVoice*, Sound3D> g_cue_3d;
std::vector<PcmVoice> g_voices;
std::vector<PcmVoice> g_bed;
// pcm_use_virtual_audio_clock: the game's voice-state queries are answered from game time instead of the device.
bool g_virtual_audio_clock = false;
u64 g_next_voice_handle = 0;

PcmVoice* find_pcm_voice(IXAudio2SourceVoice* voice)
{
    for (auto* voices : {&g_voices, &g_bed})
    {
        for (PcmVoice& slot : *voices)
        {
            if (slot.voice == voice)
            {
                return &slot;
            }
        }
    }
    return nullptr;
}

// Whether the voice still has audio to play: XAudio2's BuffersQueued, or under the virtual clock whether its
// game-time position is short of its length.
bool pcm_voice_queued(IXAudio2SourceVoice* voice)
{
    if (g_virtual_audio_clock)
    {
        if (const PcmVoice* slot = find_pcm_voice(voice); slot != nullptr)
        {
            return slot->virtual_position < slot->virtual_length;
        }
    }
    XAUDIO2_VOICE_STATE state{};
    voice->GetState(&state);
    return state.BuffersQueued != 0;
}

void start_virtual_voice(IXAudio2SourceVoice* voice, bool started)
{
    if (PcmVoice* slot = find_pcm_voice(voice); slot != nullptr)
    {
        slot->virtual_started = started;
    }
}

// A stop that flushes the buffers: the voice has nothing left to play.
void end_virtual_voice(IXAudio2SourceVoice* voice)
{
    if (PcmVoice* slot = find_pcm_voice(voice); slot != nullptr)
    {
        slot->virtual_position = slot->virtual_length;
        slot->virtual_started = false;
    }
}
bool g_device_failed = false;
HRESULT g_device_result = S_OK;
bool g_catalog_ready = false;
std::map<std::string, WaveFile> g_wavefiles;                 // WBND name -> file
std::map<std::string, std::vector<WaveChoice>> g_cues;      // full cue string -> variations
// Name copied to the stream object +0x86 by 0x0004D4C0.
std::string g_stream_cue;

constexpr const char* kNoSound = "XACT_SOUNDBANK_GMISC_NOSOUND";

std::string event_name(std::span<const u8> rec, size_t at)
{
    if (at >= rec.size())
    {
        return {};
    }
    size_t n = 0;
    while (at + n < rec.size() && rec[at + n] != 0)
    {
        ++n;
    }
    return std::string(reinterpret_cast<const char*>(rec.data() + at), n);
}

bool is_xact_cue(const std::string& name)
{
    return name.rfind("XACT_SOUNDBANK_", 0) == 0;
}

void release_voice(PcmVoice& slot)
{
    if (slot.voice == nullptr)
    {
        return;
    }
    slot.voice->Stop(0);
    slot.voice->FlushSourceBuffers();
    g_source_routes.erase(slot.voice);
    g_cue_3d.erase(slot.voice);
    slot.voice->DestroyVoice();
    slot.voice = nullptr;
}

// 0x00038780 finds the bank slot by name and releases it. `all` is the stream
// replace. A type 0x0C window stops the one voice that window started.
void stop_named(const std::string& cue, bool all)
{
    for (size_t i = 0; i < g_voices.size();)
    {
        if (g_voices[i].cue != cue)
        {
            ++i;
            continue;
        }
        release_voice(g_voices[i]);
        g_voices.erase(g_voices.begin() + static_cast<std::ptrdiff_t>(i));
        if (!all)
        {
            return;
        }
    }
}

std::vector<std::string> device_names(std::span<const u8> blob)
{
    std::vector<std::string> names;
    const u32 d0 = ru32(blob, 8);
    size_t p = d0;
    while (p + 16 <= blob.size())
    {
        size_t n = 0;
        while (n < 16 && blob[p + n] != 0)
        {
            if (blob[p + n] < 32 || blob[p + n] >= 127)
            {
                return names;
            }
            ++n;
        }
        if (n == 0)
        {
            break;
        }
        names.emplace_back(reinterpret_cast<const char*>(blob.data() + p), n);
        p += 16;
    }
    return names;
}

void push_wave(std::vector<WaveChoice>& out, std::span<const u8> blob, size_t o, const std::vector<std::string>& devs,
    u16 loop_count = 0)
{
    if (o + 8 > blob.size())
    {
        return;
    }
    const i16 wave = rs16(blob, o);
    const i16 bank = rs16(blob, o + 2);
    if (wave < 0 || bank < 0 || size_t(bank) >= devs.size())
    {
        return;
    }
    WaveChoice c;
    c.bank = devs[size_t(bank)];
    c.index = static_cast<u16>(wave);
    c.weight_lo = ru16(blob, o + 4);
    c.weight_hi = ru16(blob, o + 6);
    if (c.weight_lo == 0 && c.weight_hi == 0)
    {
        c.weight_hi = 10000;
    }
    c.loop_count = loop_count;
    out.push_back(c);
}

void push_variation_table(std::vector<WaveChoice>& out, std::span<const u8> blob, size_t jump,
                          const std::vector<std::string>& devs, u16 loop_count = 0)
{
    if (jump + 4 > blob.size())
    {
        return;
    }
    const u32 count = ru32(blob, jump) & 0x1FFF;
    if (count == 0 || count > 64)
    {
        return;
    }
    const i32 table = variation_table_at(blob, jump);
    for (u32 i = 0; i < count; ++i)
    {
        const size_t before = out.size();
        push_wave(out, blob, jump + 4 + size_t(i) * 8, devs, loop_count);
        if (out.size() != before)
        {
            out.back().sound_table = table;
            out.back().sound_slot = i;
        }
    }
}

void waves_from_entry_data(std::vector<WaveChoice>& out, std::span<const u8> blob, i16 entry, u16 simple_count,
                      const std::vector<std::string>& devs)
{
    const size_t base = 0x38 + size_t(simple_count) * 0x14 + size_t(entry) * 0x14;
    if (entry < 0 || base + 0x14 > blob.size())
    {
        return;
    }
    const u8 flags = blob[base + 0x0b];
    if ((flags & 0x10) != 0)
    {
        push_variation_table(out, blob, ru32(blob, base), devs);
        return;
    }
    if ((flags & 0x08) != 0)
    {
        push_wave(out, blob, base, devs);
        return;
    }
    const size_t jump = ru32(blob, base);
    if (jump >= blob.size())
    {
        return;
    }
    const u8 sound_count = blob[jump];
    if (sound_count > 16 || jump + 5 > blob.size())
    {
        return;
    }
    size_t sound = ru32(blob, jump + 1) & 0x00FFFFFFu;
    for (u8 i = 0; i < sound_count; ++i)
    {
        if (sound + 8 > blob.size())
        {
            break;
        }
        const u8 kind = blob[sound];
        const u8 size = blob[sound + 4];
        if (kind == 0 || kind == 1)
        {
            const u8 sub = blob[sound + 5];
            const u16 loop_count = ru16(blob, sound + 6);
            if (sub == 0x00 || sub == 0x40)
            {
                // A 4-byte payload is the wave index and the device index.
                // The next bytes belong to the following record.
                if (size == 4)
                {
                    const i16 wave = rs16(blob, sound + 8);
                    const i16 bank = rs16(blob, sound + 10);
                    if (wave >= 0 && bank >= 0 && size_t(bank) < devs.size())
                    {
                        WaveChoice c;
                        c.bank = devs[size_t(bank)];
                        c.index = static_cast<u16>(wave);
                        c.weight_hi = 10000;
                        c.loop_count = loop_count;
                        out.push_back(std::move(c));
                    }
                }
                else
                {
                    push_wave(out, blob, sound + 8, devs, loop_count);
                }
            }
            else if (sub == 0x04 || sub == 0x44)
            {
                push_variation_table(out, blob, ru32(blob, sound + 8), devs, loop_count);
            }
        }
        sound += 8u + size;
    }
}

void waves_from_entry(std::vector<WaveChoice>& out, std::span<const u8> blob, i16 entry,
    u16 simple_count, const std::vector<std::string>& devs)
{
    const size_t first = out.size();
    waves_from_entry_data(out, blob, entry, simple_count, devs);
    const size_t record = 0x38 + size_t(simple_count) * 0x14 + size_t(entry) * 0x14;
    std::optional<Sound3D> sound_3d;
    if ((blob[record + 0xB] & 1) != 0)
    {
        // 0x001C210A applies the 3D record (0x001C085E) when the entry's flags have bit 0.
        const size_t settings = size_t(ru32(blob, 0x10)) + size_t(ru16(blob, record + 0xC)) * 0x28;
        if (settings + 0x28 <= blob.size())
        {
            Sound3D values;
            values.min_distance = std::bit_cast<float>(ru32(blob, settings + 8));
            values.max_distance = std::bit_cast<float>(ru32(blob, settings + 0xC));
            values.rolloff = std::bit_cast<float>(ru32(blob, settings + 0x14));
            values.mode = blob[settings + 0x1C];
            const u8 points = blob[settings + 0x1D];
            for (u8 point = 0; point < points && settings + 0x1E + point < blob.size(); ++point)
            {
                values.curve.push_back(float(blob[settings + 0x1E + point]) * std::bit_cast<float>(0x3B808081u));
            }
            sound_3d = std::move(values);
        }
    }
    for (size_t index = first; index < out.size(); ++index)
    {
        // Retail XACT sound-entry byte +0xA, used by 0x001BB8C8.
        out[index].category = blob[record + 0xA];
        out[index].sound_3d = sound_3d;
    }
}

std::vector<WaveChoice> cue_waves(std::span<const u8> blob, u16 cue_index, const std::vector<std::string>& devs)
{
    // Xbox sound banks are XACT 1.2 (tool version 11). A cue index record at 0x38
    // names a 20-byte cue entry, and that entry names the wave. The bytes at the
    // end of a later sound are a different cue.
    if (blob.size() < 0x38 || ru16(blob, 4) != 11)
    {
        return {};
    }
    const u16 simple_count = ru16(blob, 0x1e);
    const u16 entry_count = ru16(blob, 0x1c);
    if (cue_index >= simple_count)
    {
        return {};
    }
    const size_t rec = 0x38 + size_t(cue_index) * 0x14;
    if (rec + 0x14 > blob.size())
    {
        return {};
    }
    std::vector<WaveChoice> out;
    const i16 entry = rs16(blob, rec + 2);
    if (entry < 0)
    {
        const size_t jump = ru32(blob, rec + 8);
        if (jump >= blob.size())
        {
            return {};
        }
        // The cue's own variation table of sounds (0x001C2DFC picks one with 0x001C2A3B).
        const u32 count = ru32(blob, jump) & 0x1FFF;
        const i32 table = variation_table_at(blob, jump);
        for (u32 i = 0; i < count && i < 64; ++i)
        {
            const size_t o = jump + 4 + size_t(i) * 8;
            if (o + 2 > blob.size())
            {
                break;
            }
            const i16 alt = rs16(blob, o);
            if (alt >= 0 && size_t(alt) < entry_count)
            {
                const size_t first = out.size();
                waves_from_entry(out, blob, alt, simple_count, devs);
                for (size_t index = first; index < out.size(); ++index)
                {
                    out[index].cue_table = table;
                    out[index].cue_slot = i;
                }
            }
        }
        return out;
    }
    if (size_t(entry) >= entry_count)
    {
        return {};
    }
    waves_from_entry(out, blob, entry, simple_count, devs);
    return out;
}

void index_wavebanks(const std::filesystem::path& game_dir)
{
    const auto dir = game_dir / "xwavebank";
    std::error_code ec;
    for (const auto& ent : std::filesystem::directory_iterator(dir, ec))
    {
        std::ifstream in(ent.path(), std::ios::binary);
        u8 hdr[16]{};
        in.read(reinterpret_cast<char*>(hdr), 16);
        if (!in || std::memcmp(hdr, "WBND", 4) != 0)
        {
            continue;
        }
        const u32 bd = ru32(hdr, 8);
        in.seekg(bd + 4);
        u8 countb[4]{};
        in.read(reinterpret_cast<char*>(countb), 4);
        char nameb[16]{};
        in.read(nameb, 16);
        if (!in)
        {
            continue;
        }
        WaveFile wf;
        wf.path = ent.path();
        std::memcpy(&wf.count, countb, 4);
        const std::string name(nameb, strnlen(nameb, 16));
        if (!name.empty())
        {
            g_wavefiles[name] = wf;
        }
    }
}

void build_catalog(const Game& g)
{
    if (g_catalog_ready)
    {
        return;
    }
    g_catalog_ready = true;
    index_wavebanks(g.aids.game_dir);

    std::map<std::string, std::vector<std::string>> cue_names;
    const BundleEntry* list = find_entry(g.aids.common, "aid_xcuelist_ghoulies_default");
    if (!list)
    {
        log::warn("audio missing cue list");
        return;
    }
    std::string text(reinterpret_cast<const char*>(list->cpu.data()), list->cpu.size());
    size_t line = 0;
    while (line < text.size())
    {
        size_t end = text.find('\n', line);
        if (end == std::string::npos)
        {
            end = text.size();
        }
        std::string row = text.substr(line, end - line);
        if (!row.empty() && row.back() == '\r')
        {
            row.pop_back();
        }
        const size_t tab = row.find('\t');
        if (tab != std::string::npos)
        {
            std::string bank = row.substr(0, tab);
            std::string cue = row.substr(tab + 1);
            for (char& c : bank)
            {
                if (c >= 'a' && c <= 'z')
                {
                    c = static_cast<char>(c - 32);
                }
            }
            for (char& c : cue)
            {
                if (c >= 'a' && c <= 'z')
                {
                    c = static_cast<char>(c - 32);
                }
            }
            cue_names[bank].push_back(cue);
        }
        line = end + 1;
    }

    std::vector<std::string> banks_by_length;
    for (const auto& e : g.aids.common.entries)
    {
        if (e.type != ResourceType::XSoundbank || e.cpu.size() < 64 || std::memcmp(e.cpu.data(), "SDBK", 4) != 0)
        {
            continue;
        }
        size_t n = 0;
        while (40 + n < e.cpu.size() && e.cpu[40 + n] != 0)
        {
            ++n;
        }
        std::string bank(reinterpret_cast<const char*>(e.cpu.data() + 40), n);
        for (char& c : bank)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = static_cast<char>(c - 32);
            }
        }
        const auto found = cue_names.find(bank);
        if (found == cue_names.end())
        {
            continue;
        }
        const auto devs = device_names(e.cpu);
        const auto& names = found->second;
        for (u16 i = 0; i < names.size(); ++i)
        {
            auto waves = cue_waves(e.cpu, i, devs);
            if (waves.empty())
            {
                continue;
            }
            g_cues["XACT_SOUNDBANK_" + bank + "_" + names[i]] = std::move(waves);
        }
        banks_by_length.push_back(bank);
    }
    log::verbose("audio cues %zu wavebanks %zu", g_cues.size(), g_wavefiles.size());
    (void)banks_by_length;
}

struct NativeCueNotes
{
    bool prepared = false;
    bool started = false;
    bool playing = false;
    bool stopped = false; // IXACTSoundBank_Stop: XACT reports the cue stopped even while its tails play out.
};
std::unordered_map<void*, NativeCueNotes> g_cue_notes; // By prepared XAudio2 voice.

// XACT reclaims a cue once its stopped notification (1) has been delivered: the retail callback frees its wrapper
// there (0x000398E0 -> 0x0015CE90) and nothing uses the handle again, so its host voice goes too.
void release_native_cue_voice(IXAudio2SourceVoice* voice)
{
    for (size_t i = 0; i < g_bed.size(); ++i)
    {
        if (g_bed[i].native && g_bed[i].voice == voice)
        {
            release_voice(g_bed[i]);
            g_bed.erase(g_bed.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

// The XAudio2 mixer thread reads the voices' PCM straight from g_voices and g_bed. Static destruction frees that
// storage at exit, so stop the engine and destroy the source voices first; this object is constructed after them,
// so it is destroyed before them.
struct AudioDeviceTeardown
{
    ~AudioDeviceTeardown()
    {
        if (g_xa == nullptr)
        {
            return;
        }
        g_xa->StopEngine();
        for (auto* voices : {&g_voices, &g_bed})
        {
            for (PcmVoice& slot : *voices)
            {
                release_voice(slot);
            }
        }
    }
} g_device_teardown;

bool ensure_device()
{
    if (g_xa)
    {
        return true;
    }
    if (g_device_failed)
    {
        return false;
    }
#if defined(_WIN32)
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co) && co != RPC_E_CHANGED_MODE)
    {
        g_device_result = co;
        g_device_failed = true;
        log::error("audio CoInitializeEx failed");
        return false;
    }
#endif
    g_device_result = XAudio2Create(&g_xa, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (SUCCEEDED(g_device_result))
    {
        g_device_result = g_xa->CreateMasteringVoice(&g_master);
    }
    if (FAILED(g_device_result))
    {
        g_device_failed = true;
        log::error("audio XAudio2Create failed");
        return false;
    }
    g_master->SetVolume(g_master_volume);
    XAUDIO2_VOICE_DETAILS details;
    g_master->GetVoiceDetails(&details);
    g_master_channels = details.InputChannels;
    g_mix_sample_rate = details.InputSampleRate;
#if defined(_WIN32)
    // GBTG_RECORD_AUDIO=<file.wav>: record the mix for listening (mix_recorder.h).
    if (const char* record = std::getenv("GBTG_RECORD_AUDIO"); record != nullptr && record[0] != 0)
    {
        if (IUnknown* recorder = create_mix_recorder(record))
        {
            XAUDIO2_EFFECT_DESCRIPTOR descriptor{recorder, TRUE, details.InputChannels};
            const XAUDIO2_EFFECT_CHAIN chain{1, &descriptor};
            if (FAILED(g_master->SetEffectChain(&chain)))
            {
                log::error("audio: the mix recorder could not be attached");
            }
            recorder->Release();
        }
        else
        {
            log::error("audio: GBTG_RECORD_AUDIO %s could not be opened", record);
        }
    }
#endif
    DWORD channel_mask = 0;
    if (SUCCEEDED(g_master->GetChannelMask(&channel_mask))
        && SUCCEEDED(X3DAudioInitialize(channel_mask, X3DAUDIO_SPEED_OF_SOUND, g_x3d)))
    {
        g_x3d_ready = true;
        g_x3d_listener.OrientFront = {0.f, 0.f, 1.f};
        g_x3d_listener.OrientTop = {0.f, 1.f, 0.f};
    }
    if (SUCCEEDED(g_xa->CreateSubmixVoice(&g_reverb, details.InputChannels, details.InputSampleRate)))
    {
        IUnknown* effect = nullptr;
        if (SUCCEEDED(XAudio2CreateReverb(&effect)))
        {
            XAUDIO2_EFFECT_DESCRIPTOR descriptor{effect, TRUE, details.InputChannels};
            const XAUDIO2_EFFECT_CHAIN chain{1, &descriptor};
            if (SUCCEEDED(g_reverb->SetEffectChain(&chain)))
            {
                g_listener_effect = true;
            }
            // Silent until the game sets a listener reverb.
            g_reverb->SetVolume(0.f);
            effect->Release();
        }
    }
    return true;
}

// DirectSound's mix bins for a voice given none (0x001C7493, the table at 0x001D09F4 by channel count / 2): a mono
// or a stereo voice goes to bins 0 and 1, front left and right, at 0 mB, so a mono voice plays at full level on both
// (XAudio2's own default puts it on the centre speaker alone). XACT gives a 2D voice no list (0x001C38BB), and a
// voice in a 2D source keeps it (0x001C437F); in a 2D source the bins are the source's left and right.
void apply_default_mix_bins(IXAudio2Voice* voice, IXAudio2Voice* destination)
{
    XAUDIO2_VOICE_DETAILS source{};
    XAUDIO2_VOICE_DETAILS target{};
    voice->GetVoiceDetails(&source);
    destination->GetVoiceDetails(&target);
    const UINT32 inputs = source.InputChannels;
    const UINT32 outputs = target.InputChannels;
    if (inputs > 2 || outputs < 2 || outputs > 8)
    {
        // The four- and six-channel lists name the speakers XAudio2's default already uses; no retail wave has them.
        return;
    }
    std::array<float, 16> matrix{};
    for (UINT32 input = 0; input < inputs; ++input)
    {
        for (UINT32 output = 0; output < 2; ++output)
        {
            matrix[size_t(output) * inputs + input] = inputs == 1 || input == output ? 1.f : 0.f;
        }
    }
    voice->SetOutputMatrix(destination, inputs, outputs, matrix.data());
}

// A 2D voice's send: the master voice at the default mix bins. None of DirectSound's default lists has bin 10, the
// I3DL2 reverb's input, which only the 3D lists (0x001D0B24, 0x001D0B2C, 0x001D0B34) name, so only 3D sources
// reach the listener reverb (apply_spatial_source).
HRESULT route_to_master(IXAudio2Voice* voice)
{
    const HRESULT status = voice->SetOutputVoices(nullptr);
    if (SUCCEEDED(status))
    {
        apply_default_mix_bins(voice, g_master);
    }
    return status;
}

// The retail distance attenuation of a 3D voice in millibels (DirectSound 0x001C90CF, through 0x001C5941): none
// within the minimum distance; past it the distance is clamped to the maximum and either the record's curve (equal
// steps from the minimum to the maximum, starting from full level, 0x001C58CB) or the rolloff law
// -2000 * log10(1 + (d / min - 1) * rolloff) (0x001C8D85) applies. Mode 2 turns 3D off. The listener's rolloff is
// DirectSound's default, 1, as the game never sets it. 0x001C67F7 also passes the source's mute-at-maximum flag
// (+0x9C bit 2), but nothing sets that bit: 0x001C414B and 0x001C38BB write bits 0 and 1, 0x001C4240 bit 0, so a
// sound past its maximum keeps the maximum's level.
i32 sound_3d_attenuation(const Sound3D& sound, float distance)
{
    if (sound.mode == 2 || distance <= sound.min_distance)
    {
        return 0;
    }
    if (sound.max_distance < distance)
    {
        distance = sound.max_distance;
    }
    if (sound.curve.empty())
    {
        const float x = (distance / sound.min_distance - 1.f) * sound.rolloff;
        if (x < 0.f)
        {
            return 0;
        }
        return i32(float(0.3010299956639812L * std::log2(static_cast<long double>(x) + 1.0L)) * -2000.f);
    }
    const u32 count = u32(sound.curve.size());
    const float offset = distance - sound.min_distance;
    const float step = (sound.max_distance - sound.min_distance) / float(count);
    const float inverse = 1.f / step;
    u32 index = u32(inverse * offset); // 0x001673D8 (_ftol) truncates.
    if (index >= count)
    {
        index = count - 1;
    }
    float from = 1.f;
    float to = sound.curve[0];
    float within = offset;
    if (index != 0)
    {
        from = sound.curve[index - 1];
        to = sound.curve[index];
        within = offset - float(index) * step;
    }
    const float level = (to - from) * (inverse * within) + from;
    if (!(level > 0.f))
    {
        return -10000;
    }
    if (level >= 1.f)
    {
        return 0;
    }
    return i32(float(0.3010299956639812L * std::log2(static_cast<long double>(level))) * 2000.f);
}

// DirectSound's millibels to a gain (0x001CE3FF), called with twice the level, so a power: 2^(log2(10) * x / 2000)
// in floats as the x87 code rounds them.
float directsound_millibel_power(i32 doubled)
{
    const float scaled = float(static_cast<long double>(doubled) * std::bit_cast<float>(0x3A03126Fu)); // 1 / 2000
    const float log2_10 = std::bit_cast<float>(0x40549A78u);
    return float(std::exp2(static_cast<long double>(log2_10) * scaled));
}

// The one-pole low-pass coefficient (DirectSound 0x001CE78B) that gives an HF level, in millibels, at the reference
// frequency: the pole a = (1 - cG - sqrt(2(1 - c)G - (1 - c^2)G^2)) / (1 - G), with c the cosine of the reference
// over the rate and G the level's power, as a 16.16 fraction. No level, or the first argument at -10000 or below,
// gives 0 (no filter); a pole of 1 or more gives 0xFFFF.
u32 directsound_hf_filter_coefficient(i32 level, i32 hf_level, float reference, float rate)
{
    if (!(-10000 < level) || hf_level == 0)
    {
        return 0;
    }
    const double cosine = double(std::cos(static_cast<long double>(reference) / rate
        * static_cast<long double>(std::bit_cast<float>(0x40C90FDBu)))); // DAT_0032B298, 2 pi
    const long double power = directsound_millibel_power(hf_level * 2);
    const long double numerator = (1.0L - cosine * power)
        - std::sqrt((1.0L - cosine) * power * 2.0L - (1.0L - static_cast<long double>(cosine) * cosine) * power * power);
    const long double pole = numerator / (1.0L - power);
    if (pole < 1.0L)
    {
        return u32(i32(float(pole * 65536.0L))); // DAT_001E338C; 0x001CE3F6 truncates.
    }
    return 0xFFFFu;
}

// What DirectSound's 3D calculation makes of a source's I3DL2 settings (0x001C67F7 -> 0x001CE923): the direct
// path's level and HF filter, and the room (reverb) path's.
struct SourceI3DL2Levels
{
    i32 direct = 0;
    i32 room = 0;
    u32 direct_filter = 0;
    u32 room_filter = 0;
};

// DirectSound 0x001CE923. Obstruction and occlusion both lower the direct path: their LF parts (HF x LF ratio) the
// level, their HF parts (HF x (1 - LF ratio)) the HF level, clamped to -10000..0. Only occlusion lowers the room
// path; its HF level adds the room level (+0xC) where lRoomHF (+0x10) would be expected, as retail reads it. Each
// HF level becomes a one-pole filter at the listener's HF reference and 48000 Hz (DAT_0020A16C).
SourceI3DL2Levels directsound_source_i3dl2_levels(const SourceI3DL2& source, float hf_reference)
{
    if (source.room_rolloff != 0.f)
    {
        // The room rolloff times the listener's scales a value at the 3D block's +0x68 (source +0xAC) whose writer
        // was not traced; above 1 it adds 2000 log10 of the product to the room level. No retail call sets a
        // rolloff: 0x00039620 passes 0, so the product is 0 and the room level is the occlusion's alone.
        throw Error("IXACTSoundSource_SetI3DL2Source with a room rolloff: 0x001CE923's distance term is not traced");
    }
    const long double obstruction_lf = static_cast<long double>(source.obstruction_hf) * source.obstruction_lf_ratio;
    const long double occlusion_lf_wide = static_cast<long double>(source.occlusion_hf) * source.occlusion_lf_ratio;
    const float occlusion_lf = float(occlusion_lf_wide);
    SourceI3DL2Levels levels;
    levels.direct = i32(float(occlusion_lf_wide + obstruction_lf)) + source.direct;
    const long double obstruction_hf = (1.0L - source.obstruction_lf_ratio) * source.obstruction_hf;
    const long double occlusion_hf_wide = (1.0L - source.occlusion_lf_ratio) * source.occlusion_hf;
    const float occlusion_hf = float(occlusion_hf_wide);
    i32 direct_hf = i32(float(occlusion_hf_wide + obstruction_hf)) + source.direct_hf;
    direct_hf = std::clamp(direct_hf, -10000, 0);
    levels.room = i32(occlusion_lf) + source.room;
    const i32 room_hf = std::clamp(i32(occlusion_hf) + source.room, -10000, 0);
    levels.direct_filter = directsound_hf_filter_coefficient(0, direct_hf, hf_reference, 48000.f);
    levels.room_filter = directsound_hf_filter_coefficient(0, room_hf, hf_reference, 48000.f);
    return levels;
}

// A DirectSound 16.16 pole as XAudio2's one-pole coefficient (1 - pole) at the mix's rate. The pole is retail's at
// 48000 Hz; raising it to 48000 / rate keeps its time constant, so its response is the same at another rate.
XAUDIO2_FILTER_PARAMETERS one_pole_low_pass(u32 coefficient)
{
    const double pole = double(coefficient) / 65536.0;
    const double at_rate = std::pow(pole, 48000.0 / double(g_mix_sample_rate));
    return XAUDIO2_FILTER_PARAMETERS{LowPassOnePoleFilter, float(1.0 - at_rate), 1.f};
}

// Each 3D cue routed into this source sends at its record's distance level; the source itself only pans.
void apply_routed_cue_levels(IXAudio2Voice* source, const X3DAUDIO_VECTOR& position)
{
    const float dx = position.x - g_x3d_listener.Position.x;
    const float dy = position.y - g_x3d_listener.Position.y;
    const float dz = position.z - g_x3d_listener.Position.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    for (const auto& [cue, routed] : g_source_routes)
    {
        if (routed != source)
        {
            continue;
        }
        const auto found = g_cue_3d.find(cue);
        if (found == g_cue_3d.end())
        {
            continue;
        }
        const float level = std::pow(10.f, float(sound_3d_attenuation(found->second, distance)) / 2000.f);
        XAUDIO2_VOICE_DETAILS details{};
        cue->GetVoiceDetails(&details);
        std::array<float, 8> matrix{};
        const UINT32 channels = std::min<UINT32>(details.InputChannels, 8);
        for (UINT32 channel = 0; channel < channels; ++channel)
        {
            matrix[channel] = level;
        }
        cue->SetOutputMatrix(source, channels, 1, matrix.data());
    }
}

// The master-channel matrix of one 3D source from its position and the listener: panning only, as each cue's
// distance level is its own record's (apply_routed_cue_levels).
void apply_spatial_source(IXAudio2Voice* voice, const SpatialSource& source)
{
    if (!g_x3d_ready || g_master_channels == 0 || g_master_channels > 8)
    {
        return;
    }
    static constexpr X3DAUDIO_DISTANCE_CURVE_POINT kFlatPoints[2]{{0.f, 1.f}, {1.f, 1.f}};
    static const X3DAUDIO_DISTANCE_CURVE kFlat{const_cast<X3DAUDIO_DISTANCE_CURVE_POINT*>(kFlatPoints), 2};
    X3DAUDIO_EMITTER emitter{};
    emitter.ChannelCount = 1;
    emitter.CurveDistanceScaler = 1.f;
    emitter.pVolumeCurve = const_cast<X3DAUDIO_DISTANCE_CURVE*>(&kFlat);
    emitter.DopplerScaler = 1.f;
    emitter.OrientFront = {0.f, 0.f, 1.f};
    emitter.OrientTop = {0.f, 1.f, 0.f};
    emitter.Position = source.position;
    std::array<float, 8> matrix{};
    X3DAUDIO_DSP_SETTINGS settings{};
    settings.SrcChannelCount = 1;
    settings.DstChannelCount = g_master_channels;
    settings.pMatrixCoefficients = matrix.data();
    X3DAudioCalculate(g_x3d, &g_x3d_listener, &emitter, X3DAUDIO_CALCULATE_MATRIX, &settings);
    // The source's I3DL2 obstruction and occlusion: the direct level and HF filter on the dry send, the room level
    // and filter on the reverb send. A 3D source's +0x9C bit 1 (0x001C38BB) gives its room path the direct filter.
    const SourceI3DL2Levels levels = directsound_source_i3dl2_levels(source.i3dl2, g_listener_hf_reference);
    const float direct = std::pow(10.f, float(levels.direct) / 2000.f);
    for (float& level : matrix)
    {
        level *= direct;
    }
    voice->SetOutputMatrix(g_master, 1, g_master_channels, matrix.data());
    const XAUDIO2_FILTER_PARAMETERS direct_filter = one_pole_low_pass(levels.direct_filter);
    voice->SetOutputFilterParameters(g_master, &direct_filter);
    if (g_reverb != nullptr && g_listener_effect)
    {
        const float room = std::pow(10.f, float(levels.room) / 2000.f);
        std::array<float, 8> reverb = source.reverb_send;
        for (float& level : reverb)
        {
            level *= room;
        }
        voice->SetOutputMatrix(g_reverb, 1, g_master_channels, reverb.data());
        voice->SetOutputFilterParameters(g_reverb, &direct_filter);
    }
    apply_routed_cue_levels(voice, source.position);
}

void apply_all_spatial_sources()
{
    for (const auto& [voice, source] : g_spatial_sources)
    {
        apply_spatial_source(voice, source);
    }
}

X3DAUDIO_VECTOR unit_vector(const Vec3& vector)
{
    const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    if (!(length > 0.f))
    {
        return {0.f, 0.f, 0.f};
    }
    return {vector.x / length, vector.y / length, vector.z / length};
}

void reap_voices()
{
    for (size_t i = 0; i < g_voices.size();)
    {
        const bool done = !pcm_voice_queued(g_voices[i].voice);
        if (done || g_voices.size() > 24)
        {
            g_voices[i].voice->Stop(0);
            g_voices[i].voice->DestroyVoice();
            g_voices.erase(g_voices.begin() + static_cast<std::ptrdiff_t>(i));
            if (!done)
            {
                continue;
            }
            continue;
        }
        ++i;
    }
}

void stop_all()
{
    for (auto& v : g_voices)
    {
        release_voice(v);
    }
    g_voices.clear();
    g_stream_cue.clear();
}

bool load_wave(const WaveChoice& choice, std::vector<u8>& pcm, WAVEFORMATEX& fmt,
    u32& loop_begin, u32& loop_length)
{
    loop_begin = 0;
    loop_length = 0;
    SetLastError(ERROR_INVALID_DATA);
    const auto found = g_wavefiles.find(choice.bank);
    if (found == g_wavefiles.end() || choice.index >= found->second.count)
    {
        return false;
    }
    std::ifstream in(found->second.path, std::ios::binary);
    u8 hdr[48]{};
    in.read(reinterpret_cast<char*>(hdr), 40);
    if (!in)
    {
        return false;
    }
    const u32 meta_off = ru32(hdr, 16);
    const u32 meta_len = ru32(hdr, 20);
    const u32 wave_off = ru32(hdr, 32);
    const u32 count = found->second.count;
    if (count == 0 || meta_len / count < 16)
    {
        return false;
    }
    const u32 entry = meta_len / count;
    std::vector<u8> rec(entry);
    in.seekg(size_t(meta_off) + size_t(choice.index) * entry);
    in.read(reinterpret_cast<char*>(rec.data()), entry);
    if (!in)
    {
        return false;
    }
    const u32 mini = ru32(rec, 4);
    if ((mini & 3) != 0)
    {
        // Compressed Xbox payloads require a decoder before PCM submission.
        SetLastError(ERROR_NOT_SUPPORTED);
        return false;
    }
    const u32 play_off = ru32(rec, 8);
    const u32 play_len = ru32(rec, 12);
    // The longest wave, the credits music (Gdvd0[24]), is some 56 MiB of 16-bit stereo PCM.
    if (play_len == 0 || play_len > 128 * 1024 * 1024)
    {
        return false;
    }
    // These WBND headers store the channel count in the 3-bit field. Adding 1
    // (the later XNA layout) plays the stereo music bed as 3 channels, 1.5x fast.
    int channels = int((mini >> 2) & 7);
    const int rate = int((mini >> 5) & 0x3FFFF);
    const int bits = (mini & 0x80000000u) ? 16 : 8;
    const int bpf = bits / 8;
    if (channels < 1)
    {
        channels = 1;
    }
    if (rate < 8000 || bpf == 0)
    {
        return false;
    }
    if (play_len % (channels * bpf) != 0)
    {
        const int alt = int((mini >> 2) & 7) + 1;
        if (alt > 0 && play_len % (alt * bpf) == 0)
        {
            channels = alt;
        }
    }
    // Xbox WBND v3 stores the loop region at entry +0x10/+0x14 in bytes,
    // relative to the play region. XAudio2 expects interleaved sample frames.
    if (entry >= 24)
    {
        const u32 loop_offset_bytes = ru32(rec, 0x10);
        const u32 loop_length_bytes = ru32(rec, 0x14);
        const u32 block_align = u32(channels * bpf);
        if (loop_length_bytes != 0)
        {
            if (loop_offset_bytes > play_len || loop_length_bytes > play_len - loop_offset_bytes ||
                loop_offset_bytes % block_align != 0 || loop_length_bytes % block_align != 0)
            {
                return false;
            }
            loop_begin = loop_offset_bytes / block_align;
            loop_length = loop_length_bytes / block_align;
        }
    }
    pcm.resize(play_len);
    in.seekg(size_t(wave_off) + play_off);
    in.read(reinterpret_cast<char*>(pcm.data()), play_len);
    if (!in)
    {
        pcm.clear();
        return false;
    }
    fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = WORD(channels);
    fmt.nSamplesPerSec = DWORD(rate);
    fmt.wBitsPerSample = WORD(bits);
    fmt.nBlockAlign = WORD(channels * bpf);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    return true;
}

u64 play_choice(const std::string& cue, const WaveChoice& choice, bool loop = false, bool bed = false,
    bool start = true, i32* result = nullptr, void** prepared_voice = nullptr)
{
    const auto fail = [result](HRESULT error) -> u64
    {
        if (result != nullptr)
        {
            *result = static_cast<i32>(error);
        }
        return 0;
    };
    if (!ensure_device())
    {
        return fail(g_device_result);
    }
    reap_voices();
    PcmVoice slot;
    WAVEFORMATEX fmt{};
    u32 loop_begin = 0;
    u32 loop_length = 0;
    if (!load_wave(choice, slot.pcm, fmt, loop_begin, loop_length))
    {
        log::warn("audio skip %s %s[%d]", cue.c_str(), choice.bank.c_str(), choice.index);
        const DWORD error = GetLastError();
        return fail(HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_INVALID_DATA : error));
    }
#if defined(GBTG_INSPECTOR_APP)
    slot.duration = fmt.nAvgBytesPerSec != 0 ? float(double(slot.pcm.size()) / fmt.nAvgBytesPerSec) : 0.f;
#endif
    HRESULT status = g_xa->CreateSourceVoice(&slot.voice, &fmt);
    if (FAILED(status))
    {
        log::warn("audio voice failed %s", cue.c_str());
        return fail(status);
    }
    route_to_master(slot.voice);
    slot.category = choice.category;
    status = slot.voice->SetVolume(g_category_volumes[slot.category]);
    if (FAILED(status))
    {
        slot.voice->DestroyVoice();
        return fail(status);
    }
    XAUDIO2_BUFFER buf{};
    buf.AudioBytes = UINT32(slot.pcm.size());
    buf.pAudioData = slot.pcm.data();
    buf.Flags = XAUDIO2_END_OF_STREAM;
    // The play event's loop count; 0xFFFF is XACT's infinite loop.
    if (choice.loop_count != 0)
    {
        loop = true;
    }
    buf.LoopCount = !loop ? 0
        : choice.loop_count == 0 || choice.loop_count == 0xFFFF ? XAUDIO2_LOOP_INFINITE
        : std::min<UINT32>(choice.loop_count, XAUDIO2_MAX_LOOP_COUNT);
    if (loop)
    {
        // Play from frame zero once, then repeat only the authored loop region.
        buf.LoopBegin = loop_begin;
        buf.LoopLength = loop_length;
    }
    status = slot.voice->SubmitSourceBuffer(&buf);
    if (FAILED(status))
    {
        slot.voice->DestroyVoice();
        return fail(status);
    }
    // The virtual clock's length: one pass, then each loop of the loop region (to the end when its length is 0).
    const double seconds = fmt.nAvgBytesPerSec != 0 ? double(slot.pcm.size()) / fmt.nAvgBytesPerSec : 0.0;
    if (buf.LoopCount == XAUDIO2_LOOP_INFINITE)
    {
        slot.virtual_length = std::numeric_limits<double>::infinity();
    }
    else
    {
        const double region = buf.LoopLength != 0 && fmt.nSamplesPerSec != 0
            ? double(buf.LoopLength) / fmt.nSamplesPerSec
            : seconds - (fmt.nSamplesPerSec != 0 ? double(buf.LoopBegin) / fmt.nSamplesPerSec : 0.0);
        slot.virtual_length = seconds + double(buf.LoopCount) * region;
    }
    if (start)
    {
        status = slot.voice->Start(0);
        slot.virtual_started = true;
    }
    if (FAILED(status))
    {
        slot.voice->DestroyVoice();
        return fail(status);
    }
    if (result != nullptr)
    {
        *result = static_cast<i32>(status);
    }
    if (prepared_voice != nullptr)
    {
        *prepared_voice = slot.voice;
    }
    const u64 handle = ++g_next_voice_handle;
    slot.handle = handle;
    slot.cue = cue;
    slot.native = prepared_voice != nullptr;
    log::verbose("audio cue %s %s [%u]", cue.c_str(), choice.bank.c_str(), choice.index);
    // Cutscene sync stops g_voices whenever no clip is playing. A cue the
    // frontend acquired has to live on the bed or that stop deletes it.
    if (bed || loop)
    {
        g_bed.push_back(std::move(slot));
    }
    else
    {
        g_voices.push_back(std::move(slot));
    }
    return handle;
}

// The wave a cue plays: XACT picks the cue's sound from the cue's variation table (0x001C2DFC), then that sound's
// wave from its own table (0x001C2A3B each time). Without a table the first listed wave plays.
const WaveChoice* pick_choice(const std::vector<WaveChoice>& waves)
{
    if (waves.empty())
    {
        return nullptr;
    }
    size_t first = 0;
    size_t end = waves.size();
    if (const i32 cue_table = waves[0].cue_table; cue_table >= 0)
    {
        const u32 slot = choose_variation(g_variation_tables[size_t(cue_table)]);
        const auto chosen = std::find_if(waves.begin(), waves.end(), [slot](const WaveChoice& wave)
        {
            return wave.cue_slot == slot;
        });
        if (chosen == waves.end())
        {
            return nullptr;
        }
        first = size_t(chosen - waves.begin());
        end = first;
        while (end < waves.size() && waves[end].cue_slot == slot)
        {
            ++end;
        }
    }
    if (const i32 sound_table = waves[first].sound_table; sound_table >= 0)
    {
        const u32 slot = choose_variation(g_variation_tables[size_t(sound_table)]);
        for (size_t index = first; index < end; ++index)
        {
            if (waves[index].sound_table == sound_table && waves[index].sound_slot == slot)
            {
                return &waves[index];
            }
        }
        return nullptr;
    }
    return &waves[first];
}

#endif

} // namespace

// Native audio boundary at the retail cue play calls. A failed host voice returns 0.
// The book owns its handles independently of the scene cutscene's voice list.
i32 pcm_prepare_owned_cue(const Game& g, const char* cue, u64& handle, void*& voice)
{
    handle = 0;
    voice = nullptr;
#if GBTG_HAS_XAUDIO2
    build_catalog(g);
    const auto found = g_cues.find(cue);
    if (found == g_cues.end())
    {
        return static_cast<i32>(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }
    const WaveChoice* choice = pick_choice(found->second);
    if (choice == nullptr)
    {
        return static_cast<i32>(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
    }
    i32 result;
    handle = play_choice(cue, *choice, false, true, false, &result, &voice);
    if (voice != nullptr)
    {
        // Set or cleared each time: a released voice's pointer can come back for another cue.
        auto* source_voice = static_cast<IXAudio2SourceVoice*>(voice);
        if (choice->sound_3d)
        {
            g_cue_3d[source_voice] = *choice->sound_3d;
        }
        else
        {
            g_cue_3d.erase(source_voice);
        }
    }
    return result;
#else
    (void)g;
    (void)cue;
    return static_cast<i32>(0x80004001u);
#endif
}

// Host voice-state query for the retail active-voice tests at 0x0012A510.
bool pcm_owned_cue_playing(u64 handle)
{
    if (handle == 0)
    {
        return false;
    }
#if GBTG_HAS_XAUDIO2
    for (const PcmVoice& voice : g_bed)
    {
        if (voice.handle == handle)
        {
            return pcm_voice_queued(voice.voice);
        }
    }
#else
    (void)handle;
#endif
    return false;
}

// Native audio boundary at 0x00039690. Stop the voice acquired by this event.
void pcm_stop_owned_cue(u64 handle)
{
#if GBTG_HAS_XAUDIO2
    for (size_t i = 0; i < g_bed.size(); ++i)
    {
        if (g_bed[i].handle == handle)
        {
            log::verbose("audio cue stop %s", g_bed[i].cue.c_str());
            release_voice(g_bed[i]);
            g_bed.erase(g_bed.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
#else
    (void)handle;
#endif
}

void pcm_play_pcm(const char* cue, const std::vector<u8>& pcm, int rate, int channels, bool loop, float volume)
{
#if GBTG_HAS_XAUDIO2
    if (cue == nullptr || pcm.empty() || rate < 8000 || channels < 1 || channels > 2)
    {
        return;
    }
    pcm_stop_cue(cue);
    if (!ensure_device())
    {
        return;
    }
    PcmVoice slot;
    slot.pcm = pcm;
    slot.cue = cue;
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = WORD(channels);
    fmt.nSamplesPerSec = DWORD(rate);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = WORD(channels * 2);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    if (FAILED(g_xa->CreateSourceVoice(&slot.voice, &fmt)))
    {
        return;
    }
    route_to_master(slot.voice);
    slot.voice->SetVolume(volume);
    XAUDIO2_BUFFER buf{};
    buf.AudioBytes = UINT32(slot.pcm.size());
    buf.pAudioData = slot.pcm.data();
    buf.Flags = XAUDIO2_END_OF_STREAM;
    buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    if (FAILED(slot.voice->SubmitSourceBuffer(&buf)))
    {
        slot.voice->DestroyVoice();
        return;
    }
    slot.voice->Start(0);
    slot.virtual_length = loop ? std::numeric_limits<double>::infinity() : double(slot.pcm.size()) / fmt.nAvgBytesPerSec;
    slot.virtual_started = true;
    g_bed.push_back(std::move(slot));
#else
    (void)cue;
    (void)pcm;
    (void)rate;
    (void)channels;
    (void)loop;
    (void)volume;
#endif
}

void pcm_set_master_volume(float volume)
{
#if GBTG_HAS_XAUDIO2
    if (volume < 0.f)
    {
        volume = 0.f;
    }
    else if (volume > 1.f)
    {
        volume = 1.f;
    }
    g_master_volume = volume;
    if (g_master)
    {
        g_master->SetVolume(g_master_volume);
    }
#else
    (void)volume;
#endif
}

void pcm_use_virtual_audio_clock()
{
#if GBTG_HAS_XAUDIO2
    g_virtual_audio_clock = true;
#endif
}

AudioListenerSettings& native_audio_listener()
{
    static AudioListenerSettings listener = native_audio_listener_settings();
    return listener;
}

AudioListenerSettings native_audio_listener_settings()
{
    AudioListenerSettings settings;
    settings.master_volume = [](u32 category, i32 attenuation) -> i32
    {
#if GBTG_HAS_XAUDIO2
        if (!ensure_device())
        {
            return static_cast<i32>(g_device_result);
        }
        if (category > 0xFF)
        {
            return static_cast<i32>(E_INVALIDARG);
        }
        const float volume = std::pow(10.f, float(attenuation) / 2000.f);
        if (category == 0xFF)
        {
            g_category_volumes.fill(volume);
        }
        else
        {
            g_category_volumes[category] = volume;
        }
        HRESULT result = S_OK;
        for (auto* voices : {&g_voices, &g_bed})
        {
            for (auto& voice : *voices)
            {
                if (category == 0xFF || voice.category == category)
                {
                    const HRESULT status = voice.voice->SetVolume(volume);
                    if (FAILED(status) && SUCCEEDED(result))
                    {
                        result = status;
                    }
                }
            }
        }
        return static_cast<i32>(result);
#else
        (void)category;
        (void)attenuation;
        return static_cast<i32>(0x80004001u);
#endif
    };
    settings.listener = [](const std::array<u32, 12>& words, u32 apply) -> i32
    {
#if GBTG_HAS_XAUDIO2
        // DirectSound keeps the listener's HF reference for each source's I3DL2 filters (0x001C8705 stores it).
        g_listener_hf_reference = std::bit_cast<float>(words[11]);
        apply_all_spatial_sources();
        if (!ensure_device())
        {
            return static_cast<i32>(g_device_result);
        }
        if (!g_listener_effect)
        {
            return static_cast<i32>(E_FAIL);
        }
        XAUDIO2FX_REVERB_I3DL2_PARAMETERS parameters{
            100.f,
            std::bit_cast<i32>(words[0]), std::bit_cast<i32>(words[1]),
            std::bit_cast<float>(words[2]), std::bit_cast<float>(words[3]),
            std::bit_cast<float>(words[4]), std::bit_cast<i32>(words[5]),
            std::bit_cast<float>(words[6]), std::bit_cast<i32>(words[7]),
            std::bit_cast<float>(words[8]), std::bit_cast<float>(words[9]),
            std::bit_cast<float>(words[10]), std::bit_cast<float>(words[11])};
        XAUDIO2FX_REVERB_PARAMETERS native;
        ReverbConvertI3DL2ToNative(&parameters, &native);
        (void)apply;
        const HRESULT status = g_reverb->SetEffectParameters(0, &native, sizeof(native));
        if (SUCCEEDED(status))
        {
            g_reverb->SetVolume(1.f);
        }
        return static_cast<i32>(status);
#else
        (void)words;
        (void)apply;
        return static_cast<i32>(0x80004001u);
#endif
    };
    return settings;
}

void bind_native_audio_listener(AudioCoreListener& listener)
{
    listener.set_position = [](const Vec3& position, u32)
    {
#if GBTG_HAS_XAUDIO2
        g_x3d_listener.Position = {position.x, position.y, position.z};
        apply_all_spatial_sources();
#else
        (void)position;
#endif
    };
    listener.set_velocity = [](const Vec3& velocity, u32)
    {
#if GBTG_HAS_XAUDIO2
        g_x3d_listener.Velocity = {velocity.x, velocity.y, velocity.z};
#else
        (void)velocity;
#endif
    };
    listener.set_orientation = [](const Vec3& front, const Vec3& top, u32)
    {
#if GBTG_HAS_XAUDIO2
        // X3DAudio needs an orthonormal pair; XACT took the game's vectors as they are.
        const X3DAUDIO_VECTOR unit_front = unit_vector(front);
        const float along = top.x * unit_front.x + top.y * unit_front.y + top.z * unit_front.z;
        const X3DAUDIO_VECTOR unit_top = unit_vector({top.x - unit_front.x * along, top.y - unit_front.y * along,
            top.z - unit_front.z * along});
        if (unit_front.x == 0.f && unit_front.y == 0.f && unit_front.z == 0.f)
        {
            return;
        }
        if (unit_top.x == 0.f && unit_top.y == 0.f && unit_top.z == 0.f)
        {
            return;
        }
        g_x3d_listener.OrientFront = unit_front;
        g_x3d_listener.OrientTop = unit_top;
        apply_all_spatial_sources();
#else
        (void)front;
        (void)top;
#endif
    };
}

RuntimeSoundBoundaries native_sound_boundaries()
{
    RuntimeSoundBoundaries boundaries;
    boundaries.pause_cue = [](void*, void* cue, bool paused) -> i32
    {
#if GBTG_HAS_XAUDIO2
        auto* voice = static_cast<IXAudio2SourceVoice*>(cue);
        start_virtual_voice(voice, !paused);
        return static_cast<i32>(paused ? voice->Stop(0) : voice->Start(0));
#else
        return static_cast<i32>(0x80004001u);
#endif
    };
    boundaries.stop_cue = [](void*, u32, u32 flags, void* cue)
    {
#if GBTG_HAS_XAUDIO2
        auto* voice = static_cast<IXAudio2SourceVoice*>(cue);
        if (const auto notes = g_cue_notes.find(voice); notes != g_cue_notes.end())
        {
            notes->second.stopped = true;
        }
        voice->Stop((flags & 2) != 0 ? 0 : XAUDIO2_PLAY_TAILS);
        // XACT ends the cue either way (immediately, or after its release); the source buffers have no tail to
        // play out, so the remainder is dropped and the voice drains for the in-use status and reaping.
        voice->FlushSourceBuffers();
        end_virtual_voice(voice);
#endif
    };
    boundaries.last_error = []() -> u32
    {
#if GBTG_HAS_XAUDIO2
        return GetLastError();
#else
        return static_cast<u32>(errno);
#endif
    };
    return boundaries;
}

AudioCoreCueBoundaries native_audio_core_cue_boundaries(const Game& game,
    const std::function<const char*(void*, u32)>& cue_name)
{
    AudioCoreCueBoundaries boundaries;
    boundaries.prepare = [&game, cue_name](void* bank, u32 index, void*& cue) -> i32
    {
        u64 handle;
        return pcm_prepare_owned_cue(game, cue_name(bank, index), handle, cue);
    };
    boundaries.play = [](void*, void* source, void*& cue) -> i32
    {
#if GBTG_HAS_XAUDIO2
        auto* voice = static_cast<IXAudio2SourceVoice*>(cue);
        if (source != nullptr)
        {
            XAUDIO2_SEND_DESCRIPTOR send{0, static_cast<IXAudio2Voice*>(source)};
            XAUDIO2_VOICE_SENDS sends{1, &send};
            const HRESULT status = voice->SetOutputVoices(&sends);
            if (FAILED(status))
            {
                return static_cast<i32>(status);
            }
            g_source_routes[voice] = static_cast<IXAudio2Voice*>(source);
            if (const auto spatial = g_spatial_sources.find(static_cast<IXAudio2Voice*>(source));
                spatial != g_spatial_sources.end())
            {
                apply_routed_cue_levels(spatial->first, spatial->second.position);
            }
            else
            {
                apply_default_mix_bins(voice, static_cast<IXAudio2Voice*>(source));
            }
        }
        g_cue_notes[voice].started = true;
        start_virtual_voice(voice, true);
        return static_cast<i32>(voice->Start(0));
#else
        (void)source;
        (void)cue;
        return static_cast<i32>(0x80004001u);
#endif
    };
    return boundaries;
}

void dispatch_native_audio_notifications(std::span<AudioCoreBank* const> banks)
{
#if GBTG_HAS_XAUDIO2
    for (AudioCoreBank* bank : banks)
    {
        if (bank == nullptr)
        {
            continue;
        }
        AudioCoreLink* link = bank->cues.next;
        while (link != nullptr)
        {
            AudioCoreLink* next = link->next;
            auto* cue = static_cast<AudioCoreCue*>(link);
            auto* voice = static_cast<IXAudio2SourceVoice*>(cue->runtime.handle);
            if (voice != nullptr && cue->callback != nullptr)
            {
                auto& notes = g_cue_notes[voice];
                if (notes.stopped)
                {
                    g_cue_notes.erase(voice);
                    cue->callback(cue->callback_context, *cue, 1);
                    release_native_cue_voice(voice);
                }
                else if (!notes.prepared)
                {
                    notes.prepared = true;
                    cue->callback(cue->callback_context, *cue, 3);
                }
                else if (notes.started && !pcm_voice_queued(voice))
                {
                    g_cue_notes.erase(voice);
                    cue->callback(cue->callback_context, *cue, 1);
                    release_native_cue_voice(voice);
                }
                else if (notes.started && !notes.playing)
                {
                    notes.playing = true;
                    cue->callback(cue->callback_context, *cue, 0);
                }
            }
            link = next;
        }
    }
#else
    (void)banks;
#endif
}

#if defined(GBTG_INSPECTOR_APP)
void suspend_pcm_audio()
{
#if GBTG_HAS_XAUDIO2
    if (g_xa != nullptr)
    {
        g_xa->StopEngine();
    }
#endif
}

namespace
{
float inspection_output_volume = 1.f;
}

void pcm_set_inspection_output_volume(float volume)
{
    inspection_output_volume = std::clamp(volume, 0.f, 1.f);
}

void begin_pcm_inspection_preview(void* cue, float volume)
{
#if GBTG_HAS_XAUDIO2
    if (g_xa == nullptr || cue == nullptr)
    {
        throw Error("sound preview has no prepared audio device/voice");
    }
    // Resume the mixer only after every scene voice is stopped. Gameplay's
    // native cue state is left alone; only the selected tooling cue is audible.
    if (g_reverb != nullptr)
    {
        g_reverb->SetVolume(0.f);
    }
    for (auto* voices : {&g_voices, &g_bed})
    {
        for (auto& voice : *voices)
        {
            if (voice.voice != cue)
            {
                voice.voice->Stop(0);
            }
        }
    }
    volume_pcm_inspection_preview(cue, volume);
    // The inspector deliberately keeps the game's requested master gain at zero.
    // Scene voices are stopped above, so only the tooling preview can reach this
    // temporary output gain. Its volume control remains on the cue voice.
    if (FAILED(g_master->SetVolume(inspection_output_volume)))
    {
        throw Error("could not enable the sound preview output");
    }
    if (FAILED(g_xa->StartEngine()))
    {
        throw Error("could not start the sound preview mixer");
    }
#else
    throw Error("sound preview requires the Windows audio backend");
#endif
}

void volume_pcm_inspection_preview(void* cue, float volume)
{
#if GBTG_HAS_XAUDIO2
    if (cue != nullptr)
    {
        const HRESULT result = static_cast<IXAudio2SourceVoice*>(cue)->SetVolume(std::clamp(volume, 0.f, 1.f));
        if (FAILED(result))
        {
            throw Error("could not set sound preview volume");
        }
    }
#endif
}

bool pcm_inspection_preview_pending(void* cue, float& seconds)
{
#if GBTG_HAS_XAUDIO2
    XAUDIO2_VOICE_STATE state{};
    auto* voice = static_cast<IXAudio2SourceVoice*>(cue);
    voice->GetState(&state);
    XAUDIO2_VOICE_DETAILS details{};
    voice->GetVoiceDetails(&details);
    seconds = details.InputSampleRate != 0 ? float(double(state.SamplesPlayed) / details.InputSampleRate) : 0.f;
    if (state.BuffersQueued == 0)
    {
        for (const auto& slot : g_bed)
        {
            if (slot.voice == voice)
            {
                seconds = (std::max)(seconds, slot.duration);
                break;
            }
        }
    }
    return state.BuffersQueued != 0;
#else
    return false;
#endif
}

void release_pcm_inspection_preview(void* cue)
{
#if GBTG_HAS_XAUDIO2
    g_cue_notes.erase(cue);
    release_native_cue_voice(static_cast<IXAudio2SourceVoice*>(cue));
    if (g_master != nullptr)
    {
        g_master->SetVolume(g_master_volume);
    }
#endif
}

float pcm_inspection_output_volume()
{
#if GBTG_HAS_XAUDIO2
    float volume = 0.f;
    if (g_master != nullptr)
    {
        g_master->GetVolume(&volume);
    }
    return volume;
#else
    return 0.f;
#endif
}

void shutdown_pcm_audio()
{
#if GBTG_HAS_XAUDIO2
    suspend_pcm_audio();
    for (auto* voices : {&g_voices, &g_bed})
    {
        for (auto& voice : *voices)
        {
            if (voice.voice != nullptr)
            {
                voice.voice->DestroyVoice();
            }
        }
        voices->clear();
    }
    g_cue_notes.clear();
    g_source_routes.clear();
    for (const auto& [voice, source] : g_spatial_sources)
    {
        voice->DestroyVoice();
    }
    g_spatial_sources.clear();
    if (g_reverb != nullptr)
    {
        g_reverb->DestroyVoice();
        g_reverb = nullptr;
    }
    if (g_master != nullptr)
    {
        g_master->DestroyVoice();
        g_master = nullptr;
    }
    if (g_xa != nullptr)
    {
        g_xa->Release();
        g_xa = nullptr;
    }
    g_x3d_ready = false;
    g_catalog_ready = false;
    g_wavefiles.clear();
    g_cues.clear();
    g_variation_tables.clear();
    g_variation_table_index.clear();
#endif
}

void release_native_audio_boot()
{
    if (g_native_audio == nullptr)
    {
        return;
    }
    auto& banks = *g_native_audio;
    auto* link = banks.loaded.next;
    while (link != nullptr)
    {
        auto* next = link->next;
        auto* bank = static_cast<AudioCoreBank*>(link);
        // These are host allocations made by the XACT/heap boot adapter. All scene cue owners
        // must already have been destroyed; this is not a replacement for native bank behavior.
        banks.memory.release(bank->name);
        banks.memory.release(bank);
        link = next;
    }
    banks = NativeAudioBanks{};
    g_native_audio = nullptr;
    g_audio_bank_names = nullptr;
}

#endif
std::shared_ptr<NativeAudioRuntime> boot_native_audio(const Game& game)
{
    static NativeAudioBanks banks;
    banks.memory = native_audio_core_memory();
    NativeAudioBootBoundaries boundaries;
    boundaries.resource = [&game](const char* name) -> std::span<const u8>
    {
        const BundleEntry* entry = find_entry(game.aids.common, name);
        return entry == nullptr ? std::span<const u8>() : std::span<const u8>(entry->cpu);
    };
    boundaries.resource_error = [](u32 code)
    {
        throw Error("audio boot resource error " + std::to_string(code));
    };
    boundaries.create_engine = []()
    {
#if GBTG_HAS_XAUDIO2
        ensure_device();
#endif
    };
    boundaries.download_effects = [](std::span<const u8>)
    {
        // XAudio2 has no DSP image; source reverb uses the XAudio2 reverb effect.
    };
    boundaries.register_wavebanks = [&game]()
    {
#if GBTG_HAS_XAUDIO2
        build_catalog(game);
#else
        (void)game;
#endif
    };
    boundaries.create_sound_bank = [](std::span<const u8> image, void*& handle) -> i32
    {
        // Cues resolve by name through the playback boundary; the bank's bytes identify its handle.
        handle = const_cast<u8*>(image.data());
        return 0;
    };
    boundaries.register_notification = [](void*, u32)
    {
        // Cue completion is read from the XAudio2 voice state.
    };
    boundaries.load_file = [&game](const char* name) -> std::vector<u8>
    {
        // The Xbox drive letters D: and Z: (the disc and its cache) name the game folder.
        std::string path = name;
        if (path.size() >= 2 && path[1] == ':')
        {
            path = path.substr(2);
        }
        while (!path.empty() && (path.front() == '\\' || path.front() == '/'))
        {
            path.erase(path.begin());
        }
#if !defined(GBTG_RETAIL_GAME_ONLY)
        std::ifstream file(plugin_disc_path(game.aids.game_dir / path), std::ios::binary);
#else
        std::ifstream file(game.aids.game_dir / path, std::ios::binary);
#endif
        if (!file)
        {
            throw Error(std::string("0x0015BB30 cannot open the sound bank file ") + name);
        }
        return std::vector<u8>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    boundaries.reverb_preset = [&game](i32 preset)
    {
        set_audio_reverb_preset(native_audio_listener(), preset, game.native_resources->registry,
            game.native_resources->error); // 0x0003A1F0
    };
    boundaries.category_volume = [](u32 category, i32 volume)
    {
        set_audio_category_volume(native_audio_listener(), category, volume); // 0x0015C1C0
    };
    bind_native_audio_listener(banks.listener);
    initialize_native_audio(banks, boundaries);
    log::verbose("native audio banks %d names %zu cues %zu", banks.bank_count, banks.bank_names.size(),
              banks.cue_names.size());

    // The audio core over the XACT boundaries: heap, sound sources and cue preparation.
    auto owned = std::make_shared<NativeAudioRuntime>();
    auto& runtime = *owned;
    runtime.sources.memory = native_audio_core_memory();
    runtime.sources.boundaries = native_audio_core_source_boundaries();
    // The engine's DAT_004D97F0 / DAT_004D97EC, which the source calls scale and apply with.
    runtime.sources.unit_scale = banks.listener.unit_scale;
    runtime.sources.apply = banks.listener.apply;
    runtime.cues.boundaries = native_audio_core_cue_boundaries(game, [](void* bank, u32 index) -> const char*
    {
        static std::string name;
        name = native_audio_cue_name(*g_native_audio, bank, index);
        return name.c_str();
    });
    runtime.cues.sound = native_sound_boundaries();
    // DAT_004E13B0, the sound banks 0x00039F50 created.
    runtime.voices.banks = std::span<AudioCoreBank* const>(banks.banks.data(), size_t(banks.bank_count));
    if (game.executable_resources == nullptr)
    {
        throw Error("native audio boot needs the executable image for DAT_00421A38 and DAT_0042A840");
    }
    const auto& image = *game.executable_resources;
    // DAT_00421A38: nine {millibel, volume} reverb pairs that 0x00039620 reads for each 3D voice.
    runtime.voices.reverb_curves = std::span<const std::array<u32, 2>>(
        reinterpret_cast<const std::array<u32, 2>*>(image.memory(0x00421A38, 9 * 8).data()), 9);
    std::memcpy(&runtime.default_cone, image.memory(0x0042A840, 0xC).data(), 0xC);
    // XACT's random source (0x001BAC09): the vector at 0x00338750, seeded once with GetTickCount(). Test runs keep a
    // fixed seed so their variations repeat.
    std::memcpy(g_xact_random_vector.data(), image.memory(0x00338750, sizeof(g_xact_random_vector)).data(),
        sizeof(g_xact_random_vector));
    if (g_xact_random_seed == 0)
    {
#if GBTG_HAS_XAUDIO2
        const bool fixed_seed = g_virtual_audio_clock;
#else
        const bool fixed_seed = false;
#endif
        g_xact_random_seed = fixed_seed ? 1u
            : u32(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    std::memcpy(&runtime.listener_velocity, image.memory(0x00421C70, 0xC).data(), 0xC);
    runtime.voices.listener = runtime.listener_words.data();
    for (size_t row = 0; row < runtime.wavebank_streams.ids.size(); ++row)
    {
        std::memcpy(&runtime.wavebank_streams.ids[row], image.memory(0x00421A8C + u32(row) * 0x14, 4).data(), 4);
    }
    // 0x00039DD0's file side over the rows above: the in-memory banks are copied to the utility drive (Z:) unless
    // their cached copies are current, the frame loop pumping the copies (0x000E9280). The file system is up first,
    // as fs_init 0x00033980 runs 0x000E9CE0 before 0x00033030.
    if (g_native_movie_runtime == nullptr)
    {
        throw Error("native audio boot (0x00039DD0) needs the file system (0x000E9CE0) booted first");
    }
    auto& copies = g_native_movie_runtime->session->copies();
    static std::unique_ptr<WavebankFileContext> wavebank_files;
    wavebank_files = std::make_unique<WavebankFileContext>(WavebankFileContext{image.memory(0x00421A80, 12 * 0x14),
        [&image](u32 address)
        {
            return image.string(address);
        },
        copies.state.root,
        [&copies](const char* path)
        {
            return copies.files.worker->io.attributes(path); // GetFileAttributesA, Z: mapped
        },
        [&copies]()
        {
            return copies.files.worker->io.last_error();
        },
        [](const char*, i32, u32)
        {
            // IXACTEngine_CreateInMemoryWaveBank / CreateStreamingWaveBank: the host catalog
            // (register_wavebanks) already plays every bank from the disc.
        },
        [](u32 code)
        {
            throw Error("audio wavebank file error " + std::to_string(code)); // 0x00032700
        },
        copies});
    register_runtime_wavebank_files(runtime.wavebank_streams, *wavebank_files); // 0x00039DD0
    initialize_tracked_audio_channels(runtime.streams); // Boot 0x00033030 runs 0x00038BF0 over DAT_004E030C.
    for (auto& bank : runtime.streams.banks)
    {
        bank.id = 0; // Boot 0x00033030: every tracked bank's id (DAT_004DF370, stride 0x9C) is cleared.
    }
    initialize_audio_stream_slots(runtime.streams, g_stream_slot_gate); // Boot 0x00033030 -> 0x00038600
    g_native_audio_runtime = &runtime;
    return owned;
}

void tick_native_audio(float dt, i32 stream_gate)
{
    if (g_native_audio_runtime == nullptr || g_native_audio == nullptr)
    {
        return;
    }
    auto& runtime = *g_native_audio_runtime;
#if GBTG_HAS_XAUDIO2
    if (g_virtual_audio_clock)
    {
        for (auto* voices : {&g_voices, &g_bed})
        {
            for (PcmVoice& slot : *voices)
            {
                if (slot.virtual_started)
                {
                    slot.virtual_position += dt;
                }
            }
        }
    }
#endif
    dispatch_native_audio_notifications(runtime.voices.banks); // 0x0015B780
    const auto& view = *entity_detail::g_display_view; // DAT_005077A8
    runtime.listener_words = {view.position.x, view.position.y, view.position.z, view.rotation.x, view.rotation.y,
        view.rotation.z, view.rotation.w, view.forward.x, view.forward.y, view.forward.z};
    update_audio_listener_from_view(view.position, view.rotation, view.forward, runtime.listener_velocity,
        g_native_audio->listener); // 0x0003A040
    tick_audio_event_voices(runtime.voices); // 0x000396F0
    tick_audio_stream_slots(dt, stream_gate, runtime.streams); // 0x00038640
    release_idle_tracked_audio_channels(runtime.streams); // 0x00038C20
}

AudioCoreMemory native_audio_core_memory()
{
    AudioCoreMemory memory;
    memory.allocate = [](size_t size) -> void*
    {
        void* allocation = std::malloc(size);
        if (allocation == nullptr)
        {
            throw Error("audio core heap is exhausted");
        }
        return allocation;
    };
    memory.release = [](void* allocation)
    {
        std::free(allocation);
    };
    return memory;
}

AudioCoreSourceBoundaries native_audio_core_source_boundaries()
{
    AudioCoreSourceBoundaries boundaries;
    // XACT sound sources are submix voices; the cue voices PlayEx routes into them mix through them.
    boundaries.create = [](u32 flags, void*& handle) -> i32
    {
        handle = nullptr;
#if GBTG_HAS_XAUDIO2
        if (!ensure_device())
        {
            return static_cast<i32>(g_device_result);
        }
        XAUDIO2_VOICE_DETAILS details;
        g_master->GetVoiceDetails(&details);
        IXAudio2SubmixVoice* voice = nullptr;
        // Flag 2 is a 3D (mono) source and flag 1 a 2D source.
        const HRESULT status = g_xa->CreateSubmixVoice(&voice, flags == 2 ? 1 : 2, details.InputSampleRate);
        if (SUCCEEDED(status))
        {
            handle = voice;
            if (flags != 2)
            {
                route_to_master(voice);
            }
            else
            {
                // A 3D source's sends carry its I3DL2 filters (apply_spatial_source).
                SpatialSource& source = g_spatial_sources[voice];
                source = SpatialSource{};
                if (g_reverb == nullptr || !g_listener_effect)
                {
                    XAUDIO2_SEND_DESCRIPTOR send{XAUDIO2_SEND_USEFILTER, g_master};
                    const XAUDIO2_VOICE_SENDS list{1, &send};
                    voice->SetOutputVoices(&list);
                }
                else
                {
                    XAUDIO2_SEND_DESCRIPTOR sends[2]{{XAUDIO2_SEND_USEFILTER, g_master},
                        {XAUDIO2_SEND_USEFILTER, g_reverb}};
                    const XAUDIO2_VOICE_SENDS list{2, sends};
                    voice->SetOutputVoices(&list);
                    if (g_master_channels <= source.reverb_send.size())
                    {
                        voice->GetOutputMatrix(g_reverb, 1, g_master_channels, source.reverb_send.data());
                    }
                }
                apply_spatial_source(voice, source);
            }
        }
        return static_cast<i32>(status);
#else
        (void)flags;
        return static_cast<i32>(0x80004001u);
#endif
    };
    // The PCM backend has no mix bins or 3D positioning; it keeps the submix at its default routing.
    boundaries.mix_bins = [](void*, std::span<const AudioCoreMixBin>)
    {
    };
    boundaries.mix_volumes = [](void*, std::span<const AudioCoreMixBin>)
    {
    };
    boundaries.position = [](void* handle, const Vec3& position, u32)
    {
#if GBTG_HAS_XAUDIO2
        auto* voice = static_cast<IXAudio2Voice*>(handle);
        const auto found = g_spatial_sources.find(voice);
        if (found != g_spatial_sources.end())
        {
            found->second.position = {position.x, position.y, position.z};
            apply_spatial_source(voice, found->second);
        }
#else
        (void)handle;
        (void)position;
#endif
    };
    boundaries.velocity = [](void*, const Vec3&, u32)
    {
    };
    boundaries.cone = [](void*, const Vec3&, u32)
    {
    };
    // IXACTSoundSource_SetI3DL2Source -> 0x001C4A63 stores the nine words at the source's +0xC4 and marks them dirty;
    // DirectSound's 3D calculation turns them into the sends' levels and filters (apply_spatial_source).
    boundaries.reverb = [](void* handle, std::span<const u32, 9> values, u32)
    {
#if GBTG_HAS_XAUDIO2
        IXAudio2Voice* voice = static_cast<IXAudio2Voice*>(handle);
        const auto found = g_spatial_sources.find(voice);
        if (found == g_spatial_sources.end())
        {
            return;
        }
        SourceI3DL2& i3dl2 = found->second.i3dl2;
        i3dl2.direct = std::bit_cast<i32>(values[0]);
        i3dl2.direct_hf = std::bit_cast<i32>(values[1]);
        i3dl2.room = std::bit_cast<i32>(values[2]);
        i3dl2.room_hf = std::bit_cast<i32>(values[3]);
        i3dl2.room_rolloff = std::bit_cast<float>(values[4]);
        i3dl2.obstruction_hf = std::bit_cast<i32>(values[5]);
        i3dl2.obstruction_lf_ratio = std::bit_cast<float>(values[6]);
        i3dl2.occlusion_hf = std::bit_cast<i32>(values[7]);
        i3dl2.occlusion_lf_ratio = std::bit_cast<float>(values[8]);
        apply_spatial_source(voice, found->second);
#else
        (void)handle;
        (void)values;
#endif
    };
    boundaries.bad_pointer = [](const void* pointer)
    {
        return pointer == nullptr;
    };
    boundaries.status = [](void* handle, u32& status)
    {
        // XACT reports 1 (in use) while a routed cue is still playing.
        status = 0;
#if GBTG_HAS_XAUDIO2
        for (const PcmVoice& slot : g_bed)
        {
            const auto route = g_source_routes.find(slot.voice);
            if (route == g_source_routes.end() || route->second != handle)
            {
                continue;
            }
            if (pcm_voice_queued(slot.voice))
            {
                status = 1;
            }
        }
#else
        (void)handle;
#endif
    };
    boundaries.release = [](void* handle)
    {
#if GBTG_HAS_XAUDIO2
        for (auto route = g_source_routes.begin(); route != g_source_routes.end();)
        {
            if (route->second != handle)
            {
                ++route;
                continue;
            }
            const bool live = std::any_of(g_bed.begin(), g_bed.end(), [&route](const PcmVoice& slot)
            {
                return slot.voice == route->first;
            });
            if (live)
            {
                route_to_master(route->first);
            }
            route = g_source_routes.erase(route);
        }
        g_spatial_sources.erase(static_cast<IXAudio2Voice*>(handle));
        static_cast<IXAudio2SubmixVoice*>(handle)->DestroyVoice();
#else
        (void)handle;
#endif
    };
    boundaries.debug_string = [](const char* text)
    {
#if GBTG_HAS_XAUDIO2
        OutputDebugStringA(text);
#else
        (void)text;
#endif
    };
    return boundaries;
}

void pcm_stop_cue(const char* cue)
{
#if GBTG_HAS_XAUDIO2
    if (cue == nullptr)
    {
        return;
    }
    for (size_t i = 0; i < g_bed.size();)
    {
        if (g_bed[i].cue != cue || g_bed[i].native)
        {
            ++i;
            continue;
        }
        g_bed[i].voice->Stop(0);
        g_source_routes.erase(g_bed[i].voice);
        g_bed[i].voice->DestroyVoice();
        g_bed.erase(g_bed.begin() + static_cast<std::ptrdiff_t>(i));
    }
#else
    (void)cue;
#endif
}

} // namespace gbtg
