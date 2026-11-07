#include "throw_trace.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

#pragma comment(lib, "dbghelp.lib")

namespace gbtg
{
namespace
{
constexpr DWORD kMsvcCppException = 0xE06D7363; // 'msc' | 0xE0000000, raised by _CxxThrowException.
constexpr int kReportLimit = 8;
std::atomic<int> g_reports{0};
std::mutex g_symbol_lock;

// The thrown type list of an MSVC x64 C++ exception: ExceptionInformation[2] is its ThrowInfo and [3] the image base
// its RVAs are relative to. ThrowInfo +0xC is the CatchableTypeArray (a count, then one RVA per type); each
// CatchableType +4 is its TypeDescriptor, whose decorated name follows the vtable and spare pointers.
// Returns the traced type's name (std::out_of_range or gbtg::Error), or null for any other exception.
const char* traced_type(const EXCEPTION_RECORD& record)
{
    if (record.ExceptionCode != kMsvcCppException || record.NumberParameters < 4)
    {
        return nullptr;
    }
    const auto base = static_cast<std::uintptr_t>(record.ExceptionInformation[3]);
    const auto* info = reinterpret_cast<const std::int32_t*>(record.ExceptionInformation[2]);
    if (base == 0 || info == nullptr)
    {
        return nullptr;
    }
    const auto* types = reinterpret_cast<const std::int32_t*>(base + std::uint32_t(info[3]));
    for (std::int32_t index = 0; index < types[0]; ++index)
    {
        const auto* catchable = reinterpret_cast<const std::int32_t*>(base + std::uint32_t(types[1 + index]));
        const char* name = reinterpret_cast<const char*>(base + std::uint32_t(catchable[1])) + 2 * sizeof(void*);
        if (std::strcmp(name, ".?AVout_of_range@std@@") == 0)
        {
            return "std::out_of_range";
        }
        if (std::strcmp(name, ".?AVError@gbtg@@") == 0)
        {
            return "gbtg::Error";
        }
    }
    return nullptr;
}

// The frames of the last gbtg::Error thrown on this thread. Some lookups throw an Error that their caller catches, so
// an Error's stack is kept rather than printed, and print_last_error_stack prints it once one ends the run.
struct ThrowFrames
{
    void* frames[48];
    USHORT count = 0;
    ULONG_PTR object = 0; // The exception object (ExceptionInformation[1]): a rethrow raises the same one again.
};
thread_local ThrowFrames t_last_error;

void print_frames(const char* type, void* const* frames, USHORT count)
{
    std::lock_guard<std::mutex> lock(g_symbol_lock);
    static const bool symbols = []()
    {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }();
    std::fprintf(stderr, "%s thrown at:\n", type);
    for (USHORT index = 0; index < count; ++index)
    {
        const auto address = reinterpret_cast<DWORD64>(frames[index]);
        HMODULE module = nullptr;
        char module_name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(frames[index]), &module) != FALSE)
        {
            char path[MAX_PATH];
            if (GetModuleFileNameA(module, path, MAX_PATH) != 0)
            {
                const char* slash = std::strrchr(path, '\\');
                std::snprintf(module_name, sizeof(module_name), "%s", slash != nullptr ? slash + 1 : path);
            }
        }
        const auto offset = address - reinterpret_cast<DWORD64>(module);
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;
        if (symbols && SymFromAddr(GetCurrentProcess(), address, &displacement, symbol) != FALSE)
        {
            if (SymGetLineFromAddr64(GetCurrentProcess(), address, &line_displacement, &line) != FALSE)
            {
                std::fprintf(stderr, "  %2u %s+0x%llx %s (%s:%lu)\n", index, module_name,
                    static_cast<unsigned long long>(offset), symbol->Name, line.FileName, line.LineNumber);
            }
            else
            {
                std::fprintf(stderr, "  %2u %s+0x%llx %s+0x%llx\n", index, module_name,
                    static_cast<unsigned long long>(offset), symbol->Name,
                    static_cast<unsigned long long>(displacement));
            }
        }
        else
        {
            std::fprintf(stderr, "  %2u %s+0x%llx\n", index, module_name, static_cast<unsigned long long>(offset));
        }
    }
    std::fflush(stderr);
}

LONG CALLBACK report_throw(EXCEPTION_POINTERS* pointers)
{
    const char* type = traced_type(*pointers->ExceptionRecord);
    if (type == nullptr)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (std::strcmp(type, "gbtg::Error") == 0)
    {
        // A rethrow (throw;) raises the same object again from the catch; keep the stack of its first raise.
        const ULONG_PTR object = pointers->ExceptionRecord->ExceptionInformation[1];
        if (object != t_last_error.object || t_last_error.count == 0)
        {
            t_last_error.count = RtlCaptureStackBackTrace(1, 48, t_last_error.frames, nullptr);
            t_last_error.object = object;
        }
    }
    else if (g_reports.fetch_add(1) < kReportLimit)
    {
        void* frames[48];
        const USHORT count = RtlCaptureStackBackTrace(1, 48, frames, nullptr);
        print_frames(type, frames, count);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
}

void install_throw_trace()
{
    static const PVOID handler = AddVectoredExceptionHandler(1, report_throw);
    static_cast<void>(handler);
}

void print_last_error_stack()
{
    if (t_last_error.count != 0)
    {
        print_frames("gbtg::Error", t_last_error.frames, t_last_error.count);
    }
}
}
#else
namespace gbtg
{
void install_throw_trace()
{
}

void print_last_error_stack()
{
}
}
#endif
