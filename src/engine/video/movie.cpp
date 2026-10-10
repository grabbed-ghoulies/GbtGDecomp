#include "gbtg/movie.h"
#include "movie_errors.h"
#include "movie_startup.h"
#include "movie_files.h"
#include "../runtime/crt_format.h"
#include "../runtime/file_worker.h"
#include "../frontend/dialogue_resources.h"
#include "gbtg/anim.h"

#include "gbtg/bundle.h"
#include "gbtg/engine_internal.h"
#include "gbtg/hash.h"
#include "gbtg/gfx.h"
#include "gbtg/resource.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#include <new>
#include <memory>
#include <unordered_map>
#include <malloc.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(GBTG_HAS_FFMPEG)
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#endif

namespace gbtg
{
namespace
{
u32 g_movie_io_cache = 4096;
}
#if defined(_WIN32)
void bind_native_movie_file_io(MovieFileBoundaries& files, FileWorkerBoundaries& worker)
{
    auto requests = std::make_shared<std::unordered_map<RuntimeMovieFile*, OVERLAPPED>>();
    files.allocate = []()
    {
        return new (std::nothrow) RuntimeMovieFile;
    };
    files.free = [requests](RuntimeMovieFile* file)
    {
        requests->erase(file);
        delete file;
    };
    files.close = [](std::uintptr_t handle)
    {
        return CloseHandle(reinterpret_cast<HANDLE>(handle)) != FALSE;
    };
    files.size = [](std::uintptr_t handle, u64& size)
    {
        LARGE_INTEGER length;
        const BOOL result = GetFileSizeEx(reinterpret_cast<HANDLE>(handle), &length);
        if (result != FALSE)
        {
            size = static_cast<u64>(length.QuadPart);
        }
        return result != FALSE;
    };
    files.read = [requests](RuntimeMovieFile& file, void* output, u32 size)
    {
        OVERLAPPED& request = (*requests)[&file];
        request.Offset = file.offset;
        request.OffsetHigh = 0;
        return ReadFile(reinterpret_cast<HANDLE>(file.handle), output, size, nullptr, &request) != FALSE;
    };
    files.completed = [requests](RuntimeMovieFile& file, u32& transferred)
    {
        DWORD bytes;
        const BOOL result = GetOverlappedResult(reinterpret_cast<HANDLE>(file.handle),
            &requests->at(&file), &bytes, FALSE);
        if (result != FALSE)
        {
            transferred = bytes;
        }
        return result != FALSE;
    };
    files.last_error = []()
    {
        return static_cast<u32>(GetLastError());
    };
    files.ticks = []()
    {
        return static_cast<u32>(GetTickCount());
    };
    files.sector_size = [](const char* drive)
    {
        DWORD sectors_per_cluster;
        DWORD bytes_per_sector;
        DWORD free_clusters;
        DWORD clusters;
        if (GetDiskFreeSpaceA(drive, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &clusters) == FALSE)
        {
            return 0u;
        }
        return static_cast<u32>(bytes_per_sector);
    };
    files.seek = [](std::uintptr_t handle, std::int64_t distance, u64* output, u32 method)
    {
        LARGE_INTEGER move;
        LARGE_INTEGER result;
        move.QuadPart = distance;
        const BOOL success = SetFilePointerEx(reinterpret_cast<HANDLE>(handle), move,
            output != nullptr ? &result : nullptr, method);
        if (success != FALSE && output != nullptr)
        {
            *output = static_cast<u64>(result.QuadPart);
        }
        return success != FALSE;
    };
    files.end_file = [](std::uintptr_t handle)
    {
        return SetEndOfFile(reinterpret_cast<HANDLE>(handle)) != FALSE;
    };
    files.cache_size = [](u32 capacity)
    {
        g_movie_io_cache = capacity;
    };
    worker.create_file = [](const char* path, u32 access, u32 share, void* security,
        u32 creation, u32 attributes, std::uintptr_t template_file)
    {
        return reinterpret_cast<std::uintptr_t>(CreateFileA(path, access, share,
            static_cast<SECURITY_ATTRIBUTES*>(security), creation, attributes,
            reinterpret_cast<HANDLE>(template_file)));
    };
    worker.last_error = files.last_error;
    worker.set_last_error = [](u32 status)
    {
        SetLastError(status);
    };
    worker.attributes = [](const char* path)
    {
        return static_cast<u32>(GetFileAttributesA(path));
    };
    worker.create_directory = [](const char* path)
    {
        return CreateDirectoryA(path, nullptr) != FALSE;
    };
    worker.debug_output = [](const char* text)
    {
        OutputDebugStringA(text);
    };
}

// Xbox 0x001073E0
u8* allocate_movie_decoder_memory(u32 size)
{
    return static_cast<u8*>(_aligned_malloc(size, 4096));
}

// Xbox 0x00107400
void free_movie_decoder_memory(u8* buffer)
{
    _aligned_free(buffer);
}

void bind_native_movie_heap(RuntimeMovieStartupContext& context)
{
    context.allocate_buffer = allocate_movie_decoder_memory;
    context.free_buffer = free_movie_decoder_memory;
}
#endif
namespace
{
void lookup_runtime_file_failure(i32 status, MovieFileContext& context)
{
    const char* title;
    const char* message;
    lookup_movie_error_message(status, title, message, context.error);
}
struct MovieFileSeekRequest
{
    bool result;
    u32 error;
    std::uintptr_t handle;
    std::int64_t distance;
    u64* output;
    u32 method;
    MovieFileBoundaries* io;
};

// Xbox 0x000EAA60
void execute_movie_file_seek(void* payload)
{
    auto& request = *static_cast<MovieFileSeekRequest*>(payload);
    request.result = request.io->seek(request.handle, request.distance, request.output, request.method);
    request.error = request.io->last_error();
}
}

// Xbox 0x000EA9F0
bool dispatch_movie_file_seek(RuntimeMovieFile& file, u32 low, u32 high, u64* output,
    u32 method, MovieFileContext& context)
{
    MovieFileSeekRequest request;
    request.handle = file.handle;
    request.distance = static_cast<std::int64_t>((static_cast<u64>(high) << 32) | low);
    request.output = output;
    request.method = method;
    request.io = &context.io;
    scene_detail::crt_format_bounded_string(context.worker->diagnostic, 260,
        "SetFilePointerEx: File Handle(0x%08x)", static_cast<u32>(file.handle));
    dispatch_runtime_file_operation(execute_movie_file_seek, &request, *context.worker);
    context.worker->io.set_last_error(request.error);
    return request.result;
}

// Xbox 0x000EADC0
i32 query_runtime_movie_file_position(RuntimeMovieFile& file, u64& output, MovieFileContext& context)
{
    i32 status = 0;
    if (!dispatch_movie_file_seek(file, 0, 0, &output, 0, context))
    {
        status = static_cast<i32>(context.io.last_error());
        if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
        {
            lookup_runtime_file_failure(status, context);
        }
    }
    return status;
}

// Xbox 0x000EAE20
i32 seek_runtime_movie_file(RuntimeMovieFile& file, u32 low, u32 high, MovieFileContext& context)
{
    i32 status = 0;
    if (!dispatch_movie_file_seek(file, low, high, nullptr, 0, context))
    {
        status = static_cast<i32>(context.io.last_error());
        if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
        {
            lookup_runtime_file_failure(status, context);
        }
    }
    return status;
}

// Xbox 0x000EAF30
i32 resize_runtime_movie_file(RuntimeMovieFile& file, u32 low, u32 high, MovieFileContext& context)
{
    i32 aligned;
    i32 status = align_runtime_movie_file_length(file, static_cast<i32>(low), aligned, context);
    const u32 difference = static_cast<u32>(aligned) - low;
    const u64 rounded = static_cast<u64>(difference) + low;
    const u32 rounded_high = high + static_cast<u32>(rounded >> 32);
    u64 previous;
    if (status == 0 && (status = query_runtime_movie_file_position(file, previous, context)) == 0 &&
        (status = seek_runtime_movie_file(file, static_cast<u32>(rounded), rounded_high, context)) == 0)
    {
        if (context.io.end_file(file.handle) ||
            (status = static_cast<i32>(context.io.last_error())) == 0)
        {
            status = seek_runtime_movie_file(file, static_cast<u32>(previous),
                static_cast<u32>(previous >> 32), context);
        }
    }
    if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
    {
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000EAFF0
i32 resize_runtime_movie_file_low(RuntimeMovieFile& file, u32 length, MovieFileContext& context)
{
    const i32 status = resize_runtime_movie_file(file, length, 0, context);
    if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
    {
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000E8F10
void initialize_runtime_movie_utility_drive(MovieFileContext& context, RuntimeMovieFileBootState& state)
{
    u32 status = context.worker->io.attributes("Z:\\index.dat");
    if (status == 0xFFFFFFFF)
    {
        status = context.io.last_error();
    }
    else
    {
        status = (0u - static_cast<u32>((status & 0x10) != 0)) & 0x10B;
    }
    if (status != 0)
    {
        context.io.format_utility();
        RuntimeMovieFile* index;
        open_runtime_movie_file(index, "Z:\\index.dat", 0x40000000, 1, context);
        resize_runtime_movie_file_low(*index, 0x800, context);
        close_runtime_movie_file(*index, context);
    }
    state.utility_pending = 0;
}

// Xbox 0x000E9CE0
void initialize_runtime_movie_files(MovieFileContext& context, RuntimeMovieFileBootState& state)
{
    initialize_runtime_movie_utility_drive(context, state);
    context.io.cache_size(0x20000);
    std::strcpy(state.root, state.disc_root);
    for (i32 index = 0; index < 26; ++index)
    {
        context.sector_sizes[index] = 0;
    }
    context.head.previous = nullptr;
    context.head.next = nullptr;
    context.count = 0;
    context.pending = 0;
    state.first_buffer = new (std::nothrow) u8[0x20000];
    state.second_buffer = new (std::nothrow) u8[0x20000];
    state.first_head.previous = nullptr;
    state.first_head.next = nullptr;
    state.second_head.previous = nullptr;
    state.second_head.next = nullptr;
    state.buffer_size = 0x20000;
}

// Xbox 0x000E9E60
void pump_runtime_movie_files(MovieFileContext& context)
{
    RuntimeMovieFile* file = context.head.next;
    while (file != nullptr)
    {
        RuntimeMovieFile* current = file;
        file = current->next;
        if (current->state == 1)
        {
            poll_runtime_movie_file(*current, context);
        }
        else if (current->state == 2)
        {
            if (current->previous != nullptr)
            {
                current->previous->next = current->next;
            }
            if (current->next != nullptr)
            {
                current->next->previous = current->previous;
            }
            current->previous = nullptr;
            current->next = nullptr;
            context.io.free(current);
            --context.count;
        }
    }
}

// Xbox 0x000E9F10
i32 allocate_runtime_movie_file(RuntimeMovieFile*& output, MovieFileContext& context)
{
    RuntimeMovieFile* file = context.io.allocate();
    if (file != nullptr)
    {
        output = file;
        RuntimeMovieFile* tail = &context.head;
        if (context.head.next != nullptr)
        {
            RuntimeMovieFile** cursor = &context.head.next;
            do
            {
                tail = *cursor;
                cursor = &tail->next;
            }
            while (tail->next != nullptr);
        }
        tail->next = file;
        file->previous = tail;
        file->next = nullptr;
        ++context.count;
        file->state = 0;
        file->handle = static_cast<std::uintptr_t>(-1);
        for (u32& word : file->overlapped)
        {
            word = 0;
        }
        file->offset = 0;
        return 0;
    }
    lookup_runtime_file_failure(0xE, context);
    return 0xE;
}

// Xbox 0x000EAA90
i32 open_runtime_movie_file(RuntimeMovieFile*& output, const char* path,
    u32 access, u32 creation, MovieFileContext& context)
{
    RuntimeMovieFile* file = nullptr;
    i32 status = allocate_runtime_movie_file(file, context);
    if (status == 0)
    {
        std::strcpy(file->path, path);
    }
    if ((access & 0x40000000) != 0 && creation != 0 && (creation < 3 || creation == 4))
    {
        status = ensure_runtime_file_parent_directory(path, *context.worker);
    }
    if (status == 0)
    {
        file->handle = open_runtime_file_handle(path, access, 7, creation, 0x60000080,
            *context.worker);
        if (file->handle != static_cast<std::uintptr_t>(-1))
        {
            output = file;
            return status;
        }
        status = static_cast<i32>(context.io.last_error());
        if (status == 0)
        {
            output = file;
            return status;
        }
    }
    if (file != nullptr)
    {
        file->state = 2;
        if (file->previous != nullptr)
        {
            file->previous->next = file->next;
        }
        if (file->next != nullptr)
        {
            file->next->previous = file->previous;
        }
        file->previous = nullptr;
        file->next = nullptr;
        context.io.free(file);
        --context.count;
        file = nullptr;
    }
    output = file;
    if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
    {
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000EA850
i32 wait_runtime_movie_file(RuntimeMovieFile& file, i32 initial_status, MovieFileContext& context)
{
    context.io.ticks();
    u32 last_notify = 0;
    i32 status = initial_status;
    for (;;)
    {
        const u32 state = file.state;
        if (state == 0)
        {
            return 0;
        }
        if (state == 1)
        {
            status = poll_runtime_movie_file(file, context);
        }
        else if (state == 2)
        {
            if (file.previous != nullptr)
            {
                file.previous->next = file.next;
            }
            if (file.next != nullptr)
            {
                file.next->previous = file.previous;
            }
            file.previous = nullptr;
            file.next = nullptr;
            context.io.free(&file);
            --context.count;
            return 0;
        }
        if (status < 0x3E4 || (status > 0x3E5 && status != 0x4DE))
        {
            return status;
        }
        if (context.notify && context.io.ticks() - last_notify >= context.notify_interval)
        {
            last_notify = context.io.ticks();
            context.notify();
        }
    }
}

// Xbox 0x000EB0E0
i32 read_runtime_movie_file(RuntimeMovieFile& file, void* output, u32 size,
    std::function<void(RuntimeMovieFile&, i32, void*)> completion, void* payload,
    MovieFileContext& context)
{
    i32 status = wait_runtime_movie_file(file, 0, context);
    if (status == 0)
    {
        file.completion = std::move(completion);
        file.payload = payload;
        if (!context.io.read(file, output, size))
        {
            status = static_cast<i32>(context.io.last_error());
            if (status == 0x3E5)
            {
                status = 0;
            }
            else if (status != 0)
            {
                if (!((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
                {
                    lookup_runtime_file_failure(status, context);
                }
                return status;
            }
        }
        ++context.pending;
        file.state = 1;
    }
    if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
    {
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000EB040
i32 query_runtime_movie_sector_size(RuntimeMovieFile& file, i32& output, MovieFileContext& context)
{
    char drive[8];
    std::strncpy(drive, file.path, 3);
    drive[3] = '\0';
    if (static_cast<std::int8_t>(drive[0]) >= 0x61)
    {
        drive[0] = static_cast<char>(static_cast<u8>(drive[0]) - 0x20);
    }
    const i32 index = static_cast<std::int8_t>(drive[0]) - 0x41;
    i32 status = 0;
    if (context.sector_sizes[index] == 0)
    {
        context.sector_sizes[index] = static_cast<i32>(context.io.sector_size(drive));
        if (context.sector_sizes[index] == 0)
        {
            status = static_cast<i32>(context.io.last_error());
        }
    }
    output = context.sector_sizes[index];
    if (status != 0 && !((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
    {
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000EB310
i32 align_runtime_movie_file_length(RuntimeMovieFile& file, i32 length, i32& output,
    MovieFileContext& context)
{
    i32 sector;
    const i32 status = query_runtime_movie_sector_size(file, sector, context);
    if (status == 0)
    {
        const i32 remainder = length % sector;
        if (remainder < 1)
        {
            if (remainder < 0)
            {
                length = static_cast<i32>(static_cast<u32>(length) - static_cast<u32>(remainder));
            }
            output = length;
            return 0;
        }
        output = static_cast<i32>(static_cast<u32>(sector) - static_cast<u32>(remainder) + static_cast<u32>(length));
        return 0;
    }
    if (!((status >= 0x3E4 && status <= 0x3E5) || status == 0x4DE))
    {
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000EABA0
i32 close_runtime_movie_file_handle(RuntimeMovieFile& file, MovieFileContext& context)
{
    if (file.handle == static_cast<std::uintptr_t>(-1))
    {
        return 0;
    }
    if (context.io.close(file.handle))
    {
        file.handle = static_cast<std::uintptr_t>(-1);
        file.state = 0;
        return 0;
    }
    const i32 status = static_cast<i32>(context.io.last_error());
    if (status < 0x3E6)
    {
        if (status > 0x3E3)
        {
            return status;
        }
        if (status == 0)
        {
            return status;
        }
    }
    else if (status == 0x4DE)
    {
        return status;
    }
    lookup_runtime_file_failure(status, context);
    return status;
}

// Xbox 0x000EAC20
i32 close_runtime_movie_file(RuntimeMovieFile& file, MovieFileContext& context)
{
    const i32 status = close_runtime_movie_file_handle(file, context);
    if (status == 0)
    {
        file.state = 2;
        return 0;
    }
    if (status < 0x3E6)
    {
        if (status > 0x3E3)
        {
            return status;
        }
        if (status == 0)
        {
            return status;
        }
    }
    else if (status == 0x4DE)
    {
        return status;
    }
    lookup_runtime_file_failure(status, context);
    return status;
}

// Xbox 0x000EAE80
i32 query_runtime_movie_file_size(RuntimeMovieFile& file, u64& output, MovieFileContext& context)
{
    i32 status = 0;
    if (!context.io.size(file.handle, output))
    {
        status = static_cast<i32>(context.io.last_error());
        if (status < 0x3E6)
        {
            if (status > 0x3E3)
            {
                return status;
            }
            if (status == 0)
            {
                return status;
            }
        }
        else if (status == 0x4DE)
        {
            return status;
        }
        lookup_runtime_file_failure(status, context);
    }
    return status;
}

// Xbox 0x000EAED0
i32 query_runtime_movie_file_length(RuntimeMovieFile& file, u32& output, MovieFileContext& context)
{
    u64 size;
    i32 status = query_runtime_movie_file_size(file, size, context);
    if (status == 0)
    {
        output = static_cast<u32>(size);
        if ((size >> 32) == 0)
        {
            return 0;
        }
        status = 1;
    }
    else
    {
        if (status < 0x3E6)
        {
            if (status > 0x3E3)
            {
                return status;
            }
            if (status == 0)
            {
                return status;
            }
        }
        else if (status == 0x4DE)
        {
            return status;
        }
    }
    lookup_runtime_file_failure(status, context);
    return status;
}

// Xbox 0x000E9FB0
i32 poll_runtime_movie_file(RuntimeMovieFile& file, MovieFileContext& context)
{
    i32 status = 0;
    u32 transferred;
    if (!context.io.completed(file, transferred))
    {
        status = static_cast<i32>(context.io.last_error());
        if (status > 0x3E3 && status < 0x3E6)
        {
            return 0x4DE;
        }
        transferred = 0;
    }
    --context.pending;
    file.state = 0;
    file.offset += transferred;
    if (file.completion)
    {
        file.completion(file, status, file.payload);
        if (file.state == 1)
        {
            return 0x4DE;
        }
    }
    return status;
}

// Xbox 0x00106EF0
char* resolve_runtime_movie_path(const char* name, MoviePathContext& context)
{
    u32 status;
    u32 hash;
    if (context.pal != 0 && context.pal_override == 0)
    {
        char candidate[128];
        format_dialogue_resource_name(candidate, "%spal", name);
        hash = animation_resource_hash(candidate);
        scene_detail::crt_format_string(context.path, "%s%s%08x", context.root, "movie\\", hash);
        status = context.file_attributes(context.path);
        if (status == 0xFFFFFFFF)
        {
            status = context.last_error();
        }
        else
        {
            status = (0u - static_cast<u32>((status & 0x10) != 0)) & 0x10B;
        }
        if (status == 0)
        {
            goto validate;
        }
    }
    hash = animation_resource_hash(name);
    scene_detail::crt_format_string(context.path, "%s%s%08x", context.root, "movie\\", hash);
validate:
    status = context.file_attributes(context.path);
    if (status == 0xFFFFFFFF)
    {
        status = context.last_error();
    }
    else
    {
        status = (0u - static_cast<u32>((status & 0x10) != 0)) & 0x10B;
    }
    if (status != 0)
    {
        const char* title;
        const char* message;
        lookup_movie_error_message(static_cast<i32>(status), title, message, context.error);
        resource_error(context.error.error, static_cast<i32>(status));
    }
    return context.path;
}

// Xbox 0x00106FE0
void complete_runtime_movie_read(RuntimeMovieFile& file, i32 status,
    RuntimeMovieState& movie, RuntimeMovieStartupContext& context)
{
    if (status != 0)
    {
        report_movie_error(status, context.paths.error);
    }
    static_cast<void>(file);
    close_runtime_movie_file(*movie.file, context.files);
    movie.enabled = 1;
    if (movie.logo != 0)
    {
        context.preloaded = movie.buffer;
        context.preloaded_size = movie.buffer_size;
    }
}

// Xbox 0x00107030
i32 open_runtime_movie(const char* name, const char* path, RuntimeMovieState& movie,
    RuntimeMovieStartupContext& context)
{
    movie.logo = static_cast<u32>(resource_names_match(name,
        "aid_movie_ghoulies_attractmode_logoscaleup"));
    if (movie.logo != 0 && context.preloaded != nullptr)
    {
        movie.file = nullptr;
        movie.buffer = context.preloaded;
        movie.buffer_size = context.preloaded_size;
        movie.enabled = 1;
        return 0;
    }
    if ((movie.flags & 0x10) == 0)
    {
        movie.decoder = context.decoder_open(path, movie.open_flags);
        if (movie.decoder == nullptr)
        {
            report_movie_error(0x6E, context.paths.error);
            return 0x6E;
        }
        context.decoder_track(movie.decoder, 0, 0x4000);
        movie.file = nullptr;
        movie.buffer = nullptr;
        movie.enabled = 1;
        return 0;
    }
    i32 status = open_runtime_movie_file(movie.file, path, 0x80000000, 3, context.files);
    if (status != 0)
    {
        report_movie_error(status, context.paths.error);
        return status;
    }
    u32 size;
    status = query_runtime_movie_file_length(*movie.file, size, context.files);
    if (status == 0)
    {
        i32 aligned = static_cast<i32>(size);
        align_runtime_movie_file_length(*movie.file, static_cast<i32>(size), aligned, context.files);
        size = static_cast<u32>(aligned);
        movie.buffer = context.allocate_buffer(size);
        movie.buffer_size = size;
        if (movie.buffer == nullptr)
        {
            return 0xE;
        }
        status = read_runtime_movie_file(*movie.file, movie.buffer, size,
            [&movie, &context](RuntimeMovieFile& file, i32 result, void*)
            {
                complete_runtime_movie_read(file, result, movie, context);
            }, &movie, context.files);
        if (status == 0)
        {
            movie.enabled = 0;
            return 0;
        }
        context.free_buffer(movie.buffer);
    }
    return report_movie_error(status, context.paths.error);
}

// Xbox 0x0015D380
void register_runtime_movie_memory(const std::function<u8*(u32)>& allocate,
    const std::function<void(u8*)>& free, RuntimeMovieMemoryCallbacks& callbacks)
{
    callbacks.allocate = allocate;
    callbacks.free = free;
}

// Xbox 0x001071A0
RuntimeMovieState* allocate_runtime_movie(const char* name, u32 flags,
    RuntimeMovieStartupContext& context)
{
    u32 open_flags = 0x100000;
    if (context.initialized == 0)
    {
        context.head.previous = nullptr;
        context.head.next = nullptr;
        register_runtime_movie_memory(context.allocate_buffer, context.free_buffer, context.memory);
        context.initialized = 1;
    }
    const char* path = resolve_runtime_movie_path(name, context.paths);
    if (path == nullptr)
    {
        return nullptr;
    }
    if ((flags & 8) != 0)
    {
        open_flags = 0x40100000;
    }
    RuntimeMovieState* movie = new (std::nothrow) RuntimeMovieState;
    movie->loop = (flags >> 2) & 1;
    movie->flags = flags;
    movie->play_count = 0;
    movie->pending = 0;
    movie->unknown20 = 0;
    movie->unknown24 = 1;
    movie->decoder = nullptr;
    movie->open_flags = open_flags;
    movie->file = nullptr;
    movie->buffer = nullptr;
    movie->enabled = 0;
    const i32 status = open_runtime_movie(name, path, *movie, context);
    if (status != 0)
    {
        if (movie->file != nullptr)
        {
            close_runtime_movie_file(*movie->file, context.files);
        }
        delete movie;
        return nullptr;
    }
    movie->open_flags = open_flags | 0x4000000;
    movie->unknown28 = 0;
    RuntimeMovieState* tail = &context.head;
    if (context.head.next != nullptr)
    {
        RuntimeMovieState** cursor = &context.head.next;
        do
        {
            tail = *cursor;
            cursor = &tail->next;
        }
        while (tail->next != nullptr);
    }
    tail->next = movie;
    movie->previous = tail;
    movie->next = nullptr;
    if ((flags & 2) != 0)
    {
        ++movie->play_count;
    }
    return movie;
}

// Xbox 0x00107420
void tick_runtime_movie(RuntimeMovieState& movie, RuntimeMovieStartupContext& context)
{
    if (movie.enabled != 0 && movie.unknown28 == 0)
    {
        if (movie.decoder == nullptr)
        {
            movie.decoder = context.decoder_memory_open(movie.buffer, movie.open_flags, movie.buffer_size);
            if (movie.decoder == nullptr)
            {
                movie.unknown28 = 1;
                return;
            }
        }
        const i32 status = context.decoder_wait(movie.decoder);
        if (status == 0 && static_cast<i32>(movie.play_count) > 0)
        {
            context.decoder_do_frame(movie.decoder);
            movie.pending = 1;
            if (movie.loop != 0 || context.decoder_frame_number(movie.decoder) < context.decoder_frame_count(movie.decoder))
            {
                ++movie.unknown20;
                context.decoder_next_frame(movie.decoder);
            }
        }
    }
}

// Xbox 0x00107360
bool copy_runtime_movie_frame(RuntimeMovieState& movie, void* texture, RuntimeMovieStartupContext& context)
{
    if (movie.decoder != nullptr && movie.unknown20 != 0)
    {
        const RuntimeMovieLockedSurface surface = context.lock_texture(texture);
        u32 flags = 0x80000005;
        if (movie.unknown24 != 0)
        {
            flags = 0x80080005;
        }
        const i32 status = context.decoder_copy(movie.decoder, surface.pixels, surface.pitch,
            context.decoder_height(movie.decoder), 0, 0, flags);
        if (movie.unknown24 != 0 && status == 0)
        {
            movie.unknown24 = 0;
        }
        --movie.unknown20;
        return status == 0;
    }
    return false;
}

// Xbox 0x001072B0
u32 release_runtime_movie(RuntimeMovieState& movie, RuntimeMovieStartupContext& context)
{
    if (movie.enabled == 0)
    {
        wait_runtime_movie_file(*movie.file, 0, context.files);
    }
    if (movie.decoder != nullptr)
    {
        context.decoder_close(movie.decoder);
    }
    if (movie.logo == 0 && movie.buffer != nullptr)
    {
        context.free_buffer(movie.buffer);
    }
    if (movie.previous != nullptr)
    {
        movie.previous->next = movie.next;
    }
    if (movie.next != nullptr)
    {
        movie.next->previous = movie.previous;
    }
    movie.previous = nullptr;
    movie.next = nullptr;
    delete &movie;
    return 1;
}

// Xbox 0x00106D50
void load_runtime_movie_event(const char* name, u8 id, i32 loop, i32 file_buffer,
    std::array<RuntimeMovieEventSlot, 4>& slots, RuntimeMovieStartupContext& context)
{
    i32 index = 0;
    do
    {
        if (slots[index].id == 0)
        {
            break;
        }
        ++index;
    }
    while (index < 4);
    u32 flags = 0;
    if (loop != 0)
    {
        flags = 4;
    }
    if (file_buffer != 0)
    {
        flags |= 0x10;
    }
    slots[index].movie = allocate_runtime_movie(name, flags, context);
    slots[index].play_count = reinterpret_cast<u32*>(
        reinterpret_cast<std::uintptr_t>(slots[index].movie) + offsetof(RuntimeMovieState, play_count));
    slots[index].id = id;
    slots[index].state = 1;
    std::strcpy(slots[index].name, name);
}

// Xbox 0x00106DD0
void start_runtime_movie_event(u8 id, i32 mode,
    std::array<RuntimeMovieEventSlot, 4>& slots)
{
    i32 index = 0;
    while (slots[index].id != id || slots[index].state != 1)
    {
        ++index;
        if (index >= 4)
        {
            return;
        }
    }
    if (index < 4)
    {
        if (mode == 2)
        {
            ++*slots[index].play_count;
            slots[index].state = 2;
            return;
        }
        slots[index].state = 3;
    }
}

// Xbox 0x000E96A0
void lookup_movie_error_message(i32 code, const char*& title, const char*& message,
    MovieErrorContext& context)
{
    i32 index = 0;
    if (context.messages[0].code != 0)
    {
        const MovieErrorMessage* row = context.messages;
        i32 current = row->code;
        do
        {
            if (current == code)
            {
                title = context.messages[index].title;
                message = context.messages[index].message;
                return;
            }
            current = row[1].code;
            ++row;
            ++index;
        }
        while (current != 0);
    }
    scene_detail::crt_format_bounded_string(context.fallback, 15, "%d", code);
    title = "Error Code";
    message = context.fallback;
}

// Xbox 0x001074A0
i32 report_movie_error(i32 code, MovieErrorContext& context)
{
    const char* title;
    const char* message;
    lookup_movie_error_message(code, title, message, context);
    resource_error(context.error, code);
    return code;
}

namespace
{

// 0x001A6010 open flags. 0x00100000 is the alpha open. 0x04000000 is added after a
// successful open and selects the memory pointer on the later open.
constexpr unsigned kOpenAlpha = 0x00100000u;
constexpr unsigned kOpenFromMemory = 0x04000000u;
constexpr unsigned kSoundVolume = 0x4000u;

struct MovieClip
{
    // BINK fields 0x00107420 and 0x00107360 read: Height +4, Frames +8, FrameNum +0xC.
    u32 width = 0;
    u32 height = 0;
    u32 frames = 1;
    u32 frame_num = 0;
    RgbaImage image;
    double frame_dur = 1.0 / 30.0;
    double next_time = 0.0;
    bool eof = false;
    bool audio_done = false;
    bool loop = false;
    std::string cue;
    u32 sound_volume = kSoundVolume;
    std::vector<u8> owned_bytes;
    std::function<void(u8*)> release_allocation;
#if defined(GBTG_HAS_FFMPEG)
    AVFormatContext* fmt = nullptr;
    AVCodecContext* video = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    SwsContext* scale = nullptr;
    AVIOContext* io = nullptr;
    u8* io_buf = nullptr;
    int video_index = -1;
    struct Mem
    {
        const u8* data = nullptr;
        size_t size = 0;
        size_t pos = 0;
    } mem;
#endif
};

void destroy_movie_clip(MovieClip* clip)
{
    if (clip->release_allocation)
    {
        auto free = clip->release_allocation;
        clip->~MovieClip();
        free(reinterpret_cast<u8*>(clip));
    }
    else
    {
        delete clip;
    }
}

// 0x3C node from 0x001071A0. Field order matches the offsets the tick reads.
struct MovieNode
{
    MovieNode* prev = nullptr; // +0
    MovieNode* next = nullptr; // +4
    MovieClip* clip = nullptr; // +8
    unsigned open_flags = kOpenAlpha; // +0xC
    unsigned flags = 0;               // +0x10
    int loop = 0;                     // +0x14
    int play = 0;                     // +0x18
    int decoded = 0;                  // +0x1C
    int pending = 0;                  // +0x20
    int first_copy = 1;               // +0x24
    int failed = 0;                   // +0x28
    int file = 0;                     // +0x2C
    int paced = 0;                    // cutscene event drives the displayed frame
    int show_frame = 0;
    int enabled = 0;                  // +0x34, stored after the buffer pointer below
    int logo = 0;                     // +0x38
    std::vector<u8> bytes;            // +0x30 buffer. Owned here; the dump keeps a heap block.
    std::string name;
    std::string path;
};

// One cutscene-event movie. 0x0004C6C0 / 0x0004CC28 opens it at `start`
// and 0x00106DD0 starts Bink playback at `end` (frontend movie mode is 2).
struct MovieCue
{
    float start = 0.f;
    float end = 0.f;
    int slot = 0;
    std::string name;
    bool started = false;
};

// DAT_0050E0B8. DAT_0050E0BC is g_ready. DAT_0050E0B0 is the logo preload, left clear.
MovieNode* g_movies = nullptr;
std::string g_cue_clip;
float g_cue_time = 0.f;
std::vector<MovieCue> g_cues;
bool g_ready = false;
std::filesystem::path g_root;
double g_now = 0.0;

#if defined(GBTG_HAS_FFMPEG)

int mem_read(void* opaque, u8* buf, int buf_size)
{
    auto* mem = static_cast<MovieClip::Mem*>(opaque);
    if (mem->pos >= mem->size)
    {
        return AVERROR_EOF;
    }
    const size_t n = std::min(size_t(buf_size), mem->size - mem->pos);
    std::memcpy(buf, mem->data + mem->pos, n);
    mem->pos += n;
    return int(n);
}

int64_t mem_seek(void* opaque, int64_t offset, int whence)
{
    auto* mem = static_cast<MovieClip::Mem*>(opaque);
    if (whence == AVSEEK_SIZE)
    {
        return int64_t(mem->size);
    }
    int64_t pos = 0;
    if (whence == SEEK_SET)
    {
        pos = offset;
    }
    else if (whence == SEEK_CUR)
    {
        pos = int64_t(mem->pos) + offset;
    }
    else if (whence == SEEK_END)
    {
        pos = int64_t(mem->size) + offset;
    }
    else
    {
        return AVERROR(EINVAL);
    }
    if (pos < 0 || size_t(pos) > mem->size)
    {
        return AVERROR(EINVAL);
    }
    mem->pos = size_t(pos);
    return pos;
}

void clip_close(MovieClip* clip)
{
    if (clip == nullptr)
    {
        return;
    }
    if (!clip->cue.empty())
    {
        pcm_stop_cue(clip->cue.c_str());
    }
    sws_freeContext(clip->scale);
    av_frame_free(&clip->frame);
    av_packet_free(&clip->packet);
    avcodec_free_context(&clip->video);
    if (clip->fmt != nullptr)
    {
        clip->fmt->pb = nullptr;
        avformat_close_input(&clip->fmt);
    }
    if (clip->io != nullptr)
    {
        avio_context_free(&clip->io);
    }
    av_freep(&clip->io_buf);
    destroy_movie_clip(clip);
}

bool clip_seek_start(MovieClip* clip)
{
    if (av_seek_frame(clip->fmt, clip->video_index, 0, AVSEEK_FLAG_BACKWARD) < 0 &&
        av_seek_frame(clip->fmt, clip->video_index, 0, AVSEEK_FLAG_BYTE) < 0)
    {
        return false;
    }
    avcodec_flush_buffers(clip->video);
    clip->eof = false;
    return true;
}

void clip_audio(MovieClip* clip, bool loop)
{
    const int index = av_find_best_stream(clip->fmt, AVMEDIA_TYPE_AUDIO, -1, clip->video_index, nullptr, 0);
    if (index < 0)
    {
        return;
    }
    AVStream* stream = clip->fmt->streams[index];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr)
    {
        return;
    }
    AVCodecContext* audio = avcodec_alloc_context3(codec);
    if (audio == nullptr || avcodec_parameters_to_context(audio, stream->codecpar) < 0 || avcodec_open2(audio, codec, nullptr) < 0)
    {
        avcodec_free_context(&audio);
        return;
    }
    SwrContext* swr = nullptr;
    std::vector<u8> pcm;
    int rate = audio->sample_rate > 0 ? audio->sample_rate : 44100;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    while (packet != nullptr && frame != nullptr && av_read_frame(clip->fmt, packet) >= 0)
    {
        if (packet->stream_index != index)
        {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(audio, packet) < 0)
        {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);
        while (avcodec_receive_frame(audio, frame) >= 0)
        {
            if (swr == nullptr)
            {
                AVChannelLayout out_layout;
                av_channel_layout_default(&out_layout, 2);
                AVChannelLayout in_layout = frame->ch_layout;
                if (in_layout.nb_channels <= 0)
                {
                    av_channel_layout_default(&in_layout, 1);
                }
                if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16, rate, &in_layout, AVSampleFormat(frame->format),
                                        frame->sample_rate > 0 ? frame->sample_rate : rate, 0, nullptr) < 0 ||
                    swr_init(swr) < 0)
                {
                    swr_free(&swr);
                    av_channel_layout_uninit(&out_layout);
                    av_frame_unref(frame);
                    break;
                }
                av_channel_layout_uninit(&out_layout);
            }
            const int out_samples = swr_get_out_samples(swr, frame->nb_samples);
            if (out_samples <= 0)
            {
                av_frame_unref(frame);
                continue;
            }
            const size_t at = pcm.size();
            pcm.resize(at + size_t(out_samples) * 4);
            u8* dst = pcm.data() + at;
            const int got = swr_convert(swr, &dst, out_samples, frame->extended_data, frame->nb_samples);
            if (got > 0)
            {
                pcm.resize(at + size_t(got) * 4);
            }
            else
            {
                pcm.resize(at);
            }
            av_frame_unref(frame);
        }
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr);
    avcodec_free_context(&audio);
    clip_seek_start(clip);
    if (!pcm.empty())
    {
        const float volume = float(clip->sound_volume) / 32768.f;
        pcm_play_pcm(clip->cue.c_str(), pcm, rate, 2, loop, volume);
    }
}

bool clip_decode(MovieClip* clip)
{
    while (av_read_frame(clip->fmt, clip->packet) >= 0)
    {
        if (clip->packet->stream_index != clip->video_index)
        {
            av_packet_unref(clip->packet);
            continue;
        }
        const int sent = avcodec_send_packet(clip->video, clip->packet);
        av_packet_unref(clip->packet);
        if (sent < 0)
        {
            continue;
        }
        const int got = avcodec_receive_frame(clip->video, clip->frame);
        if (got < 0)
        {
            continue;
        }
        const int w = clip->frame->width;
        const int h = clip->frame->height;
        if (w <= 0 || h <= 0)
        {
            av_frame_unref(clip->frame);
            return false;
        }
        clip->scale = sws_getCachedContext(clip->scale, w, h, AVPixelFormat(clip->frame->format), w, h, AV_PIX_FMT_BGRA,
                                           SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (clip->scale == nullptr)
        {
            av_frame_unref(clip->frame);
            return false;
        }
        const int pitch = w * 4;
        const size_t need = size_t(pitch) * size_t(h);
        // libswscale writes one 32-byte vector past the last row when the width
        // is not a multiple of 16. 439- and 440-wide Bink pages do that. The extra
        // bytes stay in this allocation, then the image is trimmed back to the frame.
        clip->image.bgra.resize(need + 64);
        u8* dst = clip->image.bgra.data();
        const int dst_stride[4] = {pitch, 0, 0, 0};
        sws_scale(clip->scale, clip->frame->data, clip->frame->linesize, 0, h, &dst, dst_stride);
        clip->image.width = w;
        clip->image.height = h;
        clip->image.bgra.resize(need);
        // yuv420p has no plane. swscale leaves alpha at 0, and an alpha-tested page discards the picture.
        const int fmt = clip->frame->format;
        const bool has_alpha = fmt == AV_PIX_FMT_YUVA420P || fmt == AV_PIX_FMT_YUVA422P || fmt == AV_PIX_FMT_YUVA444P;
        if (!has_alpha)
        {
            for (size_t i = 3; i < clip->image.bgra.size(); i += 4)
            {
                clip->image.bgra[i] = 255;
            }
        }
        clip->width = u32(w);
        clip->height = u32(h);
        clip->image.serial += 1;
        av_frame_unref(clip->frame);
        return true;
    }
    clip->eof = true;
    return false;
}

MovieClip* clip_open(const void* src, unsigned flags, bool loop, const std::string& cue,
    const std::vector<u8>* bytes, const RuntimeMovieMemoryCallbacks* memory = nullptr)
{
    if (src == nullptr && bytes == nullptr)
    {
        return nullptr;
    }
    av_log_set_level(AV_LOG_ERROR);
    const AVInputFormat* bink = av_find_input_format("bink");
    AVFormatContext* fmt = nullptr;
    MovieClip* clip;
    if (memory != nullptr)
    {
        u8* allocation = memory->allocate(sizeof(MovieClip));
        if (allocation == nullptr)
        {
            return nullptr;
        }
        clip = new (allocation) MovieClip();
        clip->release_allocation = memory->free;
    }
    else
    {
        clip = new MovieClip();
    }
    clip->cue = cue;
    if ((flags & kOpenFromMemory) != 0)
    {
        if (bytes == nullptr || bytes->size() < 4)
        {
            destroy_movie_clip(clip);
            return nullptr;
        }
        clip->mem.data = bytes->data();
        clip->mem.size = bytes->size();
        clip->io_buf = static_cast<u8*>(av_malloc(4096));
        clip->io = avio_alloc_context(clip->io_buf, 4096, 0, &clip->mem, mem_read, nullptr, mem_seek);
        if (clip->io == nullptr)
        {
            clip_close(clip);
            return nullptr;
        }
        fmt = avformat_alloc_context();
        fmt->pb = clip->io;
        fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
        if (avformat_open_input(&fmt, nullptr, bink, nullptr) < 0)
        {
            clip->fmt = fmt;
            clip_close(clip);
            return nullptr;
        }
    }
    else
    {
        const char* path = static_cast<const char*>(src);
        if (avformat_open_input(&fmt, path, bink, nullptr) < 0)
        {
            destroy_movie_clip(clip);
            return nullptr;
        }
    }
    clip->fmt = fmt;
    if (avformat_find_stream_info(fmt, nullptr) < 0)
    {
        clip_close(clip);
        return nullptr;
    }
    const int index = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (index < 0)
    {
        clip_close(clip);
        return nullptr;
    }
    AVStream* stream = fmt->streams[index];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr)
    {
        clip_close(clip);
        return nullptr;
    }
    clip->video = avcodec_alloc_context3(codec);
    if (clip->video == nullptr || avcodec_parameters_to_context(clip->video, stream->codecpar) < 0 ||
        avcodec_open2(clip->video, codec, nullptr) < 0)
    {
        clip_close(clip);
        return nullptr;
    }
    clip->video_index = index;
    clip->packet = av_packet_alloc();
    clip->frame = av_frame_alloc();
    clip->width = u32(stream->codecpar->width);
    clip->height = u32(stream->codecpar->height);
    if (stream->nb_frames > 0)
    {
        clip->frames = u32(stream->nb_frames);
    }
    const double fps = av_q2d(stream->avg_frame_rate);
    if (fps > 1.0 && fps < 120.0)
    {
        clip->frame_dur = 1.0 / fps;
    }
    if (clip->frames <= 1 && fmt->duration > 0)
    {
        const double seconds = double(fmt->duration) / double(AV_TIME_BASE);
        const u32 count = u32(seconds / clip->frame_dur + 0.5);
        if (count > 1)
        {
            clip->frames = count;
        }
    }
    clip->loop = loop;
    std::cout << "movie " << cue << " " << clip->width << "x" << clip->height << " frames " << clip->frames << "\n";
    return clip;
}

// Xbox 0x001A7870. Zero means a frame is due.
int bink_wait(MovieClip* clip)
{
    if (g_now + 1.0e-4 < clip->next_time)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x001A7D40.
void bink_do_frame(MovieClip* clip)
{
    // 0x001A7B70 arms track 0. Audio starts with the first displayed frame so a clip
    // that is open but not playing stays quiet.
    if (!clip->audio_done)
    {
        clip->audio_done = true;
        clip_audio(clip, clip->loop);
    }
    if (!clip_decode(clip) && clip->eof && clip->loop)
    {
        clip_seek_start(clip);
        clip_decode(clip);
    }
    clip->next_time = g_now + clip->frame_dur;
}

// Xbox 0x001A8350.
void bink_next_frame(MovieClip* clip)
{
    if (clip->frame_num + 1 >= clip->frames)
    {
        if (clip->loop)
        {
            clip_seek_start(clip);
            clip->frame_num = 0;
        }
        return;
    }
    clip->frame_num += 1;
}

// Cutscene events name a clock. Show that frame instead of running ahead of the page.
void pace_to_frame(MovieClip* clip, int want)
{
    if (!clip->audio_done)
    {
        clip->audio_done = true;
        clip_audio(clip, false);
    }
    const int last = clip->frames > 0 ? int(clip->frames) - 1 : 0;
    if (want < 0)
    {
        want = 0;
    }
    if (want > last)
    {
        want = last;
    }
    // A later event for the same clip starts again at its own end. 0x00106DD0
    // opens a new node; this seek is that restart on the one node we keep.
    if (want < int(clip->frame_num))
    {
        if (!clip_seek_start(clip))
        {
            return;
        }
        clip->frame_num = 0;
        clip->image.bgra.clear();
    }
    if (clip->image.bgra.empty())
    {
        if (!clip_decode(clip))
        {
            return;
        }
        clip->frame_num = 0;
    }
    while (int(clip->frame_num) < want)
    {
        if (!clip_decode(clip))
        {
            break;
        }
        clip->frame_num += 1;
    }
}

#else

void clip_close(MovieClip* clip)
{
    destroy_movie_clip(clip);
}

MovieClip* clip_open(const void* src, unsigned flags, bool loop, const std::string& cue,
    const std::vector<u8>* bytes, const RuntimeMovieMemoryCallbacks* memory = nullptr)
{
    (void)memory;
    (void)src;
    (void)flags;
    (void)loop;
    (void)cue;
    (void)bytes;
    return nullptr;
}

int bink_wait(MovieClip* clip)
{
    (void)clip;
    return 1;
}

void bink_do_frame(MovieClip* clip)
{
    (void)clip;
}

void bink_next_frame(MovieClip* clip)
{
    (void)clip;
}

void pace_to_frame(MovieClip* clip, int want)
{
    (void)clip;
    (void)want;
}

#endif

bool load_file(const std::filesystem::path& path, std::vector<u8>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff len = in.tellg();
    if (len <= 0)
    {
        return false;
    }
    in.seekg(0);
    out.resize(size_t(len));
    in.read(reinterpret_cast<char*>(out.data()), len);
    return bool(in);
}

// Xbox 0x000358B0, skipping the "aid_" the hash function starts past. aid_hash is that fold.
// Xbox 0x00106EF0. "%s%s%08x" with the disc root, "movie\\", and the hash.
// DAT_005096E0 / DAT_005096E4 would try a "pal" suffix first. Both stay clear here.
std::string movie_path(const char* name)
{
    if (name == nullptr || name[0] == 0 || g_root.empty())
    {
        return {};
    }
    return (g_root / "movie" / aid_hash_hex(aid_hash(name))).string();
}

MovieNode* find_name(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (MovieNode* node = g_movies; node != nullptr; node = node->next)
    {
        if (node->name == name)
        {
            return node;
        }
    }
    return nullptr;
}

void unlink_node(MovieNode* node)
{
    if (node->prev != nullptr)
    {
        node->prev->next = node->next;
    }
    else if (g_movies == node)
    {
        g_movies = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = node->prev;
    }
    node->prev = nullptr;
    node->next = nullptr;
}

void free_node(MovieNode* node)
{
    if (node == nullptr)
    {
        return;
    }
    // 0x001072B0. 0x001A84F0 closes the clip. The heap free around it is 0x000663E0.
    clip_close(node->clip);
    node->clip = nullptr;
    unlink_node(node);
    delete node;
}

// Xbox 0x00107030. ECX is the path, EDX is the node. Returns 0 on success.
int movie_open(MovieNode* node)
{
    // 0x000357D0 against aid_movie_ghoulies_attractmode_logoscaleup. DAT_0050E0B0 is the
    // preload pointer; it stays null, so a matching name still takes the file path.
    node->logo = node->name == "aid_movie_ghoulies_attractmode_logoscaleup" ? 1 : 0;
    if ((node->flags & 0x10) == 0)
    {
        // 0x001A6010(path, flags). 0x001A7B70(clip, 0, 0x4000) on success.
        node->clip = clip_open(node->path.c_str(), node->open_flags, node->loop != 0, node->name, nullptr);
        if (node->clip == nullptr)
        {
            // 0x000E96A0 / 0x00032700 report 0x6E. Those calls are not translated.
            std::cout << "movie missing " << node->name << "\n";
            return 0x6e;
        }
        node->file = 0;
        node->enabled = 1;
        return 0;
    }
    // 0x000EAA90, 0x000EAED0, 0x000EB310, 0x000EB0E0 and the callback at 0x00106FE0 read the
    // file and then set +0x34. Those functions are not translated. This read is that file IO.
    // The callback's store of 1 at +0x34 is the success state the tick tests.
    if (!load_file(node->path, node->bytes))
    {
        std::cout << "movie missing " << node->name << "\n";
        return 2;
    }
    node->enabled = 1;
    return 0;
}

// Xbox 0x00107420. ESI is the node.
void movie_tick(MovieNode* node)
{
    if (node->enabled == 0 || node->failed != 0)
    {
        return;
    }
    if (node->clip == nullptr)
    {
        // 0x001A6010(buffer, flags). flags already include 0x04000000.
        node->clip = clip_open(node->bytes.data(), node->open_flags, node->loop != 0, node->name, &node->bytes);
        if (node->clip == nullptr)
        {
            node->failed = 1;
            return;
        }
    }
    if (node->paced != 0)
    {
        pace_to_frame(node->clip, node->show_frame);
        node->decoded = 1;
        node->pending = 1;
        return;
    }
    if (bink_wait(node->clip) != 0)
    {
        return;
    }
    if (node->play <= 0)
    {
        return;
    }
    bink_do_frame(node->clip);
    node->decoded = 1;
    if (node->loop != 0 || node->clip->frame_num < node->clip->frames)
    {
        node->pending += 1;
        bink_next_frame(node->clip);
    }
}

} // namespace

void bind_existing_movie_decoder(RuntimeMovieStartupContext& context)
{
    context.decoder_open = [&context](const char* path, u32 flags) -> void*
    {
        return clip_open(path, flags, false, path, nullptr, &context.memory);
    };
    context.decoder_track = [](void* decoder, u32 track, u32 volume)
    {
        if (track == 0)
        {
            static_cast<MovieClip*>(decoder)->sound_volume = volume;
        }
    };
    context.decoder_memory_open = [&context](const u8* source, u32 flags, u32 size) -> void*
    {
        std::vector<u8> bytes(source, source + size);
        MovieClip* clip = clip_open(source, flags, false, "runtime_movie", &bytes, &context.memory);
        if (clip != nullptr)
        {
            clip->owned_bytes = std::move(bytes);
        }
        return clip;
    };
    context.decoder_wait = [](void* decoder)
    {
        return bink_wait(static_cast<MovieClip*>(decoder));
    };
    context.decoder_do_frame = [](void* decoder)
    {
        bink_do_frame(static_cast<MovieClip*>(decoder));
    };
    context.decoder_next_frame = [](void* decoder)
    {
        bink_next_frame(static_cast<MovieClip*>(decoder));
    };
    context.decoder_frame_number = [](void* decoder)
    {
        return static_cast<MovieClip*>(decoder)->frame_num;
    };
    context.decoder_frame_count = [](void* decoder)
    {
        return static_cast<MovieClip*>(decoder)->frames;
    };
    context.decoder_close = [](void* decoder)
    {
        clip_close(static_cast<MovieClip*>(decoder));
    };
    context.decoder_height = [](void* decoder)
    {
        return static_cast<MovieClip*>(decoder)->height;
    };
    context.decoder_copy = [](void* decoder, u8* destination, i32 pitch, u32 height,
        u32 x, u32 y, u32 flags)
    {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(flags);
        const RgbaImage& image = static_cast<MovieClip*>(decoder)->image;
        const size_t row_bytes = static_cast<size_t>(image.width) * 4;
        if (destination == nullptr || image.bgra.empty() || pitch < 0 ||
            static_cast<size_t>(pitch) < row_bytes || height < static_cast<u32>(image.height))
        {
            return 1;
        }
        for (i32 row = 0; row < image.height; ++row)
        {
            std::memcpy(destination + static_cast<size_t>(row) * static_cast<size_t>(pitch),
                image.bgra.data() + static_cast<size_t>(row) * row_bytes, row_bytes);
        }
        return 0;
    };
}

const RgbaImage* runtime_movie_image(const RuntimeMovieState& movie)
{
    if (movie.decoder == nullptr)
    {
        return nullptr;
    }
    return &static_cast<MovieClip*>(movie.decoder)->image;
}

void free_named(const std::string& name)
{
    MovieNode* node = find_name(name.c_str());
    if (node != nullptr)
    {
        free_node(node);
    }
}

void paint_movie(RgbaImage& dst, const RgbaImage& src)
{
    if (dst.width <= 0 || dst.height <= 0 || dst.bgra.empty() || src.bgra.empty())
    {
        return;
    }
    if (dst.movie_base.empty())
    {
        dst.movie_base = dst.bgra;
    }
    // Xbox 0x00107360. BinkCopyToBuffer overwrites the locked texture with the
    // frame, alpha included. The surface's own blend state shows what is
    // underneath; the texture's original pixels are not composited back in.
    if (dst.width == src.width && dst.height == src.height && dst.bgra.size() == src.bgra.size())
    {
        std::memcpy(dst.bgra.data(), src.bgra.data(), dst.bgra.size());
    }
    else
    {
        for (int y = 0; y < dst.height; ++y)
        {
            const int sy = src.height > 0 ? y * src.height / dst.height : 0;
            u8* row = dst.bgra.data() + size_t(y) * size_t(dst.width) * 4;
            const u8* srow = src.bgra.data() + size_t(sy) * size_t(src.width) * 4;
            for (int x = 0; x < dst.width; ++x)
            {
                const int sx = src.width > 0 ? x * src.width / dst.width : 0;
                std::memcpy(row + size_t(x) * 4, srow + size_t(sx) * 4, 4);
            }
        }
    }
    dst.serial += 1;
}

void restore_movie(RgbaImage& dst)
{
    if (dst.movie_base.empty())
    {
        return;
    }
    if (dst.bgra.size() == dst.movie_base.size())
    {
        std::memcpy(dst.bgra.data(), dst.movie_base.data(), dst.bgra.size());
    }
    dst.movie_base.clear();
    dst.movie_seen = 0;
    dst.serial += 1;
}

u32 rd_u32(std::span<const u8> b, size_t off)
{
    u32 v = 0;
    if (off + 4 <= b.size())
    {
        std::memcpy(&v, b.data() + off, 4);
    }
    return v;
}

float rd_f32(std::span<const u8> b, size_t off)
{
    float v = 0.f;
    if (off + 4 <= b.size())
    {
        std::memcpy(&v, b.data() + off, 4);
    }
    return v;
}

std::string rd_str(std::span<const u8> b, size_t off, size_t room)
{
    if (off >= b.size())
    {
        return {};
    }
    room = std::min(room, b.size() - off);
    size_t n = 0;
    while (n < room && b[off + n] != 0)
    {
        ++n;
    }
    return std::string(reinterpret_cast<const char*>(b.data() + off), n);
}

void add_cue(std::span<const u8> rec, int slot_off, size_t name_off)
{
    if (rec.size() < size_t(slot_off) + 4 || name_off >= rec.size())
    {
        return;
    }
    MovieCue cue;
    cue.start = rd_f32(rec, 4);
    cue.end = rd_f32(rec, 8);
    cue.slot = int(rd_u32(rec, size_t(slot_off)) & 0xff);
    cue.name = rd_str(rec, name_off, 120);
    if (cue.slot == 0 || cue.name.rfind("aid_movie_", 0) != 0 || !(cue.end > cue.start))
    {
        return;
    }
    g_cues.push_back(std::move(cue));
}

void collect_cues(const Game& g, const Cutscene& clip)
{
    g_cues.clear();
    g_cue_clip = clip.name;
    if (g.scene_bundle == nullptr)
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
        // grabbedbytheghoulies is a prefix of grabbedbytheghouliestwo. The event
        // file continues with '_' (the _camera suffix).
        if (e.name.size() > prefix.size() && e.name[prefix.size()] != '_')
        {
            continue;
        }
        size_t pos = 0;
        while (pos + 16 <= e.cpu.size())
        {
            const u32 size = rd_u32(e.cpu, pos);
            if (size < 16 || pos + size > e.cpu.size())
            {
                break;
            }
            const auto rec = e.cpu.subspan(pos, size);
            const u32 type = rd_u32(rec, 0x0c) & 0xffff;
            // 0x0004C850 case 0x0D / 0x1F. One name, destination byte at +0x90.
            if ((type == 0x0e || type == 0x20) && size > 0x90)
            {
                add_cue(rec, 0x90, 0x10);
            }
            // Case 0x1E. Up to five names, destination byte at +0x290. The first name is the one opened.
            else if (type == 0x1f && size > 0x290)
            {
                add_cue(rec, 0x290, 0x10);
            }
            pos += size;
        }
    }
}

void movie_set_root(const std::filesystem::path& game_dir)
{
    g_root = game_dir;
}

void movie_boot_probe()
{
    // 0x0014B280("d:\\movie\\0de65e94"). -1 calls GetLastError and WinMain continues.
    const std::filesystem::path path = g_root / "movie" / "0de65e94";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        std::cout << "movie probe " << path.string() << "\n";
    }
}

void movie_reset()
{
    while (g_movies != nullptr)
    {
        free_node(g_movies);
    }
    g_cues.clear();
    g_cue_clip.clear();
    g_cue_time = 0.f;
    g_now = 0.0;
}

void movie_alloc(const char* name, unsigned flags)
{
    if (!g_ready)
    {
        // 0x0015D380 push-buffer size, with 0x001073E0 / 0x00107400. Not translated.
        g_movies = nullptr;
        g_ready = true;
    }
    if (find_name(name) != nullptr)
    {
        return;
    }
    const std::string path = movie_path(name);
    if (path.empty())
    {
        return;
    }
    unsigned open_flags = kOpenAlpha;
    if ((flags & 8) != 0)
    {
        open_flags = 0x40100000u;
    }
    MovieNode* node = new MovieNode();
    node->loop = int((flags >> 2) & 1u);
    node->flags = flags;
    node->play = 0;
    node->decoded = 0;
    node->pending = 0;
    node->first_copy = 1;
    node->clip = nullptr;
    node->open_flags = open_flags;
    node->file = 0;
    node->enabled = 0;
    node->failed = 0;
    node->name = name == nullptr ? std::string() : name;
    node->path = path;
    if (movie_open(node) != 0)
    {
        delete node;
        return;
    }
    node->open_flags = open_flags | kOpenFromMemory;
    node->failed = 0;
    node->prev = nullptr;
    node->next = nullptr;
    if (g_movies == nullptr)
    {
        g_movies = node;
    }
    else
    {
        MovieNode* tail = g_movies;
        while (tail->next != nullptr)
        {
            tail = tail->next;
        }
        tail->next = node;
        node->prev = tail;
    }
    if ((flags & 2) != 0)
    {
        node->play += 1;
    }
}

void movie_open_gameselect()
{
    // 0x004290C8 stride 0x124, then 0x0042941C stride 0x10C. Both use flags 0x14.
    const char* empty_slots[] = {
        "aid_movie_ghoulies_gameselect_emptyslots_one",
        "aid_movie_ghoulies_gameselect_emptyslots_two",
        "aid_movie_ghoulies_gameselect_emptyslots_three",
    };
    const char* chapters[] = {
        "aid_movie_ghoulies_gameselect_chapter1",
        "aid_movie_ghoulies_gameselect_chapter2",
        "aid_movie_ghoulies_gameselect_chapter3",
        "aid_movie_ghoulies_gameselect_chapter4",
        "aid_movie_ghoulies_gameselect_chapter5",
    };
    for (const char* name : empty_slots)
    {
        movie_alloc(name, 0x14);
    }
    for (const char* name : chapters)
    {
        movie_alloc(name, 0x14);
    }
}

void movie_open_attract()
{
    movie_alloc("aid_movie_ghoulies_attractmode_logoscaleup", 6);
}

void movie_open_contents()
{
    const char* names[] = {
        "aid_movie_ghoulies_gameselect_house",
        "aid_movie_ghoulies_gameselect_zombieclap",
        "aid_movie_ghoulies_gameselect_skellyscratchhead",
    };
    for (const char* name : names)
    {
        movie_alloc(name, 6);
    }
}

void movie_advance_time(float dt)
{
    if (dt > 0.f)
    {
        g_now += double(dt);
    }
}

void movie_present()
{
    for (MovieNode* node = g_movies; node != nullptr; node = node->next)
    {
        movie_tick(node);
    }
}

void movie_note_visible(const char* name)
{
    MovieNode* node = find_name(name);
    if (node != nullptr && node->play < 1)
    {
        node->play += 1;
    }
}

// Xbox 0x000DE9E0 / 0x000DACE0: release the widget's playback reference.
void movie_release_visible(const char* name)
{
    MovieNode* node = find_name(name);
    if (node != nullptr && node->play > 0)
    {
        node->play -= 1;
    }
}

bool movie_is_open(const char* name)
{
    const MovieNode* node = find_name(name);
    return node != nullptr && node->clip != nullptr;
}

// Xbox 0x00107360. The host copy implements LockRect / BinkCopyToBuffer.
// Failed or absent copies leave +0x24 set and return false to the widget.
bool movie_copy_frame(const char* name, RgbaImage& target)
{
    MovieNode* node = find_name(name);
    if (node != nullptr && node->clip != nullptr && node->pending != 0)
    {
        const bool copied = !node->clip->image.bgra.empty();
        if (copied)
        {
            gfx_release_sprite(target);
            target = node->clip->image;
            if (node->first_copy != 0)
            {
                node->first_copy = 0;
            }
        }
        node->pending -= 1;
        return copied;
    }
    return false;
}

void movie_sync_cutscene(const Game& g)
{
    if (!g.cutscenes.playing || g.cutscenes.index >= g.cutscenes.items.size())
    {
        for (MovieCue& cue : g_cues)
        {
            if (cue.started)
            {
                free_named(cue.name);
                cue.started = false;
            }
        }
        g_cues.clear();
        g_cue_clip.clear();
        g_cue_time = 0.f;
        return;
    }
    const Cutscene& clip = g.cutscenes.items[g.cutscenes.index];
    if (clip.name != g_cue_clip)
    {
        for (MovieCue& cue : g_cues)
        {
            if (cue.started)
            {
                free_named(cue.name);
            }
        }
        collect_cues(g, clip);
    }
    const float time = g.cutscenes.time;
    g_cue_time = time;
    for (MovieCue& cue : g_cues)
    {
        // 0x0004CC51 opens at the window start with flags 0x10. Play stays 0, so the tick
        // does not decode yet. 0x00106DD0 increments play when the window ends.
        if (time >= cue.start && !cue.started)
        {
            movie_alloc(cue.name.c_str(), 0x10);
            cue.started = true;
        }
    }
    for (const MovieCue& cue : g_cues)
    {
        if (time < cue.end)
        {
            continue;
        }
        MovieNode* node = find_name(cue.name.c_str());
        if (node == nullptr)
        {
            continue;
        }
        double step = 1.0 / 30.0;
        if (node->clip != nullptr && node->clip->frame_dur > 0.0)
        {
            step = node->clip->frame_dur;
        }
        int show = 0;
        if (time > cue.end)
        {
            show = int((double(time) - double(cue.end)) / step);
        }
        node->paced = 1;
        node->show_frame = show;
    }
}

void movie_apply(Mesh& mesh)
{
    for (RgbaImage& tex : mesh.textures)
    {
        if (tex.movie_slot == 0)
        {
            continue;
        }
        const MovieCue* live = nullptr;
        for (const MovieCue& cue : g_cues)
        {
            // The latest window whose end has passed owns the texture. Earlier
            // plays hold their last frame until that next one begins.
            if (cue.slot == int(tex.movie_slot) && g_cue_time >= cue.end)
            {
                live = &cue;
            }
        }
        // Slots 2/3/6/7 sit under stills 11/12/13/14 on the same page quad.
        // The still is later in the mesh, so the playing movie has to draw after it.
        const bool covered = tex.movie_slot == 2 || tex.movie_slot == 3 || tex.movie_slot == 6 || tex.movie_slot == 7;
        if (live == nullptr)
        {
            tex.movie_front = false;
            restore_movie(tex);
            continue;
        }
        bool running = true;
        if (const MovieNode* node = find_name(live->name.c_str()))
        {
            if (node->clip != nullptr && node->clip->frames > 0 && node->clip->frame_dur > 0.0)
            {
                const double span = double(node->clip->frames) * node->clip->frame_dur;
                running = double(g_cue_time) - double(live->end) < span;
            }
        }
        tex.movie_front = covered && running;
        const RgbaImage* frame = movie_image(live->name.c_str());
        if (frame == nullptr || frame->serial == tex.movie_seen)
        {
            continue;
        }
        paint_movie(tex, *frame);
        tex.movie_seen = frame->serial;
    }
}

const RgbaImage* movie_image(const char* name)
{
    MovieNode* node = find_name(name);
    if (node == nullptr || node->clip == nullptr || node->clip->image.bgra.empty())
    {
        return nullptr;
    }
    return &node->clip->image;
}

} // namespace gbtg
