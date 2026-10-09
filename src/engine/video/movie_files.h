#pragma once
#include "movie_errors.h"
#include <functional>

namespace gbtg
{
struct FileWorkerContext;
struct FileWorkerBoundaries;
struct RuntimeMovieFile
{
    RuntimeMovieFile* previous;
    RuntimeMovieFile* next;
    char path[260]; // +C.
    std::uintptr_t handle; // Native XAPI handle mapping, retail +8.
    u32 state; // +110.
    u32 overlapped[5]; // +114..124.
    u32 offset; // +11C.
    std::function<void(RuntimeMovieFile&, i32, void*)> completion; // +128.
    void* payload; // +12C.
};
struct MovieFileBoundaries
{
    std::function<RuntimeMovieFile*()> allocate; // F72D0 heap boundary.
    std::function<void(RuntimeMovieFile*)> free; // F7100/HeapFree boundary.
    std::function<bool(RuntimeMovieFile&, void*, u32)> read;
    std::function<u32()> ticks;
    std::function<u32(const char*)> sector_size;
    std::function<bool(std::uintptr_t, std::int64_t, u64*, u32)> seek;
    std::function<bool(std::uintptr_t)> end_file;
    std::function<void()> format_utility;
    std::function<void(u32)> cache_size;
    std::function<bool(std::uintptr_t)> close;
    std::function<bool(std::uintptr_t, u64&)> size;
    std::function<bool(RuntimeMovieFile&, u32&)> completed;
    std::function<u32()> last_error;
};
struct MovieFileContext
{
    MovieFileBoundaries& io;
    MovieErrorContext& error;
    u32& pending; // DAT_00506624.
    RuntimeMovieFile& head; // Native mapping of DAT_00506618/1C sentinel.
    u32& count; // DAT_00506620.
    std::function<void()> notify; // Actual indirect DAT_005462D0 callback.
    u32 notify_interval; // DAT_005462D4.
    i32* sector_sizes; // DAT_00506868; indexed by drive letter minus A.
    FileWorkerContext* worker = nullptr;
};
i32 open_runtime_movie_file(RuntimeMovieFile*& output, const char* path,
    u32 access, u32 creation, MovieFileContext& context); // Xbox 0x000EAA90
i32 allocate_runtime_movie_file(RuntimeMovieFile*& output,
    MovieFileContext& context); // Xbox 0x000E9F10
i32 wait_runtime_movie_file(RuntimeMovieFile& file, i32 initial_status,
    MovieFileContext& context); // Xbox 0x000EA850
i32 read_runtime_movie_file(RuntimeMovieFile& file, void* output, u32 size,
    std::function<void(RuntimeMovieFile&, i32, void*)> completion, void* payload,
    MovieFileContext& context); // Xbox 0x000EB0E0
i32 query_runtime_movie_sector_size(RuntimeMovieFile& file, i32& output,
    MovieFileContext& context); // Xbox 0x000EB040
i32 align_runtime_movie_file_length(RuntimeMovieFile& file, i32 length, i32& output,
    MovieFileContext& context); // Xbox 0x000EB310
i32 close_runtime_movie_file_handle(RuntimeMovieFile& file,
    MovieFileContext& context); // Xbox 0x000EABA0
i32 close_runtime_movie_file(RuntimeMovieFile& file,
    MovieFileContext& context); // Xbox 0x000EAC20
i32 query_runtime_movie_file_size(RuntimeMovieFile& file, u64& output,
    MovieFileContext& context); // Xbox 0x000EAE80
i32 query_runtime_movie_file_length(RuntimeMovieFile& file, u32& output,
    MovieFileContext& context); // Xbox 0x000EAED0
i32 poll_runtime_movie_file(RuntimeMovieFile& file,
    MovieFileContext& context); // Xbox 0x000E9FB0
void pump_runtime_movie_files(MovieFileContext& context); // Xbox 0x000E9E60
i32 query_runtime_movie_file_position(RuntimeMovieFile& file, u64& output,
    MovieFileContext& context); // Xbox 0x000EADC0
i32 seek_runtime_movie_file(RuntimeMovieFile& file, u32 low, u32 high,
    MovieFileContext& context); // Xbox 0x000EAE20
i32 resize_runtime_movie_file(RuntimeMovieFile& file, u32 low, u32 high,
    MovieFileContext& context); // Xbox 0x000EAF30
i32 resize_runtime_movie_file_low(RuntimeMovieFile& file, u32 length,
    MovieFileContext& context); // Xbox 0x000EAFF0
struct RuntimeMovieFileBootState
{
    u32 utility_pending; // DAT_00506604.
    const char* disc_root; // DAT_0054F5B0.
    char* root; // DAT_0042A318.
    u8* first_buffer; // DAT_005065F8.
    u8* second_buffer; // DAT_005065FC.
    RuntimeMovieFile first_head; // DAT_005065F0/F4 native list head.
    RuntimeMovieFile second_head; // DAT_00506728/2C native list head.
    u32 buffer_size; // DAT_00506600.
};
void initialize_runtime_movie_utility_drive(MovieFileContext& context,
    RuntimeMovieFileBootState& state); // Xbox 0x000E8F10
void initialize_runtime_movie_files(MovieFileContext& context,
    RuntimeMovieFileBootState& state); // Xbox 0x000E9CE0
void bind_native_movie_file_io(MovieFileBoundaries& files, FileWorkerBoundaries& worker);
}
