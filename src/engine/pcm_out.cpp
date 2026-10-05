#include "gbtg/engine_internal.hpp"

#include "gbtg/bundle.hpp"

#include <algorithm>
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
    std::string bank; // WBND internal name, e.g. Gharddrive1
    u16 index = 0;
    u16 weight_lo = 0;
    u16 weight_hi = 10000;
};

struct SoundEvent
{
    float time = 0.f;
    std::string cue; // XACT_SOUNDBANK_...
    bool fired = false;
};

#if defined(_WIN32)

struct WaveFile
{
    std::filesystem::path path;
    u32 count = 0;
};

struct PcmVoice
{
    IXAudio2SourceVoice* voice = nullptr;
    std::vector<u8> pcm;
    std::string cue;
};

IXAudio2* g_xa = nullptr;
IXAudio2MasteringVoice* g_master = nullptr;
std::vector<PcmVoice> g_voices;
std::vector<PcmVoice> g_bed;
bool g_device_failed = false;
bool g_catalog_ready = false;
std::map<std::string, WaveFile> g_wavefiles;                 // WBND name -> file
std::map<std::string, std::vector<WaveChoice>> g_cues;      // full cue string -> variations
std::vector<SoundEvent> g_events;
size_t g_clip = static_cast<size_t>(-1);
std::string g_clip_name;

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

void waves_from_entry(std::vector<WaveChoice>& out, std::span<const u8> blob, i16 entry, u16 simple_count,
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
        g_device_failed = true;
        std::cout << "audio CoInitializeEx failed\n";
        return false;
    }
    if (FAILED(XAudio2Create(&g_xa, 0, XAUDIO2_DEFAULT_PROCESSOR)) || FAILED(g_xa->CreateMasteringVoice(&g_master)))
    {
        g_device_failed = true;
        std::cout << "audio XAudio2Create failed\n";
        return false;
    }
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
        v.voice->Stop(0);
        v.voice->FlushSourceBuffers();
        v.voice->DestroyVoice();
    }
    g_voices.clear();
}

bool load_wave(const WaveChoice& choice, std::vector<u8>& pcm, WAVEFORMATEX& fmt)
{
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

void play_choice(const std::string& cue, const WaveChoice& choice, bool loop = false, bool bed = false)
{
    if (!ensure_device())
    {
        return;
    }
    reap_voices();
    PcmVoice slot;
    WAVEFORMATEX fmt{};
    if (!load_wave(choice, slot.pcm, fmt))
    {
        std::cout << "audio skip " << cue << " " << choice.bank << "[" << choice.index << "]\n";
        return;
    }
    if (FAILED(g_xa->CreateSourceVoice(&slot.voice, &fmt)))
    {
        std::cout << "audio voice failed " << cue << "\n";
        return;
    }
    slot.voice->SetVolume(1.f);
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
            const char* s = reinterpret_cast<const char*>(e.cpu.data() + pos + 16);
            const size_t room = size - 16;
            size_t n = 0;
            while (n < room && s[n] != 0)
            {
                ++n;
            }
            if (n > 14 && std::memcmp(s, "XACT_SOUNDBANK_", 15) == 0)
            {
                SoundEvent ev;
                ev.time = rf32(e.cpu, pos + 4);
                ev.cue.assign(s, n);
                g_events.push_back(ev);
            }
            pos += size;
        }
    }
    std::cout << "audio events " << g_events.size() << " " << clip.name << "\n";
}

#endif

} // namespace

void pcm_cutscene_sync(const Game& g)
{
#if defined(_WIN32)
    build_catalog(g);
    if (!g.cutscenes.playing || g.cutscenes.index >= g.cutscenes.items.size())
    {
        if (!g_voices.empty())
        {
            stop_all();
        }
        g_clip = static_cast<size_t>(-1);
        return;
    }
    const Cutscene& clip = g.cutscenes.items[g.cutscenes.index];
    if (g.cutscenes.index != g_clip || clip.name != g_clip_name)
    {
        stop_all();
        g_clip = g.cutscenes.index;
        g_clip_name = clip.name;
        collect_events(g, clip);
    }
    for (auto& ev : g_events)
    {
        if (ev.fired || ev.time > g.cutscenes.time)
        {
            continue;
        }
        ev.fired = true;
        const auto found = g_cues.find(ev.cue);
        if (found == g_cues.end())
        {
            std::cout << "audio unmapped " << ev.cue << "\n";
            continue;
        }
        if (const WaveChoice* choice = pick_choice(found->second, g.frame))
        {
            play_choice(ev.cue, *choice);
        }
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
