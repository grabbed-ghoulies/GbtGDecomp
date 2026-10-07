#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/transition_data.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#if !defined(_WIN32)
#include <cfenv>
#endif

#if defined(_WIN32)
#include <excpt.h>
#include <float.h>
#include <malloc.h>
extern "C"
{
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
    __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
    __declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(void* time);
    __declspec(dllimport) unsigned long __stdcall GetTickCount();
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(void* counter);
    __declspec(dllimport) void __stdcall RaiseException(unsigned long code, unsigned long flags, unsigned long count,
                                                        const std::uintptr_t* arguments);
    __declspec(dllimport) int __stdcall CloseHandle(void* handle);
    __declspec(dllimport) int __stdcall FlushFileBuffers(void* handle);
    __declspec(dllimport) unsigned long __stdcall GetLastError();
    __declspec(dllimport) int __stdcall ReadFile(void* handle, void* buffer, unsigned long bytes, unsigned long* read,
                                                 void* overlapped);
    __declspec(dllimport) int __stdcall WriteFile(void* handle, const void* buffer, unsigned long bytes,
                                                  unsigned long* written, void* overlapped);
    __declspec(dllimport) unsigned long __stdcall SetFilePointer(void* handle, long distance, long* high,
                                                                 unsigned long method);
    __declspec(dllimport) int __stdcall IsBadReadPtr(const void* lp, std::uintptr_t ucb);
    __declspec(dllimport) int __stdcall IsBadWritePtr(void* lp, std::uintptr_t ucb);
}
#endif


#include "../scene/scene_types.h"

namespace gbtg
{
using namespace scene_detail;


// Xbox 0x000F71F0
void engine_copy_bytes(void* dest, const void* src, u32 count)
{
    // Xbox 0x000F71F0
    // EAX is the destination, ECX the source, and EDX the byte count.
    // A zero count or equal pointers returns. Both pointers 8-byte aligned
    // and a count above 7 copies two dwords per pass and drops the count by
    // 8. Both pointers 4-byte aligned and a count above 3 then copies one
    // dword per pass. A leftover count is copied one byte forward. The gap
    // for that tail is source minus destination, taken once.
    // 0x00124BE0 passes the DAT_0051234C stride in EDX and end minus that
    // stride in ECX, so the last record is written onto the slot in EAX.
    if (count != 0 && dest != src)
    {
        u8* out = static_cast<u8*>(dest);
        const u8* in = static_cast<const u8*>(src);
        if ((reinterpret_cast<std::uintptr_t>(out) & 7u) == 0 &&
            (reinterpret_cast<std::uintptr_t>(in) & 7u) == 0 && 7u < count)
        {
            u32 chunks = count >> 3;
            do
            {
                const u32 first = *reinterpret_cast<const u32*>(in);
                const u32 second = *reinterpret_cast<const u32*>(in + 4);
                *reinterpret_cast<u32*>(out) = first;
                *reinterpret_cast<u32*>(out + 4) = second;
                out = out + 8;
                in = in + 8;
                count = count - 8;
                chunks = chunks - 1;
            }
            while (chunks != 0);
        }
        if ((reinterpret_cast<std::uintptr_t>(out) & 3u) == 0 &&
            (reinterpret_cast<std::uintptr_t>(in) & 3u) == 0 && 3u < count)
        {
            u32 chunks = count >> 2;
            do
            {
                const u32 word = *reinterpret_cast<const u32*>(in);
                *reinterpret_cast<u32*>(out) = word;
                out = out + 4;
                in = in + 4;
                count = count - 4;
                chunks = chunks - 1;
            }
            while (chunks != 0);
        }
        if (count != 0)
        {
            const std::ptrdiff_t gap = in - out;
            do
            {
                *out = *(out + gap);
                out = out + 1;
                count = count - 1;
            }
            while (count != 0);
        }
    }
}

} // namespace gbtg
