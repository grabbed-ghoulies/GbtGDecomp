#include "gbtg/movie.h"
#include "movie_errors.h"
#include "movie_startup.h"
#include "movie_files.h"
#include "../runtime/crt_format.h"
#include "../runtime/file_worker.h"
#include "../saves/save_root.h"
#include "../frontend/dialogue_resources.h"
#include "gbtg/anim.h"

#include "gbtg/bundle.h"
#include "gbtg/engine_internal.h"
#include "gbtg/hash.h"
#include "gbtg/log.h"
#include "gbtg/gfx.h"
#include "gbtg/resource.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
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
#else
#include "gbtg/host_heap.h"
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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
    auto written = std::make_shared<std::unordered_map<RuntimeMovieFile*, std::vector<u8>>>();
    files.allocate = []()
    {
        return new (std::nothrow) RuntimeMovieFile;
    };
    files.free = [requests, written](RuntimeMovieFile* file)
    {
        requests->erase(file);
        written->erase(file);
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
    // WriteFile, overlapped. 0x0010EB60 frees its buffer as soon as the write is issued (the Xbox finishes it
    // first); the host copies the bytes so they outlive that free.
    files.write = [requests, written](RuntimeMovieFile& file, const void* data, u32 size)
    {
        OVERLAPPED& request = (*requests)[&file];
        request = {};
        request.Offset = file.offset;
        auto& bytes = (*written)[&file];
        bytes.assign(static_cast<const u8*>(data), static_cast<const u8*>(data) + size);
        return WriteFile(reinterpret_cast<HANDLE>(file.handle), bytes.data(), size, nullptr, &request) != FALSE;
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
        if (GetDiskFreeSpaceA(xbox_host_path(drive).c_str(), &sectors_per_cluster, &bytes_per_sector, &free_clusters, &clusters) == FALSE)
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
    files.file_time = [](std::uintptr_t handle, u64& output)
    {
        FILETIME written;
        const BOOL result = GetFileTime(reinterpret_cast<HANDLE>(handle), nullptr, nullptr, &written);
        if (result != FALSE)
        {
            output = (u64(written.dwHighDateTime) << 32) | written.dwLowDateTime;
        }
        return result != FALSE;
    };
    files.set_file_time = [](std::uintptr_t handle, u64 time)
    {
        FILETIME written{DWORD(time), DWORD(time >> 32)};
        return SetFileTime(reinterpret_cast<HANDLE>(handle), nullptr, nullptr, &written) != FALSE;
    };
    files.cancel = [](std::uintptr_t handle)
    {
        return CancelIo(reinterpret_cast<HANDLE>(handle)) != FALSE;
    };
    files.delete_file = [](const char* path)
    {
        return DeleteFileA(xbox_host_path(path).c_str()) != FALSE;
    };
    worker.create_file = [](const char* path, u32 access, u32 share, void* security,
        u32 creation, u32 attributes, std::uintptr_t template_file)
    {
        return reinterpret_cast<std::uintptr_t>(CreateFileA(xbox_host_path(path).c_str(), access, share,
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
        return static_cast<u32>(GetFileAttributesA(xbox_host_path(path).c_str()));
    };
    worker.create_directory = [](const char* path)
    {
        return CreateDirectoryA(xbox_host_path(path).c_str(), nullptr) != FALSE;
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
#else
namespace
{
// GetLastError for the POSIX file boundaries. Like Win32, a call sets it only on failure (and CreateFile on
// an existing file it opens or replaces).
thread_local u32 g_movie_last_error = 0;
constexpr std::uintptr_t kInvalidHandle = ~std::uintptr_t(0);

u32 win32_error_from_errno(int error)
{
    switch (error)
    {
    case ENOENT:
        return 2; // ERROR_FILE_NOT_FOUND
    case ENOTDIR:
        return 3; // ERROR_PATH_NOT_FOUND
    case EACCES:
    case EPERM:
    case EROFS:
        return 5; // ERROR_ACCESS_DENIED
    case EBADF:
        return 6; // ERROR_INVALID_HANDLE
    case EEXIST:
        return 80; // ERROR_FILE_EXISTS
    case ENOSPC:
        return 112; // ERROR_DISK_FULL
    case EINVAL:
        return 87; // ERROR_INVALID_PARAMETER
    default:
        return 31; // ERROR_GEN_FAILURE
    }
}

bool fail_with_errno()
{
    g_movie_last_error = win32_error_from_errno(errno);
    return false;
}

int movie_fd(std::uintptr_t handle)
{
    return static_cast<int>(handle);
}

// FILETIME: 100 ns intervals since 1601-01-01.
u64 filetime_from_timespec(const timespec& time)
{
    constexpr u64 kUnixEpochSeconds = 11644473600ull;
    return (u64(time.tv_sec) + kUnixEpochSeconds) * 10000000ull + u64(time.tv_nsec) / 100ull;
}

timespec timespec_from_filetime(u64 time)
{
    constexpr u64 kUnixEpochSeconds = 11644473600ull;
    timespec result{};
    result.tv_sec = static_cast<time_t>(time / 10000000ull - kUnixEpochSeconds);
    result.tv_nsec = static_cast<long>((time % 10000000ull) * 100ull);
    return result;
}

struct PosixMovieRequest
{
    u32 transferred = 0;
    bool done = false;
};
}

void bind_native_movie_file_io(MovieFileBoundaries& files, FileWorkerBoundaries& worker)
{
    // Overlapped requests complete inside ReadFile/WriteFile here; GetOverlappedResult reports the bytes.
    auto requests = std::make_shared<std::unordered_map<RuntimeMovieFile*, PosixMovieRequest>>();
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
        return ::close(movie_fd(handle)) == 0 ? true : fail_with_errno();
    };
    files.size = [](std::uintptr_t handle, u64& size)
    {
        struct stat info;
        if (fstat(movie_fd(handle), &info) != 0)
        {
            return fail_with_errno();
        }
        size = static_cast<u64>(info.st_size);
        return true;
    };
    files.read = [requests](RuntimeMovieFile& file, void* output, u32 size)
    {
        PosixMovieRequest& request = (*requests)[&file];
        request = {};
        const ssize_t count = pread(movie_fd(file.handle), output, size, static_cast<off_t>(file.offset));
        if (count < 0)
        {
            return fail_with_errno();
        }
        request.transferred = static_cast<u32>(count);
        request.done = true;
        if (count == 0 && size != 0)
        {
            g_movie_last_error = 38; // ERROR_HANDLE_EOF
            return false;
        }
        return true;
    };
    files.write = [requests](RuntimeMovieFile& file, const void* data, u32 size)
    {
        PosixMovieRequest& request = (*requests)[&file];
        request = {};
        const ssize_t count = pwrite(movie_fd(file.handle), data, size, static_cast<off_t>(file.offset));
        if (count < 0)
        {
            return fail_with_errno();
        }
        request.transferred = static_cast<u32>(count);
        request.done = true;
        return true;
    };
    files.completed = [requests](RuntimeMovieFile& file, u32& transferred)
    {
        const auto found = requests->find(&file);
        if (found == requests->end() || !found->second.done)
        {
            g_movie_last_error = 87; // ERROR_INVALID_PARAMETER: no request was issued.
            return false;
        }
        transferred = found->second.transferred;
        return true;
    };
    files.last_error = []()
    {
        return g_movie_last_error;
    };
    files.ticks = []()
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<u32>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    };
    files.sector_size = [](const char* drive)
    {
        // GetDiskFreeSpaceA bytes per sector; host block devices report 512.
        struct stat info;
        if (stat(xbox_host_path(drive).c_str(), &info) != 0)
        {
            g_movie_last_error = win32_error_from_errno(errno);
            return 0u;
        }
        return 512u;
    };
    files.seek = [](std::uintptr_t handle, std::int64_t distance, u64* output, u32 method)
    {
        // FILE_BEGIN / FILE_CURRENT / FILE_END match SEEK_SET / SEEK_CUR / SEEK_END.
        const off_t position = lseek(movie_fd(handle), static_cast<off_t>(distance), static_cast<int>(method));
        if (position < 0)
        {
            return fail_with_errno();
        }
        if (output != nullptr)
        {
            *output = static_cast<u64>(position);
        }
        return true;
    };
    files.end_file = [](std::uintptr_t handle)
    {
        const off_t position = lseek(movie_fd(handle), 0, SEEK_CUR);
        if (position < 0 || ftruncate(movie_fd(handle), position) != 0)
        {
            return fail_with_errno();
        }
        return true;
    };
    files.cache_size = [](u32 capacity)
    {
        g_movie_io_cache = capacity;
    };
    files.file_time = [](std::uintptr_t handle, u64& output)
    {
        struct stat info;
        if (fstat(movie_fd(handle), &info) != 0)
        {
            return fail_with_errno();
        }
        output = filetime_from_timespec(info.st_mtim);
        return true;
    };
    files.set_file_time = [](std::uintptr_t handle, u64 time)
    {
        // Only the last write time; UTIME_OMIT keeps the access time.
        timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1] = timespec_from_filetime(time);
        return futimens(movie_fd(handle), times) == 0 ? true : fail_with_errno();
    };
    files.cancel = [](std::uintptr_t)
    {
        // Every request has already completed.
        return true;
    };
    files.delete_file = [](const char* path)
    {
        return unlink(xbox_host_path(path).c_str()) == 0 ? true : fail_with_errno();
    };
    worker.create_file = [](const char* path, u32 access, u32, void*, u32 creation, u32, std::uintptr_t)
    {
        constexpr u32 kGenericRead = 0x80000000u;
        constexpr u32 kGenericWrite = 0x40000000u;
        int flags = O_CLOEXEC;
        const bool reads = (access & kGenericRead) != 0;
        const bool writes = (access & kGenericWrite) != 0;
        flags |= writes ? (reads ? O_RDWR : O_WRONLY) : O_RDONLY;
        const std::string host = xbox_host_path(path);
        struct stat existing;
        const bool existed = stat(host.c_str(), &existing) == 0;
        switch (creation)
        {
        case 1: // CREATE_NEW
            flags |= O_CREAT | O_EXCL;
            break;
        case 2: // CREATE_ALWAYS
            flags |= O_CREAT | O_TRUNC;
            break;
        case 3: // OPEN_EXISTING
            break;
        case 4: // OPEN_ALWAYS
            flags |= O_CREAT;
            break;
        case 5: // TRUNCATE_EXISTING
            flags |= O_TRUNC;
            break;
        default:
            g_movie_last_error = 87;
            return kInvalidHandle;
        }
        const int fd = ::open(host.c_str(), flags, 0644);
        if (fd < 0)
        {
            g_movie_last_error = win32_error_from_errno(errno);
            return kInvalidHandle;
        }
        if (existed && (creation == 2 || creation == 4))
        {
            g_movie_last_error = 183; // ERROR_ALREADY_EXISTS
        }
        return static_cast<std::uintptr_t>(fd);
    };
    worker.last_error = files.last_error;
    worker.set_last_error = [](u32 status)
    {
        g_movie_last_error = status;
    };
    worker.attributes = [](const char* path)
    {
        struct stat info;
        if (stat(xbox_host_path(path).c_str(), &info) != 0)
        {
            g_movie_last_error = win32_error_from_errno(errno);
            return 0xFFFFFFFFu; // INVALID_FILE_ATTRIBUTES
        }
        return S_ISDIR(info.st_mode) ? 0x10u /* FILE_ATTRIBUTE_DIRECTORY */ : 0x80u /* FILE_ATTRIBUTE_NORMAL */;
    };
    worker.create_directory = [](const char* path)
    {
        if (mkdir(xbox_host_path(path).c_str(), 0755) != 0)
        {
            g_movie_last_error = errno == EEXIST ? 183u : win32_error_from_errno(errno);
            return false;
        }
        return true;
    };
    worker.debug_output = [](const char* text)
    {
        std::fputs(text, stderr);
    };
}

// Xbox 0x001073E0
u8* allocate_movie_decoder_memory(u32 size)
{
    return static_cast<u8*>(host_aligned_allocate(size, 4096));
}

// Xbox 0x00107400
void free_movie_decoder_memory(u8* buffer)
{
    host_aligned_free(buffer);
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
    state.copy_head.previous = nullptr;
    state.copy_head.next = nullptr;
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

// Xbox 0x000EB180
i32 write_runtime_movie_file(RuntimeMovieFile& file, const void* data, u32 size,
    std::function<void(RuntimeMovieFile&, i32, void*)> completion, void* payload,
    MovieFileContext& context)
{
    i32 status = wait_runtime_movie_file(file, 0, context); // 0x000EA850
    if (status == 0)
    {
        file.completion = std::move(completion);
        file.payload = payload;
        if (!context.io.write(file, data, size))
        {
            status = static_cast<i32>(context.io.last_error());
            if (status == 0x3E5)
            {
                status = 0;
            }
            else if (status != 0)
            {
                goto done;
            }
        }
        ++context.pending;
        file.state = 1;
    }
done:
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
    // A Bink with the grayscale video flag (0x20000) codes no chroma; libavcodec leaves those planes unset, so the
    // frame converts through neutral chroma instead.
    bool gray = false;
    std::vector<u8> neutral_chroma;
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
        const u8* const* planes = clip->frame->data;
        const int* strides = clip->frame->linesize;
        std::array<const u8*, 4> gray_planes{};
        std::array<int, 4> gray_strides{};
        if (clip->gray && clip->frame->format == AV_PIX_FMT_YUV420P)
        {
            // The luma plane with neutral (128) chroma planes of half the width and height.
            const int chroma_width = (w + 1) / 2;
            const size_t chroma_size = size_t(chroma_width) * size_t((h + 1) / 2);
            if (clip->neutral_chroma.size() < chroma_size)
            {
                clip->neutral_chroma.assign(chroma_size, 128);
            }
            gray_planes = {clip->frame->data[0], clip->neutral_chroma.data(), clip->neutral_chroma.data(), nullptr};
            gray_strides = {clip->frame->linesize[0], chroma_width, chroma_width, 0};
            planes = gray_planes.data();
            strides = gray_strides.data();
        }
        sws_scale(clip->scale, planes, strides, 0, h, &dst, dst_stride);
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
    // libavformat's Bink demuxer passes the header's video flags (+0x24) as the stream's four extradata bytes.
    if (stream->codecpar->codec_id == AV_CODEC_ID_BINKVIDEO && stream->codecpar->extradata != nullptr
        && stream->codecpar->extradata_size >= 4)
    {
        const u8* flags = stream->codecpar->extradata;
        clip->gray = ((u32(flags[0]) | u32(flags[1]) << 8 | u32(flags[2]) << 16 | u32(flags[3]) << 24) & 0x20000) != 0;
    }
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
    log::verbose("movie %s %ux%u frames %u", cue.c_str(), clip->width, clip->height, clip->frames);
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
        // Bink object +0xC, FrameNum, counts from 1; the host clip counts its frames from 0.
        return static_cast<MovieClip*>(decoder)->frame_num + 1;
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
        log::verbose("movie probe %s", path.string().c_str());
    }
}

void movie_advance_time(float dt)
{
    if (dt > 0.f)
    {
        g_now += double(dt);
    }
}

} // namespace gbtg
