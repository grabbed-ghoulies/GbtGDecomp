#pragma once
#include "movie_errors.h"
#include "movie_files.h"
#include <array>
#include <functional>

namespace gbtg
{
struct RgbaImage;
struct RuntimeMovieLockedSurface
{
    i32 pitch;
    u8* pixels;
};
struct RuntimeMovieMemoryCallbacks
{
    std::function<u8*(u32)> allocate;
    std::function<void(u8*)> free;
};
void register_runtime_movie_memory(const std::function<u8*(u32)>& allocate,
    const std::function<void(u8*)>& free, RuntimeMovieMemoryCallbacks& callbacks); // Xbox 0x0015D380
u8* allocate_movie_decoder_memory(u32 size); // Xbox 0x001073E0
void free_movie_decoder_memory(u8* buffer); // Xbox 0x00107400
struct MoviePathContext
{
    i32 pal;
    i32 pal_override;
    const char* root; // DAT_0042A318, including trailing separator.
    char* path; // DAT_0050DFB0.
    MovieErrorContext& error;
    std::function<u32(const char*)> file_attributes; // XAPI 0x0014B280 boundary.
    std::function<u32()> last_error; // XAPI 0x0014AFE4 boundary.
};
char* resolve_runtime_movie_path(const char* name,
    MoviePathContext& context); // Xbox 0x00106EF0
struct RuntimeMovieState
{
    RuntimeMovieState* previous;
    RuntimeMovieState* next;
    void* decoder; // +8, native Bink decoder mapping.
    u32 open_flags; // +C.
    u32 flags; // +10.
    u32 loop; // +14.
    u32 play_count; // +18.
    u32 pending; // +1C.
    u32 unknown20; // +20.
    u32 unknown24; // +24.
    u32 unknown28; // +28.
    RuntimeMovieFile* file; // +2C.
    u8* buffer; // +30.
    u32 enabled; // +34.
    u32 logo; // +38.
    u32 buffer_size; // Native allocation extent for memory decoder.
};
struct RuntimeMovieStartupContext
{
    MoviePathContext& paths;
    MovieFileContext& files;
    RuntimeMovieState& head; // DAT_0050E0B4/B8 sentinel.
    i32& initialized; // DAT_0050E0BC.
    u8*& preloaded; // DAT_0050E0B0.
    std::function<void*(const char*, u32)> decoder_open; // Existing real Bink backend.
    std::function<void(void*, u32, u32)> decoder_track; // Bink 1A7B70.
    std::function<u8*(u32)> allocate_buffer; // XAPI heap allocation 14D377/14E872.
    std::function<void(u8*)> free_buffer; // XAPI heap release 14D377/14D360.
    RuntimeMovieMemoryCallbacks memory; // DAT_004D9C88/8C allocator callback mapping.
    std::function<void*(const u8*, u32, u32)> decoder_memory_open; // Native buffer extent mapping for BinkOpen.
    std::function<i32(void*)> decoder_wait; // Bink 1A7870.
    std::function<void(void*)> decoder_do_frame; // Bink 1A7D40.
    std::function<void(void*)> decoder_next_frame; // Bink 1A8350.
    std::function<u32(void*)> decoder_frame_number; // Bink object+0C.
    std::function<u32(void*)> decoder_frame_count; // Bink object+8.
    std::function<void(void*)> decoder_close; // Bink 1A84F0.
    std::function<u32(void*)> decoder_height; // Bink object+4.
    std::function<RuntimeMovieLockedSurface(void*)> lock_texture; // D3D 177950.
    std::function<i32(void*, u8*, i32, u32, u32, u32, u32)> decoder_copy; // Bink 1A7D00.
    u32 preloaded_size = 0; // Native extent mapping for DAT_0050E0B0.
};
void complete_runtime_movie_read(RuntimeMovieFile& file, i32 status,
    RuntimeMovieState& movie, RuntimeMovieStartupContext& context); // Xbox 0x00106FE0
i32 open_runtime_movie(const char* name, const char* path, RuntimeMovieState& movie,
    RuntimeMovieStartupContext& context); // Xbox 0x00107030
RuntimeMovieState* allocate_runtime_movie(const char* name, u32 flags,
    RuntimeMovieStartupContext& context); // Xbox 0x001071A0
void tick_runtime_movie(RuntimeMovieState& movie,
    RuntimeMovieStartupContext& context); // Xbox 0x00107420
bool copy_runtime_movie_frame(RuntimeMovieState& movie, void* texture,
    RuntimeMovieStartupContext& context); // Xbox 0x00107360
u32 release_runtime_movie(RuntimeMovieState& movie,
    RuntimeMovieStartupContext& context); // Xbox 0x001072B0
const RgbaImage* runtime_movie_image(const RuntimeMovieState& movie);
struct RuntimeMovieEventSlot
{
    u32* play_count; // Native pointer mapping to movie node+18.
    u8 id; // Retail event row+4.
    u8 state; // Retail event row+5.
    RuntimeMovieState* movie; // Native +0 node pointer.
    char name[130]; // +6..87.
};
void bind_existing_movie_decoder(RuntimeMovieStartupContext& context);
void bind_native_movie_heap(RuntimeMovieStartupContext& context);
void load_runtime_movie_event(const char* name, u8 id, i32 loop, i32 file_buffer,
    std::array<RuntimeMovieEventSlot, 4>& slots,
    RuntimeMovieStartupContext& context); // Xbox 0x00106D50
void start_runtime_movie_event(u8 id, i32 mode,
    std::array<RuntimeMovieEventSlot, 4>& slots); // Xbox 0x00106DD0
}
