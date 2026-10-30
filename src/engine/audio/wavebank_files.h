#pragma once
#include "native_audio.h"
#include "../video/file_copy.h"

#include <functional>
#include <span>

namespace gbtg
{
// What 0x00039DD0's file side reaches: the wavebank table (0x00421A80, stride 0x14: +0 the kind, 1 in memory or 2
// streamed, +4 and +8 the XACT bank sizes, +0xC the stream id, +0x10 the name), the disc root (DAT_0042A318), the
// file attributes (0x0014B280) and the copies. The XACT bank itself (0x0015D060) is the host's: the host catalog
// (NativeAudioBootBoundaries::register_wavebanks) already plays every bank from the disc.
struct WavebankFileContext
{
    std::span<const u8> table; // 0x00421A80, 12 rows
    std::function<const char*(u32 address)> string; // A name word's string.
    const char* root; // DAT_0042A318
    std::function<u32(const char* path)> attributes; // GetFileAttributesA, or 0xFFFFFFFF
    std::function<u32()> last_error;
    std::function<void(const char* path, i32 kind, u32 row)> create_bank; // 0x0015D060
    std::function<void(u32 code)> resource_error; // 0x00032700
    RuntimeFileCopyContext& copies;
};
// The twelve wavebank files: an in-memory bank (kind 1) is copied to the utility drive first unless its cached
// copy is current (0x00039C50); a streamed one (kind 2) opens from the disc.
void register_runtime_wavebank_files(AudioWavebankStreams& streams,
    WavebankFileContext& context); // Xbox 0x00039DD0
i32 open_runtime_memory_wavebank(AudioWavebankStreams& streams, u32 row, const char* path,
    WavebankFileContext& context); // Xbox 0x00039C50, EAX the stream row, ECX the table row
// The copy's completion: the row's job ends and the bank opens from the utility drive.
void finish_runtime_wavebank_copy(RuntimeMovieFile& target, i32 status, u32 row, AudioWavebankStreams& streams,
    WavebankFileContext& context); // Xbox 0x00039D00
}
