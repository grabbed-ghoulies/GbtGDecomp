#include "wavebank_files.h"
#include "../frontend/dialogue_resources.h"
#include "../runtime/crt_format.h"

#include <cstring>

namespace gbtg
{
namespace
{
i32 row_word(const WavebankFileContext& context, u32 row, u32 offset)
{
    return i32(read_u32(context.table, row * 0x14 + offset));
}
}

// Xbox 0x00039DD0
void register_runtime_wavebank_files(AudioWavebankStreams& streams, WavebankFileContext& context)
{
    for (u32 row = 0; row < 12; ++row)
    {
        char name[0x80]{};
        format_dialogue_resource_name(name, "aid_xwavebank_%s_%s", "ghoulies",
            context.string(u32(row_word(context, row, 0x10)))); // 0x00035830
        // The name after "aid_", hashed as 0x00039DD0 does: each letter's upper case plus the sum times 16, its top
        // nibble folded back in.
        u32 hash = 0;
        for (const char* c = name + 4; *c != 0; ++c)
        {
            hash = (u8(*c) & 0xDF) + hash * 0x10;
            const u32 top = hash & 0xF0000000u;
            if (top != 0)
            {
                hash = hash ^ (top >> 0x18 | top);
            }
        }
        char path[0x104]{};
        std::snprintf(path, sizeof(path), "%s%s%08x", context.root, "xwavebank\\", hash);
        u32 status = context.attributes(path); // 0x0014B280
        if (status == 0xFFFFFFFFu)
        {
            status = context.last_error();
        }
        else
        {
            status = (status & 0x10) != 0 ? 0x10B : 0; // A directory.
        }
        if (status != 0)
        {
            continue;
        }
        const i32 kind = row_word(context, row, 0);
        if (kind == 1)
        {
            open_runtime_memory_wavebank(streams, row, path, context); // 0x00039C50
        }
        else if (kind == 2)
        {
            context.create_bank(path, 2, row); // 0x0015D060(path, 2, row +4, +4, +8, 0)
        }
    }
}

// Xbox 0x00039C50
i32 open_runtime_memory_wavebank(AudioWavebankStreams& streams, u32 row, const char* path,
    WavebankFileContext& context)
{
    // +8 takes the table row; the cached copy is Z:\ and the path after its drive.
    char cached[0x104]{};
    std::memcpy(cached, "Z:\\", 3);
    std::strncpy(cached + 3, path + 3, 0x100 - 1); // lstrcpynA(+3, path + 3, 0x100)
    i32 status = compare_runtime_cached_file(path, cached, context.copies); // 0x000E9530, EAX the cached path
    if (status == 0)
    {
        const i32 kind = row_word(context, row, 0);
        if (0 < kind && kind < 3)
        {
            context.create_bank(cached, kind, row); // 0x0015D060
        }
        return 0;
    }
    status = start_runtime_file_copy(path, &streams.jobs[row], cached,
        [&streams, row, &context](RuntimeMovieFile& target, i32 result, void*)
        {
            finish_runtime_wavebank_copy(target, result, row, streams, context); // 0x00039D00
        },
        &streams.jobs[row], context.copies); // 0x000E9090, EAX the disc path
    if (status != 0)
    {
        context.resource_error(u32(status)); // 0x00032700
    }
    return status;
}

// Xbox 0x00039D00
void finish_runtime_wavebank_copy(RuntimeMovieFile& target, i32 status, u32 row, AudioWavebankStreams& streams,
    WavebankFileContext& context)
{
    if (status != 0)
    {
        context.resource_error(u32(status)); // 0x00032700
        return;
    }
    streams.jobs[row] = nullptr;
    const i32 kind = row_word(context, row, 0);
    if (0 < kind && kind < 3)
    {
        context.create_bank(target.path, kind, row); // 0x0015D060(+0xC, ...)
    }
}
}
