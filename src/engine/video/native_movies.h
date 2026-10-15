#pragma once
#include "movie_session.h"
#include "gbtg/model_relocation.h"
#include <memory>
#include <vector>

namespace gbtg
{
struct Game;
// The retail movie and file lists from boot, as in retail: the file rows (0x000E9CE0), the movie list
// (DAT_0050E0B4/B8), the four event slots (DAT_0050DD90) and their mode (DAT_0050DD8C). Scenes start movies
// through cutscene events into these slots; the scene's texture hooks (DAT_00545DC0) receive the frames.
struct NativeMovieRuntime
{
    std::vector<MovieErrorMessage> messages; // DAT_0042A180, zero-code terminated.
    std::array<char, 15> fallback{}; // DAT_00506608.
    ResourceErrorState error{};
    MovieErrorContext errors{nullptr, fallback.data(), error};
    std::unique_ptr<RuntimeMovieSession> session;
    i32 list_lock_a = 0; // DAT_004E5CD8, BSS; no translated code writes it.
    i32 list_lock_b = 0; // DAT_004E5CDC, BSS; no translated code writes it.
    i32 mode = 3; // DAT_0050DD8C; boot 0x00033030 stores 3.
    NativeMovieRuntime() = default;
    NativeMovieRuntime(const NativeMovieRuntime&) = delete;
    NativeMovieRuntime& operator=(const NativeMovieRuntime&) = delete;
    ~NativeMovieRuntime(); // Clears g_native_movie_runtime when it is this one.
};
extern NativeMovieRuntime* g_native_movie_runtime; // Set at boot.
NativeMovieRuntime& native_movie_runtime(); // Raises before boot.
std::shared_ptr<NativeMovieRuntime> boot_native_movies(const Game& game);
// 0x00033B50 and 0x00033A90 walk DAT_0050E0B8 with 0x00107420 while DAT_004E5CD8 and DAT_004E5CDC are clear.
void tick_native_movie_list(NativeMovieRuntime& movies);
// Releases each started slot whose movie has finished or failed and has no frames left to copy.
void release_finished_movie_events(std::array<RuntimeMovieEventSlot, 4>& slots,
    RuntimeMovieStartupContext& context); // Xbox 0x00106C70
// Scene teardown: releases every movie event slot's movie, sets the mode (DAT_0050DD8C) to 3 and frees the
// preloaded movie buffer (DAT_0050E0B0).
void release_movie_events(NativeMovieRuntime& movies); // Xbox 0x00106E30
// Copies each slot's current frame into the texture its id hooks (DAT_00545DC0 + id * 0x14).
void copy_movie_event_frames(std::array<RuntimeMovieEventSlot, 4>& slots,
    const RuntimeSpecialTextures& hooks, RuntimeMovieStartupContext& context); // Xbox 0x00106CD0
}
