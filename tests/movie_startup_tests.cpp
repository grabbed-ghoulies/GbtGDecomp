#include "../src/engine/video/movie_files.h"
#include "../src/engine/video/movie_startup.h"
#include "../src/engine/runtime/file_worker.h"
#include "../src/engine/video/movie_session.h"
#include "gbtg/movie.h"
#include <cstring>
#include <iostream>

int main(int argc, char** argv)
{
    using namespace gbtg;
    const MovieErrorMessage messages[]{{0, nullptr, nullptr}};
    char fallback[15]{};
    ResourceErrorState error{};
    MovieErrorContext errors{messages, fallback, error};
    RuntimeMovieFile head{};
    MovieFileBoundaries io{};
    u32 pending = 0;
    u32 count = 0;
    i32 sectors[26]{};
    u32 last_error = 0;
    u32 completions = 0;
    u32 sector_queries = 0;
    io.allocate = []()
    {
        return new RuntimeMovieFile;
    };
    io.free = [](RuntimeMovieFile* file)
    {
        delete file;
    };
    io.ticks = []()
    {
        return 0u;
    };
    io.last_error = [&]()
    {
        return last_error;
    };
    io.read = [&](RuntimeMovieFile&, void*, u32)
    {
        last_error = 0x3E5;
        return false;
    };
    io.completed = [&](RuntimeMovieFile&, u32& transferred)
    {
        transferred = 1537;
        return true;
    };
    io.sector_size = [&](const char* drive)
    {
        ++sector_queries;
        if (std::strcmp(drive, "D:\\") != 0)
        {
            return 0u;
        }
        return 512u;
    };
    MovieFileContext context{io, errors, pending, head, count, {}, 0, sectors};
    RuntimeMovieFile* first = nullptr;
    RuntimeMovieFile* second = nullptr;
    if (allocate_runtime_movie_file(first, context) != 0 ||
        allocate_runtime_movie_file(second, context) != 0 || count != 2 ||
        head.next != first || first->next != second || second->previous != first)
    {
        return 1;
    }
    std::strcpy(first->path, "d:\\movie\\12345678");
    i32 aligned = 0;
    if (align_runtime_movie_file_length(*first, 1537, aligned, context) != 0 || aligned != 2048 ||
        align_runtime_movie_file_length(*first, -513, aligned, context) != 0 || aligned != -512 ||
        sector_queries != 1)
    {
        return 2;
    }
    u8 data[2048];
    if (read_runtime_movie_file(*first, data, 1537,
        [&](RuntimeMovieFile& file, i32 status, void*)
        {
            ++completions;
            if (status == 0)
            {
                file.state = 2;
            }
        }, nullptr, context) != 0 || first->state != 1 || pending != 1)
    {
        return 3;
    }
    if (wait_runtime_movie_file(*first, 0, context) != 0 || completions != 1 || pending != 0)
    {
        return 4;
    }
    // Poll completes the request; the next readiness call releases its closed row.
    if (wait_runtime_movie_file(*first, 0, context) != 0 || count != 1 ||
        head.next != second || second->previous != &head)
    {
        return 5;
    }
    second->state = 2;
    if (wait_runtime_movie_file(*second, 0, context) != 0 || count != 0 || head.next != nullptr)
    {
        return 6;
    }
    char movie_path[260]{};
    MoviePathContext paths{0, 0, "d:\\", movie_path, errors,
        [](const char*)
        {
            return 0u;
        }, io.last_error};
    RuntimeMovieState movie_head{};
    i32 initialized = 0;
    u8* preloaded = nullptr;
    u32 decoder_calls = 0;
    u32 track_calls = 0;
    RuntimeMovieStartupContext startup{paths, context, movie_head, initialized, preloaded,
        [&](const char*, u32 flags) -> void*
        {
            ++decoder_calls;
            if (flags != 0x40100000)
            {
                return nullptr;
            }
            return &movie_head;
        },
        [&](void* decoder, u32 track, u32 volume)
        {
            if (decoder == &movie_head && track == 0 && volume == 0x4000)
            {
                ++track_calls;
            }
        },
        [](u32 size)
        {
            return new u8[size];
        },
        [](u8* bytes)
        {
            delete[] bytes;
        }, {}};
    RuntimeMovieState* direct = allocate_runtime_movie("aid_movie_test", 0xA, startup);
    if (direct == nullptr || direct->play_count != 1 || direct->enabled != 1 ||
        direct->open_flags != 0x44100000 || decoder_calls != 1 || track_calls != 1 ||
        !startup.memory.allocate || !startup.memory.free || movie_head.next != direct || direct->previous != &movie_head)
    {
        return 7;
    }
    RuntimeFileWorker worker{};
    FileWorkerBoundaries worker_io{};
    worker_io.create_file = [](const char*, u32 access, u32 share, void*, u32 creation,
        u32 attributes, std::uintptr_t)
    {
        if (access == 0x80000000 && share == 7 && creation == 3 && attributes == 0x60000080)
        {
            return std::uintptr_t{55};
        }
        return static_cast<std::uintptr_t>(-1);
    };
    worker_io.last_error = io.last_error;
    worker_io.set_last_error = [&](u32 status)
    {
        last_error = status;
    };
    PerformanceCounterState counter;
    initialize_performance_counter(counter);
    char diagnostic[260]{};
    FileWorkerContext file_worker{worker, worker_io, counter, {}, 0, diagnostic, errors};
    context.worker = &file_worker;
    io.size = [](std::uintptr_t handle, u64& size)
    {
        size = 1537;
        return handle == 55;
    };
    io.close = [](std::uintptr_t handle)
    {
        return handle == 55;
    };
    RuntimeMovieState* buffered = allocate_runtime_movie("aid_movie_test", 0x10, startup);
    if (buffered == nullptr || buffered->enabled != 0 || buffered->buffer_size != 2048 ||
        pending != 1 || decoder_calls != 1 || !startup.memory.allocate || !startup.memory.free)
    {
        return 8;
    }
    if (poll_runtime_movie_file(*buffered->file, context) != 0 || buffered->enabled != 1 ||
        buffered->file->state != 2 || pending != 0)
    {
        return 9;
    }
    if (wait_runtime_movie_file(*buffered->file, 0, context) != 0 || count != 0)
    {
        return 10;
    }
    delete[] buffered->buffer;
    delete buffered;
    delete direct;
    if (argc > 1)
    {
        // Real file/heap/decoder integration, without a presentation surface.
        RuntimeMovieSession session(argv[1], errors, false, false, {});
        u32 decoder_allocations = 0;
        u32 decoder_releases = 0;
        const auto allocate = session.startup().allocate_buffer;
        const auto release = session.startup().free_buffer;
        session.startup().allocate_buffer = [&decoder_allocations, allocate](u32 bytes)
        {
            ++decoder_allocations;
            return allocate(bytes);
        };
        session.startup().free_buffer = [&decoder_releases, release](u8* allocation)
        {
            ++decoder_releases;
            release(allocation);
        };
        RuntimeMovieState* live = allocate_runtime_movie(
            "aid_movie_ghoulies_gameselect_house", 6, session.startup());
        if (live == nullptr || live->decoder == nullptr || live->enabled != 1 ||
            !session.startup().memory.allocate || !session.startup().memory.free ||
            decoder_allocations != 1 || decoder_releases != 0)
        {
            return 11;
        }
        movie_advance_time(1.f / 30.f);
        session.pump();
        const RgbaImage* image = runtime_movie_image(*live);
        if (image == nullptr || image->bgra.empty() || image->width <= 0 || image->height <= 0)
        {
            return 12;
        }
        session.reset();
        if (decoder_releases != decoder_allocations)
        {
            return 15;
        }
        if (session.startup().head.next != nullptr || session.files().count != 0)
        {
            return 13;
        }
    }
    std::cout << "Movie allocation, asynchronous completion, release, and sector alignment passed.\n";
}
