#include "gbtg/wavebank.hpp"

#include <algorithm>
#include <cstring>

namespace gbtg {
namespace {

struct Region {
    u32 offset = 0;
    u32 length = 0;
};

void decode_mini_format(u32 fmt, WaveEntry& e) {
    e.codec = fmt & 0x3;
    e.channels = ((fmt >> 2) & 0x7) + 1;
    e.samples_per_sec = (fmt >> 5) & 0x3FFFF;
    e.bits = ((fmt >> 31) & 1) ? 16 : 8;
}

}  // namespace

WaveBank parse_wavebank(const std::filesystem::path& path, int max_pcm_entries) {
    auto bytes = read_file(path);
    if (bytes.size() < 40 || std::memcmp(bytes.data(), "WBND", 4) != 0) {
        throw Error("not a WBND file");
    }
    WaveBank bank;
    bank.path = path.string();
    bank.version = read_u32(bytes, 4);

    // Xbox XACT v1–v3: 4 segments immediately after dwVersion.
    // Later (XNA / 360) headers insert dwHeaderVersion and a 5th names segment.
    const bool xbox_v3 = bank.version <= 3;
    const size_t seg0 = xbox_v3 ? 8 : 12;
    const int nseg = xbox_v3 ? 4 : 5;
    Region segs[5]{};
    for (int i = 0; i < nseg; ++i) {
        const size_t o = seg0 + size_t(i) * 8;
        if (o + 8 > bytes.size()) {
            break;
        }
        segs[i].offset = read_u32(bytes, o);
        segs[i].length = read_u32(bytes, o + 4);
    }

    const Region& bd = segs[0];
    const Region& meta = segs[1];
    const Region names = xbox_v3 ? Region{} : segs[3];
    const Region& wave = xbox_v3 ? segs[3] : segs[4];

    if (bd.offset + 8 > bytes.size()) {
        return bank;
    }
    const u32 flags = read_u32(bytes, bd.offset);
    const u32 entry_count = read_u32(bytes, bd.offset + 4);
    const size_t name_field = xbox_v3 ? 16 : 64;
    u32 meta_elem = 24;
    u32 name_elem = 0;
    if (bd.offset + 8 + name_field + 8 <= bytes.size() && bd.length >= 8 + name_field + 8) {
        meta_elem = read_u32(bytes, bd.offset + 8 + name_field);
        name_elem = read_u32(bytes, bd.offset + 8 + name_field + 4);
    }
    (void)flags;
    if (entry_count == 0 || entry_count > 100000) {
        return bank;
    }

    size_t entry_size = meta_elem ? meta_elem : 24;
    if (meta.length && entry_count) {
        const size_t from_seg = meta.length / entry_count;
        if (from_seg >= 4) {
            entry_size = from_seg;
        }
    }

    bank.entries.reserve(entry_count);
    int pcm_copied = 0;
    for (u32 i = 0; i < entry_count; ++i) {
        WaveEntry e;
        const size_t off = size_t(meta.offset) + size_t(i) * entry_size;
        if (off + 16 > bytes.size()) {
            break;
        }
        if (entry_size >= 24) {
            const u32 flags_dur = read_u32(bytes, off);
            e.flags = flags_dur & 0xF;
            e.duration = flags_dur >> 4;
            decode_mini_format(read_u32(bytes, off + 4), e);
            e.play_offset = read_u32(bytes, off + 8);
            e.play_length = read_u32(bytes, off + 12);
        } else if (entry_size >= 4) {
            // compact: offset/alignment packed; skip PCM
            e.name = "wave_" + std::to_string(i);
        }
        if (names.length && name_elem && names.offset + i * name_elem + 1 <= bytes.size()) {
            char n[65]{};
            const size_t nlen = std::min<size_t>(64, std::min<size_t>(name_elem, bytes.size() - (names.offset + i * name_elem)));
            std::memcpy(n, bytes.data() + names.offset + i * name_elem, nlen);
            e.name = n;
        }
        if (e.name.empty() || e.name[0] < 32) {
            e.name = "wave_" + std::to_string(i);
        }
        if (max_pcm_entries > 0 && pcm_copied < max_pcm_entries && e.codec == 0 && e.play_length &&
            e.play_length <= 4 * 1024 * 1024 &&
            size_t(wave.offset) + e.play_offset + e.play_length <= bytes.size()) {
            e.pcm.assign(bytes.begin() + wave.offset + e.play_offset,
                         bytes.begin() + wave.offset + e.play_offset + e.play_length);
            ++pcm_copied;
        }
        bank.entries.push_back(std::move(e));
    }
    return bank;
}

void write_wav(const std::filesystem::path& path, const WaveEntry& e) {
    if (e.pcm.empty()) {
        throw Error("no PCM for " + e.name);
    }
    const u16 channels = u16(e.channels ? e.channels : 1);
    const u32 rate = e.samples_per_sec ? e.samples_per_sec : 22050;
    const u16 bits = u16(e.bits ? e.bits : 16);
    const u32 data_size = u32(e.pcm.size());
    const u32 byte_rate = rate * channels * (bits / 8);
    const u16 block_align = u16(channels * (bits / 8));
    std::vector<u8> wav(44 + e.pcm.size());
    std::memcpy(wav.data(), "RIFF", 4);
    const u32 riff_size = 36 + data_size;
    std::memcpy(wav.data() + 4, &riff_size, 4);
    std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
    u32 fmt_size = 16;
    std::memcpy(wav.data() + 16, &fmt_size, 4);
    u16 audio_fmt = 1;
    std::memcpy(wav.data() + 20, &audio_fmt, 2);
    std::memcpy(wav.data() + 22, &channels, 2);
    std::memcpy(wav.data() + 24, &rate, 4);
    std::memcpy(wav.data() + 28, &byte_rate, 4);
    std::memcpy(wav.data() + 32, &block_align, 2);
    std::memcpy(wav.data() + 34, &bits, 2);
    std::memcpy(wav.data() + 36, "data", 4);
    std::memcpy(wav.data() + 40, &data_size, 4);
    std::memcpy(wav.data() + 44, e.pcm.data(), e.pcm.size());
    write_file(path, wav);
}

}  // namespace gbtg
