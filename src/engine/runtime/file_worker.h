#pragma once
#include "gbtg/clock.h"
#include "../video/movie_errors.h"
#include <functional>

namespace gbtg
{
using RuntimeFileOperation = void (*)(void*);
struct RuntimeFileWorker
{
    std::uintptr_t thread; // +0, already-created file worker handle.
    void* critical_section; // +8, native Rtl critical section mapping.
    RuntimeFileOperation operation; // +24.
    void* payload; // +28.
    u32 active; // +2C.
    const char* name; // +34.
};
struct FileWorkerBoundaries
{
    std::function<std::uintptr_t(const char*, u32, u32, void*, u32, u32, std::uintptr_t)> create_file;
    std::function<u32()> last_error;
    std::function<void(u32)> set_last_error;
    std::function<u32(const char*)> attributes;
    std::function<bool(const char*)> create_directory;
    std::function<void(void*)> enter;
    std::function<void(void*)> leave;
    std::function<void(std::uintptr_t)> resume;
    std::function<void(std::uintptr_t)> suspend;
    std::function<void()> yield;
    std::function<void(const char*)> debug_output;
};
struct FileWorkerContext
{
    RuntimeFileWorker& worker;
    FileWorkerBoundaries& io;
    PerformanceCounterState& counter;
    std::function<void()> notify; // Actual DAT_005462D0 indirect callback.
    u32 notify_interval;
    char* diagnostic; // DAT_00506764, 260 bytes.
    MovieErrorContext& error;
};
void bind_native_file_worker_synchronization(FileWorkerBoundaries& boundaries);
void dispatch_runtime_file_operation(RuntimeFileOperation operation, void* payload,
    FileWorkerContext& context); // Xbox 0x000EA6D0
std::uintptr_t open_runtime_file_handle(const char* path, u32 access, u32 share,
    u32 creation, u32 attributes, FileWorkerContext& context); // Xbox 0x000EA940
i32 ensure_runtime_file_directory(char* path, FileWorkerContext& context); // Xbox 0x000EAD00
i32 ensure_runtime_file_parent_directory(const char* path,
    FileWorkerContext& context); // Xbox 0x000EAC80
}
