#include "file_worker.h"
#include "crt_format.h"
#include "../audio/listener_settings.h"
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cstdio>
#include <mutex>
#include <thread>
#endif

namespace gbtg
{
namespace
{
struct RuntimeCreateFileRequest
{
    std::uintptr_t result;
    u32 error;
    const char* path;
    u32 access;
    u32 share;
    void* security;
    u32 creation;
    u32 attributes;
    std::uintptr_t template_file;
    FileWorkerBoundaries* io;
};

// Xbox 0x000EA9B0
void execute_runtime_create_file(void* payload)
{
    auto& request = *static_cast<RuntimeCreateFileRequest*>(payload);
    request.result = request.io->create_file(request.path, request.access, request.share,
        request.security, request.creation, request.attributes, request.template_file);
    request.error = request.io->last_error();
}

void describe_file_worker_error(i32 status, FileWorkerContext& context)
{
    const char* title;
    const char* message;
    lookup_movie_error_message(status, title, message, context.error);
}
}

void bind_native_file_worker_synchronization(FileWorkerBoundaries& boundaries)
{
#ifdef _WIN32
    boundaries.enter = [](void* section)
    {
        EnterCriticalSection(static_cast<CRITICAL_SECTION*>(section));
    };
    boundaries.leave = [](void* section)
    {
        LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(section));
    };
    boundaries.resume = [](std::uintptr_t thread)
    {
        ResumeThread(reinterpret_cast<HANDLE>(thread));
    };
    boundaries.suspend = [](std::uintptr_t thread)
    {
        SuspendThread(reinterpret_cast<HANDLE>(thread));
    };
    boundaries.yield = []()
    {
        SwitchToThread();
    };
    boundaries.debug_output = [](const char* message)
    {
        OutputDebugStringA(message);
    };
#else
    // The host never creates the worker thread (RuntimeFileWorker::thread stays 0), so dispatch runs
    // operations inline. A section here is a std::recursive_mutex, which has RtlEnterCriticalSection's
    // reentrancy. POSIX cannot suspend another thread, so resume and suspend raise if a worker ever exists.
    boundaries.enter = [](void* section)
    {
        static_cast<std::recursive_mutex*>(section)->lock();
    };
    boundaries.leave = [](void* section)
    {
        static_cast<std::recursive_mutex*>(section)->unlock();
    };
    boundaries.resume = [](std::uintptr_t)
    {
        throw Error("file worker ResumeThread has no POSIX host boundary");
    };
    boundaries.suspend = [](std::uintptr_t)
    {
        throw Error("file worker SuspendThread has no POSIX host boundary");
    };
    boundaries.yield = []()
    {
        std::this_thread::yield();
    };
    boundaries.debug_output = [](const char* message)
    {
        std::fputs(message, stderr);
    };
#endif
}

// Xbox 0x000EA6D0
void dispatch_runtime_file_operation(RuntimeFileOperation operation, void* payload,
    FileWorkerContext& context)
{
    u32 accumulator = 0;
    u32 notifications = 0;
    const u32 initial = static_cast<u32>(truncate_runtime_float(
        performance_counter_seconds(context.counter) * 1000.0L));
    auto& worker = context.worker;
    if (worker.thread != 0)
    {
        context.io.enter(worker.critical_section);
        worker.operation = operation;
        worker.payload = payload;
        worker.active = 1;
        context.io.leave(worker.critical_section);
        context.io.resume(worker.thread);
        context.io.yield();
        u32 active;
        do
        {
            context.io.enter(worker.critical_section);
            active = worker.active;
            context.io.leave(worker.critical_section);
            if (context.notify)
            {
                const u32 negative_time = static_cast<u32>(truncate_runtime_float(
                    performance_counter_seconds(context.counter) * -1000.0L));
                const u32 delta = 0u - accumulator - negative_time;
                if (context.notify_interval <= delta)
                {
                    accumulator += delta;
                    context.notify();
                    ++notifications;
                }
            }
        }
        while (active != 0);
        context.io.suspend(worker.thread);
        const u32 negative_time = static_cast<u32>(truncate_runtime_float(
            performance_counter_seconds(context.counter) * -1000.0L));
        const u32 elapsed = 0u - initial - negative_time;
        char diagnostic[276];
        scene_detail::crt_format_bounded_string(diagnostic, 0x103,
            "%08d: %s : time = %d ms, syncNotifys = %d\n",
            static_cast<i32>(elapsed + initial), worker.name,
            static_cast<i32>(elapsed), static_cast<i32>(notifications));
        context.io.debug_output(diagnostic);
        return;
    }
    operation(payload);
}

// Xbox 0x000EA940
std::uintptr_t open_runtime_file_handle(const char* path, u32 access, u32 share,
    u32 creation, u32 attributes, FileWorkerContext& context)
{
    RuntimeCreateFileRequest request;
    request.path = path;
    request.access = access;
    request.share = share;
    request.security = nullptr;
    request.creation = creation;
    request.attributes = attributes;
    request.template_file = 0;
    request.io = &context.io;
    // The boundary reference is a host mapping, separate from the retail request fields.
    scene_detail::crt_format_bounded_string(context.diagnostic, 260,
        "CreateFile: File Handle(\"%s\")", path);
    dispatch_runtime_file_operation(execute_runtime_create_file, &request, context);
    context.io.set_last_error(request.error);
    return request.result;
}

// Xbox 0x000EAD00
i32 ensure_runtime_file_directory(char* path, FileWorkerContext& context)
{
    if (std::memcmp(path + 1, ":", 2) == 0)
    {
        return 0;
    }
    u32 attributes = context.io.attributes(path);
    i32 status;
    if (attributes == 0xFFFFFFFF)
    {
        status = static_cast<i32>(context.io.last_error());
    }
    else
    {
        status = static_cast<i32>(((0u - static_cast<u32>((attributes & 0x10) != 0)) & 0xFFFFFF49u) + 0xB7);
    }
    if (status == 3)
    {
        char* separator = std::strrchr(path, '\\');
        *separator = 0;
        status = ensure_runtime_file_directory(path, context);
        *separator = '\\';
        if (status != 0)
        {
            goto result;
        }
    }
    else if (status != 2)
    {
        goto result;
    }
    if (context.io.create_directory(path))
    {
        return 0;
    }
    status = static_cast<i32>(context.io.last_error());
result:
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
    describe_file_worker_error(status, context);
    return status;
}

// Xbox 0x000EAC80
i32 ensure_runtime_file_parent_directory(const char* path, FileWorkerContext& context)
{
    char local[260];
    char* destination = local;
    char value;
    do
    {
        value = *path;
        *destination = value;
        ++path;
        ++destination;
    }
    while (value != 0);
    char* separator = std::strrchr(local, '\\');
    i32 status;
    if (separator == nullptr)
    {
        status = 15;
    }
    else
    {
        *separator = 0;
        status = ensure_runtime_file_directory(local, context);
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
    describe_file_worker_error(status, context);
    return status;
}
}
