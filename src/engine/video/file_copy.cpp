#include "file_copy.h"
#include "movie_errors.h"
#include "../runtime/file_worker.h"

#include <algorithm>

namespace gbtg
{
RuntimeFileCopyContext* g_runtime_file_copies = nullptr;

namespace
{
bool pending_or_missing(i32 status)
{
    return (0x3E3 < status && status < 0x3E6) || status == 0x4DE;
}

// Xbox 0x000E9210, EAX the copy: off the list and freed.
void release_copy(RuntimeFileCopy* copy)
{
    if (copy->previous != nullptr)
    {
        copy->previous->next = copy->next;
    }
    if (copy->next != nullptr)
    {
        copy->next->previous = copy->previous;
    }
    copy->previous = nullptr;
    copy->next = nullptr;
    delete copy; // HeapFree on the 0x000F7100 heap.
}

// Xbox 0x000E9510: a transfer's completion. A failed one ends the copy (0x000E9380).
void transfer_done(RuntimeMovieFile&, i32 status, void* payload);

// The GetOverlappedResult / CancelIo / close / free 0x000E9380 runs on each file.
void abandon_file(RuntimeMovieFile& file, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    if (file.handle != std::uintptr_t(-1))
    {
        u32 transferred = 0;
        if (!files.io.completed(file, transferred) && files.io.last_error() == 0x3E5)
        {
            files.io.cancel(file.handle); // CancelIo
        }
        close_runtime_movie_file_handle(file, files); // 0x000EABA0
    }
    close_runtime_movie_file(file, files); // 0x000EAC20
}

// Xbox 0x000E9380, ESI the copy, EDX the status: the callback hears the failure, the target is closed and deleted,
// both files abandoned and the copy released.
void fail_copy(RuntimeFileCopy& copy, i32 status, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    if (copy.callback)
    {
        copy.callback(*copy.target, status, copy.context);
    }
    if (copy.target->handle != std::uintptr_t(-1))
    {
        close_runtime_movie_file_handle(*copy.target, files); // 0x000EABA0
        files.io.delete_file(copy.target->path); // DeleteFileA(+0xC)
    }
    abandon_file(*copy.source, copies);
    abandon_file(*copy.target, copies);
    release_copy(&copy); // 0x000E9210
}

// Xbox 0x000E9450, ESI the copy: the next chunk (at most a buffer, sector aligned) read into the read buffer; 0x26
// once the source is all read.
i32 read_chunk(RuntimeFileCopy& copy, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    u32 length = 0;
    i32 status = query_runtime_movie_file_length(*copy.source, length, files); // 0x000EAED0
    if (status != 0)
    {
        return status;
    }
    i32 left = i32(length) - i32(copy.copied);
    if (left < 1)
    {
        return 0x26;
    }
    if (i32(copies.state.buffer_size) <= left)
    {
        left = i32(copies.state.buffer_size); // DAT_00506600
    }
    copy.chunk = u32(left);
    i32 aligned = 0;
    status = align_runtime_movie_file_length(*copy.source, left, aligned, files); // 0x000EB310
    if (status == 0)
    {
        status = read_runtime_movie_file(*copy.source, copy.read_buffer, u32(aligned), transfer_done, &copy,
            files); // 0x000EB0E0
        copy.copied = copy.copied + u32(aligned);
    }
    return status;
}

// Xbox 0x000E94D0, ESI the copy: the last chunk (sector aligned) written from the write buffer.
i32 write_chunk(RuntimeFileCopy& copy, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    i32 aligned = 0;
    i32 status = align_runtime_movie_file_length(*copy.target, i32(copy.chunk), aligned, files); // 0x000EB310
    if (status == 0)
    {
        status = write_runtime_movie_file(*copy.target, copy.write_buffer, u32(aligned), transfer_done, &copy,
            files); // 0x000EB180
    }
    return status;
}

// Xbox 0x000E9310, ESI the copy: the target takes the source's write time, is closed, and the callback hears
// success before both files go.
void finish_copy(RuntimeFileCopy& copy, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    u64 time = 0;
    if (!files.io.file_time(copy.source->handle, time) || !files.io.set_file_time(copy.target->handle, time))
    {
        files.io.last_error(); // GetLastError
    }
    close_runtime_movie_file_handle(*copy.target, files); // 0x000EABA0
    if (copy.callback)
    {
        copy.callback(*copy.target, 0, copy.context);
    }
    close_runtime_movie_file(*copy.source, files); // 0x000EAC20
    close_runtime_movie_file(*copy.target, files);
}

void transfer_done(RuntimeMovieFile&, i32 status, void* payload)
{
    if (status != 0)
    {
        fail_copy(*static_cast<RuntimeFileCopy*>(payload), status, *g_runtime_file_copies); // 0x000E9380
    }
}

// Xbox 0x000E95D0, ECX the source's handle: 1 when the source was written over two seconds after the copy, -1 over
// two seconds before, else 0.
i32 compare_file_times(std::uintptr_t source, std::uintptr_t cached, RuntimeFileCopyContext& copies)
{
    u64 source_time = 0;
    u64 cached_time = 0;
    copies.files.io.file_time(source, source_time); // GetFileTime
    copies.files.io.file_time(cached, cached_time);
    const std::int64_t difference = std::int64_t(source_time - cached_time);
    if (difference < -20000000)
    {
        return -1;
    }
    if (20000000 < difference)
    {
        return 1;
    }
    return 0;
}
}

// Xbox 0x000E9090
i32 start_runtime_file_copy(const char* source, RuntimeFileCopy** job, const char* target,
    RuntimeFileCopyCallback callback, void* context, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    RuntimeMovieFile* input = nullptr;
    RuntimeMovieFile* output = nullptr;
    u32 length = 0;
    u32 existing = 0;
    i32 status = open_runtime_movie_file(input, source, 0x80000000, 3, files); // 0x000EAA90, EAX 3
    if (status == 0)
    {
        status = query_runtime_movie_file_length(*input, length, files); // 0x000EAED0
    }
    if (status == 0)
    {
        status = open_runtime_movie_file(output, target, 0x40000000, 4, files); // EAX 4
    }
    if (status == 0)
    {
        status = query_runtime_movie_file_length(*output, existing, files);
    }
    if (status == 0)
    {
        queue_runtime_file_copy(job, *output, std::move(callback), context, *input, copies); // 0x000E9190
        resize_runtime_movie_file_low(*output, length, files); // 0x000EAFF0, ECX the target
        return 0;
    }
    if (input != nullptr)
    {
        close_runtime_movie_file(*input, files); // 0x000EAC20
    }
    if (output != nullptr)
    {
        close_runtime_movie_file(*output, files);
    }
    if (status != 0 && !pending_or_missing(status))
    {
        const char* title = nullptr;
        const char* message = nullptr;
        lookup_movie_error_message(status, title, message, files.error); // 0x000E96A0
    }
    return status;
}

// Xbox 0x000E9190
void queue_runtime_file_copy(RuntimeFileCopy** job, RuntimeMovieFile& target, RuntimeFileCopyCallback callback,
    void* context, RuntimeMovieFile& source, RuntimeFileCopyContext& copies)
{
    auto* copy = new RuntimeFileCopy; // 0x000F72D0(0x34)
    copy->target = &target;
    copy->callback = std::move(callback);
    copy->state = 0;
    copy->source = &source;
    copy->context = context;
    query_runtime_movie_file_length(source, copy->length, copies.files); // 0x000EAED0, +0x10
    RuntimeFileCopy* tail = &copies.state.copy_head; // DAT_005065F0
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = copy;
    copy->previous = tail;
    copy->next = nullptr;
    if (job != nullptr)
    {
        *job = copy;
    }
}

// Xbox 0x000E9280
void pump_runtime_file_copy(RuntimeFileCopy& copy, RuntimeFileCopyContext& copies)
{
    i32 status = 0;
    if (copy.state == 0)
    {
        copy.read_buffer = copies.state.first_buffer; // DAT_005065F8
        copy.write_buffer = copies.state.second_buffer; // DAT_005065FC
        copy.copied = 0;
        status = read_chunk(copy, copies); // 0x000E9450
        if (status != 0)
        {
            fail_copy(copy, status, copies); // 0x000E9380
            return;
        }
        copy.state = 1;
        return;
    }
    if (copy.state == 1)
    {
        std::swap(copy.read_buffer, copy.write_buffer);
        status = write_chunk(copy, copies); // 0x000E94D0
        if (status == 0)
        {
            status = read_chunk(copy, copies);
        }
        if (status == 0x26)
        {
            copy.state = 3;
            return;
        }
    }
    else if (copy.state == 3)
    {
        finish_copy(copy, copies); // 0x000E9310
        release_copy(&copy); // 0x000E9210
        return;
    }
    if (status != 0)
    {
        fail_copy(copy, status, copies);
    }
}

// Xbox 0x000E9040
i32 runtime_file_copy_progress(const RuntimeFileCopy* copy, u32& remaining, u32& total,
    const RuntimeFileCopyContext& copies)
{
    for (const RuntimeFileCopy* queued = &copies.state.copy_head; queued != nullptr; queued = queued->next)
    {
        if (queued == copy)
        {
            const i32 left = i32(copy->length) - i32(copy->copied);
            remaining = left < 0 ? 0 : u32(left);
            total = copy->length;
            return 0xEA;
        }
    }
    return 0x490;
}

// Xbox 0x000E9530
i32 compare_runtime_cached_file(const char* source, const char* cached, RuntimeFileCopyContext& copies)
{
    auto& files = copies.files;
    const std::uintptr_t copy = open_runtime_file_handle(cached, 0x80000000, 7, 3, 0x20000080,
        *files.worker); // 0x000EA940, EAX the cached path
    if (copy == std::uintptr_t(-1))
    {
        const i32 status = i32(files.io.last_error());
        if (status != 0)
        {
            return status;
        }
    }
    i32 status = 0;
    const std::uintptr_t original = open_runtime_file_handle(source, 0x80000000, 7, 3, 0x20000080, *files.worker);
    if (original != std::uintptr_t(-1) || (status = i32(files.io.last_error())) == 0)
    {
        if (compare_file_times(original, copy, copies) != 0) // 0x000E95D0, ECX the source
        {
            status = 0x51A;
        }
        if (original != std::uintptr_t(-1))
        {
            files.io.close(original); // CloseHandle
        }
    }
    if (copy != std::uintptr_t(-1))
    {
        files.io.close(copy);
    }
    return status;
}
}
