#pragma once
#include "movie_files.h"

#include <functional>

namespace gbtg
{
// What the copies reach: the file rows and the boot state's buffers (DAT_005065F8 / DAT_005065FC, 0x20000 bytes
// each, DAT_00506600) and list head (DAT_005065F0 / DAT_005065F4).
struct RuntimeFileCopyContext
{
    MovieFileContext& files;
    RuntimeMovieFileBootState& state;
};
// Copy `source` to `target` (opened for writing, its length set to the source's); the copy is queued and its
// request stored in `job`. Errors other than the pending and not-found ones are looked up (0x000E96A0).
i32 start_runtime_file_copy(const char* source, RuntimeFileCopy** job, const char* target,
    RuntimeFileCopyCallback callback, void* context, RuntimeFileCopyContext& copies); // Xbox 0x000E9090, EAX source
void queue_runtime_file_copy(RuntimeFileCopy** job, RuntimeMovieFile& target, RuntimeFileCopyCallback callback,
    void* context, RuntimeMovieFile& source, RuntimeFileCopyContext& copies); // Xbox 0x000E9190, EDI source
// The frame loop's step of the first queued copy.
void pump_runtime_file_copy(RuntimeFileCopy& copy, RuntimeFileCopyContext& copies); // Xbox 0x000E9280, EAX copy
// 0xEA with the bytes left and the source's length while `copy` is queued, else 0x490.
i32 runtime_file_copy_progress(const RuntimeFileCopy* copy, u32& remaining, u32& total,
    const RuntimeFileCopyContext& copies); // Xbox 0x000E9040, EDX copy
// 0 when `cached` exists and is no older than `source` (by their write times), else the open error or 0x51A.
i32 compare_runtime_cached_file(const char* source, const char* cached,
    RuntimeFileCopyContext& copies); // Xbox 0x000E9530
// The global the frame loop and the stream query reach the copies through (DAT_005065F0); the movie session sets it.
extern RuntimeFileCopyContext* g_runtime_file_copies;
}
