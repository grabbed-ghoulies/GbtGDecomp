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
#include "../camera/camera.h"
#include "crt_runtime.h"
#include "xapi_runtime.h"
#include "../saves/save_root.h"

namespace gbtg
{
using namespace scene_detail;

namespace scene_detail
{


// Xbox 0x0014AAED
int xapi_wide_to_utf8(const u16* src, int cch, u8* dst, int cb)
{
    // Xbox 0x0014AAED
    // WideCharToMultiByte for code page 65001. A cch of -1 counts through the
    // NUL (0x00166ECB) and includes it. Bytes that fit are stored. A nonzero
    // cb shorter than the encoded size returns 0; 0x0014B00C would record
    // last-error 0x7A.
    if (cch == -1)
    {
        cch = crt_wide_string_length(src) + 1;
    }

    int count = 0;
    while (cch > 0)
    {
        const u16 unit = *src;
        ++src;
        --cch;

        if (unit < 0x80)
        {
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(unit);
                ++dst;
            }
            continue;
        }

        if (unit < 0x800)
        {
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(0xc0 | ((unit >> 6) & 0x1f));
                ++dst;
            }
        }
        else
        {
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(0xe0 | ((unit >> 12) & 0x0f));
                ++dst;
            }
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(0x80 | ((unit >> 6) & 0x3f));
                ++dst;
            }
        }

        ++count;
        if (count <= cb && dst != nullptr)
        {
            *dst = static_cast<u8>(0x80 | (unit & 0x3f));
            ++dst;
        }
    }

    if (cb != 0 && cb < count)
    {
        return 0;
    }
    return count;
}


// Xbox 0x0014AABA
void* xapi_allocate_contiguous_memory(u32 bytes, u32 lowest, u32 highest, u32 alignment, u32 protect)
{
    // Xbox 0x0014AABA
    // stdcall. The five arguments pass through to MmAllocateContiguousMemoryEx
    // (kernel ordinal 166, thunk 0x00202EA8). XMemAllocDefault (0x0014F90F)
    // calls this when the attribute alignment nibble is 1: size, lowest 0,
    // highest 0x00FFFFFF, alignment 0x1000, and a protect dword from
    // DAT_0032CA10 (0x02, 0x204, 0x04, 0x404). A null result stores last-error
    // 8 (ERROR_NOT_ENOUGH_MEMORY). A pointer leaves that slot unchanged.
    // The kernel returns the virtual address of physically contiguous pages
    // that fit in [lowest, highest]. Alignment below a page becomes a page.
    // The port allocates one aligned host block of the page-rounded size.
    // Protect stays a host default. Release the pointer with _aligned_free
    // (ordinal 171, MmFreeContiguousMemory).
    (void)protect;

    if (bytes == 0 || highest < lowest)
    {
        g_xapi_last_error = 8;
        return nullptr;
    }

    u32 align = alignment;
    if (align < 0x1000)
    {
        align = 0x1000;
    }
    if ((align & (align - 1u)) != 0)
    {
        g_xapi_last_error = 8;
        return nullptr;
    }

    const u64 span_end = highest;
    const u64 start = (static_cast<u64>(lowest) + align - 1u) & ~(static_cast<u64>(align) - 1u);
    const u64 rounded = (static_cast<u64>(bytes) + align - 1u) & ~(static_cast<u64>(align) - 1u);
    if (start > span_end || rounded > span_end - start + 1u)
    {
        g_xapi_last_error = 8;
        return nullptr;
    }

    void* block = nullptr;
#if defined(_WIN32)
    block = _aligned_malloc(static_cast<size_t>(rounded), align);
#else
    block = std::aligned_alloc(align, static_cast<size_t>(rounded));
#endif
    if (block == nullptr)
    {
        g_xapi_last_error = 8;
    }
    return block;
}


// Retail image at base 0x10000. Count is the dword at 0x0001011C. The header
// array pointer is the dword at 0x00010120. Each record is 0x38 bytes and the
// name is the C string at +0x14. Virtual address sits at +4 and virtual size
// at +8, which 0x0014AF9B and 0x0014AFDA read from the handle.
struct XbeSectionRecord
{
    u32 flags;
    u32 virtual_address;
    u32 virtual_size;
    u32 raw_address;
    u32 raw_size;
    const char* name;
};


const XbeSectionRecord g_xbe_sections[] = {
    {0x16, 0x11000, 0x160020, 0x1000, 0x160020, ".text"},
    {0x7, 0x171020, 0x135d8, 0x162000, 0xfd2c, "D3D"},
    {0x7, 0x184600, 0xe40, 0x172000, 0xe3c, "D3DX"},
    {0x7, 0x185440, 0x20598, 0x173000, 0x1f91c, "XGRPH"},
    {0x26, 0x1a59e0, 0xd3ac, 0x193000, 0xd3ac, "BINK"},
    {0x36, 0x1b2da0, 0x1175, 0x1a1000, 0x1174, "BINK32"},
    {0x36, 0x1b3f20, 0x1489, 0x1a3000, 0x1488, "BINK32A"},
    {0x36, 0x1b53c0, 0x123c, 0x1a5000, 0x123c, "BINK16"},
    {0x36, 0x1b6600, 0x14c0, 0x1a7000, 0x14c0, "BINK4444"},
    {0x36, 0x1b7ac0, 0x10b4, 0x1a9000, 0x10b4, "BINK5551"},
    {0x36, 0x1b8b80, 0xcac, 0x1ab000, 0xcac, "BINKYUY2"},
    {0x36, 0x1b9840, 0x9c, 0x1ac000, 0x9c, "BINKYUY2"},
    {0x36, 0x1b98e0, 0xb8, 0x1ad000, 0xb8, "BINKYUY2"},
    {0x36, 0x1b99a0, 0x88, 0x1ae000, 0x88, "BINKYUY2"},
    {0x36, 0x1b9a40, 0x130, 0x1af000, 0x130, "BINK16MX"},
    {0x16, 0x1b9b80, 0x558, 0x1b0000, 0x558, "BINK16X2"},
    {0x6, 0x1ba0e0, 0x1f8, 0x1b1000, 0x1f8, "BINK16M"},
    {0x6, 0x1ba2e0, 0x1bc, 0x1b2000, 0x1bc, "BINK32MX"},
    {0x6, 0x1ba4a0, 0x58c, 0x1b3000, 0x58c, "BINK32X2"},
    {0x6, 0x1baa40, 0x14c, 0x1b4000, 0x14c, "BINK32M"},
    {0x7, 0x1baba0, 0xac30, 0x1b5000, 0xabdc, "XACTENG"},
    {0x7, 0x1c57e0, 0x1e534, 0x1c0000, 0x1e2cc, "DSOUND"},
    {0x7, 0x1e3d20, 0x1907c, 0x1df000, 0x1907c, "WMADEC"},
    {0x7, 0x1fcda0, 0x60fc, 0x1f9000, 0x60fc, "XPP"},
    {0x6, 0x202ea0, 0x129958, 0x200000, 0x129948, ".rdata"},
    {0x7, 0x32c800, 0x222fc0, 0x32a000, 0x107a14, ".data"},
    {0x7, 0x54f7c0, 0xf5e0, 0x432000, 0xf5d4, "BINKDATA"},
    {0x26, 0x55eda0, 0x7180, 0x442000, 0x716c, "DOLBY"},
    {0x38, 0x565f20, 0x4800, 0x44d000, 0x4800, "$$XTIMAGE"},
    {0x38, 0x56a720, 0x4800, 0x452000, 0x4800, "$$XSIMAGE"},
    {0x3a, 0x56ef20, 0x28be, 0x44a000, 0x28be, "dirtydisk_bitmap"},
    {0x38, 0x5717e0, 0x7a0, 0x457000, 0x7a0, ".XTLID"},
};


void xapi_init_ansi(u16& length, const char*& buffer, const char* src)
{
    buffer = src;
    if (src == nullptr)
    {
        length = 0;
        return;
    }
    length = static_cast<u16>(std::strlen(src));
}


bool xapi_equal_ansi_ci(const char* left, u16 left_len, const char* right, u16 right_len)
{
    // RtlEqualString with CaseInSensitive set. Lengths must match. a-z folds
    // the way RtlUpperChar does; every other byte compares as itself.
    if (left_len != right_len)
    {
        return false;
    }
    for (u16 i = 0; i < left_len; ++i)
    {
        u8 a = static_cast<u8>(left[i]);
        u8 b = static_cast<u8>(right[i]);
        if (a >= 'a' && a <= 'z')
        {
            a = static_cast<u8>(a - 0x20);
        }
        if (b >= 'a' && b <= 'z')
        {
            b = static_cast<u8>(b - 0x20);
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}


// Xbox 0x0014AF34
std::uintptr_t xapi_find_section_handle(const char* name)
{
    // Xbox 0x0014AF34
    // XGetSectionHandleA. stdcall. RtlInitAnsiString (thunk 0x00202EE8) on
    // the argument, then walk section headers. Each name is RtlInitAnsiString
    // from header+0x14 and RtlEqualString (thunk 0x00202EE4) with the case
    // flag set. The first hit returns that header. The end test is unsigned
    // below the header base plus count*0x38, so a miss falls out, stores
    // last-error 0x490 (ERROR_NOT_FOUND) via 0x0014B00C, and returns
    // 0xFFFFFFFF. A hit leaves last-error alone. 0x0003277C asks for
    // dirtydisk_bitmap. 0x00150ADD asks for $$XTINFO (absent), $$XTIMAGE,
    // and $$XSIMAGE.
    u16 want_len = 0;
    const char* want = nullptr;
    xapi_init_ansi(want_len, want, name);
    for (const XbeSectionRecord& section : g_xbe_sections)
    {
        u16 have_len = 0;
        const char* have = nullptr;
        xapi_init_ansi(have_len, have, section.name);
        if (xapi_equal_ansi_ci(want, want_len, have, have_len))
        {
            return reinterpret_cast<std::uintptr_t>(&section);
        }
    }
    g_xapi_last_error = 0x490;
    return static_cast<std::uintptr_t>(-1);
}


// DAT_0032CA50, indexed by the low byte of a UTF-16 unit. Bit 0x100000 is
// uppercase, 0x80000 lowercase, 0x40000 a digit, 0x20000 a symbol. The high
// bit marks everything else. '.' shares '-'s weight, 0x92 shares 0x91, and
// 0xF6 shares 0xF5.
u32 xapi_char_weight(u8 unit)
{
    if (unit == 0x2E)
    {
        return 0x0002002Du;
    }
    if (unit == 0x92)
    {
        return 0x80000091u;
    }
    if (unit == 0xF6)
    {
        return 0x000800F5u;
    }
    if (unit < 0x20 || (unit >= 0x7F && unit <= 0xA0))
    {
        return 0x80000000u | unit;
    }
    if (unit >= '0' && unit <= '9')
    {
        return 0x00040000u | unit;
    }
    if ((unit >= 'A' && unit <= 'Z') || (unit >= 0xC0 && unit <= 0xD6) || (unit >= 0xD8 && unit <= 0xDF))
    {
        return 0x00100000u | unit;
    }
    if ((unit >= 'a' && unit <= 'z') || (unit >= 0xE0 && unit <= 0xF5) || unit >= 0xF8)
    {
        return 0x00080000u | unit;
    }
    return 0x00020000u | unit;
}


// Xbox 0x0014FB5B
int xapi_compare_wide_strings(u32 flags, const u16* left, int left_count, const u16* right, int right_count)
{
    // Xbox 0x0014FB5B
    // Wide compare used by lstrcmpW (0x0014A91C) with flags 0 and counts -1.
    // Bit 0 folds uppercase (weight bit 0x100000) to byte+0x20 with the
    // lowercase bit. Bit 2 drops symbol weights (0x20000), one side at a time.
    // Bit 0x1000 compares weight bit 0x10000 in the primary pass. With that
    // bit clear, those units are skipped and the difference is resolved from
    // DAT_0032CA20 when both sides end. A count of -1 walks to NUL. Running
    // out of an explicit count before NUL returns 3 if the left count hits 0
    // first and 1 if the right count does. A null pointer or a count below
    // -1 stores last-error 0x57 and returns 0. 1, 2, and 3 are less, equal,
    // and greater.
    if (left == nullptr || right == nullptr || left_count < -1 || right_count < -1)
    {
        g_xapi_last_error = 0x57;
        return 0;
    }

    const bool ignore_case = (flags & 1u) != 0;
    const bool ignore_symbols = (flags & 4u) != 0;
    const bool word_sort = (flags & 0x1000u) == 0;
    // DAT_0032CA20. Index 0 is equal. 5 and 11 are greater; 7 and 10 are less.
    static constexpr int kTie[12] = {2, 0, 0, 0, 0, 3, 0, 1, 0, 0, 1, 3};
    u32 tie = 0;

    for (;;)
    {
        if (left_count == 0 || right_count == 0)
        {
            if (left_count == 0 && right_count == 0)
            {
                return kTie[tie];
            }
            return left_count == 0 ? 3 : 1;
        }

        if (*left == 0)
        {
            if (*right != 0)
            {
                return 1;
            }
            return kTie[tie];
        }
        if (*right == 0)
        {
            return 3;
        }

        const u8 left_byte = static_cast<u8>(*left);
        const u8 right_byte = static_cast<u8>(*right);
        u32 left_w = xapi_char_weight(left_byte);
        u32 right_w = xapi_char_weight(right_byte);

        if (word_sort && (left_w & 0x10000u) != 0)
        {
            if ((right_w & 0x10000u) == 0)
            {
                if (tie == 0)
                {
                    tie = 4;
                }
                tie |= 1u;
                ++left;
                --left_count;
                continue;
            }
            if (left_w < right_w)
            {
                tie = 7;
            }
            else if (right_w < left_w)
            {
                tie = 11;
            }
            ++left;
            ++right;
            --left_count;
            --right_count;
            continue;
        }
        if (word_sort && (right_w & 0x10000u) != 0)
        {
            if (tie == 0)
            {
                tie = 8;
            }
            tie |= 2u;
            ++right;
            --right_count;
            continue;
        }

        if (ignore_symbols)
        {
            if ((left_w & 0x20000u) != 0)
            {
                ++left;
                --left_count;
                continue;
            }
            if ((right_w & 0x20000u) != 0)
            {
                ++right;
                --right_count;
                continue;
            }
        }

        if (ignore_case)
        {
            if ((left_w & 0x100000u) != 0)
            {
                left_w = (static_cast<u32>(left_byte) + 0x20u) | 0x80000u;
            }
            if ((right_w & 0x100000u) != 0)
            {
                right_w = (static_cast<u32>(right_byte) + 0x20u) | 0x80000u;
            }
        }

        if (left_w < right_w)
        {
            return 1;
        }
        if (right_w < left_w)
        {
            return 3;
        }
        ++left;
        ++right;
        --left_count;
        --right_count;
    }
}


// Xbox 0x0014FCF4
int xapi_compare_byte_strings(u32 flags, const u8* left, int left_count, const u8* right, int right_count)
{
    // Xbox 0x0014FCF4
    // Byte compare used by lstrcmpiA (0x0014AC6D) with flags 1 and counts -1.
    // Weights come from DAT_0032CA50. Bit 0 folds uppercase (weight bit
    // 0x100000) to byte+0x20 with the lowercase bit. Bit 2 drops symbol
    // weights (0x20000), one side at a time. Bit 0x1000 compares weight bit
    // 0x10000 in the primary pass. With that bit clear, those units are
    // skipped and the difference is resolved from DAT_0032CE50 when both
    // sides end. A count of -1 walks to NUL. Running out of an explicit count
    // before NUL returns 3 if the left count hits 0 first and 1 if the right
    // count does. A null pointer or a count below -1 stores last-error 0x57
    // and returns 0. 1, 2, and 3 are less, equal, and greater.
    if (left == nullptr || right == nullptr || left_count < -1 || right_count < -1)
    {
        g_xapi_last_error = 0x57;
        return 0;
    }

    const bool ignore_case = (flags & 1u) != 0;
    const bool ignore_symbols = (flags & 4u) != 0;
    const bool word_sort = (flags & 0x1000u) == 0;
    // DAT_0032CE50. Index 0 is equal. 5 and 11 are greater; 7 and 10 are less.
    static constexpr int kTie[12] = {2, 0, 0, 0, 0, 3, 0, 1, 0, 0, 1, 3};
    u32 tie = 0;

    for (;;)
    {
        if (left_count == 0 || right_count == 0)
        {
            if (left_count == 0 && right_count == 0)
            {
                return kTie[tie];
            }
            return left_count == 0 ? 3 : 1;
        }

        const u8 left_byte = *left;
        const u8 right_byte = *right;
        if (left_byte == 0)
        {
            if (right_byte != 0)
            {
                return 1;
            }
            return kTie[tie];
        }
        if (right_byte == 0)
        {
            return 3;
        }

        u32 left_w = xapi_char_weight(left_byte);
        u32 right_w = xapi_char_weight(right_byte);

        if (word_sort && (left_w & 0x10000u) != 0)
        {
            if ((right_w & 0x10000u) == 0)
            {
                if (tie == 0)
                {
                    tie = 4;
                }
                tie |= 1u;
                ++left;
                --left_count;
                continue;
            }
            if (left_w < right_w)
            {
                tie = 7;
            }
            else if (right_w < left_w)
            {
                tie = 11;
            }
            ++left;
            ++right;
            --left_count;
            --right_count;
            continue;
        }
        if (word_sort && (right_w & 0x10000u) != 0)
        {
            if (tie == 0)
            {
                tie = 8;
            }
            tie |= 2u;
            ++right;
            --right_count;
            continue;
        }

        if (ignore_symbols)
        {
            if ((left_w & 0x20000u) != 0)
            {
                ++left;
                --left_count;
                continue;
            }
            if ((right_w & 0x20000u) != 0)
            {
                ++right;
                --right_count;
                continue;
            }
        }

        if (ignore_case)
        {
            if ((left_w & 0x100000u) != 0)
            {
                left_w = (static_cast<u32>(left_byte) + 0x20u) | 0x80000u;
            }
            if ((right_w & 0x100000u) != 0)
            {
                right_w = (static_cast<u32>(right_byte) + 0x20u) | 0x80000u;
            }
        }

        if (left_w < right_w)
        {
            return 1;
        }
        if (right_w < left_w)
        {
            return 3;
        }
        ++left;
        ++right;
        --left_count;
        --right_count;
    }
}


// Cursor left by xapi_find_first_file. It already sits on the entry after the one
// copied into WIN32_FIND_DATA. FindNextFile passes a null name, and the
// kernel keeps the mask captured by the first query. "*.*" is match-all.
struct XapiFindCursor
{
    std::filesystem::directory_iterator cursor;
    std::string pattern;
    bool match_all = false;
};


bool xapi_find_match(const char* pattern, size_t pattern_len, const char* name, size_t name_len)
{
    auto fold = [](unsigned char c) -> unsigned char
    {
        if (c >= 'a' && c <= 'z')
        {
            return static_cast<unsigned char>(c - 0x20);
        }
        return c;
    };

    size_t pi = 0;
    size_t ni = 0;
    size_t star_p = static_cast<size_t>(-1);
    size_t star_n = 0;
    while (ni < name_len)
    {
        if (pi < pattern_len &&
            (pattern[pi] == '?' || fold(static_cast<unsigned char>(pattern[pi])) == fold(static_cast<unsigned char>(name[ni]))))
        {
            ++pi;
            ++ni;
            continue;
        }
        if (pi < pattern_len && pattern[pi] == '*')
        {
            star_p = pi;
            ++pi;
            star_n = ni;
            continue;
        }
        if (star_p != static_cast<size_t>(-1))
        {
            pi = star_p + 1;
            ++star_n;
            ni = star_n;
            continue;
        }
        return false;
    }
    while (pi < pattern_len && pattern[pi] == '*')
    {
        ++pi;
    }
    return pi == pattern_len;
}


std::string xapi_narrow_filename(const std::filesystem::path& path)
{
    const std::wstring wide = path.filename().wstring();
    std::string name;
    name.reserve(wide.size());
    for (wchar_t unit : wide)
    {
        if (static_cast<std::uint32_t>(unit) <= 0xFFu)
        {
            name.push_back(static_cast<char>(static_cast<unsigned char>(unit)));
        }
        else
        {
            name.push_back('?');
        }
    }
    return name;
}


u64 xapi_host_file_time(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(path, ec);
    if (ec)
    {
        return 0;
    }
    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(stamp);
    const auto unix_ticks = std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(
                                sys.time_since_epoch())
                                .count();
    const std::int64_t file_time = unix_ticks + 116444736000000000LL;
    if (file_time < 0)
    {
        return 0;
    }
    return static_cast<u64>(file_time);
}


void xapi_set_dir_open_error(const std::filesystem::path& opened, const std::error_code& ec)
{
    if (ec == std::errc::no_such_file_or_directory)
    {
        std::filesystem::path leaf = opened;
        if (leaf.filename().empty())
        {
            leaf = leaf.parent_path();
        }
        std::error_code parent_ec;
        const auto parent = leaf.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            g_xapi_last_error = 3;
        }
        else
        {
            g_xapi_last_error = 2;
        }
        return;
    }
    if (ec == std::errc::not_a_directory || ec.value() == 267)
    {
        g_xapi_last_error = 267;
        return;
    }
    if (ec == std::errc::permission_denied)
    {
        g_xapi_last_error = 5;
        return;
    }
    if (ec == std::errc::invalid_argument)
    {
        g_xapi_last_error = 0x57;
        return;
    }
    if (ec.value() != 0)
    {
        g_xapi_last_error = static_cast<u32>(ec.value());
        return;
    }
    g_xapi_last_error = 2;
}


void xapi_fill_find_data(u8* dst, const std::filesystem::directory_entry& entry, const std::string& name)
{
    // 0x0014B0B6. Attributes from FILE_DIRECTORY_INFORMATION+0x38 land at +0.
    // Creation, access, and write FILETIMEs follow; the port stores the host
    // last-write time in all three. EndOfFile high/low land at +0x1C/+0x20.
    // The ANSI name is copied to +0x2C and NUL-terminated. The first byte of
    // cAlternateFileName (+0x130) is cleared. Reserved slots at +0x24 and
    // +0x28 are left alone.
    u32 attr = 0x20;
    std::error_code ec;
    if (entry.is_directory(ec) && !ec)
    {
        attr = 0x10;
    }
    ec.clear();
    const auto perms = entry.status(ec).permissions();
    if (!ec && (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
    {
        attr |= 0x1u;
    }
    std::memcpy(dst + 0x00, &attr, 4);

    const u64 stamp = xapi_host_file_time(entry.path());
    const u32 stamp_lo = static_cast<u32>(stamp);
    const u32 stamp_hi = static_cast<u32>(stamp >> 32);
    std::memcpy(dst + 0x04, &stamp_lo, 4);
    std::memcpy(dst + 0x08, &stamp_hi, 4);
    std::memcpy(dst + 0x0C, &stamp_lo, 4);
    std::memcpy(dst + 0x10, &stamp_hi, 4);
    std::memcpy(dst + 0x14, &stamp_lo, 4);
    std::memcpy(dst + 0x18, &stamp_hi, 4);

    u32 size_lo = 0;
    u32 size_hi = 0;
    if ((attr & 0x10u) == 0)
    {
        ec.clear();
        const auto bytes = entry.file_size(ec);
        if (!ec)
        {
            size_lo = static_cast<u32>(bytes);
            size_hi = static_cast<u32>(bytes >> 32);
        }
    }
    std::memcpy(dst + 0x1C, &size_hi, 4);
    std::memcpy(dst + 0x20, &size_lo, 4);

    const size_t count = std::min(name.size(), size_t(259));
    if (count != 0)
    {
        std::memcpy(dst + 0x2C, name.data(), count);
    }
    dst[0x2C + count] = 0;
    dst[0x130] = 0;
}


// Xbox 0x0014B122
std::uintptr_t xapi_find_first_file(const char* path, void* find_data)
{
    // Xbox 0x0014B122
    // FindFirstFileA. RtlInitAnsiString, then walk back to the last backslash.
    // The head, including that slash, is the NtOpenFile object name under
    // ObDosDevicesDirectory (0xFFFFFFFD) with OBJ_CASE_INSENSITIVE,
    // FILE_LIST_DIRECTORY|SYNCHRONIZE, share read|write, and
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // The tail is the NtQueryDirectoryFile pattern (FileDirectoryInformation,
    // class 1, 0x148 bytes). A tail of "*.*" is passed with length 0 so every
    // name matches. No backslash, or an empty tail, stores last-error 0x57
    // and returns 0xFFFFFFFF. The first hit is copied by 0x0014B0B6 and the
    // directory handle is returned. A failed query closes the handle. Other
    // failures go through XapiSetLastNTError: 2 when the leaf or the match is
    // missing, 3 when a parent is missing, 267 when the path is not a directory.
    if (path == nullptr)
    {
        g_xapi_last_error = 0x57;
        return static_cast<std::uintptr_t>(-1);
    }

    const size_t length = std::strlen(path);
    size_t split = length;
    while (split != 0 && path[split - 1] != '\\')
    {
        --split;
    }
    const size_t tail = length - split;
    if (split == 0 || tail == 0)
    {
        g_xapi_last_error = 0x57;
        return static_cast<std::uintptr_t>(-1);
    }

    const bool match_all = tail == 3 && path[split] == '*' && path[split + 1] == '.' && path[split + 2] == '*';
    const std::string pattern(path + split, tail);
    const std::filesystem::path dir(xbox_host_path(std::string(path, split).c_str()));

    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        xapi_set_dir_open_error(dir, ec);
        return static_cast<std::uintptr_t>(-1);
    }

    const std::filesystem::directory_iterator end;
    while (it != end)
    {
        const std::string name = xapi_narrow_filename(it->path());
        if (match_all || xapi_find_match(pattern.data(), pattern.size(), name.data(), name.size()))
        {
            if (find_data != nullptr)
            {
                xapi_fill_find_data(static_cast<u8*>(find_data), *it, name);
            }
            std::error_code step_ec;
            it.increment(step_ec);
            if (step_ec)
            {
                it = std::filesystem::directory_iterator();
            }
            auto* state = new (std::nothrow) XapiFindCursor;
            if (state == nullptr)
            {
                g_xapi_last_error = 0x5AA;
                return static_cast<std::uintptr_t>(-1);
            }
            state->cursor = std::move(it);
            state->pattern = pattern;
            state->match_all = match_all;
            return reinterpret_cast<std::uintptr_t>(state);
        }

        it.increment(ec);
        if (ec)
        {
            xapi_set_dir_open_error(dir, ec);
            return static_cast<std::uintptr_t>(-1);
        }
    }

    g_xapi_last_error = 2;
    return static_cast<std::uintptr_t>(-1);
}


// Xbox 0x0014B22F
bool xapi_find_next_file(std::uintptr_t find_handle, void* find_data)
{
    // Xbox 0x0014B22F
    // FindNextFileA. NtQueryDirectoryFile on the handle from FindFirstFileA,
    // FileDirectoryInformation, class 1, 0x148 bytes, a null name, and
    // RestartScan false. The null name does not replace the mask from the
    // first query. A hit is copied by 0x0014B0B6. Failure stores the Win32
    // error through XapiSetLastNTError: 18 when the scan is exhausted, 6 for
    // a bad handle. The handle stays open.
    if (find_handle == 0 || find_handle == static_cast<std::uintptr_t>(-1))
    {
        g_xapi_last_error = 6;
        return false;
    }

    auto* state = reinterpret_cast<XapiFindCursor*>(find_handle);
    const std::filesystem::directory_iterator end;
    while (state->cursor != end)
    {
        const std::string name = xapi_narrow_filename(state->cursor->path());
        const bool hit = state->match_all ||
            xapi_find_match(state->pattern.data(), state->pattern.size(), name.data(), name.size());
        if (hit && find_data != nullptr)
        {
            xapi_fill_find_data(static_cast<u8*>(find_data), *state->cursor, name);
        }

        std::error_code ec;
        state->cursor.increment(ec);
        if (ec)
        {
            state->cursor = std::filesystem::directory_iterator();
        }
        if (hit)
        {
            return true;
        }
        if (ec)
        {
            g_xapi_last_error = 18;
            return false;
        }
    }

    g_xapi_last_error = 18;
    return false;
}


// CloseHandle (0x0014B385) on a handle from 0x0014B122.
void xapi_close_find_file(std::uintptr_t find_handle)
{
    if (find_handle == 0 || find_handle == static_cast<std::uintptr_t>(-1))
    {
        g_xapi_last_error = 6;
        return;
    }
    delete reinterpret_cast<XapiFindCursor*>(find_handle);
}


// Xbox 0x0014C2D5
void xapi_append_save_directory_hash(const u16* name, char* dest, int remaining)
{
    // Xbox 0x0014C2D5
    // XCreateSaveGame (0x0014C70F) appends this to the copied root path. Each
    // UTF-16 unit updates hash = (hash * 0x10000 + unit) mod (2^48 - 59) until
    // a zero unit. The 48-bit value is written as twelve uppercase hex digits,
    // low nibble at dest[11], then a NUL at dest[12]. 0x0014C2C1 adds 0x37 when
    // the nibble is greater than 9 and 0x30 otherwise. The third argument is
    // the caller's leftover path budget (0xF7 minus the root length) and is
    // left unread.
    (void)remaining;
    constexpr u64 kMod = 0xFFFFFFFFFFC5ull;
    u64 hash = 0;
    while (*name != 0)
    {
        hash = (hash * 0x10000ull + *name) % kMod;
        ++name;
    }

    for (int index = 11; index >= 0; --index)
    {
        const u32 nibble = static_cast<u32>(hash) & 0xFu;
        if (static_cast<i32>(nibble) > 9)
        {
            dest[index] = static_cast<char>(nibble + 0x37);
        }
        else
        {
            dest[index] = static_cast<char>(nibble + 0x30);
        }
        hash >>= 4;
    }
    dest[12] = '\0';
}


// Xbox 0x0014C532
int xapi_read_save_directory_entry(u8* find_data, const char* root, int root_len)
{
    // Xbox 0x0014C532
    // XFindFirstSaveGame (0x0014CA42) and XFindNextSaveGame (0x0014CB4C)
    // call this for each directory entry. Bit 0x10 of the WIN32_FIND_DATA
    // attributes at +0 selects directories. The root is copied to
    // szSaveGameDirectory at +0x140 and cFileName at +0x2C is appended at
    // the caller's root length. A joined length below 0x103 stores a
    // trailing backslash. A joined length below 0xF7 then appends
    // "\\SaveMeta.xbx" and 0x0014D08C opens it with GENERIC_READ,
    // FILE_SHARE_READ, OPEN_EXISTING, and FILE_ATTRIBUTE_NORMAL.
    // 0x0014C367 seeks to the start and requires a 2-byte read; a FEFF
    // word also passes. 0x0014C3AF reads the rest as UTF-16. The first
    // L"Name" wins only when the next wchar is '='; a hit inside
    // L"TitleName=" therefore supplies the value. Any other hit is
    // discarded through the next CR LF. The value is copied to the wide
    // name at +0x244, clipped to 0x7F wchars plus a NUL. The path is then
    // cut back to the trailing backslash. A miss returns 0 so the walk
    // continues.
    u32 attr = 0;
    std::memcpy(&attr, find_data, 4);
    if ((attr & 0x10u) == 0)
    {
        return 0;
    }

    const char* file_name = reinterpret_cast<const char*>(find_data + 0x2C);
    int name_len = 0;
    while (file_name[name_len] != '\0')
    {
        ++name_len;
    }

    char* dir = reinterpret_cast<char*>(find_data + 0x140);
    {
        int i = 0;
        char unit = 0;
        do
        {
            unit = root[i];
            dir[i] = unit;
            ++i;
        } while (unit != '\0');
    }
    {
        int i = 0;
        char unit = 0;
        do
        {
            unit = file_name[i];
            dir[root_len + i] = unit;
            ++i;
        } while (unit != '\0');
    }

    const int combined = name_len + root_len;
    if (combined < 0x103)
    {
        dir[combined] = '\\';
        dir[combined + 1] = '\0';
    }

    int result = 0;
    if (combined < 0xF7)
    {
        const char meta[] = "\\SaveMeta.xbx";
        int i = 0;
        char unit = 0;
        do
        {
            unit = meta[i];
            dir[combined + i] = unit;
            ++i;
        } while (unit != '\0');

        std::ifstream in(std::filesystem::path(xbox_host_path(dir)), std::ios::binary);
        if (in)
        {
            unsigned char probe[2] = {};
            in.read(reinterpret_cast<char*>(probe), 2);
            const std::streamsize got = in.gcount();
            const u16 word = static_cast<u16>(probe[0] | (static_cast<unsigned>(probe[1]) << 8));
            if (got == 2 || word == 0xFEFFu)
            {
                in.clear();
                std::u16string text;
                unsigned char pair[2];
                while (in.read(reinterpret_cast<char*>(pair), 2))
                {
                    text.push_back(static_cast<char16_t>(pair[0] | (static_cast<unsigned>(pair[1]) << 8)));
                }

                u16* dest = reinterpret_cast<u16*>(find_data + 0x244);
                size_t line = 0;
                while (line < text.size())
                {
                    size_t crlf = line;
                    bool broke = false;
                    while (crlf + 1 < text.size())
                    {
                        if (text[crlf] == u'\r' && text[crlf + 1] == u'\n')
                        {
                            broke = true;
                            break;
                        }
                        ++crlf;
                    }
                    if (!broke)
                    {
                        break;
                    }

                    for (size_t at = line; at + 4 <= crlf; ++at)
                    {
                        if (text[at] != u'N' || text[at + 1] != u'a' || text[at + 2] != u'm' || text[at + 3] != u'e')
                        {
                            continue;
                        }
                        if (text[at + 4] == u'=')
                        {
                            const size_t value = at + 5;
                            size_t count = crlf - value;
                            if (count > 0x7F)
                            {
                                count = 0x7F;
                            }
                            for (size_t n = 0; n < count; ++n)
                            {
                                dest[n] = static_cast<u16>(text[value + n]);
                            }
                            dest[count] = 0;
                            result = 1;
                        }
                        break;
                    }
                    if (result != 0)
                    {
                        break;
                    }
                    line = crlf + 2;
                }
            }
        }
        dir[combined + 1] = '\0';
    }
    return result;
}


// Xbox 0x0014C633
u32 xapi_create_save_metadata(const char* path, const u16* expected)
{
    // Xbox 0x0014C633
    // XCreateSaveGame (0x0014C70F) calls this with the hashed directory plus
    // "\\SaveMeta.xbx" and the wide save name. 0x0014D08C opens that path
    // GENERIC_READ|SYNCHRONIZE, share none, OPEN_ALWAYS, FILE_ATTRIBUTE_SYSTEM.
    // A failed open returns GetLastError. 0x0014C367 seeks to the start and
    // requires a 2-byte read; a FEFF word also passes. 0x0014C3AF then reads
    // L"Name" the same way 0x0014C532 does: the first hit whose next wchar is
    // '=' wins, including one inside L"TitleName=", and any other hit is
    // dropped through the next CR LF. The value is clipped to 0x7F wchars.
    // 0x00166E99 compares it with the caller's name. A match returns 0. A
    // short read, a missing Name, or a different name returns 0x491
    // (ERROR_NO_MATCH) after CloseHandle. OPEN_ALWAYS leaves an empty file
    // behind when the parent exists and the meta file does not.
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return g_xapi_last_error;
    }

    const std::filesystem::path fs_path(xbox_host_path(path));
    std::error_code ec;
    if (std::filesystem::is_directory(fs_path, ec))
    {
        const char last = path[std::strlen(path) - 1];
        g_xapi_last_error = last == '\\' ? 3u : 5u;
        return g_xapi_last_error;
    }
    if (ec && ec != std::errc::no_such_file_or_directory)
    {
        if (ec == std::errc::permission_denied)
        {
            g_xapi_last_error = 5;
        }
        else if (ec == std::errc::invalid_argument)
        {
            g_xapi_last_error = 123;
        }
        else if (ec.value() != 0)
        {
            g_xapi_last_error = static_cast<u32>(ec.value());
        }
        else
        {
            g_xapi_last_error = 3;
        }
        return g_xapi_last_error;
    }

    ec.clear();
    const bool present = std::filesystem::is_regular_file(fs_path, ec);
    if (!present)
    {
        if (ec && ec != std::errc::no_such_file_or_directory)
        {
            g_xapi_last_error = ec == std::errc::permission_denied ? 5u : 3u;
            return g_xapi_last_error;
        }
        const auto parent = fs_path.parent_path();
        std::error_code parent_ec;
        if (!parent.empty() && !std::filesystem::is_directory(parent, parent_ec))
        {
            g_xapi_last_error = 3;
            return g_xapi_last_error;
        }
        std::ofstream created(fs_path, std::ios::binary | std::ios::trunc);
        if (!created)
        {
            g_xapi_last_error = 5;
            return g_xapi_last_error;
        }
        g_xapi_last_error = 0;
    }
    else
    {
        g_xapi_last_error = 0xB7;
    }

    std::ifstream in(fs_path, std::ios::binary);
    if (!in)
    {
        g_xapi_last_error = 5;
        return g_xapi_last_error;
    }
    const std::string bytes(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});

    unsigned char probe[2] = {};
    if (!bytes.empty())
    {
        probe[0] = static_cast<unsigned char>(bytes[0]);
    }
    if (bytes.size() >= 2)
    {
        probe[1] = static_cast<unsigned char>(bytes[1]);
    }
    const u16 word = static_cast<u16>(probe[0] | (static_cast<unsigned>(probe[1]) << 8));
    if (bytes.size() < 2 && word != 0xFEFFu)
    {
        return 0x491;
    }

    std::u16string text;
    for (size_t i = 2; i + 1 < bytes.size(); i += 2)
    {
        text.push_back(static_cast<char16_t>(static_cast<unsigned char>(bytes[i]) |
                                              (static_cast<unsigned>(static_cast<unsigned char>(bytes[i + 1])) << 8)));
    }

    u16 found[0x80];
    found[0] = 0;
    bool matched_key = false;
    size_t line = 0;
    while (line < text.size())
    {
        size_t crlf = line;
        bool broke = false;
        while (crlf + 1 < text.size())
        {
            if (text[crlf] == u'\r' && text[crlf + 1] == u'\n')
            {
                broke = true;
                break;
            }
            ++crlf;
        }
        if (!broke)
        {
            break;
        }

        for (size_t at = line; at + 4 <= crlf; ++at)
        {
            if (text[at] != u'N' || text[at + 1] != u'a' || text[at + 2] != u'm' || text[at + 3] != u'e')
            {
                continue;
            }
            if (text[at + 4] == u'=')
            {
                const size_t value = at + 5;
                size_t count = crlf - value;
                if (count > 0x7F)
                {
                    count = 0x7F;
                }
                for (size_t n = 0; n < count; ++n)
                {
                    found[n] = static_cast<u16>(text[value + n]);
                }
                found[count] = 0;
                matched_key = true;
            }
            break;
        }
        if (matched_key)
        {
            break;
        }
        line = crlf + 2;
    }

    if (!matched_key || expected == nullptr)
    {
        return 0x491;
    }

    const u16* left = found;
    const u16* right = expected;
    while (*left != 0 && *left == *right)
    {
        ++left;
        ++right;
    }
    if (*left != *right)
    {
        return 0x491;
    }
    return 0;
}


// Xbox 0x0014C6B3
void xapi_set_save_directory_time(const char* path)
{
    // Xbox 0x0014C6B3
    // 0x0014D08C opens the path with GENERIC_READ|GENERIC_WRITE, share
    // read|write, a null security descriptor, OPEN_EXISTING, and
    // FILE_FLAG_BACKUP_SEMANTICS, so the handle may be a directory. A handle
    // other than -1 zeros 0x28 bytes of FILE_BASIC_INFORMATION, stores
    // KeQuerySystemTime at +0x10 (LastWriteTime), and calls NtSetInformationFile
    // with class 4. Creation, access, change, and FileAttributes stay 0, which
    // leaves those fields unchanged. CloseHandle follows. A failed open returns
    // immediately; 0x0014D08C has already stored the Win32 error.
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return;
    }

    const std::filesystem::path fs_path(xbox_host_path(path));
    std::error_code ec;
    if (!std::filesystem::exists(fs_path, ec) || ec)
    {
        if (!ec)
        {
            ec = std::make_error_code(std::errc::no_such_file_or_directory);
        }
        xapi_set_dir_open_error(fs_path, ec);
        return;
    }

    std::filesystem::last_write_time(fs_path, std::filesystem::file_time_type::clock::now(), ec);
}


// XcSHA context, 0x74 bytes. count[0] is the high half of the byte length
// and count[1] the low half.
struct XapiShaContext
{
    u32 unknown[6];
    u32 state[5];
    u32 count[2];
    u8 buffer[64];
};


static_assert(sizeof(XapiShaContext) == 0x74);


struct XapiCalcSigContext
{
    u32 flags;
    const u8* key;
    XapiShaContext sha;
};


void xapi_sha_block(u32 state[5], const u8 block[64])
{
    u32 w[80];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = (static_cast<u32>(block[i * 4]) << 24) | (static_cast<u32>(block[i * 4 + 1]) << 16) |
               (static_cast<u32>(block[i * 4 + 2]) << 8) | static_cast<u32>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i)
    {
        w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    u32 a = state[0];
    u32 b = state[1];
    u32 c = state[2];
    u32 d = state[3];
    u32 e = state[4];
    for (int i = 0; i < 80; ++i)
    {
        u32 f = 0;
        u32 k = 0;
        if (i < 20)
        {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        }
        else if (i < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        }
        else if (i < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        const u32 temp = std::rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = std::rotl(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}


void xapi_sha_init(XapiShaContext& ctx)
{
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.state[0] = 0x67452301u;
    ctx.state[1] = 0xEFCDAB89u;
    ctx.state[2] = 0x98BADCFEu;
    ctx.state[3] = 0x10325476u;
    ctx.state[4] = 0xC3D2E1F0u;
}


void xapi_sha_update(XapiShaContext& ctx, const u8* data, u32 size)
{
    u32 filled = ctx.count[1] & 63u;
    const u32 low = ctx.count[1];
    ctx.count[1] = low + size;
    if (ctx.count[1] < size)
    {
        ctx.count[0] += 1;
    }
    ctx.count[0] += size >> 29;

    if (filled + size < 64u)
    {
        std::memcpy(ctx.buffer + filled, data, size);
        return;
    }

    while (filled + size >= 64u)
    {
        const u32 take = 64u - filled;
        std::memcpy(ctx.buffer + filled, data, take);
        data += take;
        size -= take;
        xapi_sha_block(ctx.state, ctx.buffer);
        filled = 0;
    }
    if (size != 0)
    {
        std::memcpy(ctx.buffer + filled, data, size);
    }
}


void xapi_sha_final(XapiShaContext& ctx, u8* out)
{
    const u64 bits = (static_cast<u64>(ctx.count[0]) << 32 | ctx.count[1]) << 3;
    u8 tail[72];
    u32 n = 0;
    tail[n++] = 0x80;
    const u32 mod = (ctx.count[1] + 1u) & 63u;
    const u32 zeros = (56u - mod) & 63u;
    for (u32 i = 0; i < zeros; ++i)
    {
        tail[n++] = 0;
    }
    for (int i = 0; i < 8; ++i)
    {
        tail[n++] = static_cast<u8>(bits >> ((7 - i) * 8));
    }
    xapi_sha_update(ctx, tail, n);
    for (int i = 0; i < 5; ++i)
    {
        out[i * 4] = static_cast<u8>(ctx.state[i] >> 24);
        out[i * 4 + 1] = static_cast<u8>(ctx.state[i] >> 16);
        out[i * 4 + 2] = static_cast<u8>(ctx.state[i] >> 8);
        out[i * 4 + 3] = static_cast<u8>(ctx.state[i]);
    }
}


// Retail certificate TitleID is 0x4D530053. AlternateTitleIDs are all zero.
// XboxSignatureKey is the 16-byte kernel export for this title. The sixteen
// XboxAlternateSignatureKeys follow it; a scan that finishes at index 16
// reads one key past that export.
constexpr u32 kCalcSigTitleId = 0x4D530053u;

constexpr u32 kCalcSigAlternateTitleIds[16] = {};

constexpr u8 kXboxSignatureKey[16] = {
    0x26, 0x18, 0xC5, 0x08, 0x88, 0x8E, 0x75, 0xB9, 0x36, 0x3E, 0x23, 0xDA, 0x8F, 0xE0, 0x2D, 0x37};

constexpr u8 kXboxAlternateSignatureKeys[17][16] = {
    {0xD9, 0x0D, 0xBF, 0xE7, 0x68, 0x73, 0x41, 0xBF, 0xBC, 0x9A, 0xD8, 0x89, 0xB6, 0x3E, 0xBF, 0x7B},
    {0x4C, 0x3F, 0x43, 0x3A, 0x19, 0x27, 0x8C, 0x19, 0x5E, 0x12, 0x08, 0x1E, 0xF7, 0x40, 0xC9, 0xE3},
    {0xA5, 0x77, 0xD7, 0xC9, 0x17, 0x18, 0xAC, 0x79, 0xEB, 0xBB, 0x22, 0xED, 0xF7, 0xEA, 0xFC, 0xB3},
    {0xBE, 0x2F, 0x42, 0x2A, 0x8A, 0xD9, 0x8A, 0xE0, 0x2B, 0xB5, 0x67, 0x0A, 0x87, 0xE4, 0x23, 0x0A},
    {0x65, 0x56, 0x61, 0x54, 0x63, 0x62, 0x23, 0x23, 0x7C, 0x42, 0x65, 0xC8, 0x43, 0x8C, 0x02, 0x61},
    {0xB3, 0xEB, 0xDC, 0xB3, 0x1A, 0x19, 0xEE, 0xD6, 0x86, 0xCE, 0x2C, 0x84, 0xF5, 0x03, 0x26, 0x50},
    {0xA7, 0xBB, 0x8B, 0x34, 0x72, 0xC0, 0x5C, 0x72, 0x61, 0x0B, 0x5B, 0x7B, 0x20, 0xCC, 0x20, 0x81},
    {0x43, 0xBE, 0xB4, 0x77, 0xF3, 0xC8, 0xEB, 0xE5, 0x0B, 0xCA, 0x04, 0x61, 0x93, 0xF7, 0x2A, 0x1C},
    {0x4A, 0x94, 0x4D, 0x0A, 0x4A, 0x65, 0x54, 0x38, 0xEE, 0x3C, 0x8E, 0x64, 0xD5, 0xFD, 0xA3, 0xBE},
    {0xEF, 0xA5, 0x62, 0xC5, 0x2D, 0x48, 0x63, 0xE2, 0x92, 0x05, 0xEE, 0x58, 0x78, 0xD1, 0xE6, 0x40},
    {0x13, 0x2E, 0x78, 0x97, 0x61, 0x9E, 0x07, 0x79, 0x92, 0x9B, 0xDA, 0x3A, 0x56, 0x75, 0x83, 0xE7},
    {0x01, 0x30, 0xDA, 0x18, 0x70, 0x09, 0x59, 0x27, 0x34, 0x96, 0xB4, 0xF4, 0x92, 0x48, 0x60, 0xE0},
    {0xC3, 0xED, 0xA2, 0xB3, 0xA1, 0x45, 0xF7, 0x17, 0x01, 0x8F, 0x34, 0xBC, 0xCA, 0x66, 0xF0, 0x25},
    {0x6B, 0xC6, 0xC8, 0x47, 0xB2, 0x19, 0xBC, 0xFC, 0xD7, 0xAD, 0x7C, 0xCA, 0xB4, 0x2F, 0xCA, 0x15},
    {0x0A, 0x6E, 0xA0, 0xA9, 0x97, 0x13, 0x68, 0x1C, 0x2A, 0xDC, 0xA4, 0x5F, 0x3B, 0x05, 0x91, 0xB3},
    {0x3F, 0x77, 0x3A, 0x8D, 0x9A, 0x0D, 0xB3, 0x7A, 0xA9, 0x99, 0xBB, 0x1E, 0x51, 0x55, 0xFD, 0xC8},
    {}};


// Xbox 0x0014CD37
std::uintptr_t xapi_begin_keyed_signature(u32 flags, u32 title_id)
{
    // Xbox 0x0014CD37
    // XCalculateSignatureBeginEx (0x0014CEB4) calls this for flags 0 and 1.
    // XMemAlloc(0x7C, 0x24830000). Failure stores last-error 8 and returns
    // -1. The flags occupy the first dword. Certificate TitleID at
    // [0x00010118]+8 selects XboxSignatureKey (thunk 0x00202F9C). Any other
    // id walks AlternateTitleIDs at certificate +0x5C, sixteen dwords. A zero
    // id stores last-error 5 and returns -1. A hit, or a walk that ends below
    // index 0x40, selects XboxAlternateSignatureKeys (thunk 0x00202FA0) plus
    // index * 16. The key pointer is stored at the next dword. 0x0014CBE9
    // copies 0x10 key bytes into a 0x40 block, XORs each dword with
    // 0x36363636, then XcSHAInit and XcSHAUpdate of that block fill the
    // 0x74-byte SHA context. The port releases the block when the id is
    // rejected.
    auto* ctx = new (std::nothrow) XapiCalcSigContext;
    if (ctx == nullptr)
    {
        g_xapi_last_error = 8;
        return static_cast<std::uintptr_t>(-1);
    }

    ctx->flags = flags;
    const u8* key = kXboxSignatureKey;
    if (title_id != kCalcSigTitleId)
    {
        u32 index = 0;
        while (index < 0x10)
        {
            const u32 alt = kCalcSigAlternateTitleIds[index];
            if (alt == 0)
            {
                delete ctx;
                g_xapi_last_error = 5;
                return static_cast<std::uintptr_t>(-1);
            }
            if (title_id == alt)
            {
                break;
            }
            ++index;
        }
        if (index >= 0x40)
        {
            delete ctx;
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }
        key = kXboxAlternateSignatureKeys[index];
    }

    ctx->key = key;
    u8 block[64] = {};
    std::memcpy(block, key, 0x10);
    for (int i = 0; i < 16; ++i)
    {
        u32 word = 0;
        std::memcpy(&word, block + i * 4, 4);
        word ^= 0x36363636u;
        std::memcpy(block + i * 4, &word, 4);
    }
    xapi_sha_init(ctx->sha);
    xapi_sha_update(ctx->sha, block, 0x40);
    return reinterpret_cast<std::uintptr_t>(ctx);
}


// Kernel XboxHDKey (thunk 0x00202F98, ordinal 323). Per console, not stored
// in the XBE. All zeros sends 0x0014CCC2 through ExQueryNonVolatileSetting.
constexpr u8 kXboxHDKey[16] = {};


void xapi_hmac_begin(XapiShaContext& ctx, const u8* key, u32 key_len)
{
    // Xbox 0x0014CBE9. Cap the key at 0x40, zero-pad, XOR each dword with
    // 0x36363636, then XcSHAInit and XcSHAUpdate of that block.
    if (key_len > 0x40)
    {
        key_len = 0x40;
    }
    u8 block[64] = {};
    if (key != nullptr && key_len != 0)
    {
        std::memcpy(block, key, key_len);
    }
    for (int i = 0; i < 16; ++i)
    {
        u32 word = 0;
        std::memcpy(&word, block + i * 4, 4);
        word ^= 0x36363636u;
        std::memcpy(block + i * 4, &word, 4);
    }
    xapi_sha_init(ctx);
    xapi_sha_update(ctx, block, 0x40);
}


void xapi_hmac_final(XapiShaContext& ctx, const u8* key, u32 key_len, u8* out)
{
    // Xbox 0x0014CC50. Cap the key at 0x40, zero-pad, XOR each dword with
    // 0x5C5C5C5C. XcSHAFinal writes the inner digest into the 20 bytes that
    // follow that block. XcSHAInit, then one XcSHAUpdate of those 0x54 bytes,
    // then XcSHAFinal into out.
    if (key_len > 0x40)
    {
        key_len = 0x40;
    }
    u8 block[0x54] = {};
    if (key != nullptr && key_len != 0)
    {
        std::memcpy(block, key, key_len);
    }
    for (int i = 0; i < 16; ++i)
    {
        u32 word = 0;
        std::memcpy(&word, block + i * 4, 4);
        word ^= 0x5C5C5C5Cu;
        std::memcpy(block + i * 4, &word, 4);
    }
    xapi_sha_final(ctx, block + 0x40);
    xapi_sha_init(ctx);
    xapi_sha_update(ctx, block, 0x54);
    xapi_sha_final(ctx, out);
}


int xapi_signature_nonroamable(XapiCalcSigContext& ctx, const u8* old_sig, u8* out)
{
    // Xbox 0x0014CCC2. OR the 16 XboxHDKey bytes. Any nonzero byte is the
    // HMAC key. All zeros calls ExQueryNonVolatileSetting(0xFFFF, type,
    // 256-byte image, result length) and uses the 16 bytes at image+0x36.
    // The status is ignored. The inner SHA is restarted on the signature
    // context (0x0014CBE9), updated with the 20-byte roamable digest, then
    // finished by 0x0014CC50 into out. Always returns 1.
    const u8* key = kXboxHDKey;
    u8 image[0x100] = {};
    bool any = false;
    for (u8 byte : kXboxHDKey)
    {
        if (byte != 0)
        {
            any = true;
            break;
        }
    }
    if (!any)
    {
        key = image + 0x36;
    }
    xapi_hmac_begin(ctx.sha, key, 0x10);
    xapi_sha_update(ctx.sha, old_sig, 0x14);
    xapi_hmac_final(ctx.sha, key, 0x10, out);
    return 1;
}


// Xbox 0x0014CDB7
u32 xapi_end_keyed_signature(XapiCalcSigContext* ctx, u8* signature)
{
    // Xbox 0x0014CDB7
    // XCalculateSignatureEnd for flags 0 and 1 (dispatcher 0x0014CF57).
    // A null signature skips the digest and still frees the context. Otherwise
    // 0x0014CC50 writes the 20-byte HMAC. Flag bit 0 (non-roamable) lands that
    // digest in a 20-byte stack buffer and 0x0014CCC2 HMACs it with XboxHDKey
    // into the caller's buffer. XMemFree uses tag 0x24830000. A zero from
    // 0x0014CCC2 returns GetLastError; that helper returns 1.
    int ok = 1;
    if (signature != nullptr)
    {
        u8 local[20];
        u8* dest = signature;
        if ((ctx->flags & 1u) != 0)
        {
            dest = local;
        }
        xapi_hmac_final(ctx->sha, ctx->key, 0x10, dest);
        if ((ctx->flags & 1u) != 0)
        {
            ok = xapi_signature_nonroamable(*ctx, local, signature);
        }
    }
    delete ctx;
    if (ok == 0)
    {
        return g_xapi_last_error;
    }
    return 0;
}


// Xbox 0x0014CE19
std::uintptr_t xapi_begin_sha_signature()
{
    // Xbox 0x0014CE19
    // XCalculateSignatureBeginEx (0x0014CEB4) for flag 2. XMemAlloc(0x7C,
    // 0x24830000). Failure stores last-error 8 and returns -1. The flags dword
    // is 2 and the key pointer is null. XcSHAInit fills the 0x74-byte SHA
    // context. No signature-key block is mixed in. 0x0014CE4F updates that
    // SHA and 0x0014CE69 finishes it.
    auto* ctx = new (std::nothrow) XapiCalcSigContext;
    if (ctx == nullptr)
    {
        g_xapi_last_error = 8;
        return static_cast<std::uintptr_t>(-1);
    }

    ctx->key = nullptr;
    ctx->flags = 2;
    xapi_sha_init(ctx->sha);
    return reinterpret_cast<std::uintptr_t>(ctx);
}


// Xbox 0x0014CE4F
u32 xapi_update_signature(XapiCalcSigContext* ctx, const u8* data, u32 size)
{
    // Xbox 0x0014CE4F
    // XCalculateSignatureUpdate. XcSHAUpdate feeds the caller's bytes into
    // the SHA context that sits 8 bytes into the block from 0x0014CD37 or
    // 0x0014CE19. Always returns 0.
    xapi_sha_update(ctx->sha, data, size);
    return 0;
}


// Xbox 0x0014CE69
u32 xapi_end_sha_signature(XapiCalcSigContext* ctx, u8* signature)
{
    // Xbox 0x0014CE69
    // XCalculateSignatureEnd for flag 2 (dispatcher 0x0014CF57). A null
    // signature skips the digest and still frees the context. Otherwise
    // XcSHAFinal writes the 20-byte SHA-1 into the caller's buffer from the
    // context started by 0x0014CE19. XMemFree uses tag 0x24830000. Always
    // returns 0.
    if (signature != nullptr)
    {
        xapi_sha_final(ctx->sha, signature);
    }
    delete ctx;
    return 0;
}


// Host stand-in for the kernel handle NtCreateFile returns. CloseHandle is a
// separate call; until then the stream stays open and delete-on-close waits.
struct XapiFileHandle
{
    std::filesystem::path path;
    bool directory = false;
    bool delete_on_close = false;
    std::unique_ptr<std::fstream> stream;
};


// Xbox 0x0014D08C
std::uintptr_t xapi_create_file(const char* name, u32 access, u32 share, const void* security, u32 disposition, u32 flags,
                         const void* template_file)
{
    // Xbox 0x0014D08C
    // CreateFileA. The security pointer and template handle are unused.
    // Disposition 1..5 becomes FILE_CREATE, FILE_OVERWRITE_IF, FILE_OPEN,
    // FILE_OPEN_IF, or FILE_OVERWRITE. TRUNCATE_EXISTING also requires
    // GENERIC_WRITE (0x40000000); any other disposition stores
    // XapiSetLastNTError(STATUS_INVALID_PARAMETER) and returns -1.
    // RtlInitAnsiString records whether the 16-bit length ends in '\\'.
    // NtCreateFile uses ObDosDevicesDirectory (0xFFFFFFFD),
    // OBJ_CASE_INSENSITIVE, access | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
    // attributes flags & 0x7FA7, the caller's share mode, and create options
    // folded from the Win32 flags: write-through, overlapped (drops
    // FILE_SYNCHRONOUS_IO_NONALERT), no-buffering, random, sequential,
    // delete-on-close (also ORs DELETE into the access), and backup
    // semantics (FILE_OPEN_FOR_BACKUP_INTENT, and not FILE_NON_DIRECTORY_FILE).
    // Success returns the handle. CREATE_ALWAYS plus FILE_OVERWRITTEN, or
    // OPEN_ALWAYS plus FILE_OPENED, stores 0xB7; every other success stores 0.
    // STATUS_OBJECT_NAME_COLLISION stores 0x50. STATUS_FILE_IS_A_DIRECTORY
    // stores 3 when the name ends in '\\' and 5 otherwise. Other failures
    // keep the Win32 code from XapiSetLastNTError.
    // The port opens a host path. Share mode is not enforced. A directory
    // opened with FILE_FLAG_BACKUP_SEMANTICS yields a handle with no stream.
    (void)share;
    (void)security;
    (void)template_file;

    constexpr u32 kGenericWrite = 0x40000000u;
    constexpr u32 kFileCreate = 2;
    constexpr u32 kFileOverwriteIf = 5;
    constexpr u32 kFileOpen = 1;
    constexpr u32 kFileOpenIf = 3;
    constexpr u32 kFileOverwrite = 4;
    constexpr u32 kFileOpened = 1;
    constexpr u32 kFileCreated = 2;
    constexpr u32 kFileOverwritten = 3;

    u32 nt_disposition = 0;
    if (disposition == 1)
    {
        nt_disposition = kFileCreate;
    }
    else if (disposition == 2)
    {
        nt_disposition = kFileOverwriteIf;
    }
    else if (disposition == 3)
    {
        nt_disposition = kFileOpen;
    }
    else if (disposition == 4)
    {
        nt_disposition = kFileOpenIf;
    }
    else if (disposition == 5 && (access & kGenericWrite) != 0)
    {
        nt_disposition = kFileOverwrite;
    }
    else
    {
        g_xapi_last_error = 0x57;
        return static_cast<std::uintptr_t>(-1);
    }

    if (name == nullptr)
    {
        g_xapi_last_error = 123;
        return static_cast<std::uintptr_t>(-1);
    }

    const u16 ansi_length = static_cast<u16>(std::strlen(name));
    const bool ends_with_slash = ansi_length >= 2 && name[ansi_length - 1] == '\\';
    if (ansi_length == 0)
    {
        g_xapi_last_error = 123;
        return static_cast<std::uintptr_t>(-1);
    }

    const bool backup = (flags & 0x02000000u) != 0;
    const bool delete_on_close = (flags & 0x04000000u) != 0;
    const u32 attributes = flags & 0x7FA7u;
    const bool want_write = (access & kGenericWrite) != 0 || (access & 0x2u) != 0 || (access & 0x4u) != 0;
    const bool truncate = nt_disposition == kFileOverwrite || nt_disposition == kFileOverwriteIf;

    const std::filesystem::path fs_path(xbox_host_path(name));
    std::error_code ec;
    const bool exists = std::filesystem::exists(fs_path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
    {
        if (ec == std::errc::permission_denied)
        {
            g_xapi_last_error = 5;
        }
        else if (ec == std::errc::invalid_argument)
        {
            g_xapi_last_error = 123;
        }
        else if (ec.value() != 0)
        {
            g_xapi_last_error = static_cast<u32>(ec.value());
        }
        else
        {
            g_xapi_last_error = 3;
        }
        return static_cast<std::uintptr_t>(-1);
    }

    bool is_directory = false;
    if (exists)
    {
        ec.clear();
        is_directory = std::filesystem::is_directory(fs_path, ec);
        if (ec)
        {
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }
    }

    auto parent_is_directory = [](const std::filesystem::path& path)
    {
        const auto parent = path.parent_path();
        if (parent.empty())
        {
            return true;
        }
        std::error_code parent_ec;
        return std::filesystem::is_directory(parent, parent_ec) && !parent_ec;
    };

    auto missing_error = [&]()
    {
        std::filesystem::path leaf = fs_path;
        if (leaf.filename().empty())
        {
            leaf = leaf.parent_path();
        }
        const auto parent = leaf.parent_path();
        std::error_code parent_ec;
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            g_xapi_last_error = 3;
        }
        else if (ends_with_slash || nt_disposition == kFileCreate || nt_disposition == kFileOpenIf ||
                 nt_disposition == kFileOverwriteIf)
        {
            g_xapi_last_error = 3;
        }
        else
        {
            g_xapi_last_error = 2;
        }
    };

    if (is_directory && nt_disposition == kFileCreate)
    {
        g_xapi_last_error = 0x50;
        return static_cast<std::uintptr_t>(-1);
    }
    if (is_directory && !backup)
    {
        g_xapi_last_error = ends_with_slash ? 3u : 5u;
        return static_cast<std::uintptr_t>(-1);
    }
    if (is_directory && (nt_disposition == kFileOverwrite || nt_disposition == kFileOverwriteIf))
    {
        g_xapi_last_error = ends_with_slash ? 3u : 5u;
        return static_cast<std::uintptr_t>(-1);
    }

    if (!exists)
    {
        if (nt_disposition == kFileOpen || nt_disposition == kFileOverwrite || ends_with_slash ||
            !parent_is_directory(fs_path))
        {
            missing_error();
            return static_cast<std::uintptr_t>(-1);
        }
    }

    if (exists && !is_directory && nt_disposition == kFileCreate)
    {
        g_xapi_last_error = 0x50;
        return static_cast<std::uintptr_t>(-1);
    }

    const bool creating = !exists;
    if (exists && !is_directory && (truncate || want_write))
    {
        std::error_code perm_ec;
        const auto perms = std::filesystem::status(fs_path, perm_ec).permissions();
        if (!perm_ec && (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
        {
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }
    }

    auto* handle = new (std::nothrow) XapiFileHandle;
    if (handle == nullptr)
    {
        g_xapi_last_error = 8;
        return static_cast<std::uintptr_t>(-1);
    }
    handle->path = fs_path;
    handle->directory = is_directory;
    handle->delete_on_close = delete_on_close;

    u32 information = kFileOpened;
    if (!is_directory)
    {
        if (truncate || creating)
        {
            std::ofstream created(fs_path, std::ios::binary | std::ios::trunc);
            if (!created)
            {
                delete handle;
                g_xapi_last_error = 5;
                return static_cast<std::uintptr_t>(-1);
            }
            information = creating ? kFileCreated : kFileOverwritten;
        }

        auto mode = std::ios::binary | std::ios::in;
        if (want_write || truncate)
        {
            mode |= std::ios::out;
        }

        handle->stream = std::make_unique<std::fstream>(fs_path, mode);
        if (!handle->stream->is_open())
        {
            delete handle;
            if (creating)
            {
                std::error_code remove_ec;
                std::filesystem::remove(fs_path, remove_ec);
            }
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }

        if ((creating || disposition == 2) && (attributes & 1u) != 0)
        {
            std::error_code perm_ec;
            std::filesystem::permissions(fs_path, std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::remove, perm_ec);
        }
    }

    if ((disposition == 2 && information == kFileOverwritten) || (disposition == 4 && information == kFileOpened))
    {
        g_xapi_last_error = 0xB7;
    }
    else
    {
        g_xapi_last_error = 0;
    }
    return reinterpret_cast<std::uintptr_t>(handle);
}


// Xbox 0x0014D217
bool xapi_create_directory(const char* path, const void* security)
{
    // Xbox 0x0014D217
    // CreateDirectoryA for XCreateSaveGame. The security pointer is unused.
    // RtlInitAnsiString, then NtCreateFile on ObDosDevicesDirectory
    // (0xFFFFFFFD) with OBJ_CASE_INSENSITIVE, FILE_LIST_DIRECTORY|SYNCHRONIZE,
    // FILE_ATTRIBUTE_NORMAL, share read|write, FILE_CREATE, and
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // Success closes the handle. Failure stores the Win32 error: 0xB7 when
    // the name is taken, 3 when a parent is missing.
    (void)security;
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return false;
    }

    std::error_code ec;
    if (std::filesystem::create_directory(std::filesystem::path(xbox_host_path(path)), ec))
    {
        return true;
    }

    if (!ec || ec == std::errc::file_exists)
    {
        g_xapi_last_error = 0xB7;
    }
    else if (ec == std::errc::no_such_file_or_directory)
    {
        g_xapi_last_error = 3;
    }
    else if (ec == std::errc::permission_denied)
    {
        g_xapi_last_error = 5;
    }
    else if (ec == std::errc::invalid_argument)
    {
        g_xapi_last_error = 123;
    }
    else if (ec.value() != 0)
    {
        g_xapi_last_error = static_cast<u32>(ec.value());
    }
    else
    {
        g_xapi_last_error = 0xB7;
    }
    return false;
}


// Xbox 0x0014D283
bool xapi_remove_directory(const char* path)
{
    // Xbox 0x0014D283
    // RemoveDirectoryA. RtlInitAnsiString, then NtOpenFile on
    // ObDosDevicesDirectory (0xFFFFFFFD) with OBJ_CASE_INSENSITIVE,
    // DELETE|SYNCHRONIZE, share read|write|delete, and
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // Success writes FileDispositionInformation (delete) and NtClose. Failure
    // stores the Win32 error: 145 when the directory is not empty, 267 when
    // the path is not a directory, 2 when the name is missing, 3 when a parent
    // is missing.
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return false;
    }

    std::error_code ec;
    const std::filesystem::path fs_path(xbox_host_path(path));
    if (std::filesystem::is_directory(fs_path, ec))
    {
        ec.clear();
        if (std::filesystem::remove(fs_path, ec))
        {
            return true;
        }
    }
    else if (!ec)
    {
        std::error_code exists_ec;
        if (std::filesystem::exists(fs_path, exists_ec))
        {
            g_xapi_last_error = 267;
            return false;
        }
    }

    if (!ec || ec == std::errc::no_such_file_or_directory)
    {
        std::error_code parent_ec;
        const auto parent = fs_path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            g_xapi_last_error = 3;
        }
        else
        {
            g_xapi_last_error = 2;
        }
    }
    else if (ec == std::errc::directory_not_empty)
    {
        g_xapi_last_error = 145;
    }
    else if (ec == std::errc::permission_denied)
    {
        g_xapi_last_error = 5;
    }
    else if (ec == std::errc::invalid_argument)
    {
        g_xapi_last_error = 123;
    }
    else if (ec.value() != 0)
    {
        g_xapi_last_error = static_cast<u32>(ec.value());
    }
    else
    {
        g_xapi_last_error = 145;
    }
    return false;
}


// DAT_0032CA00, DAT_0032CA04, DAT_0032CA0C, DAT_0032CA08. Retail .data defaults
// for a zeroed RTL_HEAP_PARAMETERS block.
constexpr u32 kHeapSegmentReserve = 0x100000;

constexpr u32 kHeapSegmentCommit = 0x2000;

constexpr u32 kHeapDecommitFree = 0x1000;

constexpr u32 kHeapDecommitTotal = 0x10000;


// DAT_0054951D bit 3 inserts the 0x60c debug tail. DAT_0054951E bit 5 ORs
// 0x80 into the create flags. The retail image leaves both bytes zero.
u8 g_heap_nt_flag_1d = 0;

u8 g_heap_nt_flag_1e = 0;


#if defined(_WIN32)
struct HeapVm
{
    void*(__stdcall* alloc)(void* address, std::uintptr_t size, unsigned long type, unsigned long protect);
    int(__stdcall* free)(void* address, std::uintptr_t size, unsigned long type);
    std::uintptr_t(__stdcall* query)(const void* address, void* buffer, std::uintptr_t length);
};

struct HeapMemInfo
{
    void* base;
    void* allocation_base;
    unsigned long allocation_protect;
#if defined(_WIN64)
    unsigned short partition_id;
    unsigned short pad;
#endif
    std::uintptr_t region_size;
    unsigned long state;
    unsigned long protect;
    unsigned long type;
};

u32 heap_get_u32(u32 addr)
{
    u32 value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), 4);
    return value;
}

void heap_set_u32(u32 addr, u32 value)
{
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(addr)), &value, 4);
}

u16 heap_get_u16(u32 addr)
{
    u16 value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), 2);
    return value;
}

void heap_set_u16(u32 addr, u16 value)
{
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(addr)), &value, 2);
}

u8* heap_at(u32 addr)
{
    return reinterpret_cast<u8*>(static_cast<std::uintptr_t>(addr));
}

u32 heap_page_span(u32 end, u32 start)
{
    const i32 bytes = static_cast<i32>(end - start);
    return static_cast<u32>(bytes / static_cast<i32>(0x1000));
}

HeapVm heap_vm()
{
    HeapVm vm{};
    void* kernel = GetModuleHandleA("kernel32.dll");
    if (kernel == nullptr)
    {
        return vm;
    }
    void* alloc_proc = GetProcAddress(kernel, "VirtualAlloc");
    void* free_proc = GetProcAddress(kernel, "VirtualFree");
    void* query_proc = GetProcAddress(kernel, "VirtualQuery");
    std::memcpy(&vm.alloc, &alloc_proc, sizeof(vm.alloc));
    std::memcpy(&vm.free, &free_proc, sizeof(vm.free));
    std::memcpy(&vm.query, &query_proc, sizeof(vm.query));
    return vm;
}

void* heap_reserve(HeapVm vm, u32 bytes)
{
    constexpr u32 kStep = 0x1000000;
    for (u32 hint = 0x20000000u; hint < 0x70000000u; hint += kStep)
    {
        if (static_cast<u64>(hint) + bytes > 0x100000000ull)
        {
            break;
        }
        void* block = vm.alloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(hint)), bytes, 0x2000u, 4u);
        if (block != nullptr)
        {
            return block;
        }
    }
    return nullptr;
}

// 0x0014D9BC. Link `units` of free 16-byte granules at `entry`. Sizes below
// 0x80 take the dedicated list at heap+0x180 and set the bitmap at heap+0x160.
// Larger sizes walk the list at heap+0x180 in ascending size order. A run
// above 0xff00 is split; 0xff01 keeps a 0x11-granule tail instead of a
// one-granule fragment. Bit 0x10 on the entry suppresses the following
// entry's previous-size store.
void heap_add_free(u32 heap_addr, u32 entry, u32 units)
{
    const u8 index = heap_at(entry)[4];
    const u8 saved_flags = heap_at(entry)[5];
    u16 previous = heap_get_u16(entry + 2);
    heap_set_u32(heap_addr + 0x30, heap_get_u32(heap_addr + 0x30) + units);

    const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(index) * 4u + 0x60u);
    const u32 limit = segment == 0 ? 0u : heap_get_u32(segment + 0x2c);

    if (units == 0)
    {
        return;
    }

    while (units != 0)
    {
        u32 chunk = units;
        u8 flags = saved_flags;
        if (units > 0xff00u)
        {
            chunk = 0xff00u;
            if (units == 0xff01u)
            {
                chunk = 0xfeF0u;
            }
            flags = 0;
        }
        flags = static_cast<u8>(flags & 0xf8u);
        heap_set_u16(entry + 2, previous);
        heap_at(entry)[4] = index;
        heap_at(entry)[5] = flags;
        heap_set_u16(entry, static_cast<u16>(chunk));

        u32 before = 0;
        if (chunk >= 0x80u)
        {
            const u32 head = heap_addr + 0x180u;
            before = heap_get_u32(head);
            while (before != head)
            {
                if (static_cast<u16>(chunk) <= heap_get_u16(before - 8u))
                {
                    break;
                }
                before = heap_get_u32(before);
            }
        }
        else
        {
            before = heap_addr + 0x180u + chunk * 8u;
            if (heap_get_u32(before) == before)
            {
                u8* map = heap_at(heap_addr + 0x160u + (chunk >> 3));
                *map = static_cast<u8>(*map | static_cast<u8>(1u << (chunk & 7u)));
            }
        }

        const u32 blink = heap_get_u32(before + 4u);
        heap_set_u32(entry + 8u, before);
        heap_set_u32(entry + 0xcu, blink);
        heap_set_u32(blink, entry + 8u);
        heap_set_u32(before + 4u, entry + 8u);

        units -= chunk;
        entry += chunk << 4;
        previous = static_cast<u16>(chunk);
        if (limit != 0 && entry >= limit)
        {
            return;
        }
    }

    if ((saved_flags & 0x10u) == 0)
    {
        heap_set_u16(entry + 2u, previous);
    }
}

// 0x0014DEFF. `segment` sits on the 16-byte header boundary. The segment
// signature is 0xFFEEFFEE. `segment_index` is the byte stored at entry+4
// and the slot at heap+0x60. Granules past the segment header through the
// committed end become one free entry via 0x0014D9BC. Uncommitted pages are
// recorded on a descriptor popped from heap+0x4c. A commit that cannot
// cover the segment header returns false.
bool heap_init_segment(HeapVm vm, u32 heap_addr, u32 segment, u32 segment_index, u32 commit_base, u32 commit_end,
                       u32 reserve_end, u32 user_flag)
{
    const u32 first = (segment + 0x57u) & ~0xfu;
    const u32 header_units = (first - segment) >> 4;
    if (first + 0x10u >= commit_end)
    {
        if (first + 0x10u >= reserve_end)
        {
            return false;
        }
        u32 extra = first - commit_end + 0x10u;
        extra = (extra + 0xfffu) & ~0xfffu;
        if (vm.alloc(heap_at(commit_end), extra, 0x1000u, 4u) == nullptr)
        {
            return false;
        }
        commit_end += extra;
    }

    const u32 reserve_pages = heap_page_span(reserve_end, commit_base);
    const u32 uncommitted_pages = heap_page_span(reserve_end, commit_end);
    const u16 heap_units = heap_addr == commit_base ? heap_get_u16(heap_addr) : 0;

    const u8 index_byte = static_cast<u8>(segment_index);
    heap_set_u16(segment, static_cast<u16>(header_units));
    heap_set_u16(segment + 2u, heap_units);
    heap_at(segment)[4] = index_byte;
    heap_at(segment)[5] = 1;
    heap_set_u32(segment + 0x10u, 0xffeeffeeu);
    heap_set_u32(segment + 0x14u, user_flag);
    heap_set_u32(segment + 0x18u, heap_addr);
    heap_set_u32(segment + 0x20u, commit_base);
    heap_set_u32(segment + 0x24u, reserve_pages);
    heap_set_u32(segment + 0x28u, first);
    heap_set_u32(segment + 0x2cu, commit_base + (reserve_pages << 12));
    heap_at(segment)[5] = 1;
    heap_set_u32(segment + 0x30u, uncommitted_pages);

    if (uncommitted_pages != 0)
    {
        const u32 node = heap_get_u32(heap_addr + 0x4cu);
        if (node != 0)
        {
            heap_set_u32(heap_addr + 0x4cu, heap_get_u32(node));
            const u32 bytes = uncommitted_pages << 12;
            heap_set_u32(node + 4u, commit_end);
            heap_set_u32(node + 8u, bytes);
            heap_set_u32(node, heap_get_u32(segment + 0x38u));
            heap_set_u32(segment + 0x38u, node);
            heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) + 1u);
            if (bytes >= heap_get_u32(segment + 0x1cu))
            {
                heap_set_u32(segment + 0x1cu, bytes);
            }
        }
    }

    heap_set_u32(heap_addr + 0x60u + static_cast<u32>(index_byte) * 4u, segment);
    heap_at(first)[5] = 0x10;
    heap_set_u32(segment + 0x40u, first);
    heap_set_u16(first + 2u, heap_get_u16(segment));
    heap_at(first)[4] = index_byte;
    const i32 free_bytes = static_cast<i32>(commit_end - first);
    const u32 free_units = static_cast<u32>(free_bytes >> 4);
    heap_add_free(heap_addr, first, free_units);
    return true;
}

// Host stand-in for the critical section at heap+0x580. The embedded
// Xbox RTL_CRITICAL_SECTION is 0x1c bytes and is not a native x64
// CRITICAL_SECTION. Heaps that share a lock pointer share one mutex.
std::recursive_mutex* heap_lock_mutex(u32 lock_addr)
{
    static std::mutex gate;
    static std::unordered_map<u32, std::unique_ptr<std::recursive_mutex>> locks;
    std::lock_guard<std::mutex> hold(gate);
    std::unique_ptr<std::recursive_mutex>& slot = locks[lock_addr];
    if (!slot)
    {
        slot = std::make_unique<std::recursive_mutex>();
    }
    return slot.get();
}

struct HeapSerialize
{
    std::recursive_mutex* mu = nullptr;

    HeapSerialize(u32 heap_addr, u32 flags)
    {
        if ((flags & 1u) != 0)
        {
            return;
        }
        const u32 lock = heap_get_u32(heap_addr + 0x580u);
        if (lock == 0)
        {
            return;
        }
        mu = heap_lock_mutex(lock);
        mu->lock();
    }

    void unlock()
    {
        if (mu != nullptr)
        {
            mu->unlock();
            mu = nullptr;
        }
    }

    ~HeapSerialize()
    {
        unlock();
    }
};

// 0x001516A0. Bit scan forward of the bitmap word.
u32 heap_lowest_bit(u32 mask)
{
    return static_cast<u32>(std::countr_zero(mask));
}

// Unlink the free entry whose list node sits at entry+8. A small list that
// becomes empty clears its bit in the map at heap+0x160.
void heap_unlink_free(u32 heap_addr, u32 entry)
{
    const u32 flink = heap_get_u32(entry + 8u);
    const u32 blink = heap_get_u32(entry + 0xcu);
    heap_set_u32(blink, flink);
    heap_set_u32(flink + 4u, blink);
    const u16 units = heap_get_u16(entry);
    if (flink == blink && units < 0x80u)
    {
        u8* map = heap_at(heap_addr + 0x160u + (static_cast<u32>(units) >> 3));
        *map = static_cast<u8>(*map ^ static_cast<u8>(1u << (units & 7u)));
    }
}

// 0x0014D717 paints a free block whose flags include 0x04 with 0xFEEEFEEE
// from entry+0x18. Flag 0x02 shortens that run by four bytes.
void heap_paint_free(u32 entry)
{
    const u8 flags = heap_at(entry)[5];
    if ((flags & 4u) == 0)
    {
        return;
    }
    const u32 block_bytes = static_cast<u32>(heap_get_u16(entry)) << 4;
    if (block_bytes <= 0x18u)
    {
        return;
    }
    u32 bytes = block_bytes - 0x18u;
    if ((flags & 2u) != 0 && bytes > 4u)
    {
        bytes -= 4u;
    }
    u32 at = entry + 0x18u;
    const u32 end = at + (bytes & ~3u);
    while (at < end)
    {
        heap_set_u32(at, 0xfeeefeeeu);
        at += 4u;
    }
}

// Unlink a free entry, paint it when flag 0x04 is set, and drop its size
// from heap+0x30.
void heap_detach_free(u32 heap_addr, u32 entry)
{
    const u16 units = heap_get_u16(entry);
    heap_paint_free(entry);
    heap_unlink_free(heap_addr, entry);
    heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - units);
}

// 0x0014D717. Absorb adjacent free entries into `entry` while the combined
// size stays at or below 0xff00 granules. `*units_io` is the running size.
// `remove` unlinks `entry` itself first; RtlFreeHeap passes false because
// the block is still busy. The returned entry may sit in front of the
// original. Flag 0x10 is the only flag kept, and it updates segment+0x40.
u32 heap_coalesce(u32 heap_addr, u32 entry, u32* units_io, bool remove)
{
    u32 units = *units_io;
    const u32 prev_bytes = static_cast<u32>(heap_get_u16(entry + 2u)) << 4;
    const u32 prev = entry - prev_bytes;
    if (prev != entry && (heap_at(prev)[5] & 1u) == 0)
    {
        const u32 combined = static_cast<u32>(heap_get_u16(prev)) + units;
        if (combined <= 0xff00u)
        {
            if (remove)
            {
                heap_detach_free(heap_addr, entry);
                remove = false;
            }
            const u8 last = static_cast<u8>(heap_at(entry)[5] & 0x10u);
            const u16 prev_units = heap_get_u16(prev);
            heap_detach_free(heap_addr, prev);
            heap_at(prev)[5] = last;
            if ((last & 0x10u) != 0)
            {
                const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(heap_at(prev)[4]) * 4u + 0x60u);
                if (segment != 0)
                {
                    heap_set_u32(segment + 0x40u, prev);
                }
            }
            units += prev_units;
            *units_io = units;
            heap_set_u16(prev, static_cast<u16>(units));
            if ((heap_at(prev)[5] & 0x10u) == 0)
            {
                heap_set_u16(prev + (units << 4) + 2u, static_cast<u16>(units));
            }
            entry = prev;
        }
    }

    if ((heap_at(entry)[5] & 0x10u) == 0)
    {
        const u32 next = entry + (units << 4);
        if ((heap_at(next)[5] & 1u) == 0)
        {
            const u32 combined = static_cast<u32>(heap_get_u16(next)) + units;
            if (combined <= 0xff00u)
            {
                if (remove)
                {
                    heap_detach_free(heap_addr, entry);
                }
                const u8 last = static_cast<u8>(heap_at(next)[5] & 0x10u);
                const u16 next_units = heap_get_u16(next);
                heap_at(entry)[5] = last;
                if ((last & 0x10u) != 0)
                {
                    const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(heap_at(entry)[4]) * 4u + 0x60u);
                    if (segment != 0)
                    {
                        heap_set_u32(segment + 0x40u, entry);
                    }
                }
                heap_detach_free(heap_addr, next);
                units += next_units;
                *units_io = units;
                heap_set_u16(entry, static_cast<u16>(units));
                if ((heap_at(entry)[5] & 0x10u) == 0)
                {
                    heap_set_u16(entry + (units << 4) + 2u, static_cast<u16>(units));
                }
            }
        }
    }
    return entry;
}

// Insert a free entry that already has its size, flags, and segment index.
// Sizes below 0x80 use the dedicated list and set the bitmap when the list
// was empty. Larger sizes walk heap+0x180 in ascending order.
void heap_insert_free(u32 heap_addr, u32 entry)
{
    const u16 units = heap_get_u16(entry);
    u32 before = 0;
    if (units >= 0x80u)
    {
        const u32 head = heap_addr + 0x180u;
        before = heap_get_u32(head);
        while (before != head)
        {
            if (units <= heap_get_u16(before - 8u))
            {
                break;
            }
            before = heap_get_u32(before);
        }
    }
    else
    {
        before = heap_addr + 0x180u + static_cast<u32>(units) * 8u;
        if (heap_get_u32(before) == before)
        {
            u8* map = heap_at(heap_addr + 0x160u + (static_cast<u32>(units) >> 3));
            *map = static_cast<u8>(*map | static_cast<u8>(1u << (units & 7u)));
        }
    }

    const u32 blink = heap_get_u32(before + 4u);
    heap_set_u32(entry + 8u, before);
    heap_set_u32(entry + 0xcu, blink);
    heap_set_u32(blink, entry + 8u);
    heap_set_u32(before + 4u, entry + 8u);
}

// heap_insert_free, then heap+0x30 gains the entry size.
void heap_link_free_entry(u32 heap_addr, u32 entry)
{
    heap_insert_free(heap_addr, entry);
    const u16 units = heap_get_u16(entry);
    heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) + units);
}

// 0x0014D4D8. Push an uncommitted-range node back onto heap+0x4c.
void heap_release_uc_node(u32 segment, u32 node)
{
    const u32 heap_addr = heap_get_u32(segment + 0x18u);
    heap_set_u32(node, heap_get_u32(heap_addr + 0x4cu));
    heap_set_u32(heap_addr + 0x4cu, node);
    heap_set_u32(node + 4u, 0);
    heap_set_u32(node + 8u, 0);
}

// 0x0014D3B8. Pop an uncommitted-range node for `segment`. An empty list at
// heap+0x4c commits another page of the zone at heap+0x48, or reserves a
// fresh 64KB zone and commits its first page. Descriptors are 0x10 bytes
// and start at zone+0x10.
u32 heap_ucr_pop(HeapVm vm, u32 segment)
{
    const u32 heap_addr = heap_get_u32(segment + 0x18u);
    if (heap_get_u32(heap_addr + 0x4cu) == 0)
    {
        u32 zone = heap_get_u32(heap_addr + 0x48u);
        u32 desc = 0;
        if (zone != 0 && heap_get_u32(zone + 8u) != heap_get_u32(zone + 4u))
        {
            const u32 addr = zone + heap_get_u32(zone + 8u);
            if (vm.alloc == nullptr || vm.alloc(heap_at(addr), 0x1000u, 0x1000u, 4u) == nullptr)
            {
                return 0;
            }
            heap_set_u32(zone + 8u, heap_get_u32(zone + 8u) + 0x1000u);
            desc = addr;
        }
        else
        {
            void* reserved = heap_reserve(vm, 0x10000u);
            if (reserved == nullptr)
            {
                return 0;
            }
            zone = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
            if (vm.alloc == nullptr || vm.alloc(heap_at(zone), 0x1000u, 0x1000u, 4u) == nullptr)
            {
                if (vm.free != nullptr)
                {
                    vm.free(heap_at(zone), 0, 0x8000u);
                }
                return 0;
            }
            heap_set_u32(zone, heap_get_u32(heap_addr + 0x48u));
            heap_set_u32(heap_addr + 0x48u, zone);
            heap_set_u32(zone + 4u, 0x10000u);
            heap_set_u32(zone + 8u, 0x1000u);
            desc = zone + 0x10u;
        }
        const u32 end = zone + heap_get_u32(zone + 8u);
        u32 tail = heap_addr + 0x4cu;
        for (; desc < end; desc += 0x10u)
        {
            heap_set_u32(tail, desc);
            tail = desc;
        }
        heap_set_u32(tail, 0);
    }
    const u32 node = heap_get_u32(heap_addr + 0x4cu);
    if (node == 0)
    {
        return 0;
    }
    heap_set_u32(heap_addr + 0x4cu, heap_get_u32(node));
    return node;
}

// 0x0014D4F9. Record `size` bytes of uncommitted memory at `address` on
// segment+0x38, coalescing with a neighbor that touches either edge.
// segment+0x34 counts the nodes. segment+0x1c tracks the largest run.
void heap_insert_ucr(HeapVm vm, u32 segment, u32 address, u32 size)
{
    u32 slot = segment + 0x38u;
    for (;;)
    {
        const u32 node = heap_get_u32(slot);
        if (node == 0)
        {
            break;
        }
        const u32 node_at = heap_get_u32(node + 4u);
        if (node_at > address)
        {
            if (address + size == node_at)
            {
                const u32 grown = heap_get_u32(node + 8u) + size;
                heap_set_u32(node + 8u, grown);
                heap_set_u32(node + 4u, address);
                if (grown > heap_get_u32(segment + 0x1cu))
                {
                    heap_set_u32(segment + 0x1cu, grown);
                }
                return;
            }
            break;
        }
        const u32 node_end = node_at + heap_get_u32(node + 8u);
        if (node_end == address)
        {
            size += heap_get_u32(node + 8u);
            address = node_at;
            heap_set_u32(slot, heap_get_u32(node));
            heap_release_uc_node(segment, node);
            heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) - 1u);
            if (size > heap_get_u32(segment + 0x1cu))
            {
                heap_set_u32(segment + 0x1cu, size);
            }
            continue;
        }
        slot = node;
    }

    const u32 fresh = heap_ucr_pop(vm, segment);
    if (fresh == 0)
    {
        return;
    }
    heap_set_u32(fresh + 4u, address);
    heap_set_u32(fresh + 8u, size);
    heap_set_u32(fresh, heap_get_u32(slot));
    heap_set_u32(slot, fresh);
    heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) + 1u);
    if (size >= heap_get_u32(segment + 0x1cu))
    {
        heap_set_u32(segment + 0x1cu, size);
    }
}

// 0x0014E1A4. Decommit the page-aligned middle of a free run. A one-granule
// edge keeps the whole page (0x101 granules). A commit routine at heap+0x584
// skips the decommit and returns the run through 0x0014D9BC, as does a
// failed release. The committed prefix and suffix go back on the free lists.
// Prefix sizes are already counted in heap+0x30; suffix sizes are counted
// by the list insert.
void heap_decommit(HeapVm vm, u32 heap_addr, u32 entry, u32 units)
{
    if (heap_get_u32(heap_addr + 0x584u) != 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }

    u32 behind = 0;
    const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(heap_at(entry)[4]) * 4u + 0x60u);
    if (segment == 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }
    u32 page_start = (entry + 0xfffu) & ~0xfffu;
    u32 prefix = (page_start - entry) >> 4;
    if (static_cast<u16>(prefix) == 1u)
    {
        page_start += 0x1000u;
        prefix = 0x101u;
    }
    else
    {
        const u16 prev_units = heap_get_u16(entry + 2u);
        if (prev_units != 0 && page_start == entry)
        {
            behind = entry - (static_cast<u32>(prev_units) << 4);
        }
    }

    const u32 block_end = entry + (units << 4);
    u32 page_end = block_end & ~0xfffu;
    u32 suffix = (block_end - page_end) >> 4;
    u32 follower = 0;
    if (static_cast<u16>(suffix) == 1u)
    {
        page_end -= 0x1000u;
        suffix = 0x101u;
    }
    else if (static_cast<u16>(suffix) == 0 && (heap_at(entry)[5] & 0x10u) == 0)
    {
        follower = block_end;
    }

    const u32 suffix_units = static_cast<u32>(static_cast<u16>(suffix));
    const u32 suffix_bytes = suffix_units << 4;
    const u32 suffix_at = block_end - suffix_bytes;
    u32 hole = 0;
    if (page_end > page_start)
    {
        hole = page_end - page_start;
    }
    if (hole == 0 || vm.free == nullptr)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }

    const u32 node = heap_ucr_pop(vm, segment);
    if (node == 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }
    const int released = vm.free(heap_at(page_start), hole, 0x4000u);
    heap_release_uc_node(segment, node);
    if (released == 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }

    heap_insert_ucr(vm, segment, page_start, hole);
    heap_set_u32(segment + 0x30u, heap_get_u32(segment + 0x30u) + (hole >> 12));

    if (static_cast<u16>(prefix) != 0)
    {
        const u32 prefix_units = static_cast<u32>(static_cast<u16>(prefix));
        heap_at(entry)[5] = 0x10;
        heap_set_u16(entry, static_cast<u16>(prefix));
        heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) + prefix_units);
        heap_set_u32(segment + 0x40u, entry);
        heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] & 0xf8u);
        heap_insert_free(heap_addr, entry);
    }
    else if (behind != 0)
    {
        heap_at(behind)[5] = static_cast<u8>(heap_at(behind)[5] | 0x10u);
        heap_set_u32(segment + 0x40u, behind);
    }
    else
    {
        const u32 last = heap_get_u32(segment + 0x40u);
        if (last >= page_start && last < page_start + hole)
        {
            heap_set_u32(segment + 0x40u, heap_get_u32(segment + 0x28u));
        }
    }

    if (static_cast<u16>(suffix) != 0)
    {
        heap_set_u16(suffix_at + 2u, 0);
        heap_at(suffix_at)[4] = heap_at(segment)[4];
        heap_at(suffix_at)[5] = 0;
        heap_set_u16(suffix_at, static_cast<u16>(suffix));
        heap_set_u16(suffix_at + suffix_bytes + 2u, static_cast<u16>(suffix));
        heap_at(suffix_at)[5] = static_cast<u8>(heap_at(suffix_at)[5] & 0xf8u);
        heap_link_free_entry(heap_addr, suffix_at);
    }
    else if (follower != 0)
    {
        heap_set_u16(follower + 2u, 0);
    }
}

void* heap_reserve_hint(HeapVm vm, u32 bytes, bool top_down)
{
    constexpr u32 kStep = 0x1000000;
    if (top_down)
    {
        for (u32 hint = 0x70000000u - kStep; hint >= 0x20000000u; hint -= kStep)
        {
            if (static_cast<u64>(hint) + bytes > 0x100000000ull)
            {
                continue;
            }
            void* block = vm.alloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(hint)), bytes, 0x2000u, 4u);
            if (block != nullptr)
            {
                return block;
            }
            if (hint < 0x20000000u + kStep)
            {
                break;
            }
        }
        return nullptr;
    }
    return heap_reserve(vm, bytes);
}

// 0x0014D58C. Commit `*size_bytes` from an uncommitted descriptor on
// `segment`. `prefer` must match the descriptor address, or be 0. Returns
// the new heap entry at the committed address. The entry is not on a free
// list yet.
u32 heap_commit_pages(HeapVm vm, u32 heap_addr, u32 segment, u32* size_bytes, u32 prefer)
{
    (void)heap_addr;
    u32 prev = 0;
    u32 link = segment + 0x38u;
    u32 node = heap_get_u32(link);
    if (node == 0 || size_bytes == nullptr)
    {
        return 0;
    }
    const u32 need = *size_bytes;
    for (;;)
    {
        const u32 have = heap_get_u32(node + 8u);
        const u32 at = heap_get_u32(node + 4u);
        if (have >= need && (prefer == 0 || at == prefer))
        {
            break;
        }
        prev = node;
        link = node;
        node = heap_get_u32(node);
        if (node == 0)
        {
            return 0;
        }
    }

    const u32 address = heap_get_u32(node + 4u);
    if (vm.alloc(heap_at(address), need, 0x1000u, 4u) == nullptr)
    {
        return 0;
    }

    const u32 pages = need >> 12;
    heap_set_u32(segment + 0x30u, heap_get_u32(segment + 0x30u) - pages);
    if (heap_get_u32(segment + 0x1cu) == heap_get_u32(node + 8u))
    {
        heap_set_u32(segment + 0x1cu, 0);
    }

    const u32 hole = address;
    u32 preceding = 0;
    const u32 last = heap_get_u32(segment + 0x40u);
    if (last != 0 && (heap_at(last)[5] & 0x10u) != 0)
    {
        const u32 end = last + static_cast<u32>(heap_get_u16(last)) * 16u;
        if (end == hole)
        {
            preceding = last;
        }
    }
    if (preceding == 0)
    {
        u32 cursor = prev == 0 ? heap_get_u32(segment + 0x28u)
                               : heap_get_u32(prev + 4u) + heap_get_u32(prev + 8u);
        if ((heap_at(cursor)[5] & 0x10u) != 0)
        {
            preceding = cursor;
        }
        else
        {
            const u32 limit = heap_get_u32(segment + 0x2cu);
            u32 saved = cursor;
            for (;;)
            {
                const u16 step_units = heap_get_u16(cursor);
                if (step_units == 0)
                {
                    return 0;
                }
                saved = cursor;
                cursor += static_cast<u32>(step_units) * 16u;
                if (cursor >= limit || cursor == hole || heap_get_u16(cursor) == 0)
                {
                    if (cursor != hole)
                    {
                        return 0;
                    }
                    preceding = saved;
                    break;
                }
                if ((heap_at(cursor)[5] & 0x10u) != 0)
                {
                    preceding = cursor;
                    break;
                }
            }
        }
    }

    heap_at(preceding)[5] = static_cast<u8>(heap_at(preceding)[5] & 0xefu);
    heap_set_u32(node + 4u, heap_get_u32(node + 4u) + need);
    const u32 left = heap_get_u32(node + 8u) - need;
    heap_set_u32(node + 8u, left);

    u32 entry = address;
    if (left == 0)
    {
        const u32 end_addr = heap_get_u32(node + 4u);
        if (end_addr == heap_get_u32(segment + 0x2cu))
        {
            heap_at(entry)[5] = 0x10;
            heap_set_u32(segment + 0x40u, entry);
        }
        else
        {
            heap_at(entry)[5] = 0;
            heap_set_u32(segment + 0x40u, heap_get_u32(segment + 0x28u));
        }
        heap_set_u32(link, heap_get_u32(node));
        heap_release_uc_node(segment, node);
        heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) - 1u);
    }
    else
    {
        heap_at(entry)[5] = 0x10;
        heap_set_u32(segment + 0x40u, entry);
    }

    heap_at(entry)[4] = heap_at(preceding)[4];
    const u16 units = static_cast<u16>(need >> 4);
    heap_set_u16(entry, units);
    heap_set_u16(entry + 2u, heap_get_u16(preceding));
    if ((heap_at(entry)[5] & 0x10u) == 0)
    {
        const u32 next = entry + static_cast<u32>(units) * 16u;
        heap_set_u16(next + 2u, units);
    }

    if (heap_get_u32(segment + 0x1cu) == 0)
    {
        for (u32 scan = heap_get_u32(segment + 0x38u); scan != 0; scan = heap_get_u32(scan))
        {
            const u32 bytes = heap_get_u32(scan + 8u);
            if (bytes >= heap_get_u32(segment + 0x1cu))
            {
                heap_set_u32(segment + 0x1cu, bytes);
            }
        }
    }
    return entry;
}

// 0x0014E01D. Commit `bytes` (page rounded by the caller) inside an existing
// segment, or reserve a new one when the heap is growable. The returned
// entry is already on a free list. Null means the heap could not grow.
u32 heap_extend(HeapVm vm, u32 heap_addr, u32 bytes)
{
    const u32 pages = (bytes + 0xfffu) >> 12;
    u32 rounded = pages << 12;
    u8 empty_index = 0x40;
    for (u8 index = 0; index < 0x40u; ++index)
    {
        const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(index) * 4u + 0x60u);
        if (segment == 0)
        {
            if (empty_index == 0x40u)
            {
                empty_index = index;
            }
            continue;
        }
        if (pages > heap_get_u32(segment + 0x30u) || rounded > heap_get_u32(segment + 0x1cu))
        {
            continue;
        }
        u32 got = rounded;
        const u32 entry = heap_commit_pages(vm, heap_addr, segment, &got, 0);
        if (entry != 0)
        {
            heap_add_free(heap_addr, entry, got >> 4);
            return entry;
        }
    }

    if (empty_index == 0x40u || (heap_at(heap_addr)[0x14] & 2u) == 0)
    {
        return 0;
    }

    const u32 minimum = bytes + 0x1000u;
    u32 reserve = minimum;
    const u32 segment_reserve = heap_get_u32(heap_addr + 0x20u);
    if (reserve < segment_reserve)
    {
        reserve = segment_reserve;
    }

    void* reserved = nullptr;
    for (;;)
    {
        reserved = heap_reserve_hint(vm, reserve, false);
        if (reserved != nullptr)
        {
            break;
        }
        if (reserve == minimum)
        {
            return 0;
        }
        reserve >>= 1;
        if (reserve < minimum)
        {
            reserve = minimum;
        }
    }

    const u32 base = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
    heap_set_u32(heap_addr + 0x20u, heap_get_u32(heap_addr + 0x20u) + reserve);

    u32 commit = minimum;
    const u32 segment_commit = heap_get_u32(heap_addr + 0x24u);
    if (commit < segment_commit)
    {
        commit = segment_commit;
    }
    if (commit > reserve)
    {
        commit = reserve;
    }
    if (vm.alloc(heap_at(base), commit, 0x1000u, 4u) == nullptr)
    {
        vm.free(heap_at(base), 0, 0x8000u);
        return 0;
    }
    if (!heap_init_segment(vm, heap_addr, base, empty_index, base, base + commit, base + reserve, 0u))
    {
        vm.free(heap_at(base), 0, 0x8000u);
        return 0;
    }
    return heap_get_u32(base + 0x28u);
}

// Mark `entry` busy for `units` granules and return any leftover to the
// free lists. A one-granule leftover is absorbed. `kept_flags` is the free
// entry's flags before it is marked busy.
void heap_carve(u32 heap_addr, u32 entry, u32 units, u32 rounded, u32 asked)
{
    const u8 kept = heap_at(entry)[5];
    const u16 old_units = heap_get_u16(entry);
    heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - old_units);
    heap_at(entry)[5] = 1;
    u32 extra = static_cast<u32>(old_units) - units;
    heap_set_u16(entry, static_cast<u16>(units));
    heap_at(entry)[6] = static_cast<u8>(rounded - asked);
    heap_at(entry)[7] = 0;

    u8 tail_flags = kept;
    if (extra == 0)
    {
        if ((tail_flags & 0x10u) != 0)
        {
            heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] | 0x10u);
        }
        return;
    }
    if (extra == 1u)
    {
        heap_set_u16(entry, static_cast<u16>(units + 1u));
        heap_at(entry)[6] = static_cast<u8>(heap_at(entry)[6] + 0x10u);
        if ((tail_flags & 0x10u) != 0)
        {
            heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] | 0x10u);
        }
        return;
    }

    const u32 rem = entry + (units << 4);
    heap_at(rem)[5] = kept;
    heap_set_u16(rem + 2u, static_cast<u16>(units));
    heap_at(rem)[4] = heap_at(entry)[4];
    heap_set_u16(rem, static_cast<u16>(extra));

    if ((kept & 0x10u) == 0)
    {
        const u32 next = rem + (extra << 4);
        if ((heap_at(next)[5] & 1u) != 0)
        {
            heap_set_u16(next + 2u, static_cast<u16>(extra));
            heap_at(rem)[5] = static_cast<u8>(heap_at(rem)[5] & 0x10u);
            heap_link_free_entry(heap_addr, rem);
        }
        else
        {
            heap_at(rem)[5] = heap_at(next)[5];
            const u16 next_units = heap_get_u16(next);
            heap_unlink_free(heap_addr, next);
            heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - next_units);
            extra += next_units;
            if (extra < 0xff01u)
            {
                heap_set_u16(rem, static_cast<u16>(extra));
                if ((heap_at(rem)[5] & 0x10u) == 0)
                {
                    heap_set_u16(rem + (extra << 4) + 2u, static_cast<u16>(extra));
                }
                heap_at(rem)[5] = static_cast<u8>(heap_at(rem)[5] & 0x10u);
                heap_link_free_entry(heap_addr, rem);
            }
            else
            {
                heap_add_free(heap_addr, rem, extra);
            }
        }
    }
    else
    {
        heap_at(rem)[5] = static_cast<u8>(heap_at(rem)[5] & 0x10u);
        heap_link_free_entry(heap_addr, rem);
    }

    if ((heap_at(rem)[5] & 0x10u) != 0)
    {
        const u32 segment = heap_get_u32(heap_addr + 0x60u + static_cast<u32>(heap_at(rem)[4]) * 4u);
        if (segment != 0)
        {
            heap_set_u32(segment + 0x40u, rem);
        }
    }
}

// First free block on the large list at heap+0x180 whose size is at least
// `units`. The list is ordered, so the tail is the largest.
u32 heap_take_large(u32 heap_addr, u32 units)
{
    const u32 head = heap_addr + 0x180u;
    const u32 last_link = heap_get_u32(head + 4u);
    if (last_link == head)
    {
        return 0;
    }
    const u32 last_entry = last_link - 8u;
    if (static_cast<u32>(heap_get_u16(last_entry)) < units)
    {
        return 0;
    }
    for (u32 link = heap_get_u32(head); link != head; link = heap_get_u32(link))
    {
        const u32 entry = link - 8u;
        if (units <= heap_get_u16(entry))
        {
            heap_unlink_free(heap_addr, entry);
            return entry;
        }
    }
    return 0;
}

// Smallest dedicated list at or above `units` (units < 0x80) whose bitmap
// bit is set. Returns the unlinked tail entry.
u32 heap_take_bitmap(u32 heap_addr, u32 units)
{
    const u32 word = units >> 5;
    u32 mask = ~((1u << (units & 31u)) - 1u) & heap_get_u32(heap_addr + 0x160u + word * 4u);
    u32 next = heap_addr + 0x160u + (word + 1u) * 4u;
    u32 base = 0;

    if (word == 0)
    {
        if (mask == 0)
        {
            mask = heap_get_u32(next);
            next += 4u;
            if (mask == 0)
            {
                mask = heap_get_u32(next);
                next += 4u;
                if (mask == 0)
                {
                    mask = heap_get_u32(next);
                    if (mask == 0)
                    {
                        return 0;
                    }
                    base = heap_addr + 0x480u;
                }
                else
                {
                    base = heap_addr + 0x380u;
                }
            }
            else
            {
                base = heap_addr + 0x280u;
            }
        }
        else
        {
            base = heap_addr + 0x180u;
        }
    }
    else if (word == 1)
    {
        if (mask == 0)
        {
            mask = heap_get_u32(next);
            next += 4u;
            if (mask == 0)
            {
                mask = heap_get_u32(next);
                if (mask == 0)
                {
                    return 0;
                }
                base = heap_addr + 0x480u;
            }
            else
            {
                base = heap_addr + 0x380u;
            }
        }
        else
        {
            base = heap_addr + 0x280u;
        }
    }
    else if (word == 2)
    {
        if (mask == 0)
        {
            mask = heap_get_u32(next);
            if (mask == 0)
            {
                return 0;
            }
            base = heap_addr + 0x480u;
        }
        else
        {
            base = heap_addr + 0x380u;
        }
    }
    else if (word == 3)
    {
        if (mask == 0)
        {
            return 0;
        }
        base = heap_addr + 0x480u;
    }
    else
    {
        return 0;
    }

    const u32 list = base + heap_lowest_bit(mask) * 8u;
    const u32 entry = heap_get_u32(list + 4u) - 8u;
    heap_unlink_free(heap_addr, entry);
    return entry;
}

void* heap_virtual_alloc(HeapVm vm, u32 heap_addr, u32 flags, u32 asked, u32 rounded)
{
    u32 bytes = (rounded + 0x20u + 0xfffu) & ~0xfffu;
    const bool top_down = (flags & 8u) == 0;
    void* reserved = heap_reserve_hint(vm, bytes, top_down);
    if (reserved == nullptr)
    {
        return nullptr;
    }
    if (vm.alloc(reserved, bytes, 0x1000u, 4u) == nullptr)
    {
        vm.free(reserved, 0, 0x8000u);
        return nullptr;
    }

    const u32 base = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
    std::memset(heap_at(base), 0, 0x30);
    heap_set_u16(base + 0x20u, static_cast<u16>(bytes - asked));
    heap_at(base)[0x25] = 0x0b;
    heap_set_u32(base + 0x18u, bytes);
    heap_set_u32(base + 0x1cu, bytes);

    const u32 head = heap_addr + 0x58u;
    const u32 blink = heap_get_u32(head + 4u);
    heap_set_u32(base, head);
    heap_set_u32(base + 4u, blink);
    heap_set_u32(blink, base);
    heap_set_u32(head + 4u, base);
    return heap_at(base + 0x30u);
}
#endif


// Xbox 0x0014E44E
void* xapi_create_heap(u32 flags, void* heap_base, u32 reserve_size, u32 commit_size, void* lock, const void* parameters)
{
    // Xbox 0x0014E44E
    // RtlCreateHeap, stdcall, six arguments. 0x0014D311 (HeapCreate) ORs
    // the caller flags with 0x1000, forces HEAP_GROWABLE when the commit is
    // 0, and passes a null base and lock. 0x0015105A creates the process
    // heap with flags 2, reserve from XBE+0x134 (0x100000), commit from
    // XBE+0x138 (0x1000), and a zeroed 0x30-byte parameter block.
    // A parameter pointer is copied only when its first dword is 0x30. A
    // fault on that copy returns null. Zero fields take the .data defaults.
    // Commit and reserve round up to pages; a zero commit becomes 0x1000
    // and a zero reserve becomes 0x40000, or the commit rounded to 64KB
    // when the commit was already non-zero. HEAP_NO_SERIALIZE with a lock
    // returns null. A null lock embeds a critical section and grows the
    // header from 0x590 to 0x5ac. A caller lock sets flag 0x80000000.
    // A null base reserves then commits. A caller base with a commit
    // routine uses InitialCommit and InitialReserve and refuses
    // HEAP_GROWABLE. Otherwise the base has to be the start of a region
    // that is not free. The header signature is 0xEEFFEEFF. Eight
    // 16-byte descriptor nodes chain from +0x4c, 128 free lists sit at
    // +0x180, and the lock pointer is +0x580. 0x0014DEFF then builds
    // segment 0 and the first free entry. Failure of that step returns
    // null and leaves a reservation in place. The port keeps the image in
    // the low 4GB so the 32-bit links stay intact.
#if !defined(_WIN32)
    (void)flags;
    (void)heap_base;
    (void)reserve_size;
    (void)commit_size;
    (void)lock;
    (void)parameters;
    return nullptr;
#else
#if defined(_WIN64)
    static_assert(sizeof(HeapMemInfo) == 48u);
#else
    static_assert(sizeof(HeapMemInfo) == 28u);
#endif

    HeapVm vm = heap_vm();
    if (vm.alloc == nullptr || vm.free == nullptr || vm.query == nullptr)
    {
        return nullptr;
    }

    u32 segment_reserve = 0;
    u32 segment_commit = 0;
    u32 decommit_free = 0;
    u32 decommit_total = 0;
    u32 maximum_allocation = 0;
    u32 virtual_threshold = 0;
    u32 initial_commit = 0;
    u32 initial_reserve = 0;
    u32 commit_routine = 0;
    if (parameters != nullptr)
    {
        u32 length = 0;
        std::memcpy(&length, parameters, 4);
        if (length == 0x30u)
        {
            u8 raw[0x30]{};
            std::memcpy(raw, parameters, 0x30);
            std::memcpy(&segment_reserve, raw + 0x04, 4);
            std::memcpy(&segment_commit, raw + 0x08, 4);
            std::memcpy(&decommit_free, raw + 0x0c, 4);
            std::memcpy(&decommit_total, raw + 0x10, 4);
            std::memcpy(&maximum_allocation, raw + 0x14, 4);
            std::memcpy(&virtual_threshold, raw + 0x18, 4);
            std::memcpy(&initial_commit, raw + 0x1c, 4);
            std::memcpy(&initial_reserve, raw + 0x20, 4);
            std::memcpy(&commit_routine, raw + 0x24, 4);
        }
    }

    if ((g_heap_nt_flag_1e & 0x20u) != 0)
    {
        flags |= 0x80u;
    }
    if (segment_reserve == 0)
    {
        segment_reserve = kHeapSegmentReserve;
    }
    if (segment_commit == 0)
    {
        segment_commit = kHeapSegmentCommit;
    }
    if (decommit_free == 0)
    {
        decommit_free = kHeapDecommitFree;
    }
    if (decommit_total == 0)
    {
        decommit_total = kHeapDecommitTotal;
    }
    if (maximum_allocation == 0)
    {
        maximum_allocation = 0x7ffdefffu;
    }
    if (virtual_threshold == 0 || virtual_threshold > 0xff000u)
    {
        virtual_threshold = 0xff000u;
    }

    if (commit_size == 0)
    {
        commit_size = 0x1000u;
        if (reserve_size == 0)
        {
            reserve_size = 0x40000u;
        }
        else
        {
            reserve_size = (reserve_size + 0xfffu) & 0xfffff000u;
        }
    }
    else
    {
        commit_size = (commit_size + 0xfffu) & 0xfffff000u;
        if (reserve_size == 0)
        {
            reserve_size = (commit_size + 0xffffu) & 0xffff0000u;
        }
        else
        {
            reserve_size = (reserve_size + 0xfffu) & 0xfffff000u;
        }
    }

    u32 header_bytes = 0x590u;
    bool embed_lock = false;
    if ((flags & 1u) == 0)
    {
        if (lock == nullptr)
        {
            header_bytes = 0x5acu;
            embed_lock = true;
        }
        else
        {
            flags |= 0x80000000u;
            if (reinterpret_cast<std::uintptr_t>(lock) > 0xffffffffu)
            {
                return nullptr;
            }
        }
    }
    else if (lock != nullptr)
    {
        return nullptr;
    }

    const bool caller_base = heap_base != nullptr;
    u32 base_addr = 0;
    u32 commit_base = 0;
    u32 commit_end = 0;
    u32 user_flag = 0;
    if (!caller_base)
    {
        if (commit_routine != 0)
        {
            return nullptr;
        }
        void* reserved = heap_reserve(vm, reserve_size);
        if (reserved == nullptr)
        {
            return nullptr;
        }
        base_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
        commit_base = base_addr;
        commit_end = base_addr;
        user_flag = 0;
    }
    else
    {
        if (reinterpret_cast<std::uintptr_t>(heap_base) > 0xffffffffu)
        {
            return nullptr;
        }
        base_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(heap_base));
        if (commit_routine != 0)
        {
            if (initial_commit == 0 || initial_reserve == 0 || initial_reserve < initial_commit || (flags & 2u) != 0)
            {
                return nullptr;
            }
            if (static_cast<u64>(base_addr) + initial_reserve > 0x100000000ull)
            {
                return nullptr;
            }
            std::memset(heap_base, 0, 0x1000);
            commit_base = base_addr;
            commit_end = base_addr + initial_commit;
            reserve_size = initial_reserve;
            user_flag = 1;
        }
        else
        {
            HeapMemInfo info{};
            if (vm.query(heap_base, &info, sizeof(info)) == 0 || info.base != heap_base || info.state == 0x10000u)
            {
                return nullptr;
            }
            if (info.region_size > 0xffffffffu)
            {
                return nullptr;
            }
            commit_base = base_addr;
            if (info.state == 0x1000u)
            {
                const u32 region = static_cast<u32>(info.region_size);
                if (static_cast<u64>(base_addr) + region > 0x100000000ull)
                {
                    return nullptr;
                }
                std::memset(heap_base, 0, 0x1000);
                commit_size = region;
                commit_end = base_addr + region;
                reserve_size = region;
                HeapMemInfo next{};
                if (vm.query(heap_at(commit_end), &next, sizeof(next)) != 0 && next.state == 0x2000u &&
                    next.region_size <= 0xffffffffu &&
                    static_cast<u64>(next.region_size) + region <= 0xffffffffull)
                {
                    reserve_size = static_cast<u32>(next.region_size) + region;
                }
            }
            else
            {
                commit_size = 0x1000u;
                commit_end = base_addr;
            }
            user_flag = 1;
        }
    }

    if (commit_end == commit_base)
    {
        if (commit_size == 0)
        {
            commit_size = 0x1000u;
        }
        if (vm.alloc(heap_at(commit_base), commit_size, 0x1000u, 4u) == nullptr)
        {
            if (caller_base)
            {
                return nullptr;
            }
            vm.free(heap_at(base_addr), 0, 0x8000u);
            return nullptr;
        }
        commit_end = commit_base + commit_size;
    }

    u32 chain = base_addr + 0x4cu;
    u32 node = (base_addr + 0x597u) & ~7u;
    for (int count = 8; count != 0; --count)
    {
        heap_set_u32(chain, node);
        chain = node;
        node += 0x10u;
    }
    heap_set_u32(chain, 0);
    u32 tail = node;
    u32 span = header_bytes + 0x80u;
    if ((g_heap_nt_flag_1d & 8u) != 0)
    {
        const u32 aligned = (tail + 7u) & ~7u;
        heap_set_u32(base_addr + 0x17cu, aligned);
        span += 0x60cu;
        tail = aligned + 0x60cu;
    }
    span = (span + 0xfu) & ~0xfu;
    heap_set_u16(base_addr, static_cast<u16>(span >> 4));
    heap_at(base_addr)[5] = 1;
    heap_set_u32(base_addr + 0x10u, 0xeeffeeffu);
    heap_set_u32(base_addr + 0x14u, flags);
    heap_set_u32(base_addr + 0x18u, flags & 0x6001007du);
    heap_set_u16(base_addr + 0x170u, static_cast<u16>(heap_get_u16(base_addr + 0x170u) | 0xffffu));
    heap_set_u16(base_addr + 0x3au, static_cast<u16>(tail - base_addr));
    heap_set_u32(base_addr + 0x3cu, 0);
    for (u32 list = 0; list < 0x80u; ++list)
    {
        const u32 head = base_addr + 0x180u + list * 8u;
        heap_set_u32(head, head);
        heap_set_u32(head + 4u, head);
    }
    heap_set_u32(base_addr + 0x5cu, base_addr + 0x58u);
    heap_set_u32(base_addr + 0x58u, base_addr + 0x58u);

    u32 lock_addr = lock == nullptr ? 0u : static_cast<u32>(reinterpret_cast<std::uintptr_t>(lock));
    if (embed_lock)
    {
        heap_set_u32(tail + 4u, 0xffffffffu);
        lock_addr = tail;
    }
    heap_set_u32(base_addr + 0x580u, lock_addr);

    const u32 reserve_end = commit_base + reserve_size;
    if (!heap_init_segment(vm, base_addr, base_addr + span, 0u, commit_base, commit_end, reserve_end, user_flag))
    {
        return nullptr;
    }

    heap_set_u16(base_addr + 0x38u, 0);
    heap_set_u32(base_addr + 0x20u, segment_reserve);
    heap_set_u32(base_addr + 0x24u, segment_commit);
    heap_set_u32(base_addr + 0x28u, decommit_free >> 4);
    heap_set_u32(base_addr + 0x2cu, decommit_total >> 4);
    heap_set_u32(base_addr + 0x34u, maximum_allocation);
    heap_set_u32(base_addr + 0x1cu, (virtual_threshold + 0xfu) >> 4);
    heap_set_u32(base_addr + 0x584u, commit_routine);
    heap_set_u32(base_addr + 0x50u, 0x1fu);
    heap_set_u32(base_addr + 0x54u, 0xfffffff0u);
    return heap_at(base_addr);
#endif
}


// Xbox 0x0014E872
void* xapi_heap_allocate(void* heap, u32 flags, u32 size)
{
    // Xbox 0x0014E872
    // RtlAllocateHeap. Flags are OR'd with the force-flags at heap+0x18.
    // A zero size allocates one byte. The byte count plus 0x1f is rounded
    // up to a 16-byte granule; that count is the block size in granules,
    // and the caller pointer is the entry plus 0x10. Granules below 0x80
    // take the dedicated list at heap+0x180 when it is non-empty, otherwise
    // the bitmap at heap+0x160 (0x001516A0 is the lowest set bit). A miss
    // walks the large list at heap+0x180. Granules at or above 0x80 skip
    // the bitmap. Granules above the threshold at heap+0x1c are a virtual
    // block when the heap flags at +0x14 include HEAP_GROWABLE: the region
    // is rounded_size+0x20, page aligned, header flags 0x0b, user data at
    // +0x30, linked on heap+0x58. MEM_TOP_DOWN is set when HEAP_ZERO_MEMORY
    // is clear. Anything else asks 0x0014E01D to commit or reserve a
    // segment. The taken entry is marked busy. A leftover granule is
    // absorbed; a larger leftover is a free entry, coalesced with a
    // following free entry, and sizes at or above 0xff01 go through
    // 0x0014D9BC. HEAP_NO_SERIALIZE skips the lock at heap+0x580. The lock
    // drops before HEAP_ZERO_MEMORY fills the caller's bytes. Virtual
    // blocks skip that fill. HEAP_GENERATE_EXCEPTIONS raises
    // STATUS_NO_MEMORY with the rounded size. The port keeps reservations
    // in the low 4GB and uses a host mutex in place of the Xbox critical
    // section. The fs:[0x20]+0x250 alloc notify is absent here.
#if !defined(_WIN32)
    (void)heap;
    (void)flags;
    (void)size;
    return nullptr;
#else
    if (heap == nullptr || reinterpret_cast<std::uintptr_t>(heap) > 0xffffffffu)
    {
        return nullptr;
    }

    HeapVm vm = heap_vm();
    if (vm.alloc == nullptr || vm.free == nullptr)
    {
        return nullptr;
    }

    const u32 heap_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(heap));
    const u32 asked = size;
    flags |= heap_get_u32(heap_addr + 0x18u);
    u32 bytes = asked == 0 ? 1u : asked;
    bytes += 0x1fu;
    const u32 rounded = bytes & ~0xfu;
    const u32 units = bytes >> 4;

    HeapSerialize lock(heap_addr, flags);
    u32 entry = 0;
    void* user = nullptr;

    if (units < 0x80u)
    {
        const u32 list = heap_addr + 0x180u + units * 8u;
        if (heap_get_u32(list) != list)
        {
            entry = heap_get_u32(list + 4u) - 8u;
            const u8 kept = heap_at(entry)[5];
            heap_unlink_free(heap_addr, entry);
            heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - units);
            heap_at(entry)[5] = static_cast<u8>((kept & 0x10u) | 1u);
            heap_at(entry)[6] = static_cast<u8>(rounded - asked);
            heap_at(entry)[7] = 0;
        }
        else
        {
            entry = heap_take_bitmap(heap_addr, units);
            if (entry == 0)
            {
                entry = heap_take_large(heap_addr, units);
            }
            if (entry == 0)
            {
                entry = heap_extend(vm, heap_addr, rounded);
                if (entry != 0)
                {
                    heap_unlink_free(heap_addr, entry);
                }
            }
            if (entry != 0)
            {
                heap_carve(heap_addr, entry, units, rounded, asked);
            }
        }
    }
    else if (units > heap_get_u32(heap_addr + 0x1cu))
    {
        if ((heap_at(heap_addr)[0x14] & 2u) != 0)
        {
            user = heap_virtual_alloc(vm, heap_addr, flags, asked, rounded);
        }
    }
    else
    {
        entry = heap_take_large(heap_addr, units);
        if (entry == 0)
        {
            entry = heap_extend(vm, heap_addr, rounded);
            if (entry != 0)
            {
                heap_unlink_free(heap_addr, entry);
            }
        }
        if (entry != 0)
        {
            heap_carve(heap_addr, entry, units, rounded, asked);
        }
    }

    if (user == nullptr && entry != 0)
    {
        user = heap_at(entry + 0x10u);
        lock.unlock();
        if ((flags & 8u) != 0)
        {
            std::memset(user, 0, asked);
        }
        return user;
    }

    if (user != nullptr)
    {
        return user;
    }

    lock.unlock();
    if ((flags & 4u) != 0)
    {
        const std::uintptr_t info = rounded;
        RaiseException(0xC0000017u, 0, 1, &info);
    }
    return nullptr;
#endif
}


// Xbox 0x0014EFED
bool xapi_heap_free(void* heap, u32 flags, void* block)
{
    // Xbox 0x0014EFED
    // RtlFreeHeap. A null block returns true. The force-flags at heap+0x18
    // are OR'd into the caller's flags for the lock test. HEAP_NO_SERIALIZE
    // skips the lock at heap+0x580. The header is the user pointer minus
    // 0x10. Flag bit 3 is a virtual allocation: unlink the node at user-0x30
    // from heap+0x58, drop the lock, and MEM_RELEASE the region. Any other
    // block is coalesced with free neighbors by 0x0014D717 (the block is
    // still busy, so the remove flag is 0). Sizes below 0x80 join that
    // dedicated list and set the bitmap at heap+0x160. Larger sizes join
    // the ordered list at heap+0x180 when the size is below heap+0x28 or
    // the free total plus this size stays below heap+0x2c. Sizes above
    // 0xff00 go through 0x0014D9BC. Anything else is decommitted by
    // 0x0014E1A4. 0x0014F1CE drops the lock on the way out. The
    // fs:[0x20]+0x250 free notify is absent here. The port keeps addresses
    // in the low 4GB and uses a host mutex in place of the Xbox critical
    // section.
#if !defined(_WIN32)
    (void)heap;
    (void)flags;
    return block == nullptr;
#else
    if (block == nullptr)
    {
        return true;
    }
    if (heap == nullptr || reinterpret_cast<std::uintptr_t>(heap) > 0xffffffffu ||
        reinterpret_cast<std::uintptr_t>(block) > 0xffffffffu)
    {
        return false;
    }

    const u32 heap_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(heap));
    const u32 user = static_cast<u32>(reinterpret_cast<std::uintptr_t>(block));
    flags |= heap_get_u32(heap_addr + 0x18u);
    HeapSerialize lock(heap_addr, flags);

    const u32 header = user - 0x10u;
    if ((heap_at(header)[5] & 8u) != 0)
    {
        const u32 base = header - 0x20u;
        const u32 flink = heap_get_u32(base);
        const u32 blink = heap_get_u32(base + 4u);
        heap_set_u32(blink, flink);
        heap_set_u32(flink + 4u, blink);
        lock.unlock();
        HeapVm vm = heap_vm();
        if (vm.free == nullptr || vm.free(heap_at(base), 0, 0x8000u) == 0)
        {
            return false;
        }
        return true;
    }

    HeapVm vm = heap_vm();
    u32 units = heap_get_u16(header);
    const u32 entry = heap_coalesce(heap_addr, header, &units, false);
    if (units < 0x80u)
    {
        heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] & 0x10u);
        heap_set_u16(entry, static_cast<u16>(units));
        heap_link_free_entry(heap_addr, entry);
    }
    else if (units < heap_get_u32(heap_addr + 0x28u) ||
             heap_get_u32(heap_addr + 0x30u) + units < heap_get_u32(heap_addr + 0x2cu))
    {
        if (units <= 0xff00u)
        {
            heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] & 0x10u);
            heap_set_u16(entry, static_cast<u16>(units));
            heap_link_free_entry(heap_addr, entry);
        }
        else
        {
            heap_add_free(heap_addr, entry, units);
        }
    }
    else
    {
        heap_decommit(vm, heap_addr, entry, units);
    }
    return true;
#endif
}


// Kernel LaunchDataPage (thunk 0x00202EDC, ordinal 164). 0x0015028C publishes
// the 0x1000-byte page. 0x0015040D reads it on the next entry.
u8 g_xapi_launch_page[0x1000] = {};

bool g_xapi_launch_page_set = false;


// \??\D: target. OBJECT_STRING at 0x0032CED8 is "\\??\\D:" (length 6).
// NtQuerySymbolicLinkObject failure uses "\\Device\\Cdrom0".
char g_xapi_d_target[0x208] = "\\Device\\Cdrom0";


// \??\U: and \??\T: targets filled by 0x00150CC6. Empty means the link has
// not been created. A second IoCreateSymbolicLink returns
// STATUS_OBJECT_NAME_COLLISION.
char g_xapi_u_target[0x168] = {};

char g_xapi_t_target[0x168] = {};


// TitleMeta.xbx bytes 0x00150992 would write for T: when $$XTINFO is absent.
// Size is the wchar count times two, with no trailing NUL.
u8 g_xapi_title_meta[0x11C] = {};

u32 g_xapi_title_meta_size = 0;


// Xbox 0x0015028C
u32 xapi_launch_image(const char* path, const void* launch_data, u32 flags, u32 launch_type)
{
    // Xbox 0x0015028C
    // Body of XLaunchNewImageA (0x0014AD66). A null path reboots through
    // 0x0014AEED with launch type 1 when launch data is present and
    // 0xFFFFFFFF when it is absent. D:\ and d:\ keep the tail and join it
    // to the \??\D: target with a semicolon. T:\$u\ and t:\$U\ expand to
    // \Device\Harddisk0\Partition1\TDATA\%08x\$u\. Y:\ and y:\ expand to
    // \Device\Harddisk0\Partition2\ when flags bit 1 is set. 0x0014ADA8
    // fills the 0x1000 launch page (title id from the XBE certificate,
    // launch type, path at +8, and 0xC00 bytes of launch data at +0x400
    // when the type is not 0xFFFFFFFF). 0x00150089 writes the D: target
    // and the path tail at +0x214, and turns a type of 0xFFFFFFFF into 0,
    // except for a D:\ launch with no launch data. The expanded path has
    // to fit in 0x168 bytes. Success calls the object at fs:[0x20]+0x250
    // vfunc +0x14 with (2, 0), then HalReturnToFirmware(2). Any other
    //     path returns 0x57. The port keeps the page for 0x0015040D and returns
    // RtlNtStatusToDosError(0).
    constexpr u32 kTitleId = 0x4D530053u;
    constexpr char kCdrom[] = "\\Device\\Cdrom0";

    const u32 stored_type = launch_data != nullptr ? launch_type : 0xFFFFFFFFu;

    auto copy_n = [](char* dst, const char* src, int cch) -> int
    {
        if (cch <= 0)
        {
            return 0;
        }
        int i = 0;
        while (i < cch - 1 && src[i] != '\0')
        {
            dst[i] = src[i];
            ++i;
        }
        dst[i] = '\0';
        return i;
    };

    auto fill_page = [&](const char* image, const char* prefix, const char* alt_tail)
    {
        std::memset(g_xapi_launch_page, 0, sizeof(g_xapi_launch_page));
        std::memcpy(g_xapi_launch_page + 0, &stored_type, 4);
        std::memcpy(g_xapi_launch_page + 4, &kTitleId, 4);
        if (image != nullptr)
        {
            char* dest = reinterpret_cast<char*>(g_xapi_launch_page + 8);
            if (prefix != nullptr)
            {
                const int n = copy_n(dest, prefix, 0x207);
                dest[n] = ';';
                copy_n(dest + n + 1, image, 0x208 - (n + 1));
            }
            else
            {
                copy_n(dest, image, 0x208);
            }
        }
        if (stored_type != 0xFFFFFFFFu && launch_data != nullptr)
        {
            std::memcpy(g_xapi_launch_page + 0x400, launch_data, 0xC00);
        }
        if (alt_tail != nullptr)
        {
            const int n1 = static_cast<int>(std::strlen(kCdrom));
            const int n2 = static_cast<int>(std::strlen(alt_tail));
            if (n2 + (n1 + 1) + 8 < 0x1EC)
            {
                u8* rec = g_xapi_launch_page + 0x214;
                const u16 n1_nul = static_cast<u16>(n1 + 1);
                rec[0] = 1;
                rec[1] = 0;
                rec[2] = static_cast<u8>(n1_nul & 0xFF);
                rec[3] = static_cast<u8>(n1_nul >> 8);
                std::memcpy(rec + 4, &stored_type, 4);
                if (stored_type == 0xFFFFFFFFu)
                {
                    const u32 zero = 0;
                    std::memcpy(g_xapi_launch_page, &zero, 4);
                }
                std::memcpy(rec + 8, kCdrom, static_cast<size_t>(n1) + 1);
                std::memcpy(rec + 8 + n1 + 1, alt_tail, static_cast<size_t>(n2) + 1);
            }
        }
        g_xapi_launch_page_set = true;
    };

    if (path == nullptr)
    {
        fill_page(nullptr, nullptr, nullptr);
        return 0;
    }

    const char c0 = path[0];
    if ((c0 == 'D' || c0 == 'd') && path[1] == ':' && path[2] == '\\')
    {
        const char* tail = path + 3;
        fill_page(tail, kCdrom, launch_data != nullptr ? tail : nullptr);
        return 0;
    }

    std::string expanded;
    const char* tail = nullptr;
    if ((c0 == 'T' || c0 == 't') && path[1] == ':' && path[2] == '\\' && path[3] == '$' &&
        (path[4] == 'U' || path[4] == 'u') && path[5] == '\\')
    {
        char hex[9];
        for (int i = 0; i < 8; ++i)
        {
            const u32 nib = (kTitleId >> ((7 - i) * 4)) & 0xFu;
            hex[i] = static_cast<char>(nib < 10u ? '0' + nib : 'a' + (nib - 10u));
        }
        hex[8] = '\0';
        tail = path + 6;
        expanded = std::string("\\Device\\Harddisk0\\Partition1\\TDATA\\") + hex + "\\$u\\" + tail;
    }
    else if ((flags & 2u) != 0 && (c0 == 'Y' || c0 == 'y') && path[1] == ':' && path[2] == '\\')
    {
        tail = path + 3;
        expanded = std::string("\\Device\\Harddisk0\\Partition2\\") + tail;
    }
    else
    {
        return 0x57;
    }

    if (expanded.size() >= 0x168)
    {
        return 0x57;
    }
    fill_page(expanded.c_str(), nullptr, tail);
    return 0;
}


bool xapi_user_leaf_path(char* dst, int cap, const char* leaf)
{
    // 0x0014FE7D. _snprintf(dst, cap, "t:\\$u\\%s", leaf). A negative count
    // stores last-error 0x7A and returns false.
    const char prefix[] = "t:\\$u\\";
    int n = 0;
    for (const char* s = prefix; *s != '\0'; ++s)
    {
        if (n + 1 >= cap)
        {
            return false;
        }
        dst[n++] = *s;
    }
    if (leaf != nullptr)
    {
        for (const char* s = leaf; *s != '\0'; ++s)
        {
            if (n + 1 >= cap)
            {
                return false;
            }
            dst[n++] = *s;
        }
    }
    if (n >= cap)
    {
        return false;
    }
    dst[n] = '\0';
    return true;
}


bool xapi_read_capped(const char* path, std::string& out, size_t cap)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    out.clear();
    char buf[4096];
    while (in)
    {
        in.read(buf, sizeof(buf));
        const std::streamsize n = in.gcount();
        if (n <= 0)
        {
            break;
        }
        if (out.size() + static_cast<size_t>(n) > cap)
        {
            return false;
        }
        out.append(buf, static_cast<size_t>(n));
    }
    return static_cast<bool>(in) || in.eof();
}


void xapi_hmac_sha1(const u8* key, u32 key_len, const u8* data, u32 size, u8 out[20])
{
    XapiShaContext ctx;
    xapi_hmac_begin(ctx, key, key_len);
    if (data != nullptr && size != 0)
    {
        xapi_sha_update(ctx, data, size);
    }
    xapi_hmac_final(ctx, key, key_len, out);
}


int xapi_cached_xbe_status(const char* leaf, u32 title_id, u32 version)
{
    // Body of 0x0015010A, called as (leaf, title id, certificate version, 0).
    // The fourth argument is unused. Builds t:\$u\contentmeta.xbx (0x0014FE7D
    // / "t:\\$u\\%s") and opens it GENERIC_READ, share read, OPEN_EXISTING.
    // 0x00151982 reads the header: magic 0x46534358 ('XSCF') at +0x14, size at
    // +0x18 capped to 0x88. With the check flag set it HMACs XboxHDKey over the
    // title id (0x0015187E, XcHMAC) and that key over the header from +0x14,
    // plus any bytes past 0x88, and compares the digest to the first 0x14
    // bytes. Flag bit 0 at +0x20 has to be set. The dword at +0x38 has to be
    // 0x44. That many bytes are read from the file offset at +0x34. XcSHAInit /
    // Update / Final (ordinals 335, 336, 337) of those bytes must match the
    // 20 bytes at +0x3C. The first dword is a count of 1..16 versions; the
    // certificate version has to be one of them. 0x0014FEC7 then opens
    // t:\$u\<leaf>, reads 0x178 bytes of XBE header, seeks to
    // CertificateAddress - BaseAddress, reads 0x1D0 certificate bytes, and the
    // title id at +8 has to match. Failure returns 0x8007000D. Success returns 0.
    constexpr int kFail = static_cast<int>(0x8007000Du);
    constexpr u32 kMagic = 0x46534358u;

    char meta_path[0x104];
    if (!xapi_user_leaf_path(meta_path, 0x104, "contentmeta.xbx"))
    {
        return kFail;
    }

    std::string meta;
    if (!xapi_read_capped(meta_path, meta, 0x100000) || meta.size() < 0x1Cu)
    {
        return kFail;
    }

    u32 hdr_size = 0;
    std::memcpy(&hdr_size, meta.data() + 0x18, 4);
    if (hdr_size < 0x1Cu)
    {
        return kFail;
    }
    const u32 covered = hdr_size < 0x88u ? hdr_size : 0x88u;
    if (meta.size() < covered)
    {
        return kFail;
    }

    u8 header[0x88] = {};
    std::memcpy(header, meta.data(), covered);
    u32 magic = 0;
    std::memcpy(&magic, header + 0x14, 4);
    if (magic != kMagic)
    {
        return kFail;
    }

    u8 content_key[20];
    u8 title_bytes[4];
    std::memcpy(title_bytes, &title_id, 4);
    xapi_hmac_sha1(kXboxHDKey, 0x10, title_bytes, 4, content_key);

    XapiShaContext ctx;
    xapi_hmac_begin(ctx, content_key, 0x14);
    xapi_sha_update(ctx, header + 0x14, covered - 0x14u);
    if (hdr_size > 0x88u)
    {
        if (meta.size() < hdr_size)
        {
            return kFail;
        }
        xapi_sha_update(ctx, reinterpret_cast<const u8*>(meta.data() + 0x88), hdr_size - 0x88u);
    }
    u8 digest[20];
    xapi_hmac_final(ctx, content_key, 0x14, digest);
    if (std::memcmp(digest, header, 20) != 0)
    {
        return kFail;
    }
    if ((header[0x20] & 1u) == 0)
    {
        return kFail;
    }

    u32 payload_off = 0;
    u32 payload_size = 0;
    std::memcpy(&payload_off, header + 0x34, 4);
    std::memcpy(&payload_size, header + 0x38, 4);
    if (payload_size != 0x44u)
    {
        return kFail;
    }
    if (static_cast<u64>(payload_off) + payload_size > meta.size())
    {
        return kFail;
    }

    const u8* payload = reinterpret_cast<const u8*>(meta.data() + payload_off);
    xapi_sha_init(ctx);
    xapi_sha_update(ctx, payload, 0x44);
    u8 payload_digest[20];
    xapi_sha_final(ctx, payload_digest);
    if (std::memcmp(payload_digest, header + 0x3C, 20) != 0)
    {
        return kFail;
    }

    u32 count = 0;
    std::memcpy(&count, payload, 4);
    if (count == 0 || count > 0x10u)
    {
        return kFail;
    }
    bool found_version = false;
    for (u32 i = 0; i < count; ++i)
    {
        u32 entry = 0;
        std::memcpy(&entry, payload + 4u + i * 4u, 4);
        if (entry == version)
        {
            found_version = true;
            break;
        }
    }
    if (!found_version)
    {
        return kFail;
    }

    char image_path[0x104];
    if (leaf == nullptr || !xapi_user_leaf_path(image_path, 0x104, leaf))
    {
        return kFail;
    }

    std::ifstream image(image_path, std::ios::binary);
    if (!image)
    {
        return kFail;
    }
    u8 xbe_header[0x178];
    image.read(reinterpret_cast<char*>(xbe_header), sizeof(xbe_header));
    if (image.gcount() != static_cast<std::streamsize>(sizeof(xbe_header)))
    {
        return kFail;
    }
    u32 image_base = 0;
    u32 cert_addr = 0;
    std::memcpy(&image_base, xbe_header + 0x104, 4);
    std::memcpy(&cert_addr, xbe_header + 0x118, 4);
    if (cert_addr < image_base)
    {
        return kFail;
    }
    image.seekg(static_cast<std::streamoff>(cert_addr - image_base), std::ios::beg);
    u8 certificate[0x1D0];
    image.read(reinterpret_cast<char*>(certificate), sizeof(certificate));
    if (image.gcount() != static_cast<std::streamsize>(sizeof(certificate)))
    {
        return kFail;
    }
    u32 image_title = 0;
    std::memcpy(&image_title, certificate + 8, 4);
    if (image_title != title_id)
    {
        return kFail;
    }
    return 0;
}


// Xbox 0x0015040D
u32 xapi_get_launch_info()
{
    // Xbox 0x0015040D
    // Runs after the 0x1050-byte stack probe (0x00166F70). Certificate TitleID
    // 0xFFFE0000 returns 0. Otherwise 0x0014FF93 reads LaunchDataPage. Bit 0
    // at page+0x214 selects the device path at +0x21C and the type dword at
    // +0x218, and the call returns 1. With the bit clear the type is the dword
    // at the start of the page and the call returns 0. A type other than
    // 0xFFFFFFFF copies 0xC00 bytes from page+0x400. A null page leaves the
    // type at 0xFFFFFFFF. When that call returns 0, 0x0014FFF7 queries the
    // \??\D: symbolic link into a 0x168 buffer, or copies "\\Device\\Cdrom0"
    // when the query fails.
    // XeImageFileName (thunk 0x00202FC8, ordinal 326) is an OBJECT_STRING.
    // When its length is greater than the directory string, _strnicmp of that
    // prefix has to be 0. The bytes after the prefix and one separator are
    // the leaf. _snprintf of "%s%s" / "t:\\$u\\" / leaf into 0x168 bytes
    // returns 0 from this function when the count is negative. 0x0015010A
    // then checks that leaf against the certificate title id and version. A
    // non-negative status calls 0x0015028C with the t:\$u path, the launch
    // data pointer (null when the type is 0xFFFFFFFF), flags 0, and that type.
    // Retail success reboots inside 0x0015028C. When 0x0014FF93 returned
    // non-zero, IoDeleteSymbolicLink and IoCreateSymbolicLink (ordinals 69
    // and 67) retarget \??\D: at the directory string. The port stores that
    // target. Always returns 0.
    constexpr u32 kUnsetTitleId = 0xFFFE0000u;
    constexpr u32 kCertificateVersion = 2u;
    constexpr char kXeImageFileName[] = "\\Device\\CdRom0\\default.xbe";
    constexpr u16 kXeImageFileNameLength = static_cast<u16>(sizeof(kXeImageFileName) - 1u);
    constexpr char kCdromFallback[] = "\\Device\\Cdrom0";

    const u32 title_id = kCalcSigTitleId;
    if (title_id == kUnsetTitleId)
    {
        return 0;
    }

    u32 launch_type = 0xFFFFFFFFu;
    bool from_launch = false;
    char dir[0x400];
    dir[0] = '\0';
    u8 launch_data[0xC00];
    bool have_data = false;

    if (g_xapi_launch_page_set)
    {
        if ((g_xapi_launch_page[0x214] & 1u) != 0)
        {
            std::memcpy(&launch_type, g_xapi_launch_page + 0x218, 4);
            const char* page_path = reinterpret_cast<const char*>(g_xapi_launch_page + 0x21C);
            size_t n = 0;
            while (n + 1 < sizeof(dir) && page_path[n] != '\0')
            {
                dir[n] = page_path[n];
                ++n;
            }
            dir[n] = '\0';
            from_launch = true;
        }
        else
        {
            std::memcpy(&launch_type, g_xapi_launch_page, 4);
        }
        if (launch_type != 0xFFFFFFFFu)
        {
            std::memcpy(launch_data, g_xapi_launch_page + 0x400, 0xC00);
            have_data = true;
        }
    }

    if (!from_launch)
    {
        const char* src = g_xapi_d_target[0] != '\0' ? g_xapi_d_target : kCdromFallback;
        size_t n = 0;
        while (n + 1 < 0x168 && src[n] != '\0')
        {
            dir[n] = src[n];
            ++n;
        }
        dir[n] = '\0';
    }

    const size_t dir_len = std::strlen(dir);
    if (kXeImageFileNameLength > dir_len)
    {
        bool prefix = true;
        for (size_t i = 0; i < dir_len; ++i)
        {
            unsigned char a = static_cast<unsigned char>(kXeImageFileName[i]);
            unsigned char b = static_cast<unsigned char>(dir[i]);
            if (a >= 'A' && a <= 'Z')
            {
                a = static_cast<unsigned char>(a + 32);
            }
            if (b >= 'A' && b <= 'Z')
            {
                b = static_cast<unsigned char>(b + 32);
            }
            if (a != b)
            {
                prefix = false;
                break;
            }
        }
        if (prefix)
        {
            const u32 skip = static_cast<u32>(dir_len) + 1u;
            const u32 count = kXeImageFileNameLength - skip;
            char leaf[0x168];
            if (count < sizeof(leaf))
            {
                if (count != 0)
                {
                    std::memcpy(leaf, kXeImageFileName + skip, count);
                }
                leaf[count] = '\0';

                char launch_path[0x168];
                if (!xapi_user_leaf_path(launch_path, 0x168, leaf))
                {
                    return 0;
                }
                if (xapi_cached_xbe_status(leaf, title_id, kCertificateVersion) >= 0)
                {
                    const void* data = nullptr;
                    if (launch_type != 0xFFFFFFFFu && have_data)
                    {
                        data = launch_data;
                    }
                    xapi_launch_image(launch_path, data, 0, launch_type);
                }
            }
        }
    }

    if (from_launch)
    {
        size_t n = 0;
        while (n + 1 < sizeof(g_xapi_d_target) && dir[n] != '\0')
        {
            g_xapi_d_target[n] = dir[n];
            ++n;
        }
        g_xapi_d_target[n] = '\0';
    }
    return 0;
}


// Xbox 0x0015059A
std::int64_t* xapi_relative_timeout(std::int64_t* timeout, u32 milliseconds)
{
    // Xbox 0x0015059A
    // stdcall. Milliseconds become a relative NT timeout: an unsigned
    // 32x32 multiply by 10000 (100-nanosecond units per millisecond) stored
    // as a two's-complement 64-bit negation. 0xFFFFFFFF is an infinite wait:
    // the buffer is left untouched and the result is null, which the kernel
    // treats as no timeout. 0x001506C1 passes the pointer as the timeout of
    // NtWaitForSingleObjectEx (thunk 0x00202FDC, ordinal 234) with wait mode
    // 1. 0x00150705 does the same for NtWaitForMultipleObjectsEx (thunk
    // 0x00202FE0, ordinal 235). 0x00150785 passes it to KeDelayExecutionThread
    // (thunk 0x00202FE4, ordinal 99) and, when the result is null, substitutes
    // a local interval of 0x8000000000000000.
    if (milliseconds == 0xFFFFFFFFu)
    {
        return nullptr;
    }
    const u64 ticks = static_cast<u64>(milliseconds) * 10000u;
    *timeout = -static_cast<std::int64_t>(ticks);
    return timeout;
}


// OBJECT_STRING / ANSI_STRING. Length is the byte count of the object name.
struct XapiAnsiString
{
    u16 length;
    u16 maximum_length;
    const char* buffer;
};


// Xbox 0x00150833
int xapi_check_volume_cluster_size(const XapiAnsiString* name, int bytes_per_cluster)
{
    // Xbox 0x00150833
    // NtOpenFile with a null root, OBJ_CASE_INSENSITIVE, FILE_READ_DATA|SYNCHRONIZE,
    // share read|write, and FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|
    // FILE_OPEN_FOR_FREE_SPACE_QUERY. NtQueryVolumeInformationFile then reads
    // FileFsSizeInformation (class 3, 24 bytes) and NtClose drops the handle.
    // When that query succeeds and sectors-per-allocation-unit times bytes-per-sector
    // is not bytes_per_cluster, the status becomes STATUS_UNRECOGNIZED_VOLUME
    // (0xC000014F). 0x00151000 asks for 0x4000. 0x0014BFCF asks for
    // 0x4000 << (XBE init flags >> 30) on \Device\Harddisk0\Partition%d\.
    constexpr int kStatusInvalidParameter = static_cast<int>(0xC000000Du);
    constexpr int kStatusNotSupported = static_cast<int>(0xC00000BBu);

    if (name == nullptr || (name->length > 0 && name->buffer == nullptr))
    {
        return kStatusInvalidParameter;
    }

#if defined(_WIN32)
    struct UnicodeString
    {
        u16 length;
        u16 maximum_length;
        wchar_t* buffer;
    };
    struct ObjectAttributes
    {
        unsigned long length;
        void* root_directory;
        UnicodeString* object_name;
        unsigned long attributes;
        void* security_descriptor;
        void* security_quality_of_service;
    };
    struct IoStatusBlock
    {
        void* pointer;
        std::uintptr_t information;
    };
    struct FsSizeInformation
    {
        std::int64_t total_allocation_units;
        std::int64_t available_allocation_units;
        unsigned long sectors_per_allocation_unit;
        unsigned long bytes_per_sector;
    };
    using NtOpenFileFn = long(__stdcall*)(void** handle, unsigned long access, ObjectAttributes* attributes,
                                          IoStatusBlock* io, unsigned long share, unsigned long options);
    using NtQueryVolumeInformationFileFn = long(__stdcall*)(void* handle, IoStatusBlock* io, void* info,
                                                            unsigned long length, unsigned long info_class);
    using NtCloseFn = long(__stdcall*)(void* handle);

    static_assert(sizeof(FsSizeInformation) == 0x18);

    void* ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == nullptr)
    {
        return kStatusNotSupported;
    }
    NtOpenFileFn nt_open = nullptr;
    NtQueryVolumeInformationFileFn nt_query = nullptr;
    NtCloseFn nt_close = nullptr;
    void* open_proc = GetProcAddress(ntdll, "NtOpenFile");
    void* query_proc = GetProcAddress(ntdll, "NtQueryVolumeInformationFile");
    void* close_proc = GetProcAddress(ntdll, "NtClose");
    std::memcpy(&nt_open, &open_proc, sizeof(nt_open));
    std::memcpy(&nt_query, &query_proc, sizeof(nt_query));
    std::memcpy(&nt_close, &close_proc, sizeof(nt_close));
    if (nt_open == nullptr || nt_query == nullptr || nt_close == nullptr)
    {
        return kStatusNotSupported;
    }

    std::wstring wide(static_cast<size_t>(name->length), L'\0');
    for (u16 i = 0; i < name->length; ++i)
    {
        wide[i] = static_cast<unsigned char>(name->buffer[i]);
    }
    const auto wide_bytes = static_cast<size_t>(wide.size()) * sizeof(wchar_t);
    if (wide_bytes > 0xFFFFu)
    {
        return static_cast<int>(0xC0000106); // STATUS_NAME_TOO_LONG
    }

    UnicodeString unicode{};
    unicode.length = static_cast<u16>(wide_bytes);
    unicode.maximum_length = unicode.length;
    unicode.buffer = wide.empty() ? nullptr : wide.data();

    ObjectAttributes attributes{};
    attributes.length = sizeof(attributes);
    attributes.object_name = &unicode;
    attributes.attributes = 0x40; // OBJ_CASE_INSENSITIVE

    IoStatusBlock io{};
    void* handle = nullptr;
    long status = nt_open(&handle, 0x100001u, &attributes, &io, 3u, 0x800021u);
    if (status < 0)
    {
        return static_cast<int>(status);
    }

    FsSizeInformation info{};
    status = nt_query(handle, &io, &info, 0x18u, 3u);
    nt_close(handle);
    if (status >= 0)
    {
        const u32 cluster = static_cast<u32>(info.sectors_per_allocation_unit) *
                            static_cast<u32>(info.bytes_per_sector);
        if (std::bit_cast<int>(cluster) != bytes_per_cluster)
        {
            status = static_cast<int>(0xC000014Fu);
        }
    }
    return static_cast<int>(status);
#else
    (void)bytes_per_cluster;
    return kStatusNotSupported;
#endif
}


// Xbox 0x001508AB
int xapi_create_title_metadata(std::uintptr_t section, char* path, int capacity, const char* leaf)
{
    // Xbox 0x001508AB
    // stdcall. lstrcpynA (0x0014ACB2) appends leaf at the NUL of path. The
    // count is capacity minus strlen(path). RtlInitAnsiString (thunk
    // 0x00202EE8) then NtCreateFile (thunk 0x00202F88): null root,
    // OBJ_CASE_INSENSITIVE, GENERIC_WRITE|SYNCHRONIZE (0x40100000),
    // FILE_ATTRIBUTE_SYSTEM, FILE_SHARE_READ, FILE_OPEN_IF, and
    // FILE_WRITE_THROUGH|FILE_SYNCHRONOUS_IO_NONALERT (0x22). The byte at the
    // append point is cleared after that call returns.
    // STATUS_OBJECT_NAME_COLLISION (0xC0000035) becomes 0. There is no handle
    // to close on that path. Any other success calls NtQueryInformationFile
    // (thunk 0x00202F18) for FileNetworkOpenInformation (class 0x22, 0x38
    // bytes). When that succeeds and EndOfFile is 0, XLoadSectionByHandle
    // (0x0014AF9B, XeLoadSection thunk 0x00202EEC) and XGetSectionSize
    // (0x0014AFDA, the dword at handle+8) feed NtWriteFile (thunk 0x00202F14)
    // at a null offset. XFreeSectionByHandle (0x0014AFBC, XeUnloadSection
    // thunk 0x00202EF0) follows. NtClose (thunk 0x00202EF4) always runs after
    // a successful create. The NTSTATUS is the result. A failed section load
    // stores last-error through 0x0014B03A and leaves the query status in
    // place. 0x00150C73 passes the $$XTIMAGE handle, a 0x104-byte directory,
    // and \TitleImage.xbx. 0x00150C96 does the same for $$XSIMAGE and
    // \SaveImage.xbx.
    // The host create also asks for FILE_READ_ATTRIBUTES. Without it the
    // size query is STATUS_ACCESS_DENIED and the write never runs. XeLoadSection
    // reads the section raw bytes from default.xbe. The directory is --game-dir
    // when that flag is present, otherwise OriginalFiles. The buffer is
    // zero-padded out to the virtual size. Dropping that buffer is the unload.
    constexpr int kNotSupported = static_cast<int>(0xC00000BBu);
    constexpr int kInvalidParameter = static_cast<int>(0xC000000Du);
    constexpr long kNameCollision = static_cast<long>(0xC0000035u);

    if (path == nullptr)
    {
        return kInvalidParameter;
    }

    const size_t length = std::strlen(path);
    char* const append_at = path + length;
    const int room = capacity - static_cast<int>(length);
    if (room > 0 && leaf != nullptr)
    {
        int left = room;
        const char* src = leaf;
        char* dst = append_at;
        while (left > 1 && *src != '\0')
        {
            *dst = *src;
            ++dst;
            ++src;
            --left;
        }
        if (left > 0)
        {
            *dst = '\0';
        }
    }

    struct AppendGuard
    {
        char* at;
        ~AppendGuard()
        {
            if (at != nullptr)
            {
                *at = '\0';
            }
        }
    } guard{append_at};

#if !defined(_WIN32)
    (void)section;
    return kNotSupported;
#else
    struct UnicodeString
    {
        u16 length;
        u16 maximum_length;
        wchar_t* buffer;
    };
    struct ObjectAttributes
    {
        unsigned long length;
        void* root_directory;
        UnicodeString* object_name;
        unsigned long attributes;
        void* security_descriptor;
        void* security_quality_of_service;
    };
    struct IoStatusBlock
    {
        void* pointer;
        std::uintptr_t information;
    };
    using NtCreateFileFn = long(__stdcall*)(void** handle, unsigned long access, ObjectAttributes* attributes,
                                            IoStatusBlock* io, std::int64_t* allocation, unsigned long file_attributes,
                                            unsigned long share, unsigned long disposition, unsigned long options,
                                            void* ea, unsigned long ea_length);
    using NtQueryInformationFileFn = long(__stdcall*)(void* handle, IoStatusBlock* io, void* info, unsigned long length,
                                                      unsigned long info_class);
    using NtWriteFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx, IoStatusBlock* io,
                                           void* buffer, unsigned long length, std::int64_t* offset, unsigned long* key);
    using NtCloseFn = long(__stdcall*)(void* handle);
    using RtlNtStatusToDosErrorFn = unsigned long(__stdcall*)(long status);
    using GetCommandLineAFn = char*(__stdcall*)();

    void* ntdll = GetModuleHandleA("ntdll.dll");
    void* kernel = GetModuleHandleA("kernel32.dll");
    if (ntdll == nullptr || kernel == nullptr)
    {
        return kNotSupported;
    }

    NtCreateFileFn nt_create = nullptr;
    NtQueryInformationFileFn nt_query = nullptr;
    NtWriteFileFn nt_write = nullptr;
    NtCloseFn nt_close = nullptr;
    RtlNtStatusToDosErrorFn rtl_dos = nullptr;
    GetCommandLineAFn command_line = nullptr;
    void* create_proc = GetProcAddress(ntdll, "NtCreateFile");
    void* query_proc = GetProcAddress(ntdll, "NtQueryInformationFile");
    void* write_proc = GetProcAddress(ntdll, "NtWriteFile");
    void* close_proc = GetProcAddress(ntdll, "NtClose");
    void* rtl_proc = GetProcAddress(ntdll, "RtlNtStatusToDosError");
    void* cmd_proc = GetProcAddress(kernel, "GetCommandLineA");
    std::memcpy(&nt_create, &create_proc, sizeof(nt_create));
    std::memcpy(&nt_query, &query_proc, sizeof(nt_query));
    std::memcpy(&nt_write, &write_proc, sizeof(nt_write));
    std::memcpy(&nt_close, &close_proc, sizeof(nt_close));
    std::memcpy(&rtl_dos, &rtl_proc, sizeof(rtl_dos));
    std::memcpy(&command_line, &cmd_proc, sizeof(command_line));
    if (nt_create == nullptr || nt_query == nullptr || nt_write == nullptr || nt_close == nullptr)
    {
        return kNotSupported;
    }

    auto set_last = [&](long code)
    {
        if (rtl_dos != nullptr)
        {
            g_xapi_last_error = rtl_dos(code);
        }
        else
        {
            g_xapi_last_error = 0x1F;
        }
    };

    const size_t full_len = std::strlen(path);
    std::wstring wide(full_len, L'\0');
    for (size_t i = 0; i < full_len; ++i)
    {
        wide[i] = static_cast<unsigned char>(path[i]);
    }
    const auto wide_bytes = wide.size() * sizeof(wchar_t);
    if (wide_bytes > 0xFFFFu)
    {
        return static_cast<int>(0xC0000106);
    }

    UnicodeString unicode{};
    unicode.length = static_cast<u16>(wide_bytes);
    unicode.maximum_length = unicode.length;
    unicode.buffer = wide.empty() ? nullptr : wide.data();

    ObjectAttributes attributes{};
    attributes.length = sizeof(attributes);
    attributes.object_name = &unicode;
    attributes.attributes = 0x40;

    IoStatusBlock io{};
    void* handle = nullptr;
    // 0x80 is FILE_READ_ATTRIBUTES, so FileNetworkOpenInformation can succeed
    // on the host. Retail passes 0x40100000 alone.
    long status = nt_create(&handle, 0x40100080u, &attributes, &io, nullptr, 4u, 1u, 3u, 0x22u, nullptr, 0u);
    if (status < 0)
    {
        if (status == kNameCollision)
        {
            return 0;
        }
        return static_cast<int>(status);
    }

    u8 info[0x38]{};
    status = nt_query(handle, &io, info, 0x38u, 0x22u);
    if (status >= 0)
    {
        u32 eof_lo = 0;
        u32 eof_hi = 0;
        std::memcpy(&eof_lo, info + 0x28, sizeof(eof_lo));
        std::memcpy(&eof_hi, info + 0x2C, sizeof(eof_hi));
        if (eof_lo == 0 && eof_hi == 0)
        {
            const XbeSectionRecord* rec = nullptr;
            if (section != 0 && section != static_cast<std::uintptr_t>(-1))
            {
                const auto* candidate = reinterpret_cast<const XbeSectionRecord*>(section);
                for (const XbeSectionRecord& item : g_xbe_sections)
                {
                    if (&item == candidate)
                    {
                        rec = candidate;
                        break;
                    }
                }
            }

            const void* loaded = nullptr;
            unsigned long write_size = 0;
            std::unique_ptr<u8[]> image;
            if (rec == nullptr)
            {
                set_last(static_cast<long>(0xC0000008u));
            }
            else if (rec->virtual_size == 0)
            {
                static const u8 empty = 0;
                loaded = &empty;
            }
            else
            {
                image.reset(new (std::nothrow) u8[rec->virtual_size]());
                if (image == nullptr)
                {
                    set_last(static_cast<long>(0xC000009Au));
                }
                else
                {
                    std::filesystem::path game_dir{"OriginalFiles"};
                    if (command_line != nullptr)
                    {
                        const char* cmd = command_line();
                        const char* flag = cmd != nullptr ? std::strstr(cmd, "--game-dir") : nullptr;
                        if (flag != nullptr)
                        {
                            flag += std::strlen("--game-dir");
                            while (*flag == ' ' || *flag == '\t')
                            {
                                ++flag;
                            }
                            if (*flag == '"')
                            {
                                ++flag;
                                const char* end = std::strchr(flag, '"');
                                if (end != nullptr && end != flag)
                                {
                                    game_dir = std::filesystem::path(std::string(flag, end));
                                }
                            }
                            else if (*flag != '\0')
                            {
                                const char* end = flag;
                                while (*end != '\0' && *end != ' ' && *end != '\t')
                                {
                                    ++end;
                                }
                                if (end != flag)
                                {
                                    game_dir = std::filesystem::path(std::string(flag, end));
                                }
                            }
                        }
                    }

                    const u32 copy = rec->raw_size < rec->virtual_size ? rec->raw_size : rec->virtual_size;
                    std::ifstream in(game_dir / "default.xbe", std::ios::binary);
                    bool read_ok = static_cast<bool>(in);
                    if (read_ok && copy != 0)
                    {
                        in.seekg(0, std::ios::end);
                        const auto file_end = in.tellg();
                        const auto need = static_cast<std::streamoff>(rec->raw_address) + static_cast<std::streamoff>(copy);
                        if (file_end < 0 || file_end < need)
                        {
                            read_ok = false;
                        }
                        else
                        {
                            in.seekg(static_cast<std::streamoff>(rec->raw_address), std::ios::beg);
                            in.read(reinterpret_cast<char*>(image.get()), static_cast<std::streamsize>(copy));
                            read_ok = in.gcount() == static_cast<std::streamsize>(copy);
                        }
                    }
                    if (!read_ok)
                    {
                        set_last(static_cast<long>(0xC0000034u));
                        image.reset();
                    }
                    else
                    {
                        loaded = image.get();
                        write_size = rec->virtual_size;
                    }
                }
            }

            if (loaded != nullptr)
            {
                status = nt_write(handle, nullptr, nullptr, nullptr, &io, const_cast<void*>(loaded), write_size, nullptr,
                                  nullptr);
            }
        }
    }

    nt_close(handle);
    return static_cast<int>(status);
#endif
}


// OBJECT_STRING in .data. Length is mutable: 0x00150992 decrements it when
// the mapped directory already ends in a backslash.
struct XapiLetterString
{
    u16 length;
    u16 maximum_length;
    const char* buffer;
};


static_assert(sizeof("\\??\\U:") - 1 == 6);

static_assert(sizeof("\\Device\\Harddisk0\\partition1\\UDATA") - 1 == 0x22);

static_assert(sizeof("\\??\\T:") - 1 == 6);

static_assert(sizeof("\\Device\\Harddisk0\\partition1\\TDATA") - 1 == 0x22);


XapiLetterString g_xapi_u_drive{6, 7, "\\??\\U:"};

XapiLetterString g_xapi_u_data{0x22, 0x23, "\\Device\\Harddisk0\\partition1\\UDATA"};

XapiLetterString g_xapi_t_drive{6, 7, "\\??\\T:"};

XapiLetterString g_xapi_t_data{0x22, 0x23, "\\Device\\Harddisk0\\partition1\\TDATA"};


int xapi_map_letter_to_directory(XapiLetterString* drive, XapiLetterString* dir, const char* leaf, int create_directory,
                                 const u16* title_name, int set_time)
{
    // Body of XapiMapLetterToDirectory (0x00150992) for the two calls from
    // 0x00150CC6. Both pass create_directory 1 and set_time 0. Retail opens
    // the directory OBJECT_STRING with NtCreateFile FILE_OPEN_IF, access
    // FILE_LIST_DIRECTORY|SYNCHRONIZE, share read|write, and options
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // STATUS_NOT_A_DIRECTORY (0xC0000103) is treated as success. The leaf is
    // joined with a backslash and created the same way, disposition
    // FILE_OPEN_IF when create_directory is set and FILE_OPEN otherwise,
    // access 0x120117. A null title name stops there. Otherwise
    // XGetSectionHandleA looks up $$XTINFO, $$XTIMAGE, and $$XSIMAGE. A
    // missing $$XTINFO with an empty title skips TitleMeta.xbx; any other
    // case creates it and, when EndOfFile is 0, writes the section or the
    // generated wide string. 0x001508AB then writes \TitleImage.xbx and
    // \SaveImage.xbx. IoCreateSymbolicLink (thunk 0x00202F78) points the
    // drive OBJECT_STRING at that directory. The port does not open
    // \Device\Harddisk0, so the host volume stays intact. The link target
    // is stored the same way \??\D: is. A slot that is already filled
    // returns STATUS_OBJECT_NAME_COLLISION.
    (void)create_directory;
    (void)set_time;
    constexpr int kCollision = static_cast<int>(0xC0000035u);
    constexpr int kNameTooLong = static_cast<int>(0xC0000106u);

    if (drive == nullptr || dir == nullptr || drive->buffer == nullptr || dir->buffer == nullptr || leaf == nullptr)
    {
        return static_cast<int>(0xC000000Du);
    }

    char path[0x168];
    size_t n = 0;
    while (n + 1 < sizeof(path) && dir->buffer[n] != '\0')
    {
        path[n] = dir->buffer[n];
        ++n;
    }
    path[n] = '\0';

    u16 len = dir->length;
    if (len == 0 || len >= sizeof(path))
    {
        return kNameTooLong;
    }
    if (path[len - 1] != '\\')
    {
        path[len] = '\\';
    }
    else
    {
        dir->length = static_cast<u16>(len - 1);
        len = dir->length;
    }

    size_t at = static_cast<size_t>(len) + 1u;
    for (const char* src = leaf; *src != '\0'; ++src)
    {
        if (at + 1 >= sizeof(path))
        {
            return kNameTooLong;
        }
        path[at++] = *src;
    }
    if (at >= sizeof(path))
    {
        return kNameTooLong;
    }
    path[at] = '\0';

    if (title_name != nullptr)
    {
        const std::uintptr_t xtinfo = xapi_find_section_handle("$$XTINFO");
        const std::uintptr_t xtimage = xapi_find_section_handle("$$XTIMAGE");
        const std::uintptr_t xsimage = xapi_find_section_handle("$$XSIMAGE");
        const bool empty_title = title_name[0] == 0;
        // $$XTINFO present: retail writes that section into TitleMeta.xbx.
        // Missing, with a title: _snwprintf of L"%lc%ls%lc%ls%ls" (BOM,
        // L"TitleName", '=', the title, L"\r\n") into 0x8E wchars.
        if (xtinfo == static_cast<std::uintptr_t>(-1) && !empty_title)
        {
            u16 units[0x8E];
            int count = 0;
            auto push_unit = [&](u16 unit)
            {
                if (count < 0x8E)
                {
                    units[count] = unit;
                    ++count;
                }
            };
            push_unit(0xFEFFu);
            const char key[] = "TitleName";
            for (const char* src = key; *src != '\0'; ++src)
            {
                push_unit(static_cast<u8>(*src));
            }
            push_unit(static_cast<u16>('='));
            for (const u16* src = title_name; *src != 0; ++src)
            {
                push_unit(*src);
            }
            push_unit(static_cast<u16>('\r'));
            push_unit(static_cast<u16>('\n'));
            const u32 bytes = static_cast<u32>(count) * 2u;
            if (bytes <= sizeof(g_xapi_title_meta))
            {
                std::memcpy(g_xapi_title_meta, units, bytes);
                if (bytes < sizeof(g_xapi_title_meta))
                {
                    std::memset(g_xapi_title_meta + bytes, 0, sizeof(g_xapi_title_meta) - bytes);
                }
                g_xapi_title_meta_size = bytes;
            }
        }
        (void)xtimage;
        (void)xsimage;
    }

    char* slot = nullptr;
    if (drive->length >= 5 && drive->buffer[4] == 'U')
    {
        slot = g_xapi_u_target;
    }
    else if (drive->length >= 5 && drive->buffer[4] == 'T')
    {
        slot = g_xapi_t_target;
    }
    if (slot == nullptr)
    {
        return static_cast<int>(0xC000000Du);
    }
    if (slot[0] != '\0')
    {
        return kCollision;
    }

    size_t i = 0;
    while (i + 1 < sizeof(g_xapi_u_target) && path[i] != '\0')
    {
        slot[i] = path[i];
        ++i;
    }
    slot[i] = '\0';
    return 0;
}


// Xbox 0x00150CC6
int xapi_map_title_drives(u32 title_id, const u16* title_name)
{
    // Xbox 0x00150CC6
    // stdcall, ret 8. 0x001510E1 passes the certificate TitleID at +8 and
    // the TitleName pointer at +0xC. _sprintf (0x001661F9) writes "%08lx"
    // into 12 bytes. XapiMapLetterToDirectory maps \??\U: onto
    // \Device\Harddisk0\partition1\UDATA\<id> with a null title name. A
    // negative status is returned as-is. Otherwise the same call maps
    // \??\T: onto \Device\Harddisk0\partition1\TDATA\<id> and passes the
    // title name. That second status is the result. 0x001510E6 treats a
    // negative status other than STATUS_DISK_FULL as a dash boot.
    char leaf[12];
    for (int i = 0; i < 8; ++i)
    {
        const u32 nib = (title_id >> ((7 - i) * 4)) & 0xFu;
        leaf[i] = static_cast<char>(nib < 10u ? '0' + nib : 'a' + (nib - 10u));
    }
    leaf[8] = '\0';

    const int status = xapi_map_letter_to_directory(&g_xapi_u_drive, &g_xapi_u_data, leaf, 1, nullptr, 0);
    if (status < 0)
    {
        return status;
    }
    return xapi_map_letter_to_directory(&g_xapi_t_drive, &g_xapi_t_data, leaf, 1, title_name, 0);
}


// Xbox 0x00150E37
int xapi_initialize_title_security()
{
    // Xbox 0x00150E37
    // cdecl. 0x00151068 calls this after 0x0014E44E. A negative status is
    // passed to 0x00150D1B as (1, 0, 0). A non-negative status continues
    // startup.
    // [0x00010118] is the certificate. The dword at +0 is the header size.
    // Above 0x1D8, SecurityFlags is the dword at +0x1D8; otherwise it is 0.
    // Only bit 0 is kept. AllowedMedia at +0x9C is masked with 0xFFFFFF, and
    // bit 0 replaces that mask with 2 (DVD_X2). Any other media returns 0.
    // The retail certificate size is 0x1EC, SecurityFlags is 3, and
    // AllowedMedia is 2, so the DVD path runs with bit 0 set.
    // NtOpenFile (thunk 0x00202F00, ordinal 202) opens the OBJECT_STRING at
    // 0x0032CEE0 ("\Device\CdRom0"): null root, OBJ_CASE_INSENSITIVE,
    // GENERIC_READ|SYNCHRONIZE (0x80100000), FILE_SHARE_READ, and
    // FILE_SYNCHRONOUS_IO_NONALERT (0x20). A negative open status is the
    // result, with no close.
    // The 0x2C-byte pass-through is zeroed, then length 0x2C, DataIn 1,
    // transfer length 0x1C, and a 0x1C-byte data buffer. The CDB is mode
    // sense 0x5A, page 0x3E, allocation length 0x001C. NtDeviceIoControlFile
    // (thunk 0x00202F28, ordinal 196) sends IOCTL 0x4D014 with that input
    // and a null output. A negative status closes and returns. Byte 0x0B of
    // the page must be 1, or the status is STATUS_UNRECOGNIZED_MEDIA
    // (0xC0000014). Bytes 0x0A and 0x0C must both be nonzero, or the status
    // is STATUS_UNSUCCESSFUL (0xC0000001). Those page failures retry through
    // five attempts.
    // When that status is non-negative and SecurityFlags bit 0 is set, a
    // second pass-through uses transfer length 0x664 and CDB
    // AD 00 FF 02 FD FF FE 00 06 64 00 C0. The same IOCTL fills the buffer.
    // 0x00150D80 checks those 0x664 bytes. A negative result from that call
    // retries up to five times. An IOCTL failure does not. NtClose (thunk
    // 0x00202EF4, ordinal 187) follows a successful open and its status is
    // dropped. The IOCTL or 0x00150D80 status is the result.
    // The host has no Xbox DVD challenge response. Returning a negative
    // status would make 0x00151068 leave through 0x00150D1B. A genuine disc
    // returns 0, and so does the port.
    constexpr u32 kCertificateSize = 0x1ECu;
    constexpr u32 kSecurityFlags = 3u;
    constexpr u32 kAllowedMedia = 2u;

    u32 security_flags = 0;
    if (kCertificateSize > 0x1D8u)
    {
        security_flags = kSecurityFlags;
    }
    security_flags &= 1u;
    u32 media = kAllowedMedia & 0x00FFFFFFu;
    if (security_flags != 0u)
    {
        media = 2u;
    }
    if (media != 2u)
    {
        return 0;
    }
    return 0;
}


// Xbox 0x00151000
int xapi_check_utility_volume(const XapiAnsiString* name)
{
    // Xbox 0x00151000
    // stdcall, ret 4. 0x00151084 passes the OBJECT_STRING at 0x0032CEE8
    // (\Device\Harddisk0\partition1\) when XBE init-flag bit 3 is clear.
    // 0x00150833 checks that volume's cluster size against 0x4000. A
    // negative status, including STATUS_UNRECOGNIZED_VOLUME, sends startup
    // through 0x00150D1B.
    return xapi_check_volume_cluster_size(name, 0x4000);
}


// Xbox 0x0015118E
int xapi_format_utility_volume(const XapiAnsiString* name, u32 bytes_per_cluster)
{
    // Xbox 0x0015118E
    // FATX format for XMountUtilityDrive (0x0014BFCF) and 0x0014C0D4.
    // stdcall, two arguments, returns 1 or 0. 0x0014BFCF passes
    // \Device\Harddisk0\Partition%d\ and 0x0014C0D4 passes \??\Z:, both
    // with cluster size 0x4000 << (XBE init flags >> 30).
    // NtOpenFile takes the OBJECT_STRING in param_1, a null root,
    // OBJ_CASE_INSENSITIVE, FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,
    // share 0, and FILE_NO_INTERMEDIATE_BUFFERING|FILE_SYNCHRONOUS_IO_ALERT.
    // IOCTL_DISK_GET_DRIVE_GEOMETRY (0x70000) supplies bytes per sector.
    // bsf of that size is the shift that turns the cluster size into
    // sectors-per-cluster. IOCTL_DISK_GET_PARTITION_INFO (0x74004) supplies
    // the partition length. The sector size rounds up to a page, and the
    // cluster size is raised to that page. The header is max(0x1000, page).
    // Cluster slots are (length / cluster size) + 1. Counts below 0xFFF0 use
    // 16-bit FAT entries; otherwise 4-byte entries. The FAT byte count rounds
    // up to a page. The volume has to hold the header, that FAT, and one
    // cluster, or SetLastError(0x70) and return 0.
    // One page is filled with 0xFF, then the FATX superblock: magic
    // 0x58544146, volume id from the low dword of KeQuerySystemTime, sectors
    // per cluster in the low byte, root cluster 1, and a zero word at +0x10.
    // That page is written at offset 0 and the header tail is zero. The FAT
    // is written next, starting at the header offset: 16-bit media 0xFFF8
    // and 0xFFFF, or 32-bit 0xFFFFFFF8 and 0xFFFFFFFF, then zeros. One
    // cluster of 0xFF follows for the root directory. XMemFree, then
    // FSCTL_DISMOUNT_VOLUME (0x90020), NtClose. A failed write still
    // dismounts. Success returns 1. Other failures go through
    // XapiSetLastNTError, including 0xC000009A when the page allocation fails.
    // The port opens read-only and shared, builds those pages, and does not
    // call NtWriteFile or dismount. A host disk stays intact. When the layout
    // fits, last-error is 5 and the result is 0.
    constexpr long kStatusInvalidParameter = static_cast<long>(0xC000000Du);
    constexpr long kStatusAccessDenied = static_cast<long>(0xC0000022u);
    constexpr long kStatusInsufficient = static_cast<long>(0xC000009Au);
    constexpr long kStatusNotSupported = static_cast<long>(0xC00000BBu);

    auto set_last_nt = [](void* ntdll, long status)
    {
        using RtlFn = unsigned long(__stdcall*)(long);
        RtlFn rtl = nullptr;
        void* proc = ntdll != nullptr ? GetProcAddress(ntdll, "RtlNtStatusToDosError") : nullptr;
        std::memcpy(&rtl, &proc, sizeof(rtl));
        if (rtl != nullptr)
        {
            g_xapi_last_error = rtl(status);
        }
        else
        {
            g_xapi_last_error = 0x1F;
        }
    };

    if (name == nullptr || (name->length > 0 && name->buffer == nullptr))
    {
        g_xapi_last_error = 0x57;
        return 0;
    }

#if !defined(_WIN32)
    (void)bytes_per_cluster;
    (void)kStatusInvalidParameter;
    (void)kStatusAccessDenied;
    (void)kStatusInsufficient;
    (void)kStatusNotSupported;
    g_xapi_last_error = 50;
    return 0;
#else
    struct UnicodeString
    {
        u16 length;
        u16 maximum_length;
        wchar_t* buffer;
    };
    struct ObjectAttributes
    {
        unsigned long length;
        void* root_directory;
        UnicodeString* object_name;
        unsigned long attributes;
        void* security_descriptor;
        void* security_quality_of_service;
    };
    struct IoStatusBlock
    {
        void* pointer;
        std::uintptr_t information;
    };
    using NtOpenFileFn = long(__stdcall*)(void** handle, unsigned long access, ObjectAttributes* attributes,
                                          IoStatusBlock* io, unsigned long share, unsigned long options);
    using NtDeviceIoControlFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx,
                                                     IoStatusBlock* io, unsigned long code, void* in_buf,
                                                     unsigned long in_len, void* out_buf, unsigned long out_len);
    using NtWriteFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx, IoStatusBlock* io,
                                           void* buffer, unsigned long length, std::int64_t* offset, unsigned long* key);
    using NtFsControlFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx, IoStatusBlock* io,
                                               unsigned long code, void* in_buf, unsigned long in_len, void* out_buf,
                                               unsigned long out_len);
    using NtCloseFn = long(__stdcall*)(void* handle);
    using VirtualAllocFn = void*(__stdcall*)(void* addr, std::uintptr_t size, unsigned long type, unsigned long protect);
    using VirtualFreeFn = int(__stdcall*)(void* addr, std::uintptr_t size, unsigned long type);
    using GetTimeFn = void(__stdcall*)(u32* file_time);

    void* ntdll = GetModuleHandleA("ntdll.dll");
    void* kernel = GetModuleHandleA("kernel32.dll");
    if (ntdll == nullptr || kernel == nullptr)
    {
        set_last_nt(ntdll, kStatusNotSupported);
        return 0;
    }

    NtOpenFileFn nt_open = nullptr;
    NtDeviceIoControlFileFn nt_ioctl = nullptr;
    NtWriteFileFn nt_write = nullptr;
    NtFsControlFileFn nt_fsctl = nullptr;
    NtCloseFn nt_close = nullptr;
    VirtualAllocFn virtual_alloc = nullptr;
    VirtualFreeFn virtual_free = nullptr;
    GetTimeFn get_time = nullptr;
    void* open_proc = GetProcAddress(ntdll, "NtOpenFile");
    void* ioctl_proc = GetProcAddress(ntdll, "NtDeviceIoControlFile");
    void* write_proc = GetProcAddress(ntdll, "NtWriteFile");
    void* fsctl_proc = GetProcAddress(ntdll, "NtFsControlFile");
    void* close_proc = GetProcAddress(ntdll, "NtClose");
    void* alloc_proc = GetProcAddress(kernel, "VirtualAlloc");
    void* free_proc = GetProcAddress(kernel, "VirtualFree");
    void* time_proc = GetProcAddress(kernel, "GetSystemTimeAsFileTime");
    std::memcpy(&nt_open, &open_proc, sizeof(nt_open));
    std::memcpy(&nt_ioctl, &ioctl_proc, sizeof(nt_ioctl));
    std::memcpy(&nt_write, &write_proc, sizeof(nt_write));
    std::memcpy(&nt_fsctl, &fsctl_proc, sizeof(nt_fsctl));
    std::memcpy(&nt_close, &close_proc, sizeof(nt_close));
    std::memcpy(&virtual_alloc, &alloc_proc, sizeof(virtual_alloc));
    std::memcpy(&virtual_free, &free_proc, sizeof(virtual_free));
    std::memcpy(&get_time, &time_proc, sizeof(get_time));
    if (nt_open == nullptr || nt_ioctl == nullptr || nt_write == nullptr || nt_fsctl == nullptr || nt_close == nullptr ||
        virtual_alloc == nullptr || virtual_free == nullptr)
    {
        set_last_nt(ntdll, kStatusNotSupported);
        return 0;
    }

    std::wstring wide(static_cast<size_t>(name->length), L'\0');
    for (u16 i = 0; i < name->length; ++i)
    {
        wide[i] = static_cast<unsigned char>(name->buffer[i]);
    }
    const auto wide_bytes = static_cast<size_t>(wide.size()) * sizeof(wchar_t);
    if (wide_bytes > 0xFFFFu)
    {
        set_last_nt(ntdll, static_cast<long>(0xC0000106));
        return 0;
    }

    UnicodeString unicode{};
    unicode.length = static_cast<u16>(wide_bytes);
    unicode.maximum_length = unicode.length;
    unicode.buffer = wide.empty() ? nullptr : wide.data();

    ObjectAttributes attributes{};
    attributes.length = sizeof(attributes);
    attributes.object_name = &unicode;
    attributes.attributes = 0x40;

    // Retail asks for read|write and share 0. The port opens read-only and
    // shared so a \Device\Harddisk or \??\Z: name cannot lock or rewrite a
    // host volume. commit_fatx stays clear for that reason.
    const bool commit_fatx = false;
    IoStatusBlock io{};
    void* handle = nullptr;
    long status = nt_open(&handle, 0x100001u, &attributes, &io, 3u, 0x18u);
    if (status < 0)
    {
        set_last_nt(ntdll, status);
        return 0;
    }

    auto close_handle = [&]()
    {
        if (handle != nullptr)
        {
            nt_close(handle);
            handle = nullptr;
        }
    };

    u8 geometry[0x18]{};
    status = nt_ioctl(handle, nullptr, nullptr, nullptr, &io, 0x70000u, nullptr, 0, geometry, 0x18u);
    if (status < 0)
    {
        close_handle();
        set_last_nt(ntdll, status);
        return 0;
    }

    u32 bytes_per_sector = 0;
    std::memcpy(&bytes_per_sector, geometry + 0x14, sizeof(bytes_per_sector));
    if (bytes_per_sector == 0)
    {
        close_handle();
        set_last_nt(ntdll, kStatusInvalidParameter);
        return 0;
    }

    const u32 sector_shift = static_cast<u32>(std::countr_zero(bytes_per_sector)) & 0x1Fu;
    u8 part_info[0x20]{};
    status = nt_ioctl(handle, nullptr, nullptr, nullptr, &io, 0x74004u, nullptr, 0, part_info, 0x20u);
    if (status < 0)
    {
        close_handle();
        set_last_nt(ntdll, status);
        return 0;
    }

    const u32 chunk = (bytes_per_sector + 0xFFFu) & 0xFFFFF000u;
    if (chunk == 0 || (bytes_per_cluster != 0 && (bytes_per_cluster % chunk) != 0 && bytes_per_cluster > chunk))
    {
        // A span that is not a multiple of the page never reaches zero in
        // the retail write loop. Reject it instead of spinning.
        close_handle();
        set_last_nt(ntdll, kStatusInvalidParameter);
        return 0;
    }
    if (bytes_per_cluster < chunk)
    {
        bytes_per_cluster = chunk;
    }
    if (bytes_per_cluster == 0 || (bytes_per_cluster % chunk) != 0)
    {
        close_handle();
        set_last_nt(ntdll, kStatusInvalidParameter);
        return 0;
    }

    const u32 header = chunk > 0x1000u ? chunk : 0x1000u;
    u64 part_len = 0;
    std::memcpy(&part_len, part_info + 8, sizeof(part_len));
    const u32 part_high = static_cast<u32>(part_len >> 32);
    const u32 part_low = static_cast<u32>(part_len);
    if (part_high == 0 && header >= part_low)
    {
        close_handle();
        g_xapi_last_error = 0x70;
        return 0;
    }

    const u32 count = static_cast<u32>(part_len / bytes_per_cluster) + 1u;
    const bool fat16 = count < 0xFFF0u;
    const u32 fat_bytes = fat16 ? count * 2u : count * 4u;
    const u32 fat_aligned = (fat_bytes + chunk - 1u) & ~(chunk - 1u);
    const u64 need = static_cast<u64>(header) + fat_aligned + bytes_per_cluster;
    if (part_len < need)
    {
        close_handle();
        g_xapi_last_error = 0x70;
        return 0;
    }

    u8* page = static_cast<u8*>(virtual_alloc(nullptr, chunk, 0x3000u, 0x04u));
    if (page == nullptr)
    {
        close_handle();
        set_last_nt(ntdll, kStatusInsufficient);
        return 0;
    }

    auto write_page = [&](u64 offset, u32 length) -> long
    {
        if (!commit_fatx)
        {
            (void)offset;
            (void)length;
            return kStatusAccessDenied;
        }
        std::int64_t byte_offset = static_cast<std::int64_t>(offset);
        return nt_write(handle, nullptr, nullptr, nullptr, &io, page, length, &byte_offset, nullptr);
    };

    auto fail_write = [&](long write_status) -> int
    {
        virtual_free(page, 0, 0x8000u);
        page = nullptr;
        if (commit_fatx)
        {
            nt_fsctl(handle, nullptr, nullptr, nullptr, &io, 0x90020u, nullptr, 0, nullptr, 0);
        }
        close_handle();
        set_last_nt(ntdll, write_status);
        return 0;
    };

    std::memset(page, 0xFF, chunk);
    u32* words = reinterpret_cast<u32*>(page);
    words[0] = 0x58544146u;
    u32 volume_id = 0;
    if (get_time != nullptr)
    {
        u32 file_time[2]{};
        get_time(file_time);
        volume_id = file_time[0];
    }
    words[1] = volume_id;
    words[2] = (bytes_per_cluster >> sector_shift) & 0xFFu;
    words[3] = 1;
    reinterpret_cast<u16*>(page)[8] = 0;

    u64 offset = 0;
    for (u32 left = header; left != 0; left -= chunk)
    {
        status = write_page(offset, chunk);
        if (status < 0)
        {
            return fail_write(status);
        }
        std::memset(page, 0, chunk);
        offset += chunk;
    }

    offset = header;
    std::memset(page, 0, chunk);
    if (fat16)
    {
        reinterpret_cast<u16*>(page)[0] = 0xFFF8;
        reinterpret_cast<u16*>(page)[1] = 0xFFFF;
    }
    else
    {
        words[0] = 0xFFFFFFF8u;
        words[1] = 0xFFFFFFFFu;
    }
    for (u32 left = fat_aligned; left != 0; left -= chunk)
    {
        status = write_page(offset, chunk);
        if (status < 0)
        {
            return fail_write(status);
        }
        words[0] = 0;
        words[1] = 0;
        offset += chunk;
    }

    std::memset(page, 0xFF, chunk);
    for (u32 left = bytes_per_cluster; left != 0; left -= chunk)
    {
        status = write_page(offset, chunk);
        if (status < 0)
        {
            return fail_write(status);
        }
        offset += chunk;
    }

    virtual_free(page, 0, 0x8000u);
    if (commit_fatx)
    {
        nt_fsctl(handle, nullptr, nullptr, nullptr, &io, 0x90020u, nullptr, 0, nullptr, 0);
    }
    close_handle();
    if (!commit_fatx)
    {
        set_last_nt(ntdll, kStatusAccessDenied);
        return 0;
    }
    return 1;
#endif
}


// 0x00549508. Bit 0 of XC_MISC (config value 0x11). The timer DPC at
// 0x00151AA3 powers off only when this is nonzero.
u32 g_xapi_auto_power_enabled = 0;

// 0x0054950C. Set when XQueryValue of that config value fails.
u32 g_xapi_auto_power_failed = 0;

// KeSetTimer due time for the notification timer at 0x005494E0.
std::int64_t g_xapi_auto_power_due = 0;

// HalInitiateShutdown requested by the DPC. The host stays up.
u32 g_xapi_auto_power_shutdown = 0;

void (*g_xapi_auto_power_dpc)() = nullptr;


// Xbox 0x00151AA3
void xapi_auto_power_down_callback()
{
    // Xbox 0x00151AA3
    // KDPC for the auto power-down timer, stdcall, ret 0x10. The four
    // arguments are unused. A nonzero dword at 0x00549508 calls
    // HalInitiateShutdown (thunk 0x00203008, ordinal 360), which asks the
    // SMC to power off. The port records that request and keeps running.
    if (g_xapi_auto_power_enabled != 0)
    {
        g_xapi_auto_power_shutdown = 1;
    }
}


// Xbox 0x00151A88
void xapi_reset_auto_power_down_timer()
{
    // Xbox 0x00151A88
    // XAutoPowerDownResetTimer. KeSetTimer (thunk 0x00203004, ordinal 149)
    // on the timer at 0x005494E0, with the DPC at 0x005494C0. The due time
    // immediate is 0xFFFFFFCDB5659000: a relative interval of 216000000000
    // units of 100 nanoseconds, which is 6 hours. 0x0015059A builds that
    // same two's-complement timeout from 21600000 milliseconds.
    std::int64_t due = 0;
    xapi_relative_timeout(&due, 6u * 60u * 60u * 1000u);
    g_xapi_auto_power_due = due;
}


// Xbox 0x00151AB5
void xapi_initialize_auto_power_down()
{
    // Xbox 0x00151AB5
    // Called from 0x0015102E during XAPI startup, before the process heap.
    // KeInitializeDpc (thunk 0x00202FAC, ordinal 107) binds 0x005494C0 to
    // 0x00151AA3 with a null context. KeInitializeTimerEx (thunk 0x00202F48,
    // ordinal 113) makes 0x005494E0 a notification timer (type 0). Both
    // 0x00549508 and 0x0054950C are cleared.
    // XboxHardwareInfo (thunk 0x00202FF0, ordinal 322) is one byte of flags.
    // Bit 3 skips the config read. Retail leaves that bit clear, so
    // XQueryValue (0x00150811) runs: ExQueryNonVolatileSetting (ordinal 24)
    // for value 0x11 (XC_MISC), four bytes, then RtlNtStatusToDosError
    // (ordinal 301). A zero DOS error stores the value masked with 1 into
    // 0x00549508. Any other error stores 1 into 0x0054950C. 0x00151A88 then
    // arms the six-hour timer. The host has no EEPROM, so the query fails
    // and auto power-down stays off.
    g_xapi_auto_power_dpc = &xapi_auto_power_down_callback;
    g_xapi_auto_power_enabled = 0;
    g_xapi_auto_power_failed = 0;

    constexpr u32 kHardwareFlags = 0;
    if ((kHardwareFlags & 8u) == 0u)
    {
        u32 misc = 0;
        // No EEPROM image. ExQueryNonVolatileSetting does not succeed, and
        // the DOS error from that status is nonzero (ERROR_NOT_SUPPORTED).
        const int query_status = 50;
        if (query_status == 0)
        {
            g_xapi_auto_power_enabled = misc & 1u;
        }
        else
        {
            g_xapi_auto_power_failed = 1;
        }
    }

    xapi_reset_auto_power_down_timer();
}


int xapi_fs_ntstatus(const std::filesystem::path& opened, const std::error_code& ec)
{
    constexpr int kNameNotFound = static_cast<int>(0xC0000034u);
    constexpr int kPathNotFound = static_cast<int>(0xC000003Au);
    constexpr int kNotDirectory = static_cast<int>(0xC0000103u);
    constexpr int kAccessDenied = static_cast<int>(0xC0000022u);
    constexpr int kDirNotEmpty = static_cast<int>(0xC0000101u);
    constexpr int kInvalid = static_cast<int>(0xC000000Du);
    constexpr int kUnsuccessful = static_cast<int>(0xC0000001u);

    if (!ec)
    {
        return 0;
    }
    if (ec == std::errc::no_such_file_or_directory || ec.value() == 2 || ec.value() == 3)
    {
        std::filesystem::path leaf = opened;
        if (leaf.filename().empty())
        {
            leaf = leaf.parent_path();
        }
        std::error_code parent_ec;
        const auto parent = leaf.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            return kPathNotFound;
        }
        return kNameNotFound;
    }
    if (ec == std::errc::not_a_directory || ec.value() == 267)
    {
        return kNotDirectory;
    }
    if (ec == std::errc::permission_denied || ec.value() == 5)
    {
        return kAccessDenied;
    }
    if (ec == std::errc::directory_not_empty || ec.value() == 145)
    {
        return kDirNotEmpty;
    }
    if (ec == std::errc::invalid_argument || ec.value() == 123)
    {
        return kInvalid;
    }
    return kUnsuccessful;
}


int xapi_delete_file_entry(const std::filesystem::path& path)
{
    // 0x001514B2 file arm. FILE_ATTRIBUTE_READONLY (bit 0) is cleared with
    // NtSetInformationFile class 4 before FileDispositionInformation. The
    // attribute dword keeps bits 0x3126 and sets FILE_ATTRIBUTE_NORMAL.
    // A failed attribute write skips the delete and returns that status.
    std::error_code ec;
    const auto perms = std::filesystem::status(path, ec).permissions();
    if (ec)
    {
        return xapi_fs_ntstatus(path, ec);
    }
    if (perms != std::filesystem::perms::unknown &&
        (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
    {
        std::filesystem::permissions(path, perms | std::filesystem::perms::owner_write, ec);
        if (ec)
        {
            return static_cast<int>(0xC0000022u);
        }
    }

    ec.clear();
    if (!std::filesystem::remove(path, ec) || ec)
    {
        if (!ec)
        {
            return static_cast<int>(0xC0000034u);
        }
        return xapi_fs_ntstatus(path, ec);
    }
    return 0;
}


int xapi_delete_directory_tree(const std::filesystem::path& dir)
{
    // 0x001514B2. NtQueryDirectoryFile on the open directory, class 1
    // (FileDirectoryInformation), 0x146 bytes, one entry, a null pattern,
    // restart on the first call. Each name is opened under that directory.
    // A directory (attribute bit 0x10) is walked by calling this again.
    // A file with the readonly bit is given FILE_ATTRIBUTE_NORMAL first.
    // FileDispositionInformation then deletes the child. The first failure
    // closes the child and returns without deleting this directory.
    // STATUS_NO_MORE_FILES (0x80000006) or STATUS_NO_SUCH_FILE (0xC000000F)
    // ends the scan. This directory is then marked normal (readonly
    // stripped; that status is ignored) and deleted. The disposition
    // status is the result.
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        return xapi_fs_ntstatus(dir, ec);
    }

    const std::filesystem::directory_iterator end;
    while (it != end)
    {
        const std::filesystem::path child = it->path();
        const std::filesystem::path name = child.filename();
        std::error_code type_ec;
        const auto sym = it->symlink_status(type_ec);
        const bool child_dir = !type_ec && std::filesystem::is_directory(sym);
        std::error_code step_ec;
        it.increment(step_ec);

        if (name != "." && name != "..")
        {
            int status = 0;
            if (type_ec)
            {
                status = xapi_fs_ntstatus(child, type_ec);
            }
            else if (child_dir)
            {
                status = xapi_delete_directory_tree(child);
            }
            else
            {
                status = xapi_delete_file_entry(child);
            }
            if (status < 0)
            {
                return status;
            }
        }
        if (step_ec)
        {
            return xapi_fs_ntstatus(dir, step_ec);
        }
    }
    it = std::filesystem::directory_iterator();

    const auto perms = std::filesystem::status(dir, ec).permissions();
    if (!ec && perms != std::filesystem::perms::unknown &&
        (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
    {
        std::filesystem::permissions(dir, perms | std::filesystem::perms::owner_write, ec);
    }
    ec.clear();
    if (!std::filesystem::remove(dir, ec) || ec)
    {
        if (!ec)
        {
            return static_cast<int>(0xC0000034u);
        }
        return xapi_fs_ntstatus(dir, ec);
    }
    return 0;
}


// Xbox 0x0015162F
int xapi_open_save_directory(const char* path, void* root)
{
    // Xbox 0x0015162F
    // Called from 0x0014C972 when the save name in SaveMeta.xbx matches.
    // RtlInitAnsiString on the path, then NtOpenFile with that ANSI string,
    // RootDirectory = param_2, OBJ_CASE_INSENSITIVE, access
    // FILE_LIST_DIRECTORY|FILE_WRITE_ATTRIBUTES|DELETE|SYNCHRONIZE (0x110101),
    // share read|write, and FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|
    // FILE_OPEN_FOR_BACKUP_INTENT (0x4021). Success hands the handle and a
    // 0x148-byte directory buffer to 0x001514B2, then NtClose. The NTSTATUS
    // is returned as-is; the caller maps it with RtlNtStatusToDosError.
    // A null root is an absolute path. The port has no NT directory-handle
    // table, so a non-null root is STATUS_INVALID_HANDLE. A null or empty
    // name is STATUS_OBJECT_NAME_INVALID. A file is STATUS_NOT_A_DIRECTORY
    // and is left in place.
    constexpr int kInvalidHandle = static_cast<int>(0xC0000008u);
    constexpr int kNameInvalid = static_cast<int>(0xC0000033u);
    constexpr int kNotDirectory = static_cast<int>(0xC0000103u);

    if (root != nullptr)
    {
        return kInvalidHandle;
    }
    if (path == nullptr || path[0] == '\0')
    {
        return kNameInvalid;
    }

    const std::filesystem::path fs_path(xbox_host_path(path));
    std::error_code ec;
    if (std::filesystem::is_directory(fs_path, ec))
    {
        return xapi_delete_directory_tree(fs_path);
    }
    if (!ec)
    {
        std::error_code exists_ec;
        if (std::filesystem::exists(fs_path, exists_ec) && !exists_ec)
        {
            return kNotDirectory;
        }
    }
    if (!ec)
    {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
    }
    return xapi_fs_ntstatus(fs_path, ec);
}

} // namespace scene_detail

} // namespace gbtg
