#include "gbtg/engine_internal.h"

#include "gbtg/bundle.h"
#include "pcm_backend.h"

#include <algorithm>
#include <cerrno>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>
#include <xaudio2fx.h>
#endif

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
};

struct SoundEvent
{
    float time = 0.f;
    float stop = 0.f; // type 0x0C window end. 0 leaves the voice to finish.
    std::string cue;  // XACT_SOUNDBANK_...
    bool fired = false;
    bool stopped = false;
    bool stream = false; // 0x0004D4C0. Starting this cue stops the previous one.
    bool dialog = false; // Type 0x15. 0x0004C4F0 plays this when the dialog hold ends.
};

#if defined(_WIN32)

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
std::vector<PcmVoice> g_voices;
std::vector<PcmVoice> g_bed;
u64 g_next_voice_handle = 0;
bool g_device_failed = false;
HRESULT g_device_result = S_OK;
bool g_catalog_ready = false;
std::map<std::string, WaveFile> g_wavefiles;                 // WBND name -> file
std::map<std::string, std::vector<WaveChoice>> g_cues;      // full cue string -> variations
std::vector<SoundEvent> g_events;
size_t g_clip = static_cast<size_t>(-1);
std::string g_clip_name;
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

void push_wave(std::vector<WaveChoice>& out, std::span<const u8> blob, size_t o, const std::vector<std::string>& devs)
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
    out.push_back(c);
}

void push_variation_table(std::vector<WaveChoice>& out, std::span<const u8> blob, size_t jump,
                          const std::vector<std::string>& devs)
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
        push_wave(out, blob, jump + 4 + size_t(i) * 8, devs);
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
                        out.push_back(std::move(c));
                    }
                }
                else
                {
                    push_wave(out, blob, sound + 8, devs);
                }
            }
            else if (sub == 0x04 || sub == 0x44)
            {
                push_variation_table(out, blob, ru32(blob, sound + 8), devs);
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
        std::cout << "audio missing cue list\n";
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
    std::cout << "audio cues " << g_cues.size() << " wavebanks " << g_wavefiles.size() << "\n";
    (void)banks_by_length;
}

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
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co) && co != RPC_E_CHANGED_MODE)
    {
        g_device_result = co;
        g_device_failed = true;
        std::cout << "audio CoInitializeEx failed\n";
        return false;
    }
    g_device_result = XAudio2Create(&g_xa, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (SUCCEEDED(g_device_result))
    {
        g_device_result = g_xa->CreateMasteringVoice(&g_master);
    }
    if (FAILED(g_device_result))
    {
        g_device_failed = true;
        std::cout << "audio XAudio2Create failed\n";
        return false;
    }
    g_master->SetVolume(g_master_volume);
    return true;
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

bool load_wave(const WaveChoice& choice, std::vector<u8>& pcm, WAVEFORMATEX& fmt)
{
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
    if (play_len == 0 || play_len > 48 * 1024 * 1024)
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
    if (!load_wave(choice, slot.pcm, fmt))
    {
        std::cout << "audio skip " << cue << " " << choice.bank << "[" << choice.index << "]\n";
        const DWORD error = GetLastError();
        return fail(HRESULT_FROM_WIN32(error == ERROR_SUCCESS ? ERROR_INVALID_DATA : error));
    }
    HRESULT status = g_xa->CreateSourceVoice(&slot.voice, &fmt);
    if (FAILED(status))
    {
        std::cout << "audio voice failed " << cue << "\n";
        return fail(status);
    }
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
    buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
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
    std::cout << "audio cue " << cue << " " << choice.bank << "[" << choice.index << "]\n";
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

void collect_events(const Game& g, const Cutscene& clip)
{
    g_events.clear();
    if (!g.scene_bundle)
    {
        return;
    }
    std::string prefix = clip.name;
    const std::string from = "aid_cutscene_";
    if (prefix.rfind(from, 0) == 0)
    {
        prefix.replace(0, from.size(), "aid_cutsceneevents_");
    }
    for (const auto& e : g.scene_bundle->entries)
    {
        if (e.type != ResourceType::CutsceneEvents || e.name.rfind(prefix, 0) != 0)
        {
            continue;
        }
        size_t pos = 0;
        while (pos + 16 <= e.cpu.size())
        {
            const u32 size = ru32(e.cpu, pos);
            if (size < 16 || pos + size > e.cpu.size())
            {
                break;
            }
            const auto rec = e.cpu.subspan(pos, size);
            const u32 type = ru32(rec, 0x0c) & 0xffff;
            // Type 0x0C. 0x0004C3B0 plays the name at +0x14. 0x0004C4A0 stops that
            // voice when the window ends.
            if (type == 0x0c && rec.size() > 0x14)
            {
                const std::string cue = event_name(rec, 0x14);
                if (is_xact_cue(cue))
                {
                    SoundEvent ev;
                    ev.time = rf32(rec, 4);
                    ev.stop = rf32(rec, 8);
                    ev.cue = cue;
                    g_events.push_back(std::move(ev));
                }
                pos += size;
                continue;
            }
            // Type 0x15. 0x0004C4F0 plays the name at +0xD0 through 0x0004D4C0,
            // unless that name is XACT_SOUNDBANK_GMISC_NOSOUND.
            if (type == 0x15 && rec.size() > 0xd0)
            {
                const std::string cue = event_name(rec, 0xd0);
                if (is_xact_cue(cue) && cue != kNoSound)
                {
                    SoundEvent ev;
                    ev.time = rf32(rec, 4);
                    ev.cue = cue;
                    ev.stream = true;
                    // The callback 0x0004C4F0 runs when 0x0004E640 drops the hold,
                    // which is when the dialog mode returns to 0. Not at this time.
                    ev.dialog = true;
                    g_events.push_back(std::move(ev));
                }
                pos += size;
                continue;
            }
            const std::string cue = event_name(rec, 0x10);
            if (is_xact_cue(cue))
            {
                SoundEvent ev;
                ev.time = rf32(rec, 4);
                ev.cue = cue;
                // Type 0x0A. 0x0004D4C0 starts this name and remembers it so the
                // next streamed cue can stop it.
                ev.stream = type == 0x0a;
                g_events.push_back(std::move(ev));
            }
            pos += size;
        }
    }
    std::cout << "audio events " << g_events.size() << " " << clip.name << "\n";
}

#endif

} // namespace

#if defined(_WIN32)
bool g_dialog_held = false;

void hold_voice(PcmVoice& slot, bool hold)
{
    if (slot.voice == nullptr)
    {
        return;
    }
    if (hold)
    {
        if (slot.held)
        {
            return;
        }
        XAUDIO2_VOICE_STATE state{};
        slot.voice->GetState(&state);
        if (state.BuffersQueued == 0)
        {
            return;
        }
        slot.voice->Stop(0);
        slot.held = true;
        return;
    }
    if (!slot.held)
    {
        return;
    }
    slot.held = false;
    slot.voice->Start(0);
}

void fire_stream(const Game& g, SoundEvent& ev)
{
    ev.fired = true;
    if (ev.stream && !g_stream_cue.empty() && g_stream_cue != ev.cue)
    {
        stop_named(g_stream_cue, true);
    }
    const auto found = g_cues.find(ev.cue);
    if (found == g_cues.end())
    {
        std::cout << "audio unmapped " << ev.cue << "\n";
        if (ev.stream)
        {
            g_stream_cue = ev.cue;
        }
        return;
    }
    if (const WaveChoice* choice = pick_choice(found->second, g.frame))
    {
        play_choice(ev.cue, *choice);
    }
    if (ev.stream)
    {
        g_stream_cue = ev.cue;
    }
    (void)g;
}
#endif

void pcm_dialog_hold(bool hold)
{
#if defined(_WIN32)
    if (hold == g_dialog_held)
    {
        return;
    }
    g_dialog_held = hold;
    for (PcmVoice& slot : g_voices)
    {
        hold_voice(slot, hold);
    }
    // Xbox 0x0004E640 holds the cutscene's handles. Frontend/music voices
    // belong to separate owners and continue while the dialogue clock runs.
#else
    (void)hold;
#endif
}

void pcm_dialog_advance(const Game& g, const char* cue)
{
#if defined(_WIN32)
    pcm_dialog_hold(false);
    if (cue == nullptr || cue[0] == '\0')
    {
        return;
    }
    build_catalog(g);
    for (SoundEvent& ev : g_events)
    {
        if (!ev.dialog || ev.fired || ev.cue != cue)
        {
            continue;
        }
        fire_stream(g, ev);
        return;
    }
#else
    (void)g;
    (void)cue;
#endif
}

void pcm_cutscene_sync(const Game& g)
{
#if defined(_WIN32)
    build_catalog(g);
    if (!g.cutscenes.playing || g.cutscenes.index >= g.cutscenes.items.size())
    {
        if (!g_voices.empty() || g_dialog_held)
        {
            stop_all();
        }
        g_dialog_held = false;
        g_clip = static_cast<size_t>(-1);
        g_stream_cue.clear();
        return;
    }
    const Cutscene& clip = g.cutscenes.items[g.cutscenes.index];
    if (g.cutscenes.index != g_clip || clip.name != g_clip_name)
    {
        stop_all();
        g_dialog_held = false;
        g_clip = g.cutscenes.index;
        g_clip_name = clip.name;
        collect_events(g, clip);
    }
    for (auto& ev : g_events)
    {
        if (ev.fired || ev.dialog || ev.time > g.cutscenes.time)
        {
            continue;
        }
        fire_stream(g, ev);
    }
    for (auto& ev : g_events)
    {
        if (!ev.fired || ev.stopped || ev.stop <= ev.time || g.cutscenes.time < ev.stop)
        {
            continue;
        }
        ev.stopped = true;
        stop_named(ev.cue, false);
    }
#else
    (void)g;
#endif
}

void pcm_play_cue(const Game& g, const char* cue, bool loop)
{
#if defined(_WIN32)
    if (cue == nullptr || cue[0] == 0)
    {
        return;
    }
    for (const PcmVoice& bed : g_bed)
    {
        if (bed.cue == cue)
        {
            return;
        }
    }
    build_catalog(g);
    const auto found = g_cues.find(cue);
    if (found == g_cues.end())
    {
        std::cout << "audio unmapped " << cue << "\n";
        return;
    }
    if (const WaveChoice* choice = pick_choice(found->second, g.frame))
    {
        play_choice(cue, *choice, loop, true);
    }
#else
    (void)g;
    (void)cue;
    (void)loop;
#endif
}

// Native audio boundary at the retail cue play calls. A failed host voice returns 0.
// The book owns its handles independently of the scene cutscene's voice list.
i32 pcm_prepare_owned_cue(const Game& g, const char* cue, u64& handle, void*& voice)
{
    handle = 0;
    voice = nullptr;
#if defined(_WIN32)
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

u64 pcm_play_owned_cue(const Game& g, const char* cue)
{
#if defined(_WIN32)
    for (size_t i = 0; i < g_bed.size();)
    {
        XAUDIO2_VOICE_STATE state{};
        g_bed[i].voice->GetState(&state);
        if (g_bed[i].handle != 0 && state.BuffersQueued == 0)
        {
            release_voice(g_bed[i]);
            g_bed.erase(g_bed.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }
    build_catalog(g);
    const auto found = g_cues.find(cue);
    if (found == g_cues.end())
    {
        std::cout << "audio unmapped " << cue << "\n";
        return 0;
    }
    if (const WaveChoice* choice = pick_choice(found->second, g.frame))
    {
        return play_choice(cue, *choice, false, true);
    }
#else
    (void)g;
    (void)cue;
#endif
    return 0;
}

// Host voice-state query for the retail active-voice tests at 0x0012A510.
bool pcm_owned_cue_playing(u64 handle)
{
    if (handle == 0)
    {
        return false;
    }
#if defined(_WIN32)
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
#if defined(_WIN32)
    for (size_t i = 0; i < g_bed.size(); ++i)
    {
        if (g_bed[i].handle == handle)
        {
            std::cout << "audio cue stop " << g_bed[i].cue << "\n";
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
#if defined(_WIN32)
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
#if defined(_WIN32)
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
#if defined(_WIN32)
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
#if defined(_WIN32)
        if (!ensure_device())
        {
            return static_cast<i32>(g_device_result);
        }
        if (!g_listener_effect)
        {
            IUnknown* effect = nullptr;
            HRESULT status = XAudio2CreateReverb(&effect);
            if (FAILED(status))
            {
                return static_cast<i32>(status);
            }
            XAUDIO2_VOICE_DETAILS details;
            g_master->GetVoiceDetails(&details);
            XAUDIO2_EFFECT_DESCRIPTOR descriptor{effect, TRUE, details.InputChannels};
            const XAUDIO2_EFFECT_CHAIN chain{1, &descriptor};
            status = g_master->SetEffectChain(&chain);
            effect->Release();
            if (FAILED(status))
            {
                return static_cast<i32>(status);
            }
            g_listener_effect = true;
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
        return static_cast<i32>(g_master->SetEffectParameters(0, &native, sizeof(native)));
#else
        (void)words;
        (void)apply;
        return static_cast<i32>(0x80004001u);
#endif
    };
    return settings;
}

RuntimeSoundBoundaries native_sound_boundaries()
{
    RuntimeSoundBoundaries boundaries;
    boundaries.pause_cue = [](void*, void* cue, bool paused) -> i32
    {
#if defined(_WIN32)
        auto* voice = static_cast<IXAudio2SourceVoice*>(cue);
        return static_cast<i32>(paused ? voice->Stop(0) : voice->Start(0));
#else
        return static_cast<i32>(0x80004001u);
#endif
    };
    boundaries.stop_cue = [](void*, u32, u32 flags, void* cue)
    {
#if defined(_WIN32)
        auto* voice = static_cast<IXAudio2SourceVoice*>(cue);
        voice->Stop((flags & 2) != 0 ? 0 : XAUDIO2_PLAY_TAILS);
        if ((flags & 2) != 0)
        {
            voice->FlushSourceBuffers();
        }
#endif
    };
    boundaries.last_error = []() -> u32
    {
#if defined(_WIN32)
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
#if defined(_WIN32)
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
        }
        return static_cast<i32>(voice->Start(0));
#else
        (void)source;
        (void)cue;
        return static_cast<i32>(0x80004001u);
#endif
    };
    return boundaries;
}

void pcm_stop_cue(const char* cue)
{
#if defined(_WIN32)
    if (cue == nullptr)
    {
        return;
    }
    for (size_t i = 0; i < g_bed.size();)
    {
        if (g_bed[i].cue != cue)
        {
            ++i;
            continue;
        }
        g_bed[i].voice->Stop(0);
        g_bed[i].voice->DestroyVoice();
        g_bed.erase(g_bed.begin() + static_cast<std::ptrdiff_t>(i));
    }
#else
    (void)cue;
#endif
}

} // namespace gbtg
