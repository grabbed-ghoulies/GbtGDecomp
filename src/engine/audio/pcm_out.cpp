#include "gbtg/engine_internal.h"

#include "gbtg/bundle.h"
#include "gbtg/log.h"
#include "pcm_backend.h"
#include "../camera/display_view.h"
#include "../scene/executable_resources.h"
#include "../video/native_movies.h"
#include "../video/movie_session.h"
#include "wavebank_files.h"

#include <algorithm>
#include <cerrno>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>

#include "xaudio2_host.h"

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

struct WaveChoice
{
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
struct SpatialSource
{
    X3DAUDIO_VECTOR position{};
};
std::unordered_map<IXAudio2Voice*, SpatialSource> g_spatial_sources;
// The Xbox sound banks carry each 3D sound's own distance curve; the host uses one inverse-distance curve that
// stays at full level within this many world units (about the play camera's distance from Cooper).
constexpr float kSpatialFullLevelDistance = 40.f;
// Owned cue voices routed into a sound-source submix by IXACTSoundBank_PlayEx.
std::map<IXAudio2SourceVoice*, IXAudio2Voice*> g_source_routes;
std::vector<PcmVoice> g_voices;
std::vector<PcmVoice> g_bed;
u64 g_next_voice_handle = 0;
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
    if (jump >= blob.size())
    {
        return;
    }
    const u8 count = blob[jump];
    if (count == 0 || count > 32)
    {
        return;
    }
    for (u8 i = 0; i < count; ++i)
    {
        push_wave(out, blob, jump + 4 + size_t(i) * 8, devs, loop_count);
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
    for (size_t index = first; index < out.size(); ++index)
    {
        // Retail XACT sound-entry byte +0xA, used by 0x001BB8C8.
        out[index].category = blob[record + 0xA];
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
        const u8 count = blob[jump];
        for (u8 i = 0; i < count && i < 32; ++i)
        {
            const size_t o = jump + 4 + size_t(i) * 8;
            if (o + 2 > blob.size())
            {
                break;
            }
            const i16 alt = rs16(blob, o);
            if (alt >= 0 && size_t(alt) < entry_count)
            {
                waves_from_entry(out, blob, alt, simple_count, devs);
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

// A voice's sends: the dry mix to the master voice and, once the listener reverb exists, the wet send.
HRESULT route_dry_and_reverb(IXAudio2Voice* voice)
{
    if (g_reverb == nullptr || !g_listener_effect)
    {
        return voice->SetOutputVoices(nullptr);
    }
    XAUDIO2_SEND_DESCRIPTOR sends[2]{{0, g_master}, {0, g_reverb}};
    const XAUDIO2_VOICE_SENDS list{2, sends};
    return voice->SetOutputVoices(&list);
}

// The master-channel matrix of one 3D source from its position and the listener.
void apply_spatial_source(IXAudio2Voice* voice, const SpatialSource& source)
{
    if (!g_x3d_ready || g_master_channels == 0 || g_master_channels > 8)
    {
        return;
    }
    X3DAUDIO_EMITTER emitter{};
    emitter.ChannelCount = 1;
    emitter.CurveDistanceScaler = kSpatialFullLevelDistance;
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
    voice->SetOutputMatrix(g_master, 1, g_master_channels, matrix.data());
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
        XAUDIO2_VOICE_STATE state{};
        g_voices[i].voice->GetState(&state);
        const bool done = state.BuffersQueued == 0;
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
    route_dry_and_reverb(slot.voice);
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
    if (start)
    {
        status = slot.voice->Start(0);
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

const WaveChoice* pick_choice(const std::vector<WaveChoice>& waves, u32 frame)
{
    if (waves.empty())
    {
        return nullptr;
    }
    const u16 needle = static_cast<u16>(frame % 10000);
    for (const auto& w : waves)
    {
        if (needle >= w.weight_lo && needle <= w.weight_hi)
        {
            return &w;
        }
    }
    return &waves[0];
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
    const WaveChoice* choice = pick_choice(found->second, g.frame);
    if (choice == nullptr)
    {
        return static_cast<i32>(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
    }
    i32 result;
    handle = play_choice(cue, *choice, false, true, false, &result, &voice);
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
            XAUDIO2_VOICE_STATE state{};
            voice.voice->GetState(&state);
            return state.BuffersQueued != 0;
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
    route_dry_and_reverb(slot.voice);
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
        }
        g_cue_notes[voice].started = true;
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
                XAUDIO2_VOICE_STATE state{};
                voice->GetState(&state);
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
                else if (notes.started && state.BuffersQueued == 0)
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
        std::ifstream file(game.aids.game_dir / path, std::ios::binary);
        if (!file)
        {
            throw Error(std::string("0x0015BB30 cannot open the sound bank file ") + name);
        }
        return std::vector<u8>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
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
            route_dry_and_reverb(voice);
            handle = voice;
            if (flags == 2)
            {
                g_spatial_sources[voice] = SpatialSource{};
                apply_spatial_source(voice, g_spatial_sources[voice]);
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
    boundaries.reverb = [](void*, std::span<const u32, 9>, u32)
    {
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
            XAUDIO2_VOICE_STATE state{};
            slot.voice->GetState(&state);
            if (state.BuffersQueued != 0)
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
                route_dry_and_reverb(route->first);
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
