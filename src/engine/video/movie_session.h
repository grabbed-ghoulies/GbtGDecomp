#pragma once
#include "movie_startup.h"
#include "file_copy.h"
#include "../runtime/file_worker.h"
#include <filesystem>
#include <string>

namespace gbtg
{
// Scene ownership for the native retail movie/file lists. Boundary callbacks are
// supplied by the renderer and utility-drive owner, rather than simulated here.
class RuntimeMovieSession
{
public:
    RuntimeMovieSession(const std::filesystem::path& disc_root, MovieErrorContext& error,
        bool pal, bool pal_override,
        std::function<RuntimeMovieLockedSurface(void*)> lock_texture);
    ~RuntimeMovieSession();
    RuntimeMovieSession(const RuntimeMovieSession&) = delete;
    RuntimeMovieSession& operator=(const RuntimeMovieSession&) = delete;
    RuntimeMovieStartupContext& startup();
    MovieFileContext& files();
    RuntimeFileCopyContext& copies(); // DAT_005065F0's copies over the file rows.
    void pump();
    void reset();
    std::array<RuntimeMovieEventSlot, 4> slots{};

private:
    std::string root_;
    char path_[260]{};
    char diagnostic_[260]{};
    char boot_root_[260]{};
    RuntimeMovieFileBootState boot_{};
    PerformanceCounterState counter_{};
    RuntimeFileWorker worker_{};
    FileWorkerBoundaries worker_io_{};
    MovieFileBoundaries file_io_{};
    FileWorkerContext worker_context_;
    RuntimeMovieFile file_head_{};
    u32 pending_ = 0;
    u32 file_count_ = 0;
    i32 sectors_[26]{};
    MovieFileContext file_context_;
    RuntimeFileCopyContext copies_{file_context_, boot_};
    MoviePathContext paths_;
    RuntimeMovieState movie_head_{};
    i32 initialized_ = 0;
    u8* preloaded_ = nullptr;
    RuntimeMovieStartupContext startup_;
};
}
