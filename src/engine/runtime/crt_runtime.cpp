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
#include "crt_runtime.h"

namespace gbtg
{
using namespace scene_detail;

namespace scene_detail
{


// Xbox 0x001660E9
u16* crt_copy_wide_string(u16* dest, const u16* src)
{
    // Xbox 0x001660E9
    // libc wcscpy, cdecl. Each 16-bit unit is stored, then both pointers
    // advance, until the unit just stored is zero. The destination pointer
    // is the return value. The SEH wrapper at 0x0014A961 pushes the source
    // and destination, then pops both after this returns.
    u16* out = dest;
    u16 unit = 0;
    do
    {
        unit = *src;
        *out = unit;
        ++out;
        ++src;
    } while (unit != 0);
    return dest;
}


// Xbox 0x00166ECB
int crt_wide_string_length(const u16* text)
{
    // Xbox 0x00166ECB
    // libc wcslen, cdecl. Each 16-bit unit is read, then the pointer
    // advances, until the unit just read is zero. The walked span includes
    // that terminator, so the returned count is one less.
    const u16* cursor = text;
    u16 unit = 0;
    do
    {
        unit = *cursor;
        ++cursor;
    } while (unit != 0);
    return static_cast<int>(cursor - text) - 1;
}


// Xbox 0x00209788. C-locale ctype words for bytes 0..255. _pctype points here.
// The low byte holds _UPPER 0x01, _LOWER 0x02, _DIGIT 0x04, _SPACE 0x08,
// _PUNCT 0x10, _CONTROL 0x20, _BLANK 0x40, and _HEX 0x80. The high byte is
// clear in the image. Bytes 0x09..0x0D and 0x20 are the ones with _SPACE.
constexpr u16 kCrtCtype[256] = {
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0010, 0x0010, 0x0010, 0x0010, 0x0020,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};


// Xbox 0x00338180. _pctype. The image pointer is 0x00209788.
const u16* g_crt_pctype = kCrtCtype;


// Xbox 0x00338188. __mb_cur_max. The image dword is 1, so isspace takes the
// single-byte path. A signed compare greater than 1 uses _isctype.
int g_crt_mb_cur_max = 1;


// Xbox 0x0033818C. C-locale decimal point. The image byte is '.'. _winput
// widens it with mbtowc (0x0016E07A).
char g_crt_decimal_point = '.';


// Xbox 0x0020998A. C-locale _pwctype words for units 0..255. The low byte
// uses the same bits as _pctype. Units 0x80..0xFF are classified here; the
// narrow table leaves those words zero. 0x09..0x0D, 0x20, and 0xA0 have
// _SPACE (8).
constexpr u16 kWctype[256] = {
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0068, 0x0028, 0x0028, 0x0028, 0x0028, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0010, 0x0010, 0x0010, 0x0010, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0014, 0x0014, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0014, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0010, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
};


// Xbox 0x00338184. _pwctype. The image pointer is 0x0020998A.
const u16* g_crt_pwctype = kWctype;


// Xbox 0x00166BF9
int crt_wide_character_type(u16 c, u16 mask)
{
    // Xbox 0x00166BF9
    // iswctype. cdecl; the caller pops both word arguments. WEOF (0xFFFF)
    // and every unit from 0x100 up yield 0. Retail still masks that zero
    // with the caller's bits. Any other unit returns the _pwctype word
    // masked with those bits. _winput (0x0016A17C) passes mask 8 for
    // whitespace. The hex adjuster at 0x0016A101 passes mask 2 and
    // subtracts 0x27 when the result is nonzero.
    if (c == 0xffffu || c > 0xffu)
    {
        return 0;
    }
    return static_cast<int>(g_crt_pwctype[c] & mask);
}


// Xbox 0x0016AE1F
int crt_character_type(int c, unsigned mask)
{
    // Xbox 0x0016AE1F
    // _isctype. cdecl; the caller pops both arguments. isspace (0x00168883)
    // and the scanf classifiers call here when __mb_cur_max is above 1.
    // (unsigned)(c + 1) above 0x100 returns 0, so the accepted values are
    // -1..255. The ushort at 0x00209786, immediately before _pctype, is 0,
    // so EOF still returns 0. Every other accepted value returns the ctype
    // word masked with the caller's bits.
    if (static_cast<unsigned>(c) + 1u > 0x100u)
    {
        return 0;
    }
    const u16 entry = (c < 0) ? static_cast<u16>(0) : g_crt_pctype[c];
    return static_cast<int>(entry & mask);
}


// Xbox 0x00168883
int crt_is_space(u32 c)
{
    // Xbox 0x00168883
    // isspace. A value above 0xFF returns 0, which covers EOF passed as
    // 0xFFFFFFFF. The compare with __mb_cur_max is signed (jle). At 1 or
    // below, the low byte of _pctype[c] is masked with _SPACE (8). Above 1,
    // _isctype (0x0016AE1F) is called with that same mask: (unsigned)(c + 1)
    // above 0x100 returns 0, otherwise the full ctype word is masked with 8.
    // c is already in 0..255 on that path, so the range test does not fire.
    // The whitespace skip at 0x0016605A tests the return for nonzero. The
    // bit itself is 8 or 0.
    if (c > 0xffu)
    {
        return 0;
    }
    if (g_crt_mb_cur_max > 1)
    {
        return crt_character_type(static_cast<int>(c), 8u);
    }
    return static_cast<int>(static_cast<u8>(g_crt_pctype[c]) & 8u);
}


// Xbox 0x0016B050
int crt_to_lower(int c)
{
    // Xbox 0x0016B050
    // tolower. cdecl; the caller pops the character. The compare with
    // __mb_cur_max is signed (cmp 1 / jle). At 1 or below, the low byte of
    // _pctype[c] is masked with _UPPER (1). Above 1, _isctype (0x0016AE1F)
    // is called with that same mask. A nonzero result returns c + 0x20;
    // zero returns c. The image __mb_cur_max is 1, so the table path runs.
    // There is no range check: the address is _pctype + c * 2. _stricmp
    // (0x00166EE1) and _strnicmp (0x00167213) zero-extend each source byte.
    // __forcdecpt (0x0016B141) sign-extends one byte and compares the result
    // with 'e' (0x65). A value outside 0..255 would walk off the table; the
    // port returns it unchanged. EOF (-1) is that case, and the ushort just
    // before _pctype is 0, so retail also returns it unchanged.
    unsigned upper = 0;
    if (g_crt_mb_cur_max > 1)
    {
        upper = static_cast<unsigned>(crt_character_type(c, 1u));
    }
    else if (static_cast<unsigned>(c) < 256u)
    {
        upper = static_cast<unsigned>(static_cast<u8>(g_crt_pctype[c]) & 1u);
    }
    if (upper == 0)
    {
        return c;
    }
    return c + 0x20;
}


// Xbox 0x0016B308
char* crt_format_scientific(double* value, char* buffer, int ndec, int caps)
{
    // Xbox 0x0016B308
    // _cftoe. __cfltcvt (0x0016B4EC) calls here for 'e' and 'E', passing the
    // double pointer, the output buffer, the precision, and the caps flag
    // (nonzero writes 'E'). The return is that buffer pointer.
    //
    // _fltout (0x0016F027) turns the double into a _strflt on the stack: the
    // sign dword is '-' or ' ' (0x20), the decpt dword is the exponent plus
    // one, and the mantissa pointer aims at a 24-byte digit buffer. _dtold
    // (0x0016EF6D) drops the sign of a zero, so -0.0 prints as a positive
    // zero. $I10_OUTPUT (0x0016F945) is asked for 17 digits. A zero mantissa
    // is the single character '0' and decpt 0. An exponent of 0x7FFF selects
    // the image strings at 0x00209EEC..0x00209EFC: "1#INF", "1#IND",
    // "1#QNAN", or "1#SNAN", with decpt 1. The quiet bit distinguishes a
    // QNAN from an SNAN, and the negative quiet NaN with a zero payload is
    // "1#IND".
    //
    // _fptostr (0x0016EEEE) writes precision + 1 digits one byte in from a
    // slot that already accounts for a minus and, when precision is positive,
    // the decimal point. It plants a leading '0', copies digits (padding
    // '0' past the end of the mantissa), and rounds when the next character
    // is at least '5', carrying through '9'. A carry into that leading '0'
    // increments decpt and keeps the extra digit. Otherwise the guard byte
    // is shifted off. The stored mantissa pointer is left on the first
    // character; the zero test below still sees it.
    //
    // 0x0016B25F then lays the text out. This call passes 0 for its format
    // byte, so the %g shift is skipped. A minus is stored, a positive
    // precision pulls the first digit forward and writes the decimal point
    // from 0x0033818C ('.'), and "e+000" from 0x00209B9C is copied at
    // digit_end. A nonzero caps flag replaces 'e' with 'E'. A mantissa that
    // still starts with '0' leaves the exponent at "+000". Any other value
    // writes decpt - 1, three digits, with '-' over the '+' when the power
    // is negative.

    char mantissa[24];
    int sign = ' ';
    int decpt = 0;
    std::uint64_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    const std::uint64_t frac = bits & 0x000fffffffffffffull;
    const unsigned exp_bits = static_cast<unsigned>((bits >> 52) & 0x7ffu);

    if (exp_bits == 0 && frac == 0)
    {
        mantissa[0] = '0';
        mantissa[1] = '\0';
    }
    else if (exp_bits == 0x7ffu)
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        decpt = 1;
        const char* token = "1#QNAN";
        if (frac == 0)
        {
            token = "1#INF";
        }
        else if ((frac & 0x0008000000000000ull) == 0)
        {
            token = "1#SNAN";
        }
        else if ((bits >> 63) != 0 && frac == 0x0008000000000000ull)
        {
            token = "1#IND";
        }
        int i = 0;
        while (token[i] != '\0')
        {
            mantissa[i] = token[i];
            i += 1;
        }
        mantissa[i] = '\0';
    }
    else
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        const std::uint64_t mag_bits = bits & 0x7fffffffffffffffull;
        const double mag = std::bit_cast<double>(mag_bits);
        char scratch[40];
        const auto converted = std::to_chars(scratch, scratch + sizeof(scratch), mag, std::chars_format::scientific, 16);
        const char* cursor = scratch;
        int count = 0;
        int power = 0;
        if (converted.ec == std::errc{})
        {
            if (*cursor == '+' || *cursor == '-')
            {
                cursor += 1;
            }
            if (*cursor >= '0' && *cursor <= '9')
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == '.')
            {
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9' && count < 17)
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == 'e' || *cursor == 'E')
            {
                power = std::atoi(cursor + 1);
            }
        }
        if (count == 0)
        {
            mantissa[0] = '0';
            count = 1;
            power = 0;
        }
        mantissa[count] = '\0';
        decpt = power + 1;
    }

    const char* man = mantissa;
    char* placed = buffer + (ndec > 0 ? 1 : 0) + (sign == '-' ? 1 : 0);
    placed[0] = '0';
    char* out = placed + 1;
    if (ndec + 1 > 0)
    {
        int remaining = ndec + 1;
        while (remaining != 0)
        {
            char digit = '0';
            if (*man != '\0')
            {
                digit = *man;
                man += 1;
            }
            *out = digit;
            out += 1;
            remaining -= 1;
        }
    }
    *out = '\0';
    if (ndec + 1 >= 0 && *man >= '5')
    {
        char* round = out;
        while (true)
        {
            round -= 1;
            if (*round == '9')
            {
                *round = '0';
                continue;
            }
            *round = static_cast<char>(*round + 1);
            break;
        }
    }
    if (placed[0] == '1')
    {
        decpt += 1;
    }
    else
    {
        char* src = placed + 1;
        char* dst = placed;
        while (true)
        {
            const char ch = *src;
            src += 1;
            *dst = ch;
            dst += 1;
            if (ch == '\0')
            {
                break;
            }
        }
    }

    char* text = buffer;
    if (sign == '-')
    {
        *text = '-';
        text += 1;
    }
    if (ndec > 0)
    {
        char* dot = text + 1;
        *text = *dot;
        *dot = '.';
        text = dot;
    }
    char* exp_at = text + ndec + 1;
    std::memcpy(exp_at, "e+000", 6);
    if (caps != 0)
    {
        *exp_at = 'E';
    }
    exp_at += 1;
    if (mantissa[0] != '0')
    {
        int power = decpt - 1;
        if (power < 0)
        {
            power = -power;
            *exp_at = '-';
        }
        exp_at += 1;
        if (power >= 100)
        {
            *exp_at = static_cast<char>(*exp_at + power / 100);
            power %= 100;
        }
        exp_at += 1;
        if (power >= 10)
        {
            *exp_at = static_cast<char>(*exp_at + power / 10);
            power %= 10;
        }
        exp_at[1] = static_cast<char>(exp_at[1] + power);
    }
    return buffer;
}


// Xbox 0x0016B40D
char* crt_format_fixed(double* value, char* buffer, int ndec)
{
    // Xbox 0x0016B40D
    // _cftof. __cfltcvt (0x0016B4EC) calls here for 'f', passing the double
    // pointer, the output buffer, and the precision. The return is that
    // buffer pointer.
    //
    // _fltout (0x0016F027) fills a stack _strflt the same way _cftoe does:
    // sign '-' or ' ' (0x20), decpt = exponent + 1, mantissa in a 24-byte
    // buffer. _dtold (0x0016EF6D) drops the sign of a zero, so -0.0 prints
    // as a positive zero. $I10_OUTPUT (0x0016F945) is asked for 17 digits.
    // A zero is the character '0' and decpt 0. An exponent of 0x7FFF selects
    // "1#INF", "1#IND", "1#QNAN", or "1#SNAN" with decpt 1.
    //
    // _fptostr (0x0016EEEE) is called with a count of decpt + precision and
    // a destination one past a slot reserved for a minus. There is no gap
    // for the decimal point. It plants a leading '0', copies that many
    // digits (padding '0' past the mantissa), and rounds when the next
    // character is at least '5'. A count below 0 writes an empty string and
    // does not round. A count of 0 still rounds from the first mantissa
    // digit. A carry into the guard increments decpt and keeps the extra
    // digit; otherwise the guard is shifted off.
    //
    // 0x0016B366 then lays the text out. EAX holds the _strflt; the stack
    // arguments are the buffer, the precision, and a format byte. This call
    // passes 0, so the %g truncation (a trailing '0' when decpt - 1 equals
    // the precision) is skipped. A minus is stored. A decpt of 0 or below
    // shifts the digits right and writes a leading '0'; a positive decpt
    // steps over that many digits. A positive precision shifts in the
    // decimal point from 0x0033818C ('.'). When decpt is still negative,
    // zeros are inserted after that point: -decpt of them when the
    // precision is at least that large, otherwise the precision itself.
    // 0x0016B23C is the shift, a memmove of the tail including the NUL.

    char mantissa[24];
    int sign = ' ';
    int decpt = 0;
    std::uint64_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    const std::uint64_t frac = bits & 0x000fffffffffffffull;
    const unsigned exp_bits = static_cast<unsigned>((bits >> 52) & 0x7ffu);

    if (exp_bits == 0 && frac == 0)
    {
        mantissa[0] = '0';
        mantissa[1] = '\0';
    }
    else if (exp_bits == 0x7ffu)
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        decpt = 1;
        const char* token = "1#QNAN";
        if (frac == 0)
        {
            token = "1#INF";
        }
        else if ((frac & 0x0008000000000000ull) == 0)
        {
            token = "1#SNAN";
        }
        else if ((bits >> 63) != 0 && frac == 0x0008000000000000ull)
        {
            token = "1#IND";
        }
        int i = 0;
        while (token[i] != '\0')
        {
            mantissa[i] = token[i];
            i += 1;
        }
        mantissa[i] = '\0';
    }
    else
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        const std::uint64_t mag_bits = bits & 0x7fffffffffffffffull;
        const double mag = std::bit_cast<double>(mag_bits);
        char scratch[40];
        const auto converted = std::to_chars(scratch, scratch + sizeof(scratch), mag, std::chars_format::scientific, 16);
        const char* cursor = scratch;
        int count = 0;
        int power = 0;
        if (converted.ec == std::errc{})
        {
            if (*cursor == '+' || *cursor == '-')
            {
                cursor += 1;
            }
            if (*cursor >= '0' && *cursor <= '9')
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == '.')
            {
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9' && count < 17)
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == 'e' || *cursor == 'E')
            {
                power = std::atoi(cursor + 1);
            }
        }
        if (count == 0)
        {
            mantissa[0] = '0';
            count = 1;
            power = 0;
        }
        mantissa[count] = '\0';
        decpt = power + 1;
    }

    const char* man = mantissa;
    char* placed = buffer + (sign == '-' ? 1 : 0);
    const int digits = decpt + ndec;
    placed[0] = '0';
    char* out = placed + 1;
    if (digits > 0)
    {
        int remaining = digits;
        while (remaining != 0)
        {
            char digit = '0';
            if (*man != '\0')
            {
                digit = *man;
                man += 1;
            }
            *out = digit;
            out += 1;
            remaining -= 1;
        }
    }
    *out = '\0';
    if (digits >= 0 && *man >= '5')
    {
        char* round = out;
        while (true)
        {
            round -= 1;
            if (*round == '9')
            {
                *round = '0';
                continue;
            }
            *round = static_cast<char>(*round + 1);
            break;
        }
    }
    if (placed[0] == '1')
    {
        decpt += 1;
    }
    else
    {
        std::memmove(placed, placed + 1, std::strlen(placed + 1) + 1);
    }

    auto shift_right = [](char* text, int count)
    {
        if (count == 0)
        {
            return;
        }
        std::memmove(text + count, text, std::strlen(text) + 1);
    };

    char* text = buffer;
    if (sign == '-')
    {
        *text = '-';
        text += 1;
    }
    if (decpt <= 0)
    {
        shift_right(text, 1);
        *text = '0';
        text += 1;
    }
    else
    {
        text += decpt;
    }
    if (ndec > 0)
    {
        shift_right(text, 1);
        *text = '.';
        text += 1;
        if (decpt < 0)
        {
            int zeros = -decpt;
            if (ndec >= zeros)
            {
                ndec = zeros;
            }
            shift_right(text, ndec);
            for (int i = 0; i < ndec; i += 1)
            {
                text[i] = '0';
            }
        }
    }
    return buffer;
}


// Xbox 0x0016B45F
char* crt_format_general(double* value, char* buffer, int ndec, int caps)
{
    // Xbox 0x0016B45F
    // _cftog. __cfltcvt (0x0016B4EC) calls here for a conversion other than
    // 'e', 'E', or 'f', which is the 'g' and 'G' path. The arguments are the
    // double pointer, the output buffer, the precision, and the caps flag
    // (nonzero writes 'E'). EAX on return is the buffer pointer left there
    // by the layout helper.
    //
    // _fltout (0x0016F027) fills a stack _strflt the same way _cftoe does:
    // sign '-' or ' ' (0x20), decpt = exponent + 1, mantissa in a 24-byte
    // buffer. _dtold (0x0016EF6D) drops the sign of a zero, so -0.0 prints
    // as a positive zero. $I10_OUTPUT (0x0016F945) is asked for 17 digits.
    // A zero is the character '0' and decpt 0. An exponent of 0x7FFF selects
    // "1#INF", "1#IND", "1#QNAN", or "1#SNAN" with decpt 1.
    //
    // decpt - 1 is saved, then _fptostr (0x0016EEEE) runs with a count of
    // the precision and a destination one past a slot reserved for a minus.
    // There is no extra digit and no gap for the decimal point. It plants a
    // leading '0', copies that many characters (padding '0' past the
    // mantissa), and rounds when the next character is at least '5'. A count
    // below 0 writes an empty string and does not round. A count of 0 still
    // rounds from the first mantissa character. A carry into the guard
    // increments decpt and keeps the extra character; otherwise the guard is
    // shifted off.
    //
    // The format choice uses that rounded magnitude. Below -4, or at least
    // the precision, 0x0016B25F lays the text out. EBX is the _strflt and the
    // stack arguments are the buffer, the precision, the caps flag, and a
    // format byte of 1. A positive precision shifts the digits right by one
    // (0x0016B23C) before the decimal point from 0x0033818C ('.') is written,
    // and "e+000" from 0x00209B9C is copied at that point plus the precision.
    // That slot covers the extra character a carry kept. A nonzero caps flag
    // replaces 'e' with 'E'. A mantissa that still starts with '0' leaves
    // the exponent at "+000". Any other value writes decpt - 1, three
    // digits, with '-' over the '+' when the power is negative.
    //
    // Otherwise 0x0016B366 lays the text out. EAX is the _strflt and the
    // stack arguments are the buffer, the precision, and a format byte of 1.
    // If the round incremented decpt, the last character of the digit string
    // is cleared first. A minus is stored. A decpt of 0 or below shifts the
    // digits right and writes a leading '0'; a positive decpt steps over
    // that many characters. A positive precision shifts in the decimal
    // point. When decpt is still negative the format byte inserts -decpt
    // zeros after that point. The helper's other format-byte store, a
    // trailing '0' when decpt - 1 equals the precision, sits on the compare
    // that already selected the scientific helper.

    char mantissa[24];
    int sign = ' ';
    int decpt = 0;
    std::uint64_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    const std::uint64_t frac = bits & 0x000fffffffffffffull;
    const unsigned exp_bits = static_cast<unsigned>((bits >> 52) & 0x7ffu);

    if (exp_bits == 0 && frac == 0)
    {
        mantissa[0] = '0';
        mantissa[1] = '\0';
    }
    else if (exp_bits == 0x7ffu)
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        decpt = 1;
        const char* token = "1#QNAN";
        if (frac == 0)
        {
            token = "1#INF";
        }
        else if ((frac & 0x0008000000000000ull) == 0)
        {
            token = "1#SNAN";
        }
        else if ((bits >> 63) != 0 && frac == 0x0008000000000000ull)
        {
            token = "1#IND";
        }
        int i = 0;
        while (token[i] != '\0')
        {
            mantissa[i] = token[i];
            i += 1;
        }
        mantissa[i] = '\0';
    }
    else
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        const std::uint64_t mag_bits = bits & 0x7fffffffffffffffull;
        const double mag = std::bit_cast<double>(mag_bits);
        char scratch[40];
        const auto converted = std::to_chars(scratch, scratch + sizeof(scratch), mag, std::chars_format::scientific, 16);
        const char* cursor = scratch;
        int count = 0;
        int power = 0;
        if (converted.ec == std::errc{})
        {
            if (*cursor == '+' || *cursor == '-')
            {
                cursor += 1;
            }
            if (*cursor >= '0' && *cursor <= '9')
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == '.')
            {
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9' && count < 17)
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == 'e' || *cursor == 'E')
            {
                power = std::atoi(cursor + 1);
            }
        }
        if (count == 0)
        {
            mantissa[0] = '0';
            count = 1;
            power = 0;
        }
        mantissa[count] = '\0';
        decpt = power + 1;
    }

    auto shift_right = [](char* text, int count)
    {
        if (count == 0)
        {
            return;
        }
        std::memmove(text + count, text, std::strlen(text) + 1);
    };

    const int magnitude = decpt - 1;
    char* digits = buffer + (sign == '-' ? 1 : 0);
    const char* man = mantissa;
    digits[0] = '0';
    char* out = digits + 1;
    if (ndec > 0)
    {
        int remaining = ndec;
        while (remaining != 0)
        {
            char digit = '0';
            if (*man != '\0')
            {
                digit = *man;
                man += 1;
            }
            *out = digit;
            out += 1;
            remaining -= 1;
        }
    }
    *out = '\0';
    if (ndec >= 0 && *man >= '5')
    {
        char* round = out;
        while (true)
        {
            round -= 1;
            if (*round == '9')
            {
                *round = '0';
                continue;
            }
            *round = static_cast<char>(*round + 1);
            break;
        }
    }
    if (digits[0] == '1')
    {
        decpt += 1;
    }
    else
    {
        std::memmove(digits, digits + 1, std::strlen(digits + 1) + 1);
    }

    const int rounded = decpt - 1;
    if (rounded < -4 || rounded >= ndec)
    {
        if (ndec > 0)
        {
            shift_right(digits, 1);
        }
        char* text = buffer;
        if (sign == '-')
        {
            *text = '-';
            text += 1;
        }
        if (ndec > 0)
        {
            char* dot = text + 1;
            *text = *dot;
            *dot = '.';
            text = dot;
        }
        char* exp_at = text + ndec;
        std::memcpy(exp_at, "e+000", 6);
        if (caps != 0)
        {
            *exp_at = 'E';
        }
        exp_at += 1;
        if (mantissa[0] != '0')
        {
            int power = decpt - 1;
            if (power < 0)
            {
                power = -power;
                *exp_at = '-';
            }
            exp_at += 1;
            if (power >= 100)
            {
                *exp_at = static_cast<char>(*exp_at + power / 100);
                power %= 100;
            }
            exp_at += 1;
            if (power >= 10)
            {
                *exp_at = static_cast<char>(*exp_at + power / 10);
                power %= 10;
            }
            exp_at[1] = static_cast<char>(exp_at[1] + power);
        }
    }
    else
    {
        if (magnitude < rounded)
        {
            char* end = digits;
            while (*end != '\0')
            {
                end += 1;
            }
            end[-1] = '\0';
        }
        char* text = buffer;
        if (sign == '-')
        {
            *text = '-';
            text += 1;
        }
        if (decpt <= 0)
        {
            shift_right(text, 1);
            *text = '0';
            text += 1;
        }
        else
        {
            text += decpt;
        }
        if (ndec > 0)
        {
            shift_right(text, 1);
            *text = '.';
            text += 1;
            if (decpt < 0)
            {
                const int zeros = -decpt;
                shift_right(text, zeros);
                for (int i = 0; i < zeros; i += 1)
                {
                    text[i] = '0';
                }
            }
        }
    }
    return buffer;
}


// Xbox 0x00337A70. __NLG_Destination. The image signature is the C++ EH
// magic 0x19930520. The next three dwords are zero until __NLG_Notify1
// writes them. Nothing in the image reads the record back.
struct NlgDestination
{
    u32 sig;
    u32 destination;
    u32 code;
    u32 frame;
};


static_assert(sizeof(NlgDestination) == 0x10);


NlgDestination g_nlg_destination = {0x19930520u, 0, 0, 0};


// Xbox 0x001670F6
void crt_notify_nonlocal_goto(u32 destination, const u32* frame)
{
    // Xbox 0x001670F6
    // __NLG_Notify1. stdcall, ret 4. _except_handler3 (0x00166DAC) and
    // __local_unwind2 (0x00167062) call here before transferring to a
    // scope handler. The stack cookie (1 when a filter accepts, 0x101
    // when a scopetable entry has no filter) is popped and not read.
    // Incoming EAX is the handler. This body does not build a frame, so
    // EBP is the caller's and the dword at [EBP+8] is that frame's first
    // argument. _except_handler3 sets EBP to the registration node + 0x10
    // before its own call and before __local_unwind2, which leaves EBP
    // alone. EBX and ECX are saved across the three stores. The entry
    // at 0x001670ED (__NLG_Notify) is the same stores with ECX already
    // holding the code; _CallSettingFrame (0x0016C1E0) uses that entry.
    g_nlg_destination.destination = destination;
    g_nlg_destination.code = frame[2];
    g_nlg_destination.frame = static_cast<u32>(reinterpret_cast<std::uintptr_t>(frame));
}


// Xbox 0x003381A0. The image points every slot at _fptrap (0x0016F08C),
// which exits with code 2. _fpmath overwrites the six dwords.
u32 g_cfltcvt_tab[6] = {
    0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu,
};


// Xbox 0x001673C1
void crt_initialize_float_math(int set_precision)
{
    // Xbox 0x001673C1
    // _fpmath. _cinit (0x001516D4) calls the pointer at 0x00337A88, which
    // holds this address, before the __xi and __xc initializer walks.
    // 0x00167389 writes the six converter slots:
    //   [0] and [5] __cfltcvt   0x0016B4EC
    //   [1]         __cropzeros 0x0016B199
    //   [2]         __fassign   0x0016B1FE
    //   [3]         __forcdecpt 0x0016B141
    //   [4]         __positive  0x0016B1E4
    // A nonzero argument calls 0x00169774: __controlfp(0x10000, 0x30000),
    // mask _MCW_PC and value _PC_53 (53-bit mantissa). The compare reads the
    // dword above the return address. From _cinit that dword is the caller's
    // return address, so the precision change runs. Both paths execute fnclex.
    g_cfltcvt_tab[0] = 0x0016B4ECu;
    g_cfltcvt_tab[1] = 0x0016B199u;
    g_cfltcvt_tab[2] = 0x0016B1FEu;
    g_cfltcvt_tab[3] = 0x0016B141u;
    g_cfltcvt_tab[4] = 0x0016B1E4u;
    g_cfltcvt_tab[5] = 0x0016B4ECu;
    if (set_precision != 0)
    {
#if defined(_WIN32)
        unsigned int previous = 0;
        (void)_controlfp_s(&previous, _PC_53, _MCW_PC);
#else
        // Host IEEE doubles already round to a 53-bit mantissa.
#endif
    }
#if defined(_WIN32)
    (void)_clearfp();
#else
    std::feclearexcept(FE_ALL_EXCEPT);
#endif
}


// 0x00337AAC. __security_cookie. The image value is DEFAULT_SECURITY_COOKIE.
u32 g_security_cookie = 0xBB40E64Eu;


// Xbox 0x0016D805
void crt_initialize_security_cookie()
{
    // Xbox 0x0016D805
    // __security_init_cookie. The pointer sits at 0x0032C814, the first slot
    // of the __xc_a..__xc_z span (null at 0x0032C810, null at 0x0032C834).
    // _cinit walks that span. Nothing calls this address directly.
    // A cookie other than 0 or 0xBB40E64E is left alone. Otherwise the mix is
    // the high and low dwords of GetSystemTimeAsFileTime (0x0014BC60),
    // GetCurrentThreadId (0x0014BC53), GetTickCount (0x0014B956), and both
    // dwords of QueryPerformanceCounter (0x0014C29C). A zero mix is stored
    // back as 0xBB40E64E.
    constexpr u32 kDefaultCookie = 0xBB40E64Eu;
    if (g_security_cookie != 0 && g_security_cookie != kDefaultCookie)
    {
        return;
    }

    u32 time_low = 0;
    u32 time_high = 0;
    u32 thread_id = 0;
    u32 tick = 0;
    u32 perf_low = 0;
    u32 perf_high = 0;

#if defined(_WIN32)
    struct FileTime
    {
        u32 low;
        u32 high;
    };
    struct PerfCount
    {
        u32 low;
        u32 high;
    };
    FileTime file_time{};
    PerfCount counter{};
    GetSystemTimeAsFileTime(&file_time);
    time_low = file_time.low;
    time_high = file_time.high;
    thread_id = static_cast<u32>(GetCurrentThreadId());
    tick = static_cast<u32>(GetTickCount());
    QueryPerformanceCounter(&counter);
    perf_low = counter.low;
    perf_high = counter.high;
#else
    using FileTicks = std::chrono::duration<std::uint64_t, std::ratio<1, 10000000>>;
    const std::uint64_t file_time =
        std::chrono::duration_cast<FileTicks>(std::chrono::system_clock::now().time_since_epoch()).count() +
        116444736000000000ull;
    time_low = static_cast<u32>(file_time);
    time_high = static_cast<u32>(file_time >> 32);
    thread_id = 1;
    tick = static_cast<u32>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
    const std::uint64_t perf = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    perf_low = static_cast<u32>(perf);
    perf_high = static_cast<u32>(perf >> 32);
#endif

    g_security_cookie = time_high ^ time_low ^ thread_id ^ tick ^ perf_high ^ perf_low;
    if (g_security_cookie == 0)
    {
        g_security_cookie = kDefaultCookie;
    }
}


// Xbox 0x0016805F
void crt_report_security_failure()
{
    // Xbox 0x0016805F
    // __report_gsfailure. __security_check_cookie (0x0016808F) compares ECX
    // with __security_cookie at 0x00337AAC. A mismatch jumps here from
    // 0x00168098. Nothing else calls this address. __SEH_prolog (0x00166D58)
    // is entered with local size 8 and the scope table at 0x00209180:
    // enclosing level -1, filter 0x0016807C, handler 0x00168080. The filter
    // returns 1 (EXCEPTION_EXECUTE_HANDLER). The handler reloads ESP from
    // [ebp-0x18] and falls through.
    //
    // The try calls __security_error_handler (0x0016D85F) with
    // _SECERR_BUFFER_OVERRUN (1) and a null data pointer. The image hook at
    // 0x004DD2D0 is zero, so that callee writes the buffer-overrun text and
    // then calls _exit (0x0016985C) with status 3. It does not return. An
    // exception is swallowed here, the try level is set to -1, and _exit(3)
    // runs anyway. _exit pushes 0 then 1 into doexit (0x00169798), which
    // does not return when the return-to-caller argument is 0. The int 3
    // after that call is the noreturn trap. Status 3 is what this frame
    // pushes. The host exit skips atexit handlers, matching that _exit.
    // The filter accepts every exception (return value 1). The report is
    // 0x0016D85F and is not this body; both paths end in the same exit.
#if defined(_WIN32)
    __try
    {
    }
    __except (1)
    {
    }
#else
    try
    {
    }
    catch (...)
    {
    }
#endif
    std::_Exit(3);
}


// Image _locktable at 0x00337AB0. Thirty-six entries, eight bytes each:
// critical-section pointer, then kind. Kind 1 is a static lock.
constexpr u32 kCrtLockKind[0x24] = {
    1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


struct CrtLockSlot
{
    std::recursive_mutex* cs = nullptr;
    u32 kind = 0;
};


// Xbox XTHREAD_NOTIFICATION is a LIST_ENTRY plus the routine. The list head
// at 0x0032C9E0 is an empty list (Flink and Blink point at the head). The
// node _mtinit registers lives at 0x004DD288; the routine dword is +8.
struct XThreadNotify
{
    XThreadNotify* flink = nullptr;
    XThreadNotify* blink = nullptr;
    u32 routine = 0;
};


// Per-thread CRT block. Retail calloc is 0x88 bytes. +0x00 is the thread id,
// +0x04 is the thread handle, +0x14 is _holdrand, +0x54 is _pxcptacttab.
// The terminate pointer is the dword at +0x68. _curexception is the dword
// at +0x78, _curcontext is +0x7C, and the frame-info chain is +0x84. Those
// image slots are 32-bit. The host pointers follow the 0x88-byte block.
struct CrtTidData
{
    u32 tid;
    u32 thandle;
    u8 rest[0x80];
    void* curexception;
    void* curcontext;
    void* frame_chain;
    void (*terminate_fn)();
};


static_assert(offsetof(CrtTidData, curexception) == 0x88);


struct CrtModuleTls
{
    // Xbox stores the _tiddata pointer at offset 0xC of the 0x90-byte TLS
    // block (TLS directory SizeOfZeroFill, AddressOfIndex 0x00434234).
    void* ptd = nullptr;
};


CrtLockSlot g_crt_locks[0x24];

std::unique_ptr<std::recursive_mutex> g_crt_lock_mu[0x24];


// 0x004DD2D4. BSS, so the image dword is 0. 0x0016D9F8 writes 0x0016D9E8
// here when the slot is still null. The target is stdcall and returns BOOL.
int (*g_crt_crit_init)(std::recursive_mutex* section, u32 spin_count) = nullptr;

XThreadNotify g_xthread_notify_head;

XThreadNotify g_crt_thread_notify;

std::recursive_mutex g_xthread_notify_cs;

CrtModuleTls g_crt_module_tls;

u32 g_crt_tls_index = 0;


// 0x0054DD64. BSS, so the image dword is 0. A thread whose TlsData is null
// keeps its _tiddata pointer here instead of in the module TLS block.
void* g_crt_ptd_nontls = nullptr;


// Host stand-in for the dword at KTHREAD+0x28 (TlsData), read through
// fs:[0x28]. The game thread has TLS, so this stays non-null.
void* g_kthread_tls_data = &g_crt_module_tls;


// 0x0054DD60. BSS, so the image value is 0. 1 is __SYSTEM_HEAP and 3 is
// __V6_HEAP. Nothing in the image stores this dword; the compares still read it.
int g_crt_active_heap = 0;


// Xbox 0x00165F0E
void crt_unlock_type_info()
{
    // Xbox 0x00165F0E
    // Unwind funclet for type_info::~type_info (0x00165ED1). The state-0
    // entry at 0x002090F8 points here, and the destructor calls it on the
    // success path after setting the EH state to -1. The body pushes 0xE
    // (_TYPEINFO_LOCK) and calls _unlock (0x0016810F), then pops the cdecl
    // argument. _unlock leaves the critical section in _locktable[0xE]
    // (image 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    if (g_crt_locks[0xe].cs != nullptr)
    {
        g_crt_locks[0xe].cs->unlock();
    }
}


// _iob at 0x00337CF8. Twenty FILE records, 0x20 bytes each. _unlock_file
// treats a pointer as one of these when it is between that base and
// 0x00337F58, the address of the last record.
constexpr u32 kCrtIobCount = 20;


struct CrtFile
{
    u8 bytes[0x20];
};


static_assert(sizeof(CrtFile) == 0x20);


// A stream allocated past _iob is a _FILEX: the FILE bytes, then the
// critical section _unlock_file passes to RtlLeaveCriticalSection.
struct CrtFileX
{
    CrtFile file;
    std::recursive_mutex lock;
};


static_assert(offsetof(CrtFileX, lock) == 0x20);


CrtFile g_crt_iob[kCrtIobCount];


// 0x0054DD78. BSS, so the image value is 0. _nstream. 0x001688B3 raises a
// zero to 0x200 and a smaller value up to 0x14, then allocates that many
// FILE* slots. This body does not read the count; the port uses it so an
// index cannot walk off __piob.
u32 g_crt_nstream = 0;


// 0x0054DD74. BSS, so the image pointer is null. __piob. 0x001688B3 points
// the first twenty entries at _iob. The unlock body indexes it with the
// stream number left in ESI.
CrtFile** g_crt_piob = nullptr;


// Xbox 0x0016619A
void crt_unlock_closed_stream(void* stream)
{
    // Xbox 0x0016619A
    // Shared unlock body for fclose (0x00166151). The scope table at
    // 0x00209100 points the __finally at 0x00166197, which loads the
    // FILE* from [ebp+8] into ESI and falls through to this address.
    // The success path at 0x00166190 calls here after _fclose_lk with
    // ESI already holding that pointer. The eight bytes push ESI, call
    // _unlock_file (0x001689C2), pop the cdecl argument, and return.
    //
    // _unlock_file compares the pointer with _iob and with 0x00337F58.
    // Inside that span the lock number is (byte delta >> 5) + 0x10
    // (_STREAM_LOCKS). _unlock (0x0016810F) then leaves the critical
    // section stored in _locktable[index] at 0x00337AB0. Outside the
    // span it calls RtlLeaveCriticalSection (kernel ordinal 294, thunk
    // 0x00202EA4) on the critical section at FILE+0x20. A null ESI
    // would take that path at address 0x20; the port returns instead
    // of faulting.
    if (stream == nullptr)
    {
        return;
    }

    const auto addr = reinterpret_cast<std::uintptr_t>(stream);
    const auto base = reinterpret_cast<std::uintptr_t>(&g_crt_iob[0]);
    const auto last = reinterpret_cast<std::uintptr_t>(&g_crt_iob[kCrtIobCount - 1]);
    if (addr >= base && addr <= last)
    {
        const u32 index = static_cast<u32>((addr - base) >> 5) + 0x10u;
        if (g_crt_locks[index].cs != nullptr)
        {
            g_crt_locks[index].cs->unlock();
        }
        return;
    }

    reinterpret_cast<CrtFileX*>(stream)->lock.unlock();
}


int crt_mtinitlocknum(int locknum);


// Xbox 0x001681C1
void crt_lock(int locknum)
{
    // Xbox 0x001681C1
    // _lock. The slot is the 8-byte record at 0x00337AB0 + index*8. A null
    // critical-section pointer calls __mtinitlocknum (0x00168121). A zero
    // return calls _amsg_exit (0x00168B7A) with 0x11 (_RT_LOCK): the banner
    // at 0x00169894, the runtime message at 0x00169868, then the pointer at
    // 0x003380E0 (0x0016985C) which pushes 1,0 into 0x00169798. That path
    // does not come back. Otherwise RtlEnterCriticalSection (kernel ordinal
    // 277, thunk 0x00202EA0) takes the section stored in the slot. An index
    // outside the 36 slots walks off the table; the port returns.
    if (locknum < 0 || static_cast<u32>(locknum) >= 0x24u)
    {
        return;
    }

    if (g_crt_locks[locknum].cs == nullptr)
    {
        if (crt_mtinitlocknum(locknum) == 0)
        {
            std::exit(0xFF);
        }
    }
    g_crt_locks[locknum].cs->lock();
}


// Xbox 0x001681B8
void crt_unlock_lock_table()
{
    // Xbox 0x001681B8
    // __finally of __mtinitlocknum (0x00168121). The scope table at
    // 0x00209190 points here. The nine bytes push 10 (_LOCKTAB_LOCK),
    // call _unlock (0x0016810F), pop the cdecl argument, and return.
    // _unlock leaves the critical section in _locktable[10] (image
    // 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    // _local_unwind2 (0x00167062) runs this handler when
    // InitializeCriticalSection (0x0016D9F8) fails. The success path at
    // 0x001681AA calls it after the try level at [ebp-4] is set to -1.
    if (g_crt_locks[10].cs != nullptr)
    {
        g_crt_locks[10].cs->unlock();
    }
}


// Xbox 0x0016AE40
CrtTidData* crt_thread_data()
{
    // Xbox 0x0016AE40
    // _getptd. The IRQL byte at fs:[0x24] above 1 is KeBugCheck(10). The
    // host thread stays at passive level, so that check does not fire.
    // fs:[0x28] is the current KTHREAD. A zero dword at KTHREAD+0x28
    // (TlsData) selects the global pointer at 0x0054DD64. Otherwise the
    // slot is fs:[4][__tls_index] + 0xC: fs:[4] is the thread TLS array
    // and __tls_index is the dword at 0x00434234. This port keeps that
    // slot in g_crt_module_tls.ptd. The game thread has TLS, so it takes
    // that path.
    //
    // A null slot is calloc of the host block (retail 0x88 bytes,
    // 0x00168012). A failed
    // allocation is _amsg_exit (0x00168B7A) with 0x10 (_RT_THREAD) and
    // does not return. The new block is stored in the slot, +0x54
    // receives the static exception table 0x00338280, +0x14 receives 1
    // (_holdrand), +4 receives 0xFFFFFFFF, and +0 receives the thread id
    // from GetCurrentThreadId (0x0014BC53).
    void** slot = &g_crt_ptd_nontls;
    if (g_kthread_tls_data != nullptr)
    {
        slot = &g_crt_module_tls.ptd;
    }

    CrtTidData* ptd = static_cast<CrtTidData*>(*slot);
    if (ptd == nullptr)
    {
        ptd = static_cast<CrtTidData*>(std::calloc(1, sizeof(CrtTidData)));
        if (ptd == nullptr)
        {
            std::exit(0xFF);
        }
        *slot = ptd;
        auto* raw = reinterpret_cast<u8*>(ptd);
        *reinterpret_cast<u32*>(raw + 0x54) = 0x00338280u;
        *reinterpret_cast<u32*>(raw + 0x14) = 1u;
#if defined(_WIN32)
        const u32 tid = static_cast<u32>(GetCurrentThreadId());
#else
        const u32 tid = 1;
#endif
        ptd->thandle = 0xffffffffu;
        ptd->tid = tid;
    }
    return ptd;
}


// Xbox 0x0016C177
[[noreturn]] void crt_terminate()
{
    // Xbox 0x0016C177
    // terminate. __SEH_prolog (0x00166D58) is entered with local size 8 and
    // the scope table at 0x00209C38: enclosing level -1, filter 0x0016C19C,
    // handler 0x0016C1A0. _getptd (0x0016AE40) returns _tiddata. A null
    // dword at +0x68 jumps to the bugcheck. Otherwise the try level at
    // [ebp-4] is set to 0, a second _getptd loads that slot, and the
    // pointer is called. The filter is xor eax,eax / inc eax / ret, so it
    // returns 1 and every exception is swallowed. The handler reloads ESP
    // from [ebp-0x18]. A normal return skips that reload with a short jump.
    // Both paths then set the try level to -1, push 0xC0000144, and call
    // KeBugCheck through the dword at 0x00203040 (kernel ordinal 0x5F).
    // The int 3 is the noreturn trap. The host pointer lives past the
    // 0x88-byte image, and abort matches a bugcheck that does not return.
    CrtTidData* ptd = crt_thread_data();
    if (ptd->terminate_fn != nullptr)
    {
#if defined(_WIN32)
        __try
        {
            ptd = crt_thread_data();
            ptd->terminate_fn();
        }
        __except (1)
        {
        }
#else
        try
        {
            ptd = crt_thread_data();
            ptd->terminate_fn();
        }
        catch (...)
        {
        }
#endif
    }
    std::abort();
}


// Catch frame pushed by _CreateFrameInfo (0x001678B8). Retail is two dwords:
// the exception object, then the previous chain head.
struct CrtFrameInfo
{
    void* object;
    CrtFrameInfo* next;
};


// ThrowInfo as CallCatchBlock reads it. The unwind function is the pointer
// at +4. Retail passes that pointer to the thiscall thunk at 0x001675FB.
struct CrtThrowLite
{
    u32 attributes;
    void (*dtor)(void* self);
};


// Exception record. Retail object is +0x18 and ThrowInfo is +0x1C. Host
// pointers follow the same field order with natural alignment.
struct CrtExceptRec
{
    u32 code;
    u32 flags;
    void* next;
    void* address;
    u32 nparams;
    u32 magic;
    void* object;
    CrtThrowLite* throw_info;
};


// EHRegistrationNode. state is the dword at +8. CallCatchBlock also reads
// and writes the dword immediately before the node.
struct CrtEhNode
{
    void* next;
    void* handler;
    i32 state;
};


// Unwind map slot. Retail is 8 bytes: toState, then the action.
struct CrtUnwindSlot
{
    i32 to_state;
    void (*action)(void* registration);
};


// Try block. Retail stride is 20 bytes: tryLow, tryHigh, catchHigh,
// nCatches, handler array.
struct CrtTrySlot
{
    i32 try_low;
    i32 try_high;
    i32 catch_high;
    i32 n_catches;
    void* handlers;
};


// FuncInfo fields CallCatchBlock uses. maxState is +4, the unwind map is
// +8, nTryBlocks is +0xC, and the try map is +0x10 in the image.
struct CrtFuncRec
{
    u32 magic;
    i32 max_state;
    const CrtUnwindSlot* unwind;
    i32 n_try_blocks;
    const CrtTrySlot* tries;
};


void crt_link_frame(CrtFrameInfo* frame, void* object)
{
    // _CreateFrameInfo (0x001678B8). The object is stored, the chain head
    // at _tiddata+0x84 becomes the next pointer, and that head becomes
    // this frame. Each link is a separate _getptd.
    frame->object = object;
    CrtTidData* ptd = crt_thread_data();
    frame->next = static_cast<CrtFrameInfo*>(ptd->frame_chain);
    ptd = crt_thread_data();
    ptd->frame_chain = frame;
}


void crt_unlink_frame(CrtFrameInfo* frame)
{
    // _FindAndUnlinkFrame (0x00167901). A frame that is the chain head is
    // replaced by its next pointer. Otherwise the chain is walked until
    // the previous node's next pointer is this frame. A missing frame is
    // __inconsistency (0x0016C1B3).
    CrtTidData* ptd = crt_thread_data();
    if (ptd->frame_chain == frame)
    {
        ptd = crt_thread_data();
        ptd->frame_chain = frame->next;
        return;
    }

    ptd = crt_thread_data();
    auto* prev = static_cast<CrtFrameInfo*>(ptd->frame_chain);
    while (prev != nullptr && prev->next != nullptr)
    {
        if (prev->next == frame)
        {
            prev->next = frame->next;
            return;
        }
        prev = prev->next;
    }
    std::terminate();
}


int crt_exception_object_to_destroy(void* object)
{
    // _IsExceptionObjectToBeDestroyed (0x001678E0). Walks the chain at
    // _tiddata+0x84. A frame whose object matches returns 0. The end of
    // the chain returns 1.
    CrtTidData* ptd = crt_thread_data();
    auto* frame = static_cast<CrtFrameInfo*>(ptd->frame_chain);
    while (frame != nullptr)
    {
        if (frame->object == object)
        {
            return 0;
        }
        frame = frame->next;
    }
    return 1;
}


int crt_dtor_throw_fails()
{
    // 0x001670CA. Returns 1 when fs:[0] is the registration __local_unwind2
    // (0x00167062) installs, whose handler is 0x00167040, and the trylevel
    // dword of the node saved on that frame equals the -2 cookie at
    // frame+8. The port does not install that registration, so the result
    // stays 0. ___DestructExceptionObject's filter (0x0016BA7E) returns
    // that byte: 0 continues the search, and 1 enters terminate.
    return 0;
}


void crt_destruct_except(CrtExceptRec* except)
{
    // Body of ___DestructExceptionObject (0x0016BA4A) for the call from
    // the CallCatchBlock finally. A null record or a null ThrowInfo
    // returns. A null unwind function at ThrowInfo+4 returns. Otherwise
    // the function runs on the exception object. The second argument is
    // 0x001670CA.
    if (except == nullptr || except->throw_info == nullptr || except->throw_info->dtor == nullptr)
    {
        return;
    }
    const int fail = crt_dtor_throw_fails();
    try
    {
        except->throw_info->dtor(except->object);
    }
    catch (...)
    {
        if (fail != 0)
        {
            crt_terminate();
        }
        throw;
    }
}


i32* crt_processing_throw(CrtTidData* ptd)
{
    return reinterpret_cast<i32*>(reinterpret_cast<u8*>(ptd) + 0x80);
}


void crt_unwind_rethrow(CrtEhNode* rn, CrtFuncRec* info)
{
    // Except handler at 0x0016BB76, then __FrameUnwindToState (0x0016B97C).
    // The state is the dword at registration+8. The first try whose
    // tryHigh is below that state and whose catchHigh covers it selects
    // unwindMap[tryHigh+1].toState. No match unwinds to the current
    // state. The dispatcher argument is null. _ProcessingThrow at
    // _tiddata+0x80 is incremented for the walk. A state below 0, a
    // missing unwind map, or a state past maxState is __inconsistency.
    // An action runs after the state dword is updated. An exception from
    // an action clears _ProcessingThrow and terminates (0x0016C177).
    // 0x0016BA2F then decrements a positive count.
    if (rn == nullptr || info == nullptr)
    {
        std::terminate();
    }

    i32 target = rn->state;
    if (info->n_try_blocks > 0)
    {
        if (info->tries == nullptr)
        {
            std::terminate();
        }
        for (i32 i = 0; i < info->n_try_blocks; i += 1)
        {
            const CrtTrySlot& entry = info->tries[i];
            if (target <= entry.try_high || target > entry.catch_high)
            {
                continue;
            }
            const i32 index = entry.try_high + 1;
            if (info->unwind == nullptr || index < 0 || index >= info->max_state)
            {
                std::terminate();
            }
            target = info->unwind[index].to_state;
            break;
        }
    }

    CrtTidData* ptd = crt_thread_data();
    ++(*crt_processing_throw(ptd));
    i32 state = rn->state;
    while (state != target)
    {
        if (state <= -1 || info->unwind == nullptr || state >= info->max_state)
        {
            std::terminate();
        }
        const CrtUnwindSlot& entry = info->unwind[state];
        const i32 next = entry.to_state;
        if (entry.action != nullptr)
        {
            rn->state = next;
            try
            {
                entry.action(rn);
            }
            catch (...)
            {
                *crt_processing_throw(crt_thread_data()) = 0;
                crt_terminate();
            }
        }
        state = next;
    }
    ptd = crt_thread_data();
    if (*crt_processing_throw(ptd) > 0)
    {
        ptd = crt_thread_data();
        --(*crt_processing_throw(ptd));
    }
    if (state != target)
    {
        std::terminate();
    }
    rn->state = state;
}


void crt_catch_cleanup(
    CrtExceptRec* except,
    CrtEhNode* rn,
    i32 saved_before,
    CrtFrameInfo* frame,
    void* saved_ex,
    void* saved_ctx,
    void* continuation)
{
    // Finally at 0x0016BBF1. Restores the dword before the registration
    // node, unlinks the frame, then writes the saved _curexception and
    // _curcontext back through two _getptd calls. A C++ exception
    // (0xE06D7363, three parameters, magic 0x19930520) with a non-null
    // continuation destroys its object when 0x001678E0 returns 1.
    // [ebp-0x48] is cleared on entry to CallCatchBlock and is not stored
    // again, so that compare does not skip the destroy.
    reinterpret_cast<i32*>(rn)[-1] = saved_before;
    crt_unlink_frame(frame);
    CrtTidData* ptd = crt_thread_data();
    ptd->curexception = saved_ex;
    ptd = crt_thread_data();
    ptd->curcontext = saved_ctx;
    if (except == nullptr)
    {
        return;
    }
    if (except->code != 0xE06D7363u || except->nparams != 3 || except->magic != 0x19930520u)
    {
        return;
    }
    if (continuation == nullptr)
    {
        return;
    }
    if (crt_exception_object_to_destroy(except->object) == 0)
    {
        return;
    }
    crt_destruct_except(except);
}


#if defined(_WIN32)
int crt_rethrow_filter(void* pointers)
{
    // Filter at 0x0016BB33. Returns 1 (EXCEPTION_EXECUTE_HANDLER) only for
    // a C++ exception whose ThrowInfo pointer is null. Every other record
    // returns 0 (EXCEPTION_CONTINUE_SEARCH). The image reads the record
    // from the exception pointers at [ebp-0x14]: code at +0, parameter
    // count at +0x10, magic at +0x14, ThrowInfo at +0x1C. The host record
    // uses the same fields; a 64-bit pointer moves the parameter count to
    // +0x18 and the information array to +0x20.
    if (pointers == nullptr)
    {
        return 0;
    }
    void* record = *static_cast<void**>(pointers);
    if (record == nullptr)
    {
        return 0;
    }
    auto* bytes = static_cast<u8*>(record);
    const u32 code = *reinterpret_cast<u32*>(bytes);
    constexpr u32 kParams = sizeof(void*) == 8 ? 0x18u : 0x10u;
    constexpr u32 kInfo = sizeof(void*) == 8 ? 0x20u : 0x14u;
    const u32 nparams = *reinterpret_cast<u32*>(bytes + kParams);
    const auto* info = reinterpret_cast<const std::uintptr_t*>(bytes + kInfo);
    if (code != 0xE06D7363u || nparams != 3 || info[0] != 0x19930520u || info[2] != 0)
    {
        return 0;
    }
    return 1;
}
#endif


// Xbox 0x0016BAAE
void* crt_call_catch_block(
    void* handler,
    void* except_record,
    void* registration,
    void* context,
    void* func_info,
    int catch_depth,
    u32 nlg_code)
{
    // Xbox 0x0016BAAE
    // CallCatchBlock. The funclet arrives in ECX. The stack arguments, in
    // order, are the exception record, the registration node, the context,
    // FuncInfo, the catch depth, and the NLG code. __SEH_prolog
    // (0x00166D58) is entered with local size 0x40 and the scope table at
    // 0x00209C10. Level 0 is the __finally at 0x0016BBEB. Level 1 is the
    // __except with filter 0x0016BB33 and handler 0x0016BB76.
    //
    // The dword before the registration node is saved. The thrown object
    // at exception+0x18 is pushed on the frame-info chain. _curexception
    // and _curcontext are saved and replaced for the funclet.
    // __CallCatchBlock2 (0x0016794D) calls _CallSettingFrame (0x0016C1E0)
    // with the funclet, the registration node, and the NLG code, and
    // returns the continuation. Catch depth is the argument before the
    // NLG code. The continuation slot is first written with the funclet,
    // so an exception that leaves the inner try still sees that pointer.
    // A rethrow accepted by the filter unwinds and stores a null
    // continuation. A null funclet is left uncalled.
    (void)catch_depth;
    (void)nlg_code;

    auto* except = static_cast<CrtExceptRec*>(except_record);
    auto* rn = static_cast<CrtEhNode*>(registration);
    auto* info = static_cast<CrtFuncRec*>(func_info);
    i32 saved_before = reinterpret_cast<i32*>(rn)[-1];
    CrtFrameInfo frame{};
    crt_link_frame(&frame, except->object);

    CrtTidData* ptd = crt_thread_data();
    void* saved_ex = ptd->curexception;
    ptd = crt_thread_data();
    void* saved_ctx = ptd->curcontext;
    ptd = crt_thread_data();
    ptd->curexception = except_record;
    ptd = crt_thread_data();
    ptd->curcontext = context;

    void* volatile continuation = handler;
#if defined(_WIN32)
    __try
    {
        __try
        {
            if (handler != nullptr)
            {
                continuation = reinterpret_cast<void* (*)()>(handler)();
            }
        }
        __except (crt_rethrow_filter(GetExceptionInformation()))
        {
            crt_unwind_rethrow(rn, info);
            continuation = nullptr;
        }
    }
    __finally
    {
        crt_catch_cleanup(except, rn, saved_before, &frame, saved_ex, saved_ctx, continuation);
    }
#else
    (void)info;
    try
    {
        if (handler != nullptr)
        {
            continuation = reinterpret_cast<void* (*)()>(handler)();
        }
    }
    catch (...)
    {
        crt_catch_cleanup(except, rn, saved_before, &frame, saved_ex, saved_ctx, continuation);
        throw;
    }
    crt_catch_cleanup(except, rn, saved_before, &frame, saved_ex, saved_ctx, continuation);
#endif
    return continuation;
}


// Xbox 0x00168AF5
int* crt_errno_pointer()
{
    // Xbox 0x00168AF5
    // _errno. Nine bytes: call _getptd (0x0016AE40), add eax, 8, ret.
    // The dword at _tiddata+8 is _terrno. __mtinitlocknum stores ENOMEM
    // (0xC) through this pointer.
    CrtTidData* ptd = crt_thread_data();
    return reinterpret_cast<int*>(reinterpret_cast<u8*>(ptd) + 8);
}


// Xbox 0x00168AFE
u32* crt_dos_errno_pointer()
{
    // Xbox 0x00168AFE
    // __doserrno. Nine bytes: call _getptd (0x0016AE40), add eax, 0xC, ret.
    // The dword at _tiddata+0xC is _tdoserrno.
    CrtTidData* ptd = crt_thread_data();
    return reinterpret_cast<u32*>(reinterpret_cast<u8*>(ptd) + 0xc);
}


// One lowio record. Retail stride is 0x28: a 32-bit OS handle, osfile at +4,
// pipech at +5, a dword at +8, and the critical section at +0xC.
// _ioinit (0x0016D401) allocates 32 of them and stores the pointer in
// __pioinfo[0]. Handles start at -1, pipech is 10, and the dword at +8 is 0.
// The first three records are then marked FOPEN|FDEV|FTEXT (0xC1) for stdin,
// stdout, and stderr. The port keeps that order and stores a host handle, so
// the record is wider than 0x28. _lock_fhandle (0x0016DBAE) treats the dword
// at +8 as an init count: zero creates the section and increments it. Until
// then the pointer stays null. cs_owner keeps the section for the process.
struct CrtIoInfo
{
    std::intptr_t osfhnd = -1;
    u8 osfile = 0;
    u8 pipech = 10;
    u32 field8 = 0;
    std::recursive_mutex* cs = nullptr;
    std::unique_ptr<std::recursive_mutex> cs_owner;
};


constexpr u32 kCrtIoArrayCount = 64;

constexpr u32 kCrtIoPerArray = 32;


CrtIoInfo g_crt_ioinfo0[kCrtIoPerArray] = {
    {-1, 0xC1, 10, 0},
    {-1, 0xC1, 10, 0},
    {-1, 0xC1, 10, 0},
};


// 0x0054DC60. Sixty-four pointers. Only the first is filled by _ioinit.
CrtIoInfo* g_crt_pioinfo[kCrtIoArrayCount] = {g_crt_ioinfo0};


// 0x0054DC40. _nhandle. _ioinit stores 32.
u32 g_crt_nhandle = kCrtIoPerArray;


// Image pairs at 0x00337F78. __dosmaperr walks all 45. Win32 code 6 appears
// twice; the first hit (EBADF, 9) wins.
struct CrtDosMap
{
    u32 dos;
    int err;
};


constexpr CrtDosMap kCrtDoserrMap[45] = {
    {1, 22},    {2, 2},     {3, 2},     {4, 24},    {5, 13},    {6, 9},     {7, 12},   {8, 12},
    {9, 12},    {10, 7},    {11, 8},    {12, 22},   {13, 22},   {15, 2},    {16, 13},  {17, 18},
    {18, 2},    {33, 13},   {53, 2},    {65, 13},   {67, 2},    {80, 17},   {82, 13},  {83, 13},
    {87, 22},   {89, 11},   {108, 13},  {109, 32},  {112, 28},  {114, 9},   {6, 22},   {128, 10},
    {129, 10},  {130, 9},   {131, 22},  {132, 13},  {145, 41},  {158, 13},  {161, 2},  {164, 11},
    {167, 13},  {183, 17},  {206, 2},   {215, 11},  {1816, 12},
};


void crt_dosmaperr(u32 code)
{
    // __dosmaperr at 0x00168B07. _doserrno is stored first. A table hit
    // writes that errno. Codes 0x13 through 0x24 are EACCES (13). Codes
    // 0xBC through 0xCA are ENOEXEC (8). Every other code is EINVAL (22).
    *crt_dos_errno_pointer() = code;
    for (const CrtDosMap& entry : kCrtDoserrMap)
    {
        if (entry.dos == code)
        {
            *crt_errno_pointer() = entry.err;
            return;
        }
    }
    if (code >= 0x13u && code <= 0x24u)
    {
        *crt_errno_pointer() = 13;
        return;
    }
    if (code >= 0xBCu && code <= 0xCAu)
    {
        *crt_errno_pointer() = 8;
        return;
    }
    *crt_errno_pointer() = 22;
}


CrtIoInfo* crt_ioinfo_checked(u32 fh)
{
    if (fh >= g_crt_nhandle)
    {
        return nullptr;
    }
    const int block_index = static_cast<int>(fh) >> 5;
    if (block_index < 0 || static_cast<u32>(block_index) >= kCrtIoArrayCount)
    {
        return nullptr;
    }
    CrtIoInfo* block = g_crt_pioinfo[block_index];
    if (block == nullptr)
    {
        return nullptr;
    }
    return &block[fh & 0x1fu];
}


std::intptr_t crt_get_osfhandle(u32 fh)
{
    // __get_osfhandle at 0x0016DB6D. A descriptor outside _nhandle, or one
    // whose osfile lacks FOPEN, stores EBADF (9), clears _doserrno, and
    // returns -1. Otherwise the OS handle is returned, including -1.
    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr || (info->osfile & 1u) == 0)
    {
        *crt_errno_pointer() = 9;
        *crt_dos_errno_pointer() = 0;
        return -1;
    }
    return info->osfhnd;
}


// Xbox 0x0016D466
int crt_seek_unlocked(u32 fh, int offset, u32 method)
{
    // Xbox 0x0016D466
    // _lseek_lk. cdecl. The caller pops the descriptor, the signed byte
    // offset, and the origin. FILE_BEGIN, FILE_CURRENT, and FILE_END are
    // the same values as SEEK_SET, SEEK_CUR, and SEEK_END. This body does
    // not lock the descriptor; the locked _lseek calls here with that lock
    // already held.
    //
    // __get_osfhandle (0x0016DB6D) supplies the OS handle. A handle of -1
    // stores EBADF (9) through _errno (0x00168AF5) and returns -1.
    // _doserrno stays as __get_osfhandle left it. An open slot whose stored
    // handle is already -1 takes this path too.
    //
    // Otherwise XAPILIB::SetFilePointer is called with a null high distance.
    // A return other than -1 stores a zero DOS code. A return of -1 stores
    // GetLastError (0x0014AFE4). A zero code, including a file position of
    // 0xFFFFFFFF with NO_ERROR, clears FEOFLAG (bit 1) and returns that
    // position. The osfile byte is __pioinfo[fd >> 5] at 0x0054DC60, slot
    // (fd & 0x1F), stride 0x28, offset +4, masked with 0xFD. A null
    // __pioinfo slot is left unwritten; retail would fault on that address.
    //
    // Any other DOS code goes through __dosmaperr (0x00168B07) and the
    // return is -1. errno is not cleared on success.
    const std::intptr_t handle = crt_get_osfhandle(fh);
    if (handle == -1)
    {
        *crt_errno_pointer() = 9;
        return -1;
    }

    int position = -1;
    u32 doserr = 0;
#if defined(_WIN32)
    const unsigned long moved =
        SetFilePointer(reinterpret_cast<void*>(handle), static_cast<long>(offset), nullptr, method);
    position = static_cast<int>(moved);
    if (moved == 0xffffffffu)
    {
        doserr = GetLastError();
    }
#else
    (void)handle;
    (void)offset;
    (void)method;
    position = 0;
#endif

    if (doserr == 0)
    {
        CrtIoInfo* info = crt_ioinfo_checked(fh);
        if (info != nullptr)
        {
            info->osfile = static_cast<u8>(info->osfile & 0xfdu);
        }
    }
    else
    {
        crt_dosmaperr(doserr);
        position = -1;
    }
    return position;
}


void crt_free_osfhnd(u32 fh)
{
    // _free_osfhnd at 0x0016DB24. An open slot whose handle is not -1 stores
    // -1. Every other slot stores EBADF and clears _doserrno. The return
    // value is unused by _close_lk.
    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info != nullptr && (info->osfile & 1u) != 0 && info->osfhnd != -1)
    {
        info->osfhnd = -1;
        return;
    }
    *crt_errno_pointer() = 9;
    *crt_dos_errno_pointer() = 0;
}


bool crt_close_os_handle(std::intptr_t handle, u32* doserr)
{
    // CloseHandle at 0x0014B785. Nonzero is success. Zero keeps GetLastError
    // (0x0014AFE4) for __dosmaperr.
#if defined(_WIN32)
    if (CloseHandle(reinterpret_cast<void*>(handle)) != 0)
    {
        return true;
    }
    *doserr = static_cast<u32>(GetLastError());
    return false;
#else
    (void)handle;
    (void)doserr;
    return true;
#endif
}


// Xbox 0x00168B96
int crt_close_unlocked(u32 fh)
{
    // Xbox 0x00168B96
    // _close_lk. __close (0x00168C17) calls here with the file lock already
    // held. cdecl; the caller pops the descriptor. Zero is success. Minus
    // one is an OS failure.
    //
    // A handle of -1 from __get_osfhandle skips CloseHandle. Descriptors 1
    // and 2 also skip it when stdout and stderr share one OS handle, so
    // closing one standard stream leaves that console handle open. Any
    // other handle is closed. A failed CloseHandle keeps GetLastError.
    //
    // _free_osfhnd runs either way, then the osfile byte is cleared. The
    // index is the arithmetic shift of the descriptor by 5, and the byte
    // sits at offset 4 of record (fd & 0x1F). A null __pioinfo slot is
    // left unwritten; retail would fault on that address.
    //
    // A zero DOS code returns 0. Any other code goes through __dosmaperr
    // and the return is -1.
    std::intptr_t handle = crt_get_osfhandle(fh);
    u32 doserr = 0;
    if (handle != -1)
    {
        bool skip_close = false;
        if (fh == 1u || fh == 2u)
        {
            const std::intptr_t stderr_handle = crt_get_osfhandle(2u);
            const std::intptr_t stdout_handle = crt_get_osfhandle(1u);
            if (stdout_handle == stderr_handle)
            {
                skip_close = true;
            }
        }
        if (!skip_close)
        {
            handle = crt_get_osfhandle(fh);
            if (crt_close_os_handle(handle, &doserr))
            {
                doserr = 0;
            }
        }
    }

    crt_free_osfhnd(fh);

    const int block_index = static_cast<int>(fh) >> 5;
    if (block_index >= 0 && static_cast<u32>(block_index) < kCrtIoArrayCount)
    {
        CrtIoInfo* block = g_crt_pioinfo[block_index];
        if (block != nullptr)
        {
            block[fh & 0x1fu].osfile = 0;
        }
    }

    if (doserr == 0)
    {
        return 0;
    }
    crt_dosmaperr(doserr);
    return -1;
}


// Xbox 0x00168C8E
void crt_unlock_close_descriptor(int fh)
{
    // Xbox 0x00168C8E
    // Shared unlock body for __close (0x00168C17). The scope table at
    // 0x00209258 has one entry: enclosing level -1, a null filter, and the
    // handler at 0x00168C8B. That handler loads the descriptor from [ebp+8]
    // into EBX and falls through to this address. The success path at
    // 0x00168C81 calls here after _close_lk (0x00168B96) with EBX already
    // holding that descriptor. The eight bytes push EBX, call
    // _unlock_fhandle (0x0016DC4E), pop the cdecl argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    const int block_index = fh >> 5;
    if (block_index < 0 || static_cast<u32>(block_index) >= kCrtIoArrayCount)
    {
        return;
    }
    CrtIoInfo* block = g_crt_pioinfo[block_index];
    if (block == nullptr)
    {
        return;
    }
    std::recursive_mutex* lock = block[fh & 0x1f].cs;
    if (lock == nullptr)
    {
        return;
    }
    lock->unlock();
}


// Xbox 0x0016D55F
void crt_unlock_seek_descriptor(int fh)
{
    // Xbox 0x0016D55F
    // Shared unlock tail for _lseek (0x0016D4D8). The scope table at
    // 0x00209C88 has one entry: enclosing level -1, a null filter, and the
    // handler at 0x0016D55C. That handler loads the descriptor from [ebp+8]
    // into EBX and falls through to this address. The success path at
    // 0x0016D552 calls here after _lseek_lk (0x0016D466) with EBX already
    // holding that descriptor. The eight bytes push EBX, call
    // _unlock_fhandle (0x0016DC4E), pop the cdecl argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    crt_unlock_close_descriptor(fh);
}


bool crt_read_file(std::intptr_t handle, void* buffer, u32 bytes, u32* got, u32* doserr)
{
    // ReadFile at 0x0014B3A3. Nonzero is success. Zero keeps GetLastError
    // (0x0014AFE4). The transferred count is reported either way.
#if defined(_WIN32)
    unsigned long n = 0;
    const int ok = ReadFile(reinterpret_cast<void*>(handle), buffer, bytes, &n, nullptr);
    *got = static_cast<u32>(n);
    if (ok != 0)
    {
        return true;
    }
    *doserr = static_cast<u32>(GetLastError());
    return false;
#else
    (void)handle;
    (void)buffer;
    (void)bytes;
    (void)doserr;
    *got = 0;
    return true;
#endif
}


// Xbox 0x0016D583
int crt_read_unlocked(u32 fh, char* buf, u32 count)
{
    // Xbox 0x0016D583
    // _read_lk. cdecl. The caller pops the descriptor, the buffer, and the
    // byte count. This body does not lock the descriptor; the locked _read
    // calls here with that lock already held.
    //
    // A count of 0 returns 0. The record is __pioinfo[fd >> 5] at
    // 0x0054DC60, slot (fd & 0x1F), stride 0x28. osfile is at +4 and the
    // pipe lookahead is at +5. FEOFLAG (bit 1) returns 0 without reading.
    // A null __pioinfo slot is the same zero return; retail would fault on
    // that address.
    //
    // FPIPE or FDEV (0x48) with a lookahead other than LF (10) stores that
    // byte at the front of the buffer, decrements the count, sets the
    // lookahead back to LF, and reads the rest one byte later.
    //
    // XAPILIB::ReadFile uses the OS handle at record+0. A zero return takes
    // GetLastError. Code 5 stores EBADF (9) through _errno (0x00168AF5) and
    // 5 through __doserrno (0x00168AFE), then returns -1. Code 0x6D
    // (ERROR_BROKEN_PIPE) returns 0 and leaves errno alone. Any other code
    // goes through __dosmaperr (0x00168B07) and returns -1.
    //
    // A successful read that is not FTEXT (bit 0x80) returns the lookahead
    // byte plus the OS count. FTEXT then sets FCRLF (bit 2) when the OS
    // count is nonzero and the first buffer byte is LF, and clears it
    // otherwise. That test uses the original buffer, so a consumed
    // lookahead is what gets tested.
    //
    // The buffer is then scanned in place. CR LF becomes LF. A lone CR
    // stays CR. CTRL-Z (0x1A) stops the scan and is not copied. Unless
    // FDEV (bit 0x40) is set, CTRL-Z also sets FEOFLAG. A CR that is the
    // last buffered byte reads one more OS byte. A failed peek, or a peek
    // of zero bytes, stores the CR. On a pipe or device, LF replaces the
    // CR and any other byte is saved as the lookahead after the CR. On a
    // disk file, LF at the start of an otherwise empty result is stored
    // and consumed. Any other peek is pushed back with _lseek_lk
    // (0x0016D466) by -1 from the current position; LF is dropped with the
    // CR, and any other byte leaves the CR in the buffer.
    if (count == 0)
    {
        return 0;
    }

    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr || (info->osfile & 2u) != 0)
    {
        return 0;
    }

    char* read_at = buf;
    u32 carried = 0;
    if ((info->osfile & 0x48u) != 0 && info->pipech != 10)
    {
        count -= 1u;
        *buf = static_cast<char>(info->pipech);
        read_at = buf + 1;
        carried = 1u;
        info->pipech = 10;
    }

    u32 os_read = 0;
    u32 doserr = 0;
    if (!crt_read_file(info->osfhnd, read_at, count, &os_read, &doserr))
    {
        if (doserr == 5u)
        {
            *crt_errno_pointer() = 9;
            *crt_dos_errno_pointer() = 5u;
            return -1;
        }
        if (doserr == 0x6du)
        {
            return 0;
        }
        crt_dosmaperr(doserr);
        return -1;
    }

    const u32 total = carried + os_read;
    if ((info->osfile & 0x80u) == 0)
    {
        return static_cast<int>(total);
    }

    if (os_read != 0 && *buf == '\n')
    {
        info->osfile = static_cast<u8>(info->osfile | 4u);
    }
    else
    {
        info->osfile = static_cast<u8>(info->osfile & 0xfbu);
    }

    char* src = buf;
    char* dst = buf;
    char* end = buf + total;
    while (src < end)
    {
        const char ch = *src;
        if (ch == '\x1a')
        {
            if ((info->osfile & 0x40u) == 0)
            {
                info->osfile = static_cast<u8>(info->osfile | 2u);
            }
            break;
        }
        if (ch != '\r')
        {
            *dst = ch;
            dst += 1;
            src += 1;
            continue;
        }
        if (src < end - 1)
        {
            if (src[1] == '\n')
            {
                src += 2;
                *dst = '\n';
            }
            else
            {
                src += 1;
                *dst = '\r';
            }
            dst += 1;
            continue;
        }

        src += 1;
        char peek = 0;
        u32 peek_n = 0;
        u32 peek_err = 0;
        const bool peek_ok = crt_read_file(info->osfhnd, &peek, 1u, &peek_n, &peek_err);
        if ((!peek_ok && peek_err != 0) || peek_n == 0)
        {
            *dst = '\r';
            dst += 1;
            continue;
        }
        if ((info->osfile & 0x48u) != 0)
        {
            if (peek == '\n')
            {
                *dst = '\n';
            }
            else
            {
                *dst = '\r';
                info->pipech = static_cast<u8>(static_cast<unsigned char>(peek));
            }
            dst += 1;
            continue;
        }
        if (dst == buf && peek == '\n')
        {
            *dst = '\n';
            dst += 1;
            continue;
        }

        crt_seek_unlocked(fh, -1, 1u);
        if (peek != '\n')
        {
            *dst = '\r';
            dst += 1;
        }
    }
    return static_cast<int>(dst - buf);
}


// Xbox 0x0016D7E1
void crt_unlock_read_descriptor(int fh)
{
    // Xbox 0x0016D7E1
    // Shared unlock tail for _read (0x0016D75A). __SEH_prolog (0x00166D58)
    // is entered with local size 0x0C and the scope table at 0x00209C98:
    // one entry, enclosing level -1, a null filter, and the handler at
    // 0x0016D7DE. That handler loads the descriptor from [ebp+8] into EBX
    // and falls through to this address. The success path at 0x0016D7D4
    // calls here after _read_lk (0x0016D583) with EBX already holding that
    // descriptor. The eight bytes push EBX, call _unlock_fhandle
    // (0x0016DC4E), pop the cdecl argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    crt_unlock_close_descriptor(fh);
}


// Xbox 0x0016DC45
void crt_unlock_file_handle_table()
{
    // Xbox 0x0016DC45
    // Shared unlock tail of _lock_fhandle (0x0016DBAE). __SEH_prolog
    // (0x00166D58) is entered with local size 8 and the scope table at
    // 0x00209E98: one entry, enclosing level -1, a null filter, and the
    // handler at 0x0016DC42. That handler loads the descriptor from
    // [ebp+8] into EDI and falls through to this address. The success
    // path at 0x0016DC13 sets the try level at [ebp-4] to -1 and calls
    // here. EDI already holds the descriptor, and these nine bytes do
    // not read it: push 10 (_LOCKTAB_LOCK), call _unlock (0x0016810F),
    // pop the cdecl argument, and return.
    //
    // _unlock leaves the critical section in _locktable[10] (image
    // 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    // _local_unwind2 (0x00167062) runs the handler when
    // InitializeCriticalSectionAndSpinCount (0x0016D9F8) returns 0.
    // _lock_fhandle then returns 0 and leaves the file-handle section
    // unentered. After this return on the success path, _lock_fhandle
    // enters the section at record+0xC through RtlEnterCriticalSection
    // (thunk 0x00202EA0). A section __mtinitlocks has not created is
    // left alone; retail would fault on the null pointer.
    if (g_crt_locks[10].cs != nullptr)
    {
        g_crt_locks[10].cs->unlock();
    }
}


bool crt_write_file(std::intptr_t handle, const void* buffer, u32 bytes, u32* wrote, u32* doserr)
{
    // WriteFile at 0x0014B490. Nonzero is success. Zero keeps GetLastError
    // (0x0014AFE4). The transferred count is stored either way; _write_lk
    // reads it only after a nonzero return.
#if defined(_WIN32)
    unsigned long n = 0;
    const int ok = WriteFile(reinterpret_cast<void*>(handle), buffer, bytes, &n, nullptr);
    *wrote = static_cast<u32>(n);
    if (ok != 0)
    {
        return true;
    }
    *doserr = static_cast<u32>(GetLastError());
    return false;
#else
    (void)handle;
    (void)buffer;
    (void)doserr;
    *wrote = bytes;
    return true;
#endif
}


// Xbox 0x0016DC70
int crt_write_unlocked(u32 fh, const char* buf, u32 count)
{
    // Xbox 0x0016DC70
    // _write_lk. cdecl. The caller pops the descriptor, the buffer, and the
    // byte count. This body does not lock the descriptor; the locked _write
    // calls here with that lock already held.
    //
    // A count of 0 returns 0. The record is __pioinfo[fd >> 5] at
    // 0x0054DC60, slot (fd & 0x1F), stride 0x28. The OS handle is at +0 and
    // osfile is at +4. A null __pioinfo slot is reported as EBADF; retail
    // would fault on that address.
    //
    // FAPPEND (bit 0x20) calls _lseeki64_lk (0x0016F270) with offset halves
    // 0 and 0 and FILE_END (2). The 64-bit result is ignored, including a
    // failure that has already stored errno. Both halves are zero, so
    // _lseek_lk (0x0016D466) leaves the pointer at the same place and clears
    // FEOFLAG on success.
    //
    // FTEXT (bit 0x80) copies the buffer through a 1028-byte stack slot.
    // Each LF is counted and preceded by CR. The copy stops once the
    // translated count reaches 0x400, so one last LF can land at 0x401.
    // Each chunk is WriteFile (0x0014B490). A zero return stores
    // GetLastError and stops. A short write, or a chunk that consumes the
    // rest of the input, stops as well. A full chunk with bytes remaining
    // translates another chunk.
    //
    // Any other osfile writes the caller's buffer in one WriteFile. Success
    // stores a zero DOS code and the transferred count. Failure stores
    // GetLastError and leaves the count at zero.
    //
    // A nonzero transferred total returns that total minus the LF count.
    // A zero total with DOS code 5 (ERROR_ACCESS_DENIED) stores EBADF (9)
    // through _errno (0x00168AF5) and 5 through __doserrno (0x00168AFE),
    // then returns -1. Any other DOS code goes through __dosmaperr
    // (0x00168B07) and returns -1. A zero total and a zero DOS code returns
    // 0 when FDEV (bit 0x40) is set and the first buffer byte is CTRL-Z
    // (0x1A). Otherwise errno becomes ENOSPC (0x1C), _doserrno becomes 0,
    // and the return is -1.
    if (count == 0)
    {
        return 0;
    }

    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr)
    {
        *crt_errno_pointer() = 9;
        return -1;
    }

    if ((info->osfile & 0x20u) != 0)
    {
        crt_seek_unlocked(fh, 0, 2u);
    }

    int written_total = 0;
    int lfcount = 0;
    u32 doserr = 0;

    if ((info->osfile & 0x80u) == 0)
    {
        u32 wrote = 0;
        if (crt_write_file(info->osfhnd, buf, count, &wrote, &doserr))
        {
            doserr = 0;
            written_total = static_cast<int>(wrote);
        }
    }
    else
    {
        const char* src = buf;
        char lfbuf[1028];
        while (static_cast<u32>(src - buf) < count)
        {
            char* out = lfbuf;
            int filled = 0;
            u32 consumed = static_cast<u32>(src - buf);
            do
            {
                if (consumed >= count)
                {
                    break;
                }
                const char ch = *src;
                src += 1;
                consumed += 1u;
                if (ch == '\n')
                {
                    lfcount += 1;
                    *out = '\r';
                    out += 1;
                    filled += 1;
                }
                *out = ch;
                out += 1;
                filled += 1;
            } while (filled < 0x400);

            const int chunk = static_cast<int>(out - lfbuf);
            u32 wrote = 0;
            if (!crt_write_file(info->osfhnd, lfbuf, static_cast<u32>(chunk), &wrote, &doserr))
            {
                break;
            }
            written_total += static_cast<int>(wrote);
            doserr = 0;
            if (static_cast<int>(wrote) < chunk || static_cast<u32>(src - buf) >= count)
            {
                break;
            }
        }
    }

    if (written_total != 0)
    {
        return written_total - lfcount;
    }
    if (doserr != 0)
    {
        if (doserr == 5u)
        {
            *crt_errno_pointer() = 9;
            *crt_dos_errno_pointer() = 5u;
            return -1;
        }
        crt_dosmaperr(doserr);
        return -1;
    }
    if ((info->osfile & 0x40u) != 0 && *buf == '\x1a')
    {
        return 0;
    }
    *crt_errno_pointer() = 0x1c;
    *crt_dos_errno_pointer() = 0;
    return -1;
}


// Xbox 0x0016DE82
void crt_unlock_write_descriptor(int fh)
{
    // Xbox 0x0016DE82
    // Shared unlock tail for _write (0x0016DDFB). __SEH_prolog (0x00166D58)
    // is entered with local size 0x0C and the scope table at 0x00209EA8:
    // one entry, enclosing level -1, a null filter, and the handler at
    // 0x0016DE7F. That handler loads the descriptor from [ebp+8] into EBX
    // and falls through to this address. After _lock_fhandle (0x0016DBAE)
    // the try level at [ebp-4] is 0. The join at 0x0016DE71, after
    // _write_lk (0x0016DC70) or the second FOPEN failure, sets that level
    // to -1 and calls here. EBX already holds the descriptor. The eight
    // bytes push EBX, call _unlock_fhandle (0x0016DC4E), pop the cdecl
    // argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    crt_unlock_close_descriptor(fh);
}


int crt_initialize_critical_section(std::recursive_mutex* section, u32 spin_count); // Xbox 0x0016D9F8
 // Xbox 0x0016D9F8

// Xbox 0x0016DEA6
int crt_commit_descriptor(u32 fh)
{
    // Xbox 0x0016DEA6
    // _commit. cdecl. __SEH_prolog (0x00166D58) is entered with local size
    // 0x0C and the scope table at 0x00209EB8: one entry, enclosing level -1,
    // a null filter, and the handler at 0x0016DF41. __SEH_epilog (0x00166D93)
    // returns the value left in EAX.
    //
    // A descriptor greater than or equal to _nhandle (0x0054DC40), or one
    // whose osfile byte lacks FOPEN (bit 0), stores EBADF (9) through
    // _errno (0x00168AF5) and returns -1. _doserrno is left alone. The
    // record is __pioinfo[fd >> 5] at 0x0054DC60, slot (fd & 0x1F), stride
    // 0x28, osfile at +4. A null __pioinfo slot is the same EBADF return;
    // retail would fault on that address.
    //
    // Otherwise _lock_fhandle (0x0016DBAE) is called and its return is
    // discarded. That callee creates the critical section at record+0xC
    // when the dword at +8 is still zero: it takes _LOCKTAB_LOCK (10),
    // calls InitializeCriticalSectionAndSpinCount (0x0016D9F8) with spin
    // 0xFA0, increments the dword, and _unlocks lock 10. A failed init
    // unwinds lock 10 and returns 0 without entering the section. A live
    // section is entered with RtlEnterCriticalSection (thunk 0x00202EA0).
    // The try level at [ebp-4] is then 0.
    //
    // FOPEN is tested again. If it is clear, errno becomes 9 and the result
    // at [ebp-0x1c] becomes -1. If it is set, __get_osfhandle (0x0016DB6D)
    // supplies the OS handle to FlushFileBuffers (0x0014B884, stdcall ret 4).
    // A nonzero return stores 0. A zero return stores GetLastError
    // (0x0014AFE4). A zero code leaves the result 0 and does not touch
    // errno. Any other code is stored through __doserrno (0x00168AFE), errno
    // becomes 9, and the result becomes -1. __dosmaperr is not used.
    //
    // The success path stores try level -1 and calls 0x0016DF44. The unwind
    // handler at 0x0016DF41 reloads the descriptor from [ebp+8] into EBX and
    // falls through to that address. Both push the descriptor and call
    // _unlock_fhandle (0x0016DC4E). A section this call did not create is
    // left alone.
    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr || (info->osfile & 1u) == 0)
    {
        *crt_errno_pointer() = 9;
        return -1;
    }

    if (info->field8 == 0)
    {
        crt_lock(10);
        if (info->field8 == 0)
        {
            bool created = false;
            try
            {
                info->cs_owner = std::make_unique<std::recursive_mutex>();
                created = crt_initialize_critical_section(info->cs_owner.get(), 0xFA0) != 0;
            }
            catch (...)
            {
                created = false;
            }
            if (!created)
            {
                info->cs_owner.reset();
                info->cs = nullptr;
                crt_unlock_lock_table();
            }
            else
            {
                info->cs = info->cs_owner.get();
                info->field8 += 1u;
                crt_unlock_lock_table();
                info->cs->lock();
            }
        }
        else if (info->cs != nullptr)
        {
            crt_unlock_lock_table();
            info->cs->lock();
        }
        else
        {
            crt_unlock_lock_table();
        }
    }
    else if (info->cs != nullptr)
    {
        info->cs->lock();
    }

    int result = -1;
    if ((info->osfile & 1u) != 0)
    {
        const std::intptr_t handle = crt_get_osfhandle(fh);
        u32 dos = 0;
#if defined(_WIN32)
        if (FlushFileBuffers(reinterpret_cast<void*>(handle)) == 0)
        {
            dos = static_cast<u32>(GetLastError());
        }
#else
        (void)handle;
#endif
        if (dos == 0)
        {
            result = 0;
        }
        else
        {
            *crt_dos_errno_pointer() = dos;
            *crt_errno_pointer() = 9;
            result = -1;
        }
    }
    else
    {
        *crt_errno_pointer() = 9;
        result = -1;
    }

    crt_unlock_close_descriptor(static_cast<int>(fh));
    return result;
}


// Xbox 0x0016DFB5
u32 crt_put_wide_character_unlocked(u32 unit, void* stream)
{
    // Xbox 0x0016DFB5
    // _fputwc_nolock. cdecl. The only caller is write_char in _woutput
    // (0x001698B6): it pushes the FILE and the unit, then compares AX with
    // 0xFFFF. A match stores -1 in the character count. Anything else
    // increments that count. A string whose base is null never reaches here.
    //
    // The host FILE matches _woutput: ptr, cnt, base, flag, then the
    // descriptor. The low flag byte is tested for _IOSTRG (0x40). A string
    // stream skips the text check. Any other stream reads osfile. A
    // descriptor of -1 uses the dummy ioinfo at 0x00338250, whose image
    // bytes are osfhnd -1, osfile 0x80, and pipech 10. Any other descriptor
    // is __pioinfo[fd >> 5] at 0x0054DC60, slot (fd & 0x1F), stride 0x28,
    // osfile at +4. Bit 0x80 is FTEXT. A null __pioinfo slot is left with a
    // clear osfile; retail would fault on that address.
    //
    // FTEXT calls wctomb (0x0016DF8A) with an 8-byte buffer. That callee
    // reads the low 16 bits. A unit above 0xFF stores EILSEQ (0x2A) through
    // _errno (0x00168AF5) and returns -1; this function stores 0x2A again
    // and returns WEOF. A unit that fits is one byte. cnt drops by 1. A
    // negative cnt is _flsbuf (0x00168E46): a string stream, or a stream
    // that is not open for write, sets _IOERR (flag 0x20) and returns EOF.
    // The low half of that EOF is WEOF, which is what write_char tests.
    // The text path then returns the unit masked to 16 bits.
    //
    // A string stream, or a stream whose osfile lacks FTEXT, drops cnt by 2
    // and stores the low 16 bits at ptr. A negative cnt is _flswbuf
    // (0x0016F2F1), which sets _IOERR and returns WEOF for a string stream
    // and for a stream that is not open for write. The port has no file
    // flush, so a full buffer takes that same WEOF. A store that fits
    // returns the original unit.

    struct StreamFile
    {
        char* ptr;
        int cnt;
        char* base;
        int flag;
        int file;
        int charbuf;
        int bufsiz;
        char* tmpfname;
    };

    auto* file = static_cast<StreamFile*>(stream);
    if ((static_cast<unsigned char>(file->flag) & 0x40) == 0)
    {
        u8 osfile = 0;
        if (file->file == -1)
        {
            osfile = 0x80;
        }
        else
        {
            CrtIoInfo* info = crt_ioinfo_checked(static_cast<u32>(file->file));
            if (info != nullptr)
            {
                osfile = info->osfile;
            }
        }
        if ((osfile & 0x80) != 0)
        {
            const u16 wc = static_cast<u16>(unit);
            if (wc > 0xffu)
            {
                *crt_errno_pointer() = 0x2a;
                return 0xffffu;
            }

            file->cnt -= 1;
            if (file->cnt < 0)
            {
                file->flag |= 0x20;
                return 0xffffu;
            }
            *file->ptr = static_cast<char>(wc);
            file->ptr += 1;
            return unit & 0xffffu;
        }
    }

    file->cnt -= 2;
    if (file->cnt < 0)
    {
        file->flag |= 0x20;
        return 0xffffu;
    }
    const auto wide = static_cast<std::uint16_t>(unit);
    std::memcpy(file->ptr, &wide, sizeof(wide));
    file->ptr += 2;
    return unit;
}


// Xbox 0x0016E07A
int crt_byte_to_wide_character(char16_t* dest, const char* src, int count)
{
    // Xbox 0x0016E07A
    // mbtowc. cdecl; the caller pops the three arguments. This body is the
    // single-byte form. A null source or a zero count returns 0 and leaves
    // the destination alone. A zero byte stores 0 when the destination is
    // non-null and returns 0. Any other byte is zero-extended into that
    // word and returns 1. A null destination skips the store and still
    // returns 1 for a nonzero byte. The count is tested only against zero,
    // so a lead-byte caller that passes 2 still converts one byte. The
    // return is 0 or 1.
    //
    // _woutput (0x0016993D) passes __mb_cur_max. 0x00169C84 widens the %hc
    // byte, and 0x0016A068 walks narrow text one byte at a time. _winput
    // (0x0016A17C) at 0x0016A4E1 widens the decimal byte at 0x0033818C.
    if (src != nullptr && count != 0)
    {
        const unsigned char byte = static_cast<unsigned char>(*src);
        if (byte != 0)
        {
            if (dest != nullptr)
            {
                *dest = static_cast<char16_t>(byte);
            }
            return 1;
        }
        if (dest != nullptr)
        {
            *dest = 0;
        }
    }
    return 0;
}


// Xbox 0x00168E08
void crt_unlock_flush_stream(int index)
{
    // Xbox 0x00168E08
    // Shared unlock body of flsall (0x00168D68). The scope table at
    // 0x00209268 has two entries. The inner one (enclosing level 0, a null
    // filter) is the handler at 0x00168E03: it clears EDI and reloads the
    // stream index from [ebp-0x20] into ESI, then falls through to this
    // address. The success path at 0x00168DFB calls here after the try
    // level at [ebp-4] is stored as 0, with ESI already holding that index.
    // The seventeen bytes load __piob (0x0054DD74), push __piob[index] and
    // the index, call _unlock_file2 (0x001689F1), pop both cdecl arguments,
    // and return.
    //
    // _unlock_file2 compares the index with 0x14 (_IOB_ENTRIES). The
    // compare is signed jge. Below that it adds 0x10 (_STREAM_LOCKS) and
    // calls _unlock (0x0016810F), which leaves the critical section in
    // _locktable (image 0x00337AB0). The kernel thunk is the dword at
    // 0x00202EA4. An index of 0x14 or more calls RtlLeaveCriticalSection
    // on the critical section at FILE+0x20. A null __piob, an index
    // outside the allocated table, or a null FILE on the FILEX path would
    // fault; the port returns. A stream lock __mtinit has not created is
    // left alone.
    if (g_crt_piob == nullptr || index < 0 || static_cast<u32>(index) >= g_crt_nstream)
    {
        return;
    }

    CrtFile* stream = g_crt_piob[index];
    if (index < 0x14)
    {
        const u32 locknum = static_cast<u32>(index) + 0x10u;
        if (g_crt_locks[locknum].cs != nullptr)
        {
            g_crt_locks[locknum].cs->unlock();
        }
        return;
    }

    if (stream == nullptr)
    {
        return;
    }
    reinterpret_cast<CrtFileX*>(stream)->lock.unlock();
}


// Xbox 0x00168E34
void crt_unlock_flush_table()
{
    // Xbox 0x00168E34
    // Outer __finally of flsall (0x00168D68). The scope table at
    // 0x00209268 has two entries. This one (enclosing level -1, a null
    // filter) is the handler. The inner entry (enclosing level 0, a null
    // filter) is the handler at 0x00168E03, which falls into crt_unlock_flush_stream.
    // flsall pushes 1 (_IOB_SCAN_LOCK) and calls _lock (0x001681C1)
    // before the __piob scan, stores try level 0 at [ebp-4], and on the
    // success path at 0x00168E19 sets that level to -1 and calls here.
    // _local_unwind2 (0x00167062) runs the same handler. The nine bytes
    // push 1, call _unlock (0x0016810F), pop the cdecl argument, and
    // return. _unlock leaves the critical section in _locktable[1]
    // (image 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    // A section __mtinitlocks has not created is left alone; retail
    // would fault on the null pointer.
    if (g_crt_locks[1].cs != nullptr)
    {
        g_crt_locks[1].cs->unlock();
    }
}


int crt_mtinitlocknum(int locknum)
{
    // __mtinitlocknum at 0x00168121. Scope table 0x00209190, prolog
    // 0x00166D58. A live pointer returns 1. Otherwise 0x00167F1C allocates
    // 0x1C bytes. A null block stores ENOMEM (0xC) through 0x00168AF5
    // (per-thread data + 8) and returns 0. The host mutex stands in for
    // that block; a failed allocation is the same zero return.
    //
    // The try level at [ebp-4] is 0 while _lock holds _LOCKTAB_LOCK (10).
    // Kind 1 at that index is created by __mtinitlocks. When this index is
    // 10 and the pointer is still null, the section is stored and the
    // caller enters it. Taking _lock(10) from inside that init would call
    // back here before the pointer is stored. A section that appeared while
    // the table lock was held is freed (0x00167F2E) and the new one is kept.
    //
    // 0x0016D9F8 initializes the section with spin count 0xFA0. A zero
    // return frees the block, stores ENOMEM, and _local_unwind2 (0x00167062)
    // runs the finally at 0x001681B8, which is _unlock (0x0016810F) of lock
    // 10. The host constructor runs before that call. The finally still
    // leaves lock 10. Index 10 is stored without taking the table lock, so
    // a failed init on that index does not unlock. Success returns 1.
    if (g_crt_locks[locknum].cs != nullptr)
    {
        return 1;
    }

    std::unique_ptr<std::recursive_mutex> pcs;
    try
    {
        pcs = std::make_unique<std::recursive_mutex>();
    }
    catch (...)
    {
        *crt_errno_pointer() = 12;
        return 0;
    }

    if (locknum == 10)
    {
        if (crt_initialize_critical_section(pcs.get(), 0xFA0) == 0)
        {
            *crt_errno_pointer() = 12;
            return 0;
        }
        g_crt_lock_mu[10] = std::move(pcs);
        g_crt_locks[10].cs = g_crt_lock_mu[10].get();
        return 1;
    }

    crt_lock(10);
    if (g_crt_locks[locknum].cs == nullptr)
    {
        if (crt_initialize_critical_section(pcs.get(), 0xFA0) == 0)
        {
            pcs.reset();
            *crt_errno_pointer() = 12;
            crt_unlock_lock_table();
            return 0;
        }
        g_crt_lock_mu[locknum] = std::move(pcs);
        g_crt_locks[locknum].cs = g_crt_lock_mu[locknum].get();
    }
    else
    {
        pcs.reset();
    }
    crt_unlock_lock_table();
    return 1;
}


// Xbox 0x00167BDE
void crt_unwind_array_construction(void* ptr, u32 element_size, int constructed, void (*dtor)(void*), int success)
{
    // Xbox 0x00167BDE
    // Finally of __ehvec_ctor (0x00167B94). The scope table at 0x00209140
    // has one entry: enclosing level -1, a null filter, and this handler.
    // __SEH_prolog (0x00166D58) is entered with local size 0x10. This body
    // builds no frame, so EBP stays the constructor iterator's.
    // [ebp-0x20] is the success flag, [ebp-0x1c] is how many constructors
    // have returned, [ebp+8] is the element pointer, [ebp+0xc] is the
    // stride, and [ebp+0x18] is the destructor. The iterator stores 0 in
    // the flag and the count, sets the try level at [ebp-4] to 0, and
    // constructs [ebp+0x10] elements through the thiscall at [ebp+0x14].
    // After a constructor returns, the pointer advances by the stride and
    // the count increments. A throw leaves both as they were: the pointer
    // addresses the element that failed, and the count is the number
    // already built. The success path at 0x00167BC6 stores 1, sets the try
    // level to -1, and calls here. The compare is jne, so a nonzero flag
    // returns. A zero flag pushes the destructor, the count, the stride,
    // and the pointer, then calls __ArrayUnwind (0x00167BF6, stdcall,
    // ret 0x10). That callee decrements the count and stops once the
    // result is negative, subtracts the stride, and calls the destructor
    // with ECX on that element. A C++ exception (0xE06D7363) from a
    // destructor takes the filter at 0x00167C25 into terminate
    // (0x0016C177). Any other exception code returns 0 from the filter.
    if (success != 0)
    {
        return;
    }

    auto* cursor = static_cast<char*>(ptr);
    while (constructed > 0)
    {
        --constructed;
        cursor -= element_size;
        try
        {
            dtor(cursor);
        }
        catch (...)
        {
            crt_terminate();
        }
    }
}


// Xbox 0x00167C9C
void crt_unwind_array_destruction(void* ptr, u32 element_size, int count, void (*dtor)(void*), int success)
{
    // Xbox 0x00167C9C
    // Finally of __ehvec_dtor (0x00167C54). The scope table at 0x00209160
    // has one entry: enclosing level -1, a null filter, and this handler.
    // __SEH_prolog (0x00166D58) is entered with local size 0x0C. This body
    // builds no frame, so EBP stays the destructor iterator's.
    // [ebp-0x1c] is the success flag, [ebp+8] is the element pointer,
    // [ebp+0xc] is the stride, [ebp+0x10] is the count, and [ebp+0x14] is
    // the destructor. The iterator stores 0 in the flag, multiplies the
    // stride by the count, and adds that product to the pointer so it sits
    // just past the last element. It sets the try level at [ebp-4] to 0,
    // then decrements the count. A negative result leaves the loop.
    // Otherwise it subtracts the stride and calls the destructor with ECX
    // on that element. A throw leaves the pointer on the element that
    // failed and the count already decremented for it. The success path at
    // 0x00167C84 stores 1, sets the try level to -1, and calls here. The
    // compare is jne, so a nonzero flag returns. A zero flag pushes the
    // destructor, the count, the stride, and the pointer, then calls
    // __ArrayUnwind (0x00167BF6, stdcall, ret 0x10). That callee decrements
    // the count and stops once the result is negative, subtracts the stride,
    // and calls the destructor with ECX on that element. A C++ exception
    // (0xE06D7363) from a destructor takes the filter at 0x00167C25 into
    // terminate (0x0016C177). Any other exception code returns 0 from the
    // filter.
    if (success != 0)
    {
        return;
    }

    auto* cursor = static_cast<char*>(ptr);
    while (count > 0)
    {
        --count;
        cursor -= element_size;
        try
        {
            dtor(cursor);
        }
        catch (...)
        {
            crt_terminate();
        }
    }
}


// Xbox 0x00167EC9
void* crt_heap_allocate(u32 size)
{
    // Xbox 0x00167EC9
    // _heap_alloc. The caller is _nh_malloc_base (0x00167EF0), which malloc
    // (0x00167F1C) calls with _newmode. A zero request becomes one byte.
    // __active_heap (0x0054DD60) is __SYSTEM_HEAP when it equals 1. Any other
    // value, including the BSS zero, rounds the count up to a multiple of
    // 16. The two pushes are the HeapAlloc size and flags. 0x0014D377 is the
    // six-byte load of _crtheap (0x00549520) and does not consume them.
    // 0x0014E872 is HeapAlloc(heap, 0, size) and returns the block in EAX
    // after its stdcall cleanup. The host allocator matches free (0x00167F2E).
    if (size == 0)
    {
        size = 1;
    }
    if (g_crt_active_heap != 1)
    {
        size = (size + 0xfu) & 0xfffffff0u;
    }
    return std::malloc(size);
}


// Xbox 0x00167F2E
void crt_heap_free(void* block)
{
    // Xbox 0x00167F2E
    // free. The printf helpers at 0x00168FED and 0x0016993D release a
    // temporary buffer here, and _fcloseall (0x0016DAB4) releases FILE
    // records past the first twenty. A null block returns before the heap
    // check. __SEH_prolog (0x00166D58) builds the frame from the scope table
    // at 0x00209170; __SEH_epilog (0x00166D93) tears it down.
    //
    // When __active_heap is __V6_HEAP (3), _lock (0x001681C1) takes
    // _HEAP_LOCK (4). A missing critical section is the __mtinitlocknum path
    // inside _lock; kind 1 matches the descriptor table. The __try stores 0
    // and returns the EH state to -1. The __finally at 0x00167F7B clears ESI
    // on the unwind entry, and both that entry and the direct call at
    // 0x00167F7D fall into _unlock (0x0016810F). The stored 0 fails the
    // compare, so the early-out is not taken.
    //
    // That fall-through and every other heap kind push the block and flags
    // 0, call 0x0014D377 (EAX becomes _crtheap at 0x00549520), push the
    // handle, and call the HeapFree wrapper at 0x0014D360. The host releases
    // the block with the process heap, as 0x000663E0 does.
    if (block == nullptr)
    {
        return;
    }

    if (g_crt_active_heap == 3)
    {
        crt_lock(4);
        g_crt_locks[4].cs->unlock();
    }

    std::free(block);
}


// Xbox 0x0016AFDD
int crt_initialize_thread_runtime()
{
    // Xbox 0x0016AFDD
    // _mtinit. Pointer at 0x0032C804, in the __xi_a..__xi_z span
    // (0x0032C800..0x0032C80C). Startup 0x0014FAC1 calls 0x0015172C, which
    // walks that span, then _cinit at 0x001516D4, then WinMain. The walker
    // ignores the return. This function returns 0 on every path that returns.
    //
    // __mtinitlocks (0x0016809D) walks the 36 descriptors. Kind 1 stores a
    // critical section from the pool at 0x004DD0D8 (0x1C bytes each) and
    // calls 0x0016D9F8 with spin count 0xFA0. Any other kind is left alone.
    // A failed init clears that descriptor's pointer and returns 0. Success
    // returns 1. The port uses a host recursive mutex in place of the Xbox
    // RTL_CRITICAL_SECTION. The spin count is passed through; the default
    // target ignores it. A zero return from that call clears the pointer.
    //
    // Failure calls 0x001680E6, which walks 0x00337AB0 up to 0x00337BD0 and
    // deletes a critical section only when the pointer is set and the kind
    // is not 1. Kind-1 sections created before the failure stay in place.
    //
    // Success calloc's 0x88 bytes (0x00168012) and stores the pointer at
    // fs:[4][__tls_index] + 0xC. fs:[4] is the thread TLS array. __tls_index
    // is the dword at 0x00434234, zero in the image until the loader writes
    // it. A null block writes 0x00338280 through address 0x54 and 1 through
    // address 0x14 (_pxcptacttab and _holdrand with a null base), then faults
    // on the thread-handle store. The port does not write low memory and
    // returns 0 instead of faulting.
    //
    // A live block gets GetCurrentThreadId (0x0014BC53, fs:[0x28] + 0x12C)
    // in the first dword and 0xFFFFFFFF in the second. 0x004DD290 becomes
    // 0x0016EEC4, and XRegisterThreadNotifyRoutine (0x0014BAC8) inserts
    // 0x004DD288 at the tail of the list headed by 0x0032C9E0 while holding
    // the critical section at 0x0032C9C4. 0x0016EEC4 is the notify routine:
    // a nonzero argument calls [0x00337A8C] when that pointer is set, and a
    // zero argument calls [0x00337A90] when set, then _freeptd(0) at
    // 0x0016AF8B. Both image pointers are 0x00167388, which is a ret.
    for (u32 i = 0; i < 0x24; ++i)
    {
        g_crt_locks[i].kind = kCrtLockKind[i];
    }

    int locks_ok = 1;
    for (u32 i = 0; i < 0x24; ++i)
    {
        if (g_crt_locks[i].kind != 1)
        {
            continue;
        }
        try
        {
            g_crt_lock_mu[i] = std::make_unique<std::recursive_mutex>();
            g_crt_locks[i].cs = g_crt_lock_mu[i].get();
        }
        catch (...)
        {
            g_crt_locks[i].cs = nullptr;
            locks_ok = 0;
            break;
        }
        if (crt_initialize_critical_section(g_crt_locks[i].cs, 0xFA0) == 0)
        {
            g_crt_lock_mu[i].reset();
            g_crt_locks[i].cs = nullptr;
            locks_ok = 0;
            break;
        }
    }

    if (locks_ok == 0)
    {
        for (CrtLockSlot& slot : g_crt_locks)
        {
            if (slot.cs != nullptr && slot.kind != 1)
            {
                slot.cs = nullptr;
            }
        }
        return 0;
    }

    CrtTidData* ptd = static_cast<CrtTidData*>(std::calloc(1, sizeof(CrtTidData)));
    if (g_crt_tls_index == 0)
    {
        g_crt_module_tls.ptd = ptd;
    }
    if (ptd == nullptr)
    {
        return 0;
    }

#if defined(_WIN32)
    const u32 tid = static_cast<u32>(GetCurrentThreadId());
#else
    const u32 tid = 1;
#endif
    ptd->thandle |= 0xffffffffu;
    ptd->tid = tid;

    g_crt_thread_notify.routine = 0x0016EEC4u;
    {
        std::lock_guard<std::recursive_mutex> hold(g_xthread_notify_cs);
        if (g_xthread_notify_head.flink == nullptr)
        {
            g_xthread_notify_head.flink = &g_xthread_notify_head;
            g_xthread_notify_head.blink = &g_xthread_notify_head;
        }
        XThreadNotify* head = &g_xthread_notify_head;
        XThreadNotify* prev = head->blink;
        g_crt_thread_notify.flink = head;
        g_crt_thread_notify.blink = prev;
        prev->flink = &g_crt_thread_notify;
        head->blink = &g_crt_thread_notify;
    }
    return 0;
}


// Win32 code stored by 0x0014B00C. 0x0014B03A maps the NTSTATUS, and
// 0x0014AFE4 reads this slot. XCreateSaveGame (0x0014C70F) treats 0xB7 as
// "the directory is already there".
u32 g_xapi_last_error = 0;


// Xbox 0x0016D9E8
int crt_initialize_critical_section_spin(std::recursive_mutex* section, u32 spin_count)
{
    // Xbox 0x0016D9E8
    // stdcall, ret 8. Pushes the critical section and calls
    // RtlInitializeCriticalSection (kernel ordinal 291, thunk 0x00202FC0).
    // The spin count is left for that epilogue. EAX is 1 when the call
    // returns. The host mutex is already constructed. A null section is the
    // access fault that kernel call would raise. That status is not
    // 0xC0000017, so the caller returns 0 and leaves the last-error slot.
    (void)spin_count;
    if (section == nullptr)
    {
#if defined(_WIN32)
        RaiseException(0xC0000005u, 0, 0, nullptr);
#else
        throw static_cast<unsigned long>(0xC0000005u);
#endif
    }
    return 1;
}


// Xbox 0x0016D9F8
int crt_initialize_critical_section(std::recursive_mutex* section, u32 spin_count)
{
    // Xbox 0x0016D9F8
    // cdecl. The callers pop both arguments. __SEH_prolog (0x00166D58) is
    // entered with local size 0x10 and the scope table at 0x00209E78:
    // enclosing level -1, filter 0x0016DA2C, handler 0x0016DA3A.
    //
    // 0x004DD2D4 starts at 0. A null pointer is replaced with 0x0016D9E8.
    // The try level at [ebp-4] is then 0, and both arguments are passed to
    // that target. Its EAX is the success return. The filter returns 1, so
    // every exception runs the handler. The handler reloads ESP from
    // [ebp-0x18]. Exception code 0xC0000017 (STATUS_NO_MEMORY) stores 8
    // through 0x0014B00C. Every exception returns 0. __SEH_epilog
    // (0x00166D93) leaves EAX unchanged.
    if (g_crt_crit_init == nullptr)
    {
        g_crt_crit_init = crt_initialize_critical_section_spin;
    }

#if defined(_WIN32)
    __try
    {
        return g_crt_crit_init(section, spin_count);
    }
    __except (1)
    {
        if (GetExceptionCode() == 0xC0000017u)
        {
            g_xapi_last_error = 8;
        }
        return 0;
    }
#else
    try
    {
        return g_crt_crit_init(section, spin_count);
    }
    catch (unsigned long code)
    {
        if (code == 0xC0000017u)
        {
            g_xapi_last_error = 8;
        }
        return 0;
    }
    catch (...)
    {
        return 0;
    }
#endif
}


// Xbox 0x0016E770
void crt_parse_double(double* result, const char* text)
{
    // Xbox 0x0016E770
    // _atodbl. cdecl. __fassign (0x0016B1FE) calls this when its size flag
    // is nonzero, then copies the eight bytes into the caller's pointer.
    // The frame holds a 12-byte _LDBL12 and an end pointer. __strgtold12
    // (0x0016E7CA) is called with mult12, scale, decpt, and implicit_E all
    // zero. _ld12tod (0x0016E744) pushes the double descriptor at
    // 0x00338308 and _ld12cvt (0x0016E5EC) writes the IEEE double. Both
    // returns are discarded; __fassign overwrites EAX.
    //
    // Whitespace is 0x20, 0x09, 0x0A, and 0x0D. An optional sign comes
    // next, then digits, one decimal point (the byte at 0x0033818C, '.'),
    // and an optional e/E/d/D exponent. No significant digits store a
    // signed zero, so "-" and "-0" are negative zero. At most 25 digits
    // are kept. A 25th digit drops back to 24, and when digit 23 is at
    // least 5 that digit is bumped. The parsed exponent clamps at 5201.
    // Above 5200 the double is a signed infinity; below -5200 it is a
    // signed zero. __mb_cur_max above 1 classifies digits with _isctype
    // (0x0016AE1F) and mask 4; otherwise the low byte of _pctype.

    auto digit_byte = [](unsigned char c) -> bool
    {
        if (g_crt_mb_cur_max > 1)
        {
            return crt_character_type(static_cast<int>(c), 4u) != 0;
        }
        return (static_cast<unsigned>(static_cast<u8>(g_crt_pctype[c])) & 4u) != 0;
    };

    auto store = [&](double value)
    {
        std::memcpy(result, &value, sizeof(value));
    };

    const char* p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    {
        ++p;
    }

    bool negative = false;
    if (*p == '+' || *p == '-')
    {
        negative = *p == '-';
        ++p;
        const unsigned char next = static_cast<unsigned char>(*p);
        if (next != '.' && !digit_byte(next))
        {
            store(negative ? -0.0 : 0.0);
            return;
        }
    }

    unsigned char digits[25]{};
    int ndigits = 0;
    int exp_adj = 0;
    bool saw = false;

    auto bump = [&](int delta)
    {
        if (delta > 0)
        {
            if (exp_adj < 20000)
            {
                exp_adj += 1;
            }
        }
        else if (exp_adj > -20000)
        {
            exp_adj -= 1;
        }
    };

    while (digit_byte(static_cast<unsigned char>(*p)) && *p == '0')
    {
        saw = true;
        ++p;
    }
    while (digit_byte(static_cast<unsigned char>(*p)))
    {
        saw = true;
        if (ndigits < 25)
        {
            digits[ndigits] = static_cast<unsigned char>(*p - '0');
            ndigits += 1;
        }
        else
        {
            bump(1);
        }
        ++p;
    }

    if (*p == '.')
    {
        ++p;
        if (!saw && !digit_byte(static_cast<unsigned char>(*p)))
        {
            store(negative ? -0.0 : 0.0);
            return;
        }
        if (ndigits == 0)
        {
            while (*p == '0')
            {
                saw = true;
                bump(-1);
                ++p;
            }
        }
        while (digit_byte(static_cast<unsigned char>(*p)))
        {
            saw = true;
            if (ndigits < 25)
            {
                digits[ndigits] = static_cast<unsigned char>(*p - '0');
                ndigits += 1;
                bump(-1);
            }
            ++p;
        }
    }

    int exp_mag = 0;
    bool exp_neg = false;
    bool saw_exp = false;
    const unsigned char exp_ch = static_cast<unsigned char>(*p);
    if (saw && (exp_ch == 'e' || exp_ch == 'E' || exp_ch == 'd' || exp_ch == 'D'))
    {
        const char* exp_mark = p;
        ++p;
        if (*p == '+' || *p == '-')
        {
            exp_neg = *p == '-';
            ++p;
        }
        if (!digit_byte(static_cast<unsigned char>(*p)))
        {
            p = exp_mark;
        }
        else
        {
            saw_exp = true;
            while (*p == '0')
            {
                ++p;
            }
            while (digit_byte(static_cast<unsigned char>(*p)))
            {
                if (exp_mag <= 5200)
                {
                    exp_mag = exp_mag * 10 + (*p - '0');
                    if (exp_mag > 5200)
                    {
                        exp_mag = 5201;
                    }
                }
                ++p;
            }
        }
    }

    if (!saw || ndigits == 0)
    {
        store(negative ? -0.0 : 0.0);
        return;
    }

    if (ndigits > 24)
    {
        if (digits[23] >= 5)
        {
            digits[23] = static_cast<unsigned char>(digits[23] + 1);
        }
        ndigits = 24;
        bump(1);
    }
    while (ndigits > 0 && digits[ndigits - 1] == 0)
    {
        ndigits -= 1;
        bump(1);
    }
    if (ndigits == 0)
    {
        store(negative ? -0.0 : 0.0);
        return;
    }

    int carry = 0;
    for (int i = ndigits - 1; i >= 0; --i)
    {
        const int sum = digits[i] + carry;
        digits[i] = static_cast<unsigned char>(sum % 10);
        carry = sum / 10;
    }
    if (carry != 0)
    {
        exp_adj += ndigits;
        digits[0] = 1;
        ndigits = 1;
    }

    int exp10 = exp_adj;
    if (saw_exp)
    {
        exp10 += exp_neg ? -exp_mag : exp_mag;
    }

    double value = 0.0;
    if (exp10 > 5200)
    {
        value = std::bit_cast<double>(0x7ff0000000000000ull);
    }
    else if (exp10 < -5200)
    {
        value = 0.0;
    }
    else
    {
        char buf[64];
        int n = 0;
        for (int i = 0; i < ndigits; ++i)
        {
            buf[n] = static_cast<char>('0' + digits[i]);
            n += 1;
        }
        if (exp10 != 0)
        {
            buf[n] = 'e';
            n += 1;
            int power = exp10;
            if (power < 0)
            {
                buf[n] = '-';
                n += 1;
                power = -power;
            }
            char rev[8];
            int rn = 0;
            do
            {
                rev[rn] = static_cast<char>('0' + power % 10);
                rn += 1;
                power /= 10;
            } while (power != 0);
            while (rn != 0)
            {
                rn -= 1;
                buf[n] = rev[rn];
                n += 1;
            }
        }
        buf[n] = '\0';
        const int saved_errno = errno;
        value = std::strtod(buf, nullptr);
        errno = saved_errno;
    }
    if (negative)
    {
        value = -value;
    }
    store(value);
}


// Xbox 0x0016E79D
void crt_parse_float(float* result, const char* text)
{
    // Xbox 0x0016E79D
    // _atoflt. cdecl, same 45-byte frame as _atodbl (0x0016E770): a 12-byte
    // _LDBL12 and an end pointer. __strgtold12 (0x0016E7CA) is called with
    // mult12, scale, decpt, and implicit_E all zero. _ld12tof (0x0016E75A)
    // pushes the float descriptor at 0x00338320 and _ld12cvt (0x0016E5EC)
    // writes an IEEE float. Both returns are discarded.
    //
    // That descriptor is max exponent 128, min exponent -127, 24 mantissa
    // bits, pack shift 8, width 32, bias 127. An unbiased exponent above
    // 127 is a signed infinity. Below -151 the result is a signed zero.
    // Exponents from -151 through -127 are subnormals. The sign lives in
    // bit 15 of the word at offset 0xA of the _LDBL12.
    //
    // __fassign (0x0016B1FE) calls this when the size flag is zero. The
    // string is already on the stack; the result pointer is the flag slot,
    // and those four bytes are copied to the caller's pointer. A nonzero
    // flag calls _atodbl and copies eight bytes.
    //
    // Digit scanning, the 25-digit cutoff, and the 5200 exponent clamp are
    // __strgtold12, shared with _atodbl. The host narrows that double to
    // the 24-bit mantissa _ld12tof stores.
    double parsed = 0.0;
    crt_parse_double(&parsed, text);
    const float value = static_cast<float>(parsed);
    std::memcpy(result, &value, sizeof(value));
}


// 0x00338278. The image dword is 0x2694. Nothing stores this address.
// _87except skips _matherr while the dword is nonzero. The pow entries at
// 0x00170460 and 0x0017049E take the same test.
u32 g_crt_fp_skip_matherr = 0x2694u;


// 0x004DD0D0. BSS, image value 0. Nothing in the image writes this address.
// The math epilogues compare it and, when it is nonzero, restore the saved
// x87 control word without building an exception record. Sites: 0x00167548,
// 0x001675AC, 0x00167D37, 0x00167D54, 0x0017052A, 0x00170726, 0x0017078C.
u32 g_crt_fp_skip_epilogue = 0;


struct CrtMathError
{
    int type;
    char* name;
    double arg1;
    double arg2;
    double retval;
};


// EDX descriptor saved at [ebp-0x94]. Byte 0 is the name length. The C string
// starts at +1 and is padded to 12 bytes. +0x0D is 1 for a unary operation
// (sinh, cosh, tanh, exp). +0x0E is the opcode passed to _87except.
struct CrtMathDesc
{
    u8 name_len;
    char name[12];
    u8 unary;
    u8 opcode;
};


static_assert(offsetof(CrtMathDesc, unary) == 0x0D);

static_assert(offsetof(CrtMathDesc, opcode) == 0x0E);


// Caller's frame. 0x00170523 and the fall-through at 0x0017052A never build
// their own EBP; both are called with the math wrapper's frame still in EBP
// and the result in ST0.
struct CrtMathCall
{
    u8 flags;
    u16 cw;
    u8 status;
    CrtMathDesc* desc;
    double arg1;
    double arg2;
    double result;
};


void crt_restore_x87_cw(u32 raw)
{
    // __ctrlfp (0x00168809) with mask 0xFFFF. fnstcw / fldcw on the raw
    // control word. The host control word uses the _controlfp layout.
#if defined(_WIN32)
    u32 value = 0;
    if ((raw & 0x01u) != 0)
    {
        value |= _EM_INVALID;
    }
    if ((raw & 0x02u) != 0)
    {
        value |= _EM_DENORMAL;
    }
    if ((raw & 0x04u) != 0)
    {
        value |= _EM_ZERODIVIDE;
    }
    if ((raw & 0x08u) != 0)
    {
        value |= _EM_OVERFLOW;
    }
    if ((raw & 0x10u) != 0)
    {
        value |= _EM_UNDERFLOW;
    }
    if ((raw & 0x20u) != 0)
    {
        value |= _EM_INEXACT;
    }
    switch (raw & 0xC00u)
    {
    case 0x400u:
        value |= _RC_DOWN;
        break;
    case 0x800u:
        value |= _RC_UP;
        break;
    case 0xC00u:
        value |= _RC_CHOP;
        break;
    default:
        value |= _RC_NEAR;
        break;
    }
    switch (raw & 0x300u)
    {
    case 0x000u:
        value |= _PC_24;
        break;
    case 0x200u:
        value |= _PC_53;
        break;
    default:
        value |= _PC_64;
        break;
    }
    unsigned int previous = 0;
    (void)_controlfp_s(&previous, value, _MCW_EM | _MCW_RC | _MCW_PC);
#else
    int round = FE_TONEAREST;
    switch (raw & 0xC00u)
    {
    case 0x400u:
        round = FE_DOWNWARD;
        break;
    case 0x800u:
        round = FE_UPWARD;
        break;
    case 0xC00u:
        round = FE_TOWARDZERO;
        break;
    default:
        break;
    }
    std::fesetround(round);
#endif
}


int crt_math_exc_handled(u32 flags, double* retval, u32 cw)
{
    // _handle_exc (0x00168493). Software flag bits, not the x87 status
    // layout: 1 overflow, 2 underflow, 4 zero-divide, 8 invalid, 0x10
    // inexact. A masked primary exception is retired and the rest of the
    // primary tests are skipped. Inexact is then retired when precision
    // is masked (control-word bit 5). The return is 1 only when every
    // raised bit was masked. 0x0016882D plants the sticky x87 status bit;
    // ported callers do not read that word.
    u32 remain = flags & 0x1fu;
    bool primary = false;
    if ((flags & 8u) != 0)
    {
        if ((cw & 1u) != 0)
        {
            remain &= ~8u;
            primary = true;
        }
    }
    if (!primary && (flags & 4u) != 0)
    {
        if ((cw & 4u) != 0)
        {
            remain &= ~4u;
            primary = true;
        }
    }
    if (!primary && (flags & 1u) != 0)
    {
        if ((cw & 8u) != 0)
        {
            const double positive_inf = std::bit_cast<double>(0x7FF0000000000000ULL);
            const double max_finite = std::bit_cast<double>(0x7FEFFFFFFFFFFFFFULL);
            const bool positive = *retval > 0.0;
            double adjusted = *retval;
            switch (cw & 0xC00u)
            {
            case 0x400u:
                adjusted = positive ? max_finite : -positive_inf;
                break;
            case 0x800u:
                adjusted = positive ? positive_inf : -max_finite;
                break;
            case 0xC00u:
                adjusted = positive ? max_finite : -max_finite;
                break;
            case 0x000u:
                adjusted = positive ? positive_inf : -positive_inf;
                break;
            default:
                break;
            }
            *retval = adjusted;
            remain &= ~1u;
            primary = true;
        }
    }
    if (!primary && (flags & 2u) != 0)
    {
        if ((cw & 0x10u) != 0)
        {
            // The math routine hands over a value scaled by 2^1536 so the
            // 80-bit interim did not underflow. Scale it back. Below
            // 2^-1074 the result is a signed zero. From there up to the
            // smallest normal the mantissa is shifted, discarding low bits.
            if (*retval == 0.0)
            {
                remain &= ~2u;
            }
            else
            {
                const bool neg = std::signbit(*retval);
                int exp = 0;
                const double mant = std::frexp(*retval, &exp);
                exp -= 1536;
                if (exp < -1074)
                {
                    *retval = neg ? -0.0 : 0.0;
                }
                else if (exp >= -1021)
                {
                    double scaled = std::ldexp(std::fabs(mant), exp);
                    if (neg)
                    {
                        scaled = -scaled;
                    }
                    *retval = scaled;
                }
                else
                {
                    u64 bits = std::bit_cast<u64>(std::fabs(mant));
                    bits &= 0x000fffffffffffffULL;
                    bits |= 0x0010000000000000ULL;
                    int shift = -1021 - exp;
                    while (shift > 0)
                    {
                        bits >>= 1u;
                        shift -= 1;
                    }
                    double scaled = std::bit_cast<double>(bits);
                    if (neg)
                    {
                        scaled = -scaled;
                    }
                    *retval = scaled;
                }
                remain &= ~2u;
            }
            primary = true;
        }
    }
    if ((flags & 0x10u) != 0 && (cw & 0x20u) != 0)
    {
        remain &= ~0x10u;
    }
    return remain == 0 ? 1 : 0;
}


// Xbox 0x0016F095
void crt_handle_math_exception(int opcode, CrtMathError* exc, u16* cw_ptr)
{
    // Xbox 0x0016F095
    // _87except. The unary dispatcher at 0x0016B723 and the binary one at
    // 0x0016B70C build a struct _exception (type, name, arg1, arg2, retval)
    // and call here with the operation code in the first argument, that
    // record, and the caller's x87 control word. Codes 0x10, 0x16, and 0x1D
    // are atan2, fmod, and pow: two operands. The other codes are unary.
    //
    // Type 1 (_DOMAIN) and 5 (_TLOSS) raise invalid (8). Type 2 (_SING)
    // raises zero-divide (4). Type 3 (_OVERFLOW) raises overflow|inexact
    // (0x11). Type 4 (_UNDERFLOW) raises underflow|inexact (0x12). Type 8
    // is the inexact report from the dispatcher epilogue and raises 0x10.
    // Type 7 is the pow special case: the type is rewritten to _DOMAIN and
    // the IEEE handler is skipped. Any other type skips it as well.
    //
    // _handle_exc (0x00168493) retires masked bits and, for overflow and
    // underflow, rewrites retval. A zero return calls _raise_exc
    // (0x001681EF), which raises the Win32 float status (invalid 0xC0000090,
    // zero-divide 0xC000008E, overflow 0xC0000091, underflow 0xC0000093,
    // inexact 0xC000008F) with one argument, the IEEE record. The later
    // status overwrites the earlier one, so invalid wins over zero-divide,
    // then overflow, underflow, and inexact.
    //
    // __ctrlfp (0x00168809) then reloads the saved control word. Type 8,
    // or a nonzero dword at 0x00338278, skips _matherr (0x0016DA59). That
    // body returns 0, and a nonzero return would skip the errno store.
    // 0x00168680 stores EDOM (0x21) for type 1 and ERANGE (0x22) for types
    // 2 and 3, through _errno (0x00168AF5).
    const u32 saved_cw = static_cast<u32>(*cw_ptr);
    u32 flags = 0;
    bool report = true;
    switch (exc->type)
    {
    case 1:
    case 5:
        flags = 8u;
        break;
    case 2:
        flags = 4u;
        break;
    case 3:
        flags = 0x11u;
        break;
    case 4:
        flags = 0x12u;
        break;
    case 7:
        exc->type = 1;
        report = false;
        break;
    case 8:
        flags = 0x10u;
        break;
    default:
        report = false;
        break;
    }

    if (report)
    {
        if (crt_math_exc_handled(flags, &exc->retval, saved_cw) == 0)
        {
            alignas(8) u8 record[0x80] = {};
            u32* operand2_flags = reinterpret_cast<u32*>(record + 0x40);
            if (opcode == 0x10 || opcode == 0x16 || opcode == 0x1d)
            {
                std::memcpy(record + 0x30, &exc->arg2, sizeof(exc->arg2));
                *operand2_flags = (*operand2_flags & 0xffffffe3u) | 3u;
            }
            else
            {
                *operand2_flags &= 0xfffffffeu;
            }

            u32 cause = 0;
            u32 code = 0;
            if ((flags & 0x10u) != 0)
            {
                cause |= 1u;
                code = 0xC000008Fu;
            }
            if ((flags & 2u) != 0)
            {
                cause |= 2u;
                code = 0xC0000093u;
            }
            if ((flags & 1u) != 0)
            {
                cause |= 4u;
                code = 0xC0000091u;
            }
            if ((flags & 4u) != 0)
            {
                cause |= 8u;
                code = 0xC000008Eu;
            }
            if ((flags & 8u) != 0)
            {
                cause |= 0x10u;
                code = 0xC0000090u;
            }
            std::memcpy(record + 4, &cause, sizeof(cause));
#if defined(_WIN32)
            std::uintptr_t info = reinterpret_cast<std::uintptr_t>(record);
            RaiseException(code, 0, 1, &info);
#else
            (void)code;
#endif
        }
    }

    crt_restore_x87_cw(saved_cw);

    if (exc->type != 8 && g_crt_fp_skip_matherr == 0)
    {
        // 0x0016DA59 returns 0, so control falls through to the errno store.
        const int matherr_result = 0;
        if (matherr_result != 0)
        {
            return;
        }
    }

    if (exc->type == 1)
    {
        *crt_errno_pointer() = 0x21;
    }
    else if (exc->type > 1 && exc->type <= 3)
    {
        *crt_errno_pointer() = 0x22;
    }
}


bool crt_fpu_inexact()
{
    // fnstsw bit 5 (precision). The host status word uses a different layout.
#if defined(_WIN32)
    return (_statusfp() & _SW_INEXACT) != 0;
#else
    return std::fetestexcept(FE_INEXACT) != 0;
#endif
}


double crt_math_scale_underflow(double value)
{
    // fscale by +1536.0 (0x0020A244). A result still below the smallest
    // normal is multiplied by +0.0 (0x0020A254), which keeps the sign.
    value = std::ldexp(value, 1536);
    const double min_normal = std::bit_cast<double>(0x0010000000000000ULL);
    if (std::fabs(value) < min_normal)
    {
        value *= 0.0;
    }
    return value;
}


double crt_math_scale_overflow(double value)
{
    // fscale by -1536.0 (0x0020A23C). A result still above the largest
    // finite double is multiplied by +infinity (0x0020A24C).
    value = std::ldexp(value, -1536);
    const double max_finite = std::bit_cast<double>(0x7FEFFFFFFFFFFFFFULL);
    if (std::fabs(value) > max_finite)
    {
        const double positive_inf = std::bit_cast<double>(0x7FF0000000000000ULL);
        value *= positive_inf;
    }
    return value;
}


// Xbox 0x0017052A
double crt_finish_math_dispatch(CrtMathCall& call)
{
    // Xbox 0x0017052A
    // Math-wrapper epilogue. The caller's EBP is still the wrapper frame
    // and the result is in ST0. 0x00170523 clears flags bit 0 and falls in.
    // 0x00170460 and 0x0017049E call this entry with bit 0 already set.
    //
    // 0x004DD0D0 nonzero loads the control word saved at [ebp-0xA4] and
    // returns ST0 unchanged (0x00170582). Otherwise ST0 is stored as a
    // double at [ebp-0x2D0] and the status byte at [ebp-0x90] selects the
    // record:
    //   0     inexact check
    //   -1    overflow when the double's exponent field is 0x7FF0, else
    //         the inexact check
    //   -2    underflow when that field is 0, overflow when it is 0x7FF0,
    //         else the inexact check
    //   else  the byte, sign-extended, is the exception type
    // A second test for 0 after the -1/-2 compares restores the control
    // word and returns. Status 0 already took the inexact check, so that
    // test is unreachable.
    //
    // The inexact check reports type 8 only when control-word bit 5 is
    // clear (precision unmasked) and status-word bit 5 is set. Any other
    // inexact outcome loads the saved control word and returns ST0.
    // Overflow (type 3) scales ST0 by 2^-1536. Underflow (type 4) scales
    // by 2^1536. The exponent field is the top word of the stored double,
    // masked with 0x7FF0.
    //
    // The record at [ebp-0x8E] is type, name at descriptor+1, the operands,
    // and the scaled ST0. Flags bit 0 clear copies the operands from
    // [ebp+8] and, when descriptor+0x0D is not 1, from [ebp+0x10]. Bit 0
    // set keeps the slots the caller already wrote; this port holds those
    // in arg1 and arg2. _87except (0x0016F095) is called with the opcode
    // at descriptor+0x0E, that record, and the saved control word. ST0 is
    // reloaded from the record's retval, then the control word is loaded
    // again at 0x00170582. _87except already reloads it; the port leaves
    // that restore to crt_handle_math_exception.
    if (g_crt_fp_skip_epilogue != 0)
    {
        crt_restore_x87_cw(call.cw);
        return call.result;
    }

    const u64 bits = std::bit_cast<u64>(call.result);
    const u16 exponent = static_cast<u16>((bits >> 48) & 0x7FF0u);
    const int status = static_cast<int>(static_cast<std::int8_t>(call.status));

    int type = 0;
    bool report = false;
    double value = call.result;
    if (status == -1)
    {
        if (exponent == 0x7FF0u)
        {
            type = 3;
            value = crt_math_scale_overflow(value);
            report = true;
        }
    }
    else if (status == -2)
    {
        if (exponent == 0)
        {
            type = 4;
            value = crt_math_scale_underflow(value);
            report = true;
        }
        else if (exponent == 0x7FF0u)
        {
            type = 3;
            value = crt_math_scale_overflow(value);
            report = true;
        }
    }
    else if (status != 0)
    {
        type = status;
        report = true;
    }

    if (!report)
    {
        if ((call.cw & 0x20u) != 0 || !crt_fpu_inexact())
        {
            crt_restore_x87_cw(call.cw);
            return call.result;
        }
        type = 8;
    }

    CrtMathError exc{};
    exc.type = type;
    exc.name = call.desc->name;
    exc.arg1 = call.arg1;
    if ((call.flags & 1u) != 0 || call.desc->unary != 1)
    {
        exc.arg2 = call.arg2;
    }
    exc.retval = value;
    u16 cw = call.cw;
    const int opcode = static_cast<int>(static_cast<std::int8_t>(call.desc->opcode));
    crt_handle_math_exception(opcode, &exc, &cw);
    call.result = exc.retval;
    return call.result;
}


// Xbox 0x00170523
double crt_dispatch_math_arguments(CrtMathCall& call)
{
    // Xbox 0x00170523
    // Seven-byte entry: and byte ptr [ebp-0x2C8], 0xFE. It clears bit 0 of
    // the dispatch flag and falls into 0x0017052A. Bit 0 set means the
    // operands were already stored in the exception record; clearing it
    // forces the copy from the wrapper's stack args at [ebp+8] and, for a
    // binary op, [ebp+0x10]. The unary wrappers at 0x00170671 and the
    // two-argument wrapper at 0x001704DB call this entry.
    call.flags = static_cast<u8>(call.flags & 0xFEu);
    return crt_finish_math_dispatch(call);
}


// Xbox 0x0016F266
void crt_unlock_ungetc_stream(void* stream)
{
    // Xbox 0x0016F266
    // __finally of ungetc (0x0016F22B). __SEH_prolog (0x00166D58) is entered
    // with local size 0x0C and the scope table at 0x00209ED8: one entry,
    // enclosing level -1, a null filter, and this handler. ungetc pushes
    // the FILE* at [ebp+0xc] and calls _lock_file (0x00168970), stores try
    // level 0 at [ebp-4], then calls _ungetc_lk (0x0016F1BF) with the
    // character at [ebp+8] and that FILE*. The success path at 0x0016F258
    // sets the try level to -1 and calls here. _local_unwind2 (0x00167062)
    // runs the same handler. The ten bytes push [ebp+0xc], call _unlock_file
    // (0x001689C2), pop the cdecl argument, and return.
    //
    // _unlock_file compares the pointer with _iob and with 0x00337F58.
    // Inside that span the lock number is (byte delta >> 5) + 0x10
    // (_STREAM_LOCKS). _unlock (0x0016810F) then leaves the critical
    // section stored in _locktable[index] at 0x00337AB0. Outside the
    // span it calls RtlLeaveCriticalSection (kernel ordinal 294, thunk
    // 0x00202EA4) on the critical section at FILE+0x20. A null pointer
    // would take that path at address 0x20; the port returns instead
    // of faulting.
    crt_unlock_closed_stream(stream);
}

} // namespace scene_detail


// Xbox 0x0016F172
int crt_validate_read(const void* lp, u32 ucb)
{
    // Xbox 0x0016F172
    // _ValidateRead. cdecl; the caller pops the address and the byte count.
    // ESI is set to 1, then XAPILIB::IsBadReadPtr (0x0014A840) is called.
    // A nonzero result clears ESI, and EAX returns that flag: 1 when the
    // range can be read, 0 when it cannot. A zero count is readable.
    // FindHandler (0x0016BEF6) probes one byte of a rethrown exception and
    // treats 0 as __inconsistency.
#if defined(_WIN32)
    return IsBadReadPtr(lp, ucb) == 0 ? 1 : 0;
#else
    if (ucb == 0)
    {
        return 1;
    }
    return lp != nullptr ? 1 : 0;
#endif
}


// Xbox 0x0016F18D
int crt_validate_write(void* lp, u32 ucb)
{
    // Xbox 0x0016F18D
    // _ValidateWrite. cdecl; the caller pops the address and the byte count.
    // ESI is set to 1, then XAPILIB::IsBadWritePtr (0x0014A8A5) is called.
    // A nonzero result clears ESI, and EAX returns that flag: 1 when the
    // range can be written, 0 when it cannot. A zero count is writable.
    // BuildCatchObject (0x0016BC55) probes one byte of the catch slot and
    // treats 0 as __inconsistency.
#if defined(_WIN32)
    return IsBadWritePtr(lp, ucb) == 0 ? 1 : 0;
#else
    if (ucb == 0)
    {
        return 1;
    }
    return lp != nullptr ? 1 : 0;
#endif
}


// Xbox 0x0016F1A8
int crt_validate_execute(const void* lpfn)
{
    // Xbox 0x0016F1A8
    // _ValidateExecute. cdecl; the caller pops the address.
    // ESI is set to 1, then XAPILIB::IsBadCodePtr (0x0014A90E) is called.
    // That thunk probes one byte with IsBadReadPtr (0x0014A840) and returns
    // stdcall. A nonzero result clears ESI, and EAX returns that flag: 1
    // when the byte can be read, 0 when it cannot. A null pointer fails.
    // BuildCatchObject (0x0016BC55) probes the copy function at
    // CatchableType+0x18 and treats 0 as __inconsistency. The filter at
    // 0x0016C22C probes the routine at DAT_004DD2C4 and returns 0 when the
    // probe fails.
    return crt_validate_read(lpfn, 1);
}

} // namespace gbtg
