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
bool throws_out_of_range(const EXCEPTION_RECORD& record)
{
    if (record.ExceptionCode != kMsvcCppException || record.NumberParameters < 4)
    {
        return false;
    }
    const auto base = static_cast<std::uintptr_t>(record.ExceptionInformation[3]);
    const auto* info = reinterpret_cast<const std::int32_t*>(record.ExceptionInformation[2]);
    if (base == 0 || info == nullptr)
    {
        return false;
    }
    const auto* types = reinterpret_cast<const std::int32_t*>(base + std::uint32_t(info[3]));
    for (std::int32_t index = 0; index < types[0]; ++index)
    {
        const auto* catchable = reinterpret_cast<const std::int32_t*>(base + std::uint32_t(types[1 + index]));
        const char* name = reinterpret_cast<const char*>(base + std::uint32_t(catchable[1])) + 2 * sizeof(void*);
        if (std::strcmp(name, ".?AVout_of_range@std@@") == 0)
        {
            return true;
        }
    }
    return false;
}

void print_throw_stack()
{
    void* frames[48];
    const USHORT count = RtlCaptureStackBackTrace(1, 48, frames, nullptr);
    std::lock_guard<std::mutex> lock(g_symbol_lock);
    static const bool symbols = []()
    {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }();
    std::fprintf(stderr, "std::out_of_range thrown at:\n");
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

LONG CALLBACK report_out_of_range(EXCEPTION_POINTERS* pointers)
{
    if (throws_out_of_range(*pointers->ExceptionRecord) && g_reports.fetch_add(1) < kReportLimit)
    {
        print_throw_stack();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
}

void install_out_of_range_trace()
{
    static const PVOID handler = AddVectoredExceptionHandler(1, report_out_of_range);
    static_cast<void>(handler);
}
}
#else
namespace gbtg
{
void install_out_of_range_trace()
{
}
}
#endif
