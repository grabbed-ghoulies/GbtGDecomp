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
#include "crt_format.h"

namespace gbtg
{
using namespace scene_detail;

namespace scene_detail
{

// Xbox 0x00166A72
int crt_scan_wide_string(const char16_t* input, const char16_t* format, ...)
{
    CrtOutputFile file;
    file.flag = 0x49;
    file.base = reinterpret_cast<char*>(const_cast<char16_t*>(input));
    file.ptr = file.base;
    file.cnt = crt_wide_string_length(reinterpret_cast<const u16*>(input)) << 1;
    std::va_list arguments;
    va_start(arguments, format);
    const int result = crt_scan_wide_input(&file, format, arguments);
    va_end(arguments);
    return result;
}

// Xbox 0x00166A03
int crt_format_wide_string(char16_t* output, const char16_t* format, ...)
{
    CrtOutputFile file;
    file.ptr = reinterpret_cast<char*>(output);
    file.base = file.ptr;
    file.flag = 0x42;
    file.cnt = 0x7FFFFFFF;
    std::va_list arguments;
    va_start(arguments, format);
    const int result = crt_format_wide_output(&file, format, arguments);
    va_end(arguments);
    --file.cnt;
    if (file.cnt < 0)
    {
        // 0x00168E46: fixed string-stream flag selects IOERR/EOF.
        file.flag |= 0x20;
    }
    else
    {
        *file.ptr++ = 0;
    }
    --file.cnt;
    if (file.cnt < 0)
    {
        file.flag |= 0x20;
    }
    else
    {
        *file.ptr = 0;
    }
    return result;
}

// Xbox 0x00166FB1
int crt_format_wide_arguments(char16_t* output, const char16_t* format,
    std::va_list arguments)
{
    CrtOutputFile file;
    file.ptr = reinterpret_cast<char*>(output);
    file.base = file.ptr;
    file.flag = 0x42;
    file.cnt = 0x7FFFFFFF;
    const int result = crt_format_wide_output(&file, format, arguments);
    --file.cnt;
    if (file.cnt < 0)
    {
        // 0x00168E46: fixed string-stream flag selects IOERR/EOF.
        file.flag |= 0x20;
    }
    else
    {
        *file.ptr++ = 0;
    }
    --file.cnt;
    if (file.cnt < 0)
    {
        file.flag |= 0x20;
    }
    else
    {
        *file.ptr = 0;
    }
    return result;
}

// Xbox 0x0014BC66
int xapi_format_wide_arguments(char16_t* output, const char16_t* format,
    std::va_list arguments)
{
    return crt_format_wide_arguments(output, format, arguments);
}

// Xbox 0x0014BC7D
int xapi_format_wide_string(char16_t* output, const char16_t* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    const int result = xapi_format_wide_arguments(output, format, arguments);
    va_end(arguments);
    return result;
}

// Xbox 0x00166449
int crt_format_bounded(char* output, int capacity, const char* format,
    std::va_list arguments)
{
    CrtOutputFile file;
    file.cnt = capacity;
    file.flag = 0x42;
    file.base = output;
    file.ptr = output;
    const int result = crt_format_output(&file, format, arguments);
    if (output != nullptr)
    {
        --file.cnt;
        if (file.cnt < 0)
        {
            // The fixed string-stream flag selects 0x00168E46's IOERR/EOF branch.
            file.flag |= 0x20;
        }
        else
        {
            *file.ptr = 0;
        }
    }
    return result;
}

// Xbox 0x001663B9
int crt_format_bounded_string(char* output, int capacity, const char* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    // 0x00166449 has the same stream initialization, output and termination branches.
    const int result = crt_format_bounded(output, capacity, format, arguments);
    va_end(arguments);
    return result;
}

// Xbox 0x001661F9
int crt_format_string(char* output, const char* format, ...)
{
    CrtOutputFile file;
    file.cnt = 0x7FFFFFFF;
    file.flag = 0x42;
    file.base = output;
    file.ptr = output;
    std::va_list arguments;
    va_start(arguments, format);
    const int count = crt_format_output(&file, format, arguments);
    va_end(arguments);
    if (output != nullptr)
    {
        --file.cnt;
        if (file.cnt < 0)
        {
            // Inline 0x00168E46's string-stream branch: flag 0x42 always
            // has bit 0x40, so it sets IOERR and returns EOF without IO.
            file.flag |= 0x20;
        }
        else
        {
            *file.ptr = '\0';
        }
    }
    return count;
}


// Xbox 0x00168FED
int crt_format_output(void* stream, const char* format, std::va_list argptr)
{
    // Xbox 0x00168FED
    // _output. The printf family builds a FILE and calls here. The return is
    // the number of characters accepted, or -1 after a failed write.
    //
    // Retail FILE is 0x20 bytes with 32-bit pointers: ptr, cnt, base, flag,
    // then the handle fields. The port keeps that order and stores host
    // pointers, so a destination buffer is addressable. A null format
    // returns 0. The walk is the table at 0x00209280. A character from 0x20
    // through 0x78 takes its class from the low nibble. Characters outside
    // that span are class 0.
    // The next state is the high nibble of the byte at class * 8 + state.
    // The states are literal, conversion, flag, width, dot, precision, size,
    // and type. The jump table for those states is at 0x00169754.
    //
    // Flag bits are the CRT set: '+' 0x1, ' ' 0x2, '-' 0x4, '0' 0x8, 'l' 0x10,
    // 'h' 0x20, signed 0x40, '#' 0x80, negative 0x100, forced octal 0x200,
    // 'w' 0x800, and 'I64' 0x8000. %I64 and %I32 consume the two digits. %I
    // immediately before d, i, o, u, x, or X is ignored. Any other character
    // after %I is written as a literal and the state returns to literal.
    //
    // Literals go through write_char (0x00168F5F). When the high byte of
    // _pctype[ch] (0x00338180) has bit 0x80, the next source byte is written
    // as well. The C locale table has that bit clear. write_multi_char
    // (0x00168F92) pads. write_string (0x00168FB6) copies. A stream whose
    // flag has 0x40 and whose base is null only advances the count. Any
    // other stream stores the byte at ptr while cnt stays non-negative.
    // A full buffer is _flsbuf (0x00168E46). A string stream, or a stream
    // that is not open for write, sets flag bit 0x20 and fails the count.
    // The port reports that same failure when a real file buffer is full.
    //
    // Integers are reduced by __aulldvrm (0x0016CFA0). %n stores the count.
    // %Z reads a counted record: the word at +0 is the byte length, and the
    // pointer at +4 is the buffer. A null record prints "(null)" from
    // 0x002092DC; the wide null string is the UTF-16 text at 0x002092E4.
    // Floats take a double and call the slots _fpmath fills at 0x003381A0
    // (__cfltcvt), 0x003381AC (__forcdecpt), and 0x003381A4 (__cropzeros).
    // While a slot is still _fptrap (0x0016F08C) the process exits with
    // code 2. Precision above 0xA3 mallocs (0x00167F1C) precision + 0x15D
    // bytes and this function frees that block (0x00167F2E). Wide units
    // expand through wctomb (0x0016DF8A). With __mb_cur_max at 1 that stores
    // one byte, or returns -1 and sets errno to EILSEQ (42) when the unit
    // does not fit in a byte.
    //
    // The host va_list is the Xbox argument cursor. Each promoted integer or
    // pointer is one argument. A double or an I64 value is one 8-byte
    // argument. %p prints a host pointer in uppercase hex, at least eight
    // digits. On Xbox the pointer is 32 bits, so eight digits are the whole
    // value.

    using OutputFile = CrtOutputFile;

    // Image bytes at 0x00209280. Index 0 is the character ' '.
    static constexpr unsigned char kLookup[] = {
        0x06, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x03, 0x06, 0x00, 0x06, 0x02, 0x10,
        0x04, 0x45, 0x45, 0x45, 0x05, 0x05, 0x05, 0x05, 0x05, 0x35, 0x30, 0x00, 0x50, 0x00, 0x00, 0x00,
        0x00, 0x20, 0x28, 0x38, 0x50, 0x58, 0x07, 0x08, 0x00, 0x37, 0x30, 0x30, 0x57, 0x50, 0x07, 0x00,
        0x00, 0x20, 0x20, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x60, 0x68, 0x60, 0x60, 0x60, 0x60, 0x00,
        0x00, 0x70, 0x70, 0x78, 0x78, 0x78, 0x78, 0x08, 0x07, 0x08, 0x00, 0x00, 0x07, 0x00, 0x08, 0x08,
        0x08, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x07, 0x08,
    };
    static_assert(sizeof(kLookup) == 89);

    constexpr int kSign = 0x1;
    constexpr int kBlank = 0x2;
    constexpr int kLeft = 0x4;
    constexpr int kZero = 0x8;
    constexpr int kLong = 0x10;
    constexpr int kShort = 0x20;
    constexpr int kSigned = 0x40;
    constexpr int kAlt = 0x80;
    constexpr int kNeg = 0x100;
    constexpr int kOctal = 0x200;
    constexpr int kWide = 0x800;
    constexpr int kI64 = 0x8000;
    constexpr u32 kFptrap = 0x0016F08Cu;

    static constexpr char kNullText[] = "(null)";
    static constexpr char16_t kNullWide[] = {'(', 'n', 'u', 'l', 'l', ')', 0};

    auto* out = static_cast<OutputFile*>(stream);
    int charsout = 0;
    int state = 0;
    int flags = 0;
    int precision = -1;
    int width = 0;
    int prefix_len = 0;
    int text_len = 0;
    int no_output = 0;
    int wide_chars = 0;
    int capexp = 0;
    int hex_shift = 0;
    int radix = 10;
    char prefix[2] = {};
    const char* text = "";
    char* heap = nullptr;
    char stack_buf[0x200];
    char digits[513];
    char mb[8];
    u32 lo = 0;
    u32 hi = 0;

    auto write_char = [&](unsigned char byte)
    {
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += 1;
            return;
        }

        int wrote = static_cast<int>(byte);
        out->cnt -= 1;
        if (out->cnt >= 0)
        {
            *out->ptr = static_cast<char>(byte);
            out->ptr += 1;
        }
        else
        {
            // _flsbuf sets the error bit and returns EOF for a string stream
            // and for a stream that is not open for write. A buffered file
            // would flush here; that body lives at 0x00168E46.
            out->flag |= 0x20;
            wrote = -1;
        }
        if (wrote == -1)
        {
            charsout = -1;
        }
        else
        {
            charsout += 1;
        }
    };

    auto write_multi = [&](unsigned char byte, int count)
    {
        while (count > 0)
        {
            count -= 1;
            write_char(byte);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    auto write_string = [&](const char* bytes, int count)
    {
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += count;
            return;
        }
        while (count > 0)
        {
            const unsigned char byte = static_cast<unsigned char>(*bytes);
            count -= 1;
            bytes += 1;
            write_char(byte);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    const unsigned char* cursor = nullptr;
    auto write_literal = [&](unsigned char byte)
    {
        wide_chars = 0;
        if ((g_crt_pctype[byte] & 0x8000u) != 0)
        {
            write_char(byte);
            byte = *cursor;
            cursor += 1;
        }
        write_char(byte);
    };

    auto wctomb_byte = [&](char* dest, int wchar) -> int
    {
        if (g_crt_mb_cur_max <= 1)
        {
            if (wchar < 0 || wchar > 0xff)
            {
                errno = 42;
                return -1;
            }
            dest[0] = static_cast<char>(wchar);
            return 1;
        }
        errno = 42;
        return -1;
    };

    auto force_decimal = [](char* s)
    {
        char* p = s;
        while (*p != '\0')
        {
            if (*p == '.')
            {
                return;
            }
            if (*p == 'e' || *p == 'E')
            {
                break;
            }
            p += 1;
        }
        char* end = p;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= p)
        {
            *(end + 1) = *end;
            end -= 1;
        }
        *p = '.';
    };

    auto crop_zeros = [](char* s)
    {
        char* p = s;
        while (*p != '\0' && *p != '.')
        {
            p += 1;
        }
        if (*p == '\0')
        {
            return;
        }
        while (*p != '\0' && *p != 'e' && *p != 'E')
        {
            p += 1;
        }
        char* stop = p;
        if (p > s)
        {
            p -= 1;
        }
        while (p > s && *p == '0')
        {
            p -= 1;
        }
        if (*p == '.' && p > s)
        {
            p -= 1;
        }
        p += 1;
        while (*stop != '\0')
        {
            *p = *stop;
            p += 1;
            stop += 1;
        }
        *p = '\0';
    };

    auto pad_exponent = [](char* s, int buf_size)
    {
        char* mark = std::strchr(s, 'e');
        if (mark == nullptr)
        {
            mark = std::strchr(s, 'E');
        }
        if (mark == nullptr || (mark[1] != '+' && mark[1] != '-'))
        {
            return;
        }
        char* digits_at = mark + 2;
        int count = 0;
        while (digits_at[count] >= '0' && digits_at[count] <= '9')
        {
            count += 1;
        }
        if (count == 0 || count >= 3)
        {
            return;
        }
        const int need = 3 - count;
        const int used = static_cast<int>(std::strlen(s));
        if (used + need + 1 > buf_size)
        {
            return;
        }
        char* end = digits_at + count;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= digits_at)
        {
            *(end + need) = *end;
            end -= 1;
        }
        for (int i = 0; i < need; i += 1)
        {
            digits_at[i] = '0';
        }
    };

    auto format_float = [&](char* buffer, int buf_size, char fmt, double value)
    {
        if (g_cfltcvt_tab[0] == kFptrap)
        {
            std::exit(2);
        }
        if (!std::isfinite(value))
        {
            const char* token = "1.#QNAN";
            if (std::isnan(value))
            {
                token = std::signbit(value) ? "-1.#IND" : "1.#QNAN";
            }
            else
            {
                token = std::signbit(value) ? "-1.#INF" : "1.#INF";
            }
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%s", token);
            return;
        }

        const bool upper = capexp != 0;
        if (fmt == 'e')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision, value);
        }
        else if (fmt == 'f')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", precision, value);
        }
        else
        {
            bool scientific = false;
            int exponent = 0;
            if (value != 0.0)
            {
                char probe[64];
                std::snprintf(probe, sizeof(probe), "%.*e", precision - 1, value);
                const char* mark = std::strchr(probe, 'e');
                if (mark == nullptr)
                {
                    mark = std::strchr(probe, 'E');
                }
                if (mark != nullptr)
                {
                    exponent = std::atoi(mark + 1);
                }
                scientific = exponent < -4 || exponent >= precision;
            }
            if (scientific)
            {
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision - 1, value);
            }
            else
            {
                int frac = precision - exponent - 1;
                if (frac < 0)
                {
                    frac = 0;
                }
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", frac, value);
            }
        }
        pad_exponent(buffer, buf_size);
    };

    auto write_number = [&](bool as_i64)
    {
        if ((flags & kSigned) != 0 && static_cast<std::int32_t>(hi) < 0)
        {
            unsigned long long mag = (static_cast<unsigned long long>(hi) << 32) | lo;
            mag = 0ull - mag;
            lo = static_cast<u32>(mag);
            hi = static_cast<u32>(mag >> 32);
            flags |= kNeg;
        }
        if (!as_i64)
        {
            hi = 0;
        }
        if (precision < 0)
        {
            precision = 1;
        }
        else
        {
            flags &= ~kZero;
            if (precision > 0x200)
            {
                precision = 0x200;
            }
        }
        if (lo == 0 && hi == 0)
        {
            prefix_len = 0;
        }

        char* place = digits + 512;
        while (true)
        {
            const int prec = precision;
            precision -= 1;
            if (prec < 1 && lo == 0 && hi == 0)
            {
                break;
            }
            const unsigned long long value = (static_cast<unsigned long long>(hi) << 32) | lo;
            const unsigned long long quot = value / static_cast<unsigned>(radix);
            const unsigned rem = static_cast<unsigned>(value % static_cast<unsigned>(radix));
            lo = static_cast<u32>(quot);
            hi = static_cast<u32>(quot >> 32);
            int digit = static_cast<int>(rem) + 0x30;
            if (digit > 0x39)
            {
                digit += hex_shift;
            }
            *place = static_cast<char>(digit);
            place -= 1;
        }
        text_len = static_cast<int>((digits + 512) - place);
        text = place + 1;
        if ((flags & kOctal) != 0 && (text_len == 0 || text[0] != '0'))
        {
            *place = '0';
            text = place;
            text_len += 1;
        }
    };

    auto load_integer = [&]()
    {
        if ((flags & kI64) != 0)
        {
            unsigned long long bits = 0;
            if ((flags & kSigned) != 0)
            {
                bits = static_cast<unsigned long long>(va_arg(argptr, long long));
            }
            else
            {
                bits = va_arg(argptr, unsigned long long);
            }
            lo = static_cast<u32>(bits);
            hi = static_cast<u32>(bits >> 32);
            return;
        }
        if ((flags & kShort) != 0)
        {
            const int value = va_arg(argptr, int);
            if ((flags & kSigned) != 0)
            {
                const auto narrow = static_cast<std::int16_t>(value);
                lo = static_cast<u32>(static_cast<std::int32_t>(narrow));
                hi = narrow < 0 ? 0xffffffffu : 0u;
            }
            else
            {
                lo = static_cast<u32>(static_cast<std::uint16_t>(value));
                hi = 0;
            }
            return;
        }
        if ((flags & kSigned) != 0)
        {
            const int value = va_arg(argptr, int);
            lo = static_cast<u32>(value);
            hi = value < 0 ? 0xffffffffu : 0u;
            return;
        }
        lo = va_arg(argptr, unsigned int);
        hi = 0;
    };

    auto set_hex_prefix = [&]()
    {
        if ((flags & kAlt) == 0)
        {
            return;
        }
        prefix[0] = '0';
        prefix[1] = static_cast<char>(hex_shift + 'Q');
        prefix_len = 2;
    };

    auto emit = [&]()
    {
        if (no_output != 0)
        {
            return;
        }
        if ((flags & kSigned) != 0)
        {
            char sign = 0;
            if ((flags & kNeg) != 0)
            {
                sign = '-';
            }
            else if ((flags & kSign) != 0)
            {
                sign = '+';
            }
            else if ((flags & kBlank) != 0)
            {
                sign = ' ';
            }
            if (sign != 0)
            {
                prefix[0] = sign;
                prefix_len = 1;
            }
        }

        const int pad = width - prefix_len - text_len;
        if ((flags & (kLeft | kZero)) == 0)
        {
            write_multi(0x20, pad);
        }
        write_string(prefix, prefix_len);
        if ((flags & kZero) != 0 && (flags & kLeft) == 0)
        {
            write_multi(0x30, pad);
        }
        if (wide_chars == 0 || text_len < 1)
        {
            write_string(text, text_len);
        }
        else
        {
            const auto* units = reinterpret_cast<const char16_t*>(text);
            int left = text_len;
            while (left != 0)
            {
                left -= 1;
                const unsigned unit = static_cast<unsigned>(*units);
                units += 1;
                const int produced = wctomb_byte(mb, static_cast<int>(unit));
                if (produced < 1)
                {
                    break;
                }
                write_string(mb, produced);
            }
        }
        if ((flags & kLeft) != 0)
        {
            write_multi(0x20, pad);
        }
    };

    auto counted_length = [](const char* s) -> int
    {
        const char* start = s;
        while (*s != '\0')
        {
            s += 1;
        }
        return static_cast<int>(s - start);
    };

    if (format == nullptr || *format == '\0')
    {
        return charsout;
    }

    cursor = reinterpret_cast<const unsigned char*>(format);
    unsigned char ch = *cursor;
    for (;;)
    {
        cursor += 1;
        if (charsout < 0)
        {
            return charsout;
        }

        int cls = 0;
        if (ch >= 0x20 && ch <= 0x78)
        {
            cls = static_cast<signed char>(kLookup[ch - 0x20]) & 0x0f;
        }
        const int slot = cls * 8 + state;
        state = static_cast<signed char>(kLookup[slot]) >> 4;
        if (static_cast<unsigned>(state) <= 7u)
        {
            switch (state)
            {
            case 0:
                write_literal(ch);
                break;
            case 1:
                precision = -1;
                capexp = 0;
                no_output = 0;
                width = 0;
                prefix_len = 0;
                flags = 0;
                wide_chars = 0;
                break;
            case 2:
                if (ch == 0x20)
                {
                    flags |= kBlank;
                }
                else if (ch == '#')
                {
                    flags |= kAlt;
                }
                else if (ch == '+')
                {
                    flags |= kSign;
                }
                else if (ch == '-')
                {
                    flags |= kLeft;
                }
                else if (ch == '0')
                {
                    flags |= kZero;
                }
                break;
            case 3:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    if (value < 0)
                    {
                        flags |= kLeft;
                        width = static_cast<int>(0u - static_cast<unsigned>(value));
                    }
                    else
                    {
                        width = value;
                    }
                }
                else
                {
                    width = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(width) * 10u - 0x30u);
                }
                break;
            case 4:
                precision = 0;
                break;
            case 5:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    precision = value < 0 ? -1 : value;
                }
                else
                {
                    precision = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(precision) * 10u - 0x30u);
                }
                break;
            case 6:
                if (ch == 'I')
                {
                    const unsigned char n0 = *cursor;
                    unsigned char n1 = 0;
                    if (n0 != 0)
                    {
                        n1 = *(cursor + 1);
                    }
                    if (n0 == '6' && n1 == '4')
                    {
                        cursor += 2;
                        flags |= kI64;
                    }
                    else if (n0 == '3' && n1 == '2')
                    {
                        cursor += 2;
                        flags &= ~kI64;
                    }
                    else if (n0 != 'd' && n0 != 'i' && n0 != 'o' && n0 != 'u' && n0 != 'x' && n0 != 'X')
                    {
                        state = 0;
                        write_literal(ch);
                    }
                }
                else if (ch == 'h')
                {
                    flags |= kShort;
                }
                else if (ch == 'l')
                {
                    flags |= kLong;
                }
                else if (ch == 'w')
                {
                    flags |= kWide;
                }
                break;
            case 7:
                if (ch == 'E' || ch == 'G')
                {
                    capexp = 1;
                    ch = static_cast<unsigned char>(ch + 0x20);
                }
                if (ch == 'C' || ch == 'S')
                {
                    if ((flags & (kWide | kShort | kLong)) == 0)
                    {
                        flags |= kWide;
                    }
                }

                if (ch == 'd' || ch == 'i')
                {
                    flags |= kSigned;
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'u')
                {
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'o')
                {
                    radix = 8;
                    if ((flags & kAlt) != 0)
                    {
                        flags |= kOctal;
                    }
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'x' || ch == 'X' || ch == 'p')
                {
                    hex_shift = ch == 'x' ? 0x27 : 7;
                    radix = 16;
                    set_hex_prefix();
                    if (ch == 'p')
                    {
                        const auto bits = reinterpret_cast<std::uintptr_t>(va_arg(argptr, void*));
                        lo = static_cast<u32>(bits);
                        hi = static_cast<u32>(bits >> 32);
                        precision = 8;
                        write_number(true);
                    }
                    else
                    {
                        load_integer();
                        write_number((flags & kI64) != 0);
                    }
                    emit();
                }
                else if (ch == 'e' || ch == 'f' || ch == 'g')
                {
                    flags |= kSigned;
                    char* buffer = stack_buf;
                    int buf_size = 0x200;
                    if (precision < 0)
                    {
                        precision = 6;
                    }
                    else if (precision == 0)
                    {
                        if (ch == 'g')
                        {
                            precision = 1;
                        }
                    }
                    else
                    {
                        if (precision > 0x200)
                        {
                            precision = 0x200;
                        }
                        if (precision > 0xa3)
                        {
                            heap = static_cast<char*>(std::malloc(static_cast<std::size_t>(precision) + 0x15du));
                            if (heap != nullptr)
                            {
                                buffer = heap;
                                buf_size = precision + 0x15d;
                            }
                            else
                            {
                                precision = 0xa3;
                            }
                        }
                    }
                    text = buffer;
                    const double value = va_arg(argptr, double);
                    format_float(buffer, buf_size, static_cast<char>(ch), value);
                    if ((flags & kAlt) != 0 && precision == 0)
                    {
                        if (g_cfltcvt_tab[3] == kFptrap)
                        {
                            std::exit(2);
                        }
                        force_decimal(buffer);
                    }
                    if (ch == 'g' && (flags & kAlt) == 0)
                    {
                        if (g_cfltcvt_tab[1] == kFptrap)
                        {
                            std::exit(2);
                        }
                        crop_zeros(buffer);
                    }
                    if (buffer[0] == '-')
                    {
                        flags |= kNeg;
                        text = buffer + 1;
                    }
                    else
                    {
                        text = buffer;
                    }
                    text_len = counted_length(text);
                    emit();
                }
                else if (ch == 'c' || ch == 'C')
                {
                    if ((flags & (kWide | kLong)) != 0)
                    {
                        const int value = va_arg(argptr, int);
                        const int wchar = static_cast<std::int16_t>(value);
                        text_len = wctomb_byte(mb, wchar);
                        text = mb;
                        if (text_len < 0)
                        {
                            no_output = 1;
                        }
                    }
                    else
                    {
                        const int value = va_arg(argptr, int);
                        mb[0] = static_cast<char>(value);
                        text = mb;
                        text_len = 1;
                    }
                    emit();
                }
                else if (ch == 's' || ch == 'S')
                {
                    int limit = precision == -1 ? 0x7fffffff : precision;
                    if ((flags & (kWide | kLong)) != 0)
                    {
                        using WidePtr = char16_t*;
                        char16_t* units = va_arg(argptr, WidePtr);
                        if (units == nullptr)
                        {
                            units = const_cast<char16_t*>(kNullWide);
                        }
                        char16_t* start = units;
                        while (limit != 0)
                        {
                            limit -= 1;
                            if (*units == 0)
                            {
                                break;
                            }
                            units += 1;
                        }
                        text = reinterpret_cast<const char*>(start);
                        text_len = static_cast<int>(units - start);
                        wide_chars = 1;
                    }
                    else
                    {
                        using CharPtr = char*;
                        char* str = va_arg(argptr, CharPtr);
                        if (str == nullptr)
                        {
                            str = const_cast<char*>(kNullText);
                        }
                        char* start = str;
                        while (limit != 0)
                        {
                            limit -= 1;
                            if (*str == '\0')
                            {
                                break;
                            }
                            str += 1;
                        }
                        text = start;
                        text_len = static_cast<int>(str - start);
                    }
                    emit();
                }
                else if (ch == 'Z')
                {
                    const auto* block = static_cast<const char*>(va_arg(argptr, void*));
                    const char* buffer = nullptr;
                    if (block != nullptr)
                    {
                        std::memcpy(&buffer, block + 4, sizeof(buffer));
                    }
                    if (block == nullptr || buffer == nullptr)
                    {
                        text = kNullText;
                        text_len = counted_length(text);
                    }
                    else
                    {
                        std::int16_t raw = 0;
                        std::memcpy(&raw, block, sizeof(raw));
                        text = buffer;
                        text_len = raw;
                        if ((flags & kWide) != 0)
                        {
                            text_len /= 2;
                            wide_chars = 1;
                        }
                        else
                        {
                            wide_chars = 0;
                        }
                    }
                    emit();
                }
                else if (ch == 'n')
                {
                    void* dest = va_arg(argptr, void*);
                    if ((flags & kShort) != 0)
                    {
                        *static_cast<std::uint16_t*>(dest) = static_cast<std::uint16_t>(charsout);
                    }
                    else
                    {
                        *static_cast<int*>(dest) = charsout;
                    }
                    no_output = 1;
                }
                else
                {
                    emit();
                }
                if (heap != nullptr)
                {
                    crt_heap_free(heap);
                    heap = nullptr;
                }
                break;
            default:
                break;
            }
        }

        ch = *cursor;
        if (ch == 0)
        {
            break;
        }
    }
    return charsout;
}


// Xbox 0x0016993D
int crt_format_wide_output(void* stream, const char16_t* format, std::va_list argptr)
{
    // Xbox 0x0016993D
    // _woutput. The wide printf family builds a FILE and calls here.
    // 0x00166A03 and 0x00166FB1 are the swprintf / vswprintf bodies: flag
    // 0x42 (_IOWRT|_IOSTRG), cnt 0x7FFFFFFF, ptr and base on the caller's
    // buffer. 0x001671A5 is _snwprintf: cnt is the character count times
    // two, because each unit occupies two bytes. The return is the number
    // of wide characters accepted, or -1 after a failed write. A null or
    // empty format returns 0. Retail faults on a null format; the port
    // returns 0.
    //
    // The walk is the same table as _output (0x00168FED), at 0x00209280.
    // A unit from 0x20 through 0x78 takes its class from the low nibble of
    // the byte at 0x00209260 plus the unit. Every other unit is class 0.
    // The next state is the high nibble of the byte at class * 8 + state.
    // States are literal, conversion, flag, width, dot, precision, size,
    // and type. The jump table is at 0x0016A0E1.
    //
    // Flag bits match _output: '+' 0x1, ' ' 0x2, '-' 0x4, '0' 0x8, 'l' 0x10,
    // 'h' 0x20, signed 0x40, '#' 0x80, negative 0x100, forced octal 0x200,
    // 'w' 0x800, and 'I64' 0x8000. %C and %S set the short bit when no size
    // is set yet, so they are the narrow forms. %c and %s stay wide. %hc
    // and %hs are narrow. %I64 and %I32 consume the two digits. %I
    // immediately before d, i, o, u, x, or X is ignored. Any other unit
    // after %I is written as a literal and the state returns to literal.
    //
    // Literals go through write_char (0x001698B6). write_multi_char
    // (0x001698DB) pads. write_string (0x00169900) copies wide units. A
    // stream whose flag has 0x40 and whose base is null only advances the
    // count. Any other string stream calls _fputwc (0x0016DFB5), which
    // subtracts 2 from cnt and stores the unit. A negative cnt sets flag
    // bit 0x20 and returns 0xFFFF, and the count becomes -1. A stream that
    // is not a string takes the text-mode path: the default ioinfo at
    // 0x00338250 has FTEXT set, so the unit is expanded with wctomb
    // (0x0016DF8A) and each byte is stored with cnt dropping by 1. A real
    // file flush is _flsbuf (0x00168E46) or _flswbuf (0x0016F2F1). The port
    // reports that same failure when the buffer is full.
    //
    // Narrow text (integers, floats, %hs, %hc, a null %Z) is expanded with
    // mbtowc (0x0016E07A). With __mb_cur_max at 1 that turns one byte into
    // one unit and returns 1, or 0 for a zero byte. %Z reads a counted
    // record: the word at +0 is the byte length, and the pointer at +4 is
    // the buffer. A null record prints "(null)" from 0x002092DC. The wide
    // null string is the UTF-16 text at 0x002092E4. The 'w' flag halves the
    // counted length and leaves those units wide.
    //
    // Floats take a double and call the slots _fpmath fills at 0x003381A0
    // (__cfltcvt), 0x003381AC (__forcdecpt), and 0x003381A4 (__cropzeros).
    // While a slot is still _fptrap (0x0016F08C) the process exits with
    // code 2. Precision above 0xA3 mallocs (0x00167F1C) precision + 0x15D
    // bytes and this function frees that block (0x00167F2E).
    //
    // The host va_list is the Xbox argument cursor. Each promoted integer
    // or pointer is one argument. A double or an I64 value is one 8-byte
    // argument. %p prints a host pointer in uppercase hex, at least eight
    // digits. On Xbox the pointer is 32 bits, so eight digits are the whole
    // value. %n stores the wide count.

    using OutputFile = CrtOutputFile;

    static constexpr unsigned char kLookup[] = {
        0x06, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x03, 0x06, 0x00, 0x06, 0x02, 0x10,
        0x04, 0x45, 0x45, 0x45, 0x05, 0x05, 0x05, 0x05, 0x05, 0x35, 0x30, 0x00, 0x50, 0x00, 0x00, 0x00,
        0x00, 0x20, 0x28, 0x38, 0x50, 0x58, 0x07, 0x08, 0x00, 0x37, 0x30, 0x30, 0x57, 0x50, 0x07, 0x00,
        0x00, 0x20, 0x20, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x60, 0x68, 0x60, 0x60, 0x60, 0x60, 0x00,
        0x00, 0x70, 0x70, 0x78, 0x78, 0x78, 0x78, 0x08, 0x07, 0x08, 0x00, 0x00, 0x07, 0x00, 0x08, 0x08,
        0x08, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x07, 0x08,
    };
    static_assert(sizeof(kLookup) == 89);

    constexpr int kSign = 0x1;
    constexpr int kBlank = 0x2;
    constexpr int kLeft = 0x4;
    constexpr int kZero = 0x8;
    constexpr int kLong = 0x10;
    constexpr int kShort = 0x20;
    constexpr int kSigned = 0x40;
    constexpr int kAlt = 0x80;
    constexpr int kNeg = 0x100;
    constexpr int kOctal = 0x200;
    constexpr int kWide = 0x800;
    constexpr int kI64 = 0x8000;
    constexpr u32 kFptrap = 0x0016F08Cu;

    static constexpr char kNullText[] = "(null)";
    static constexpr char16_t kNullWide[] = {'(', 'n', 'u', 'l', 'l', ')', 0};

    auto* out = static_cast<OutputFile*>(stream);
    int charsout = 0;
    int state = 0;
    int flags = 0;
    int precision = -1;
    int width = 0;
    int prefix_len = 0;
    int text_len = 0;
    int no_output = 0;
    int wide_text = 0;
    int capexp = 0;
    int hex_shift = 0;
    int radix = 10;
    char16_t prefix[2] = {};
    const char* narrow = "";
    const char16_t* wide = kNullWide;
    char* heap = nullptr;
    char stack_buf[0x200];
    char digits[513];
    char16_t wchar_buf[2] = {};
    u32 lo = 0;
    u32 hi = 0;

    auto wctomb_byte = [&](char* dest, int wchar) -> int
    {
        if (g_crt_mb_cur_max <= 1)
        {
            if (wchar < 0 || wchar > 0xff)
            {
                errno = 42;
                return -1;
            }
            dest[0] = static_cast<char>(wchar);
            return 1;
        }
        errno = 42;
        return -1;
    };

    auto write_wchar = [&](unsigned unit)
    {
        const auto code = static_cast<std::uint16_t>(unit);
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += 1;
            return;
        }

        if ((crt_put_wide_character_unlocked(code, out) & 0xffffu) == 0xffffu)
        {
            charsout = -1;
            return;
        }
        charsout += 1;
    };

    auto write_multi = [&](unsigned unit, int count)
    {
        while (count > 0)
        {
            count -= 1;
            write_wchar(unit);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    auto write_wchars = [&](const char16_t* units, int count)
    {
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += count;
            return;
        }
        while (count > 0)
        {
            count -= 1;
            const unsigned unit = static_cast<unsigned>(*units);
            units += 1;
            write_wchar(unit);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    auto force_decimal = [](char* s)
    {
        char* p = s;
        while (*p != '\0')
        {
            if (*p == '.')
            {
                return;
            }
            if (*p == 'e' || *p == 'E')
            {
                break;
            }
            p += 1;
        }
        char* end = p;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= p)
        {
            *(end + 1) = *end;
            end -= 1;
        }
        *p = '.';
    };

    auto crop_zeros = [](char* s)
    {
        char* p = s;
        while (*p != '\0' && *p != '.')
        {
            p += 1;
        }
        if (*p == '\0')
        {
            return;
        }
        while (*p != '\0' && *p != 'e' && *p != 'E')
        {
            p += 1;
        }
        char* stop = p;
        if (p > s)
        {
            p -= 1;
        }
        while (p > s && *p == '0')
        {
            p -= 1;
        }
        if (*p == '.' && p > s)
        {
            p -= 1;
        }
        p += 1;
        while (*stop != '\0')
        {
            *p = *stop;
            p += 1;
            stop += 1;
        }
        *p = '\0';
    };

    auto pad_exponent = [](char* s, int buf_size)
    {
        char* mark = std::strchr(s, 'e');
        if (mark == nullptr)
        {
            mark = std::strchr(s, 'E');
        }
        if (mark == nullptr || (mark[1] != '+' && mark[1] != '-'))
        {
            return;
        }
        char* digits_at = mark + 2;
        int count = 0;
        while (digits_at[count] >= '0' && digits_at[count] <= '9')
        {
            count += 1;
        }
        if (count == 0 || count >= 3)
        {
            return;
        }
        const int need = 3 - count;
        const int used = static_cast<int>(std::strlen(s));
        if (used + need + 1 > buf_size)
        {
            return;
        }
        char* end = digits_at + count;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= digits_at)
        {
            *(end + need) = *end;
            end -= 1;
        }
        for (int i = 0; i < need; i += 1)
        {
            digits_at[i] = '0';
        }
    };

    auto format_float = [&](char* buffer, int buf_size, char fmt, double value)
    {
        if (g_cfltcvt_tab[0] == kFptrap)
        {
            std::exit(2);
        }
        if (!std::isfinite(value))
        {
            const char* token = "1.#QNAN";
            if (std::isnan(value))
            {
                token = std::signbit(value) ? "-1.#IND" : "1.#QNAN";
            }
            else
            {
                token = std::signbit(value) ? "-1.#INF" : "1.#INF";
            }
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%s", token);
            return;
        }

        const bool upper = capexp != 0;
        if (fmt == 'e')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision, value);
        }
        else if (fmt == 'f')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", precision, value);
        }
        else
        {
            bool scientific = false;
            int exponent = 0;
            if (value != 0.0)
            {
                char probe[64];
                std::snprintf(probe, sizeof(probe), "%.*e", precision - 1, value);
                const char* mark = std::strchr(probe, 'e');
                if (mark == nullptr)
                {
                    mark = std::strchr(probe, 'E');
                }
                if (mark != nullptr)
                {
                    exponent = std::atoi(mark + 1);
                }
                scientific = exponent < -4 || exponent >= precision;
            }
            if (scientific)
            {
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision - 1, value);
            }
            else
            {
                int frac = precision - exponent - 1;
                if (frac < 0)
                {
                    frac = 0;
                }
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", frac, value);
            }
        }
        pad_exponent(buffer, buf_size);
    };

    auto write_number = [&](bool as_i64)
    {
        if ((flags & kSigned) != 0 && static_cast<std::int32_t>(hi) < 0)
        {
            unsigned long long mag = (static_cast<unsigned long long>(hi) << 32) | lo;
            mag = 0ull - mag;
            lo = static_cast<u32>(mag);
            hi = static_cast<u32>(mag >> 32);
            flags |= kNeg;
        }
        if (!as_i64)
        {
            hi = 0;
        }
        if (precision < 0)
        {
            precision = 1;
        }
        else
        {
            flags &= ~kZero;
            if (precision > 0x200)
            {
                precision = 0x200;
            }
        }
        if (lo == 0 && hi == 0)
        {
            prefix_len = 0;
        }

        char* place = digits + 512;
        while (true)
        {
            const int prec = precision;
            precision -= 1;
            if (prec < 1 && lo == 0 && hi == 0)
            {
                break;
            }
            const unsigned long long value = (static_cast<unsigned long long>(hi) << 32) | lo;
            const unsigned long long quot = value / static_cast<unsigned>(radix);
            const unsigned rem = static_cast<unsigned>(value % static_cast<unsigned>(radix));
            lo = static_cast<u32>(quot);
            hi = static_cast<u32>(quot >> 32);
            int digit = static_cast<int>(rem) + 0x30;
            if (digit > 0x39)
            {
                digit += hex_shift;
            }
            *place = static_cast<char>(digit);
            place -= 1;
        }
        text_len = static_cast<int>((digits + 512) - place);
        narrow = place + 1;
        wide_text = 0;
        if ((flags & kOctal) != 0 && (text_len == 0 || narrow[0] != '0'))
        {
            *place = '0';
            narrow = place;
            text_len += 1;
        }
    };

    auto load_integer = [&]()
    {
        if ((flags & kI64) != 0)
        {
            unsigned long long bits = 0;
            if ((flags & kSigned) != 0)
            {
                bits = static_cast<unsigned long long>(va_arg(argptr, long long));
            }
            else
            {
                bits = va_arg(argptr, unsigned long long);
            }
            lo = static_cast<u32>(bits);
            hi = static_cast<u32>(bits >> 32);
            return;
        }
        if ((flags & kShort) != 0)
        {
            const int value = va_arg(argptr, int);
            if ((flags & kSigned) != 0)
            {
                const auto narrow_bits = static_cast<std::int16_t>(value);
                lo = static_cast<u32>(static_cast<std::int32_t>(narrow_bits));
                hi = narrow_bits < 0 ? 0xffffffffu : 0u;
            }
            else
            {
                lo = static_cast<u32>(static_cast<std::uint16_t>(value));
                hi = 0;
            }
            return;
        }
        if ((flags & kSigned) != 0)
        {
            const int value = va_arg(argptr, int);
            lo = static_cast<u32>(value);
            hi = value < 0 ? 0xffffffffu : 0u;
            return;
        }
        lo = va_arg(argptr, unsigned int);
        hi = 0;
    };

    auto set_hex_prefix = [&]()
    {
        if ((flags & kAlt) == 0)
        {
            return;
        }
        prefix[0] = static_cast<char16_t>(0x30);
        prefix[1] = static_cast<char16_t>(hex_shift + 0x51);
        prefix_len = 2;
    };

    auto emit = [&]()
    {
        if (no_output != 0)
        {
            return;
        }
        if ((flags & kSigned) != 0)
        {
            char16_t sign = 0;
            if ((flags & kNeg) != 0)
            {
                sign = static_cast<char16_t>(0x2d);
            }
            else if ((flags & kSign) != 0)
            {
                sign = static_cast<char16_t>(0x2b);
            }
            else if ((flags & kBlank) != 0)
            {
                sign = static_cast<char16_t>(0x20);
            }
            if (sign != 0)
            {
                prefix[0] = sign;
                prefix_len = 1;
            }
        }

        const int pad = width - prefix_len - text_len;
        if ((flags & (kLeft | kZero)) == 0)
        {
            write_multi(0x20, pad);
        }
        write_wchars(prefix, prefix_len);
        if ((flags & kZero) != 0 && (flags & kLeft) == 0)
        {
            write_multi(0x30, pad);
        }
        if (wide_text == 0 && text_len > 0)
        {
            const char* bytes = narrow;
            int left = text_len;
            while (left > 0)
            {
                left -= 1;
                char16_t unit = 0;
                const int produced = crt_byte_to_wide_character(&unit, bytes, g_crt_mb_cur_max);
                if (produced < 1)
                {
                    break;
                }
                write_wchar(static_cast<unsigned>(unit));
                bytes += produced;
                if (charsout == -1)
                {
                    break;
                }
            }
        }
        else
        {
            write_wchars(wide, text_len);
        }
        if ((flags & kLeft) != 0)
        {
            write_multi(0x20, pad);
        }
    };

    auto counted_length = [](const char* s) -> int
    {
        const char* start = s;
        while (*s != '\0')
        {
            s += 1;
        }
        return static_cast<int>(s - start);
    };

    if (format == nullptr || *format == 0)
    {
        return charsout;
    }

    char16_t ch = *format;
    for (;;)
    {
        format += 1;
        if (charsout < 0)
        {
            return charsout;
        }

        int cls = 0;
        if (ch >= 0x20 && ch <= 0x78)
        {
            cls = static_cast<signed char>(kLookup[ch - 0x20]) & 0x0f;
        }
        const int slot = cls * 8 + state;
        state = static_cast<signed char>(kLookup[slot]) >> 4;
        if (static_cast<unsigned>(state) <= 7u)
        {
            switch (state)
            {
            case 0:
                wide_text = 1;
                write_wchar(static_cast<unsigned>(ch));
                break;
            case 1:
                precision = -1;
                capexp = 0;
                no_output = 0;
                width = 0;
                prefix_len = 0;
                flags = 0;
                wide_text = 0;
                break;
            case 2:
                if (ch == 0x20)
                {
                    flags |= kBlank;
                }
                else if (ch == '#')
                {
                    flags |= kAlt;
                }
                else if (ch == '+')
                {
                    flags |= kSign;
                }
                else if (ch == '-')
                {
                    flags |= kLeft;
                }
                else if (ch == '0')
                {
                    flags |= kZero;
                }
                break;
            case 3:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    if (value < 0)
                    {
                        flags |= kLeft;
                        width = static_cast<int>(0u - static_cast<unsigned>(value));
                    }
                    else
                    {
                        width = value;
                    }
                }
                else
                {
                    width = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(width) * 10u - 0x30u);
                }
                break;
            case 4:
                precision = 0;
                break;
            case 5:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    precision = value < 0 ? -1 : value;
                }
                else
                {
                    precision = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(precision) * 10u - 0x30u);
                }
                break;
            case 6:
                if (ch == 'I')
                {
                    const char16_t n0 = *format;
                    char16_t n1 = 0;
                    if (n0 != 0)
                    {
                        n1 = format[1];
                    }
                    if (n0 == '6' && n1 == '4')
                    {
                        format += 2;
                        flags |= kI64;
                    }
                    else if (n0 == '3' && n1 == '2')
                    {
                        format += 2;
                        flags &= ~kI64;
                    }
                    else if (n0 != 'd' && n0 != 'i' && n0 != 'o' && n0 != 'u' && n0 != 'x' && n0 != 'X')
                    {
                        state = 0;
                        wide_text = 1;
                        write_wchar(static_cast<unsigned>(ch));
                    }
                }
                else if (ch == 'h')
                {
                    flags |= kShort;
                }
                else if (ch == 'l')
                {
                    flags |= kLong;
                }
                else if (ch == 'w')
                {
                    flags |= kWide;
                }
                break;
            case 7:
                if (ch == 'E' || ch == 'G')
                {
                    capexp = 1;
                    ch = static_cast<char16_t>(ch + 0x20);
                }
                if (ch == 'C' || ch == 'S')
                {
                    if ((flags & (kWide | kShort | kLong)) == 0)
                    {
                        flags |= kShort;
                    }
                }

                if (ch == 'd' || ch == 'i')
                {
                    flags |= kSigned;
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'u')
                {
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'o')
                {
                    radix = 8;
                    if ((flags & kAlt) != 0)
                    {
                        flags |= kOctal;
                    }
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'x' || ch == 'X' || ch == 'p')
                {
                    hex_shift = ch == 'x' ? 0x27 : 7;
                    radix = 16;
                    set_hex_prefix();
                    if (ch == 'p')
                    {
                        const auto bits = reinterpret_cast<std::uintptr_t>(va_arg(argptr, void*));
                        lo = static_cast<u32>(bits);
                        hi = static_cast<u32>(bits >> 32);
                        precision = 8;
                        write_number(true);
                    }
                    else
                    {
                        load_integer();
                        write_number((flags & kI64) != 0);
                    }
                    emit();
                }
                else if (ch == 'e' || ch == 'f' || ch == 'g')
                {
                    flags |= kSigned;
                    char* buffer = stack_buf;
                    int buf_size = 0x200;
                    if (precision < 0)
                    {
                        precision = 6;
                    }
                    else if (precision == 0)
                    {
                        if (ch == 'g')
                        {
                            precision = 1;
                        }
                    }
                    else
                    {
                        if (precision > 0x200)
                        {
                            precision = 0x200;
                        }
                        if (precision > 0xa3)
                        {
                            heap = static_cast<char*>(std::malloc(static_cast<std::size_t>(precision) + 0x15du));
                            if (heap != nullptr)
                            {
                                buffer = heap;
                                buf_size = precision + 0x15d;
                            }
                            else
                            {
                                precision = 0xa3;
                            }
                        }
                    }
                    narrow = buffer;
                    wide_text = 0;
                    const double value = va_arg(argptr, double);
                    format_float(buffer, buf_size, static_cast<char>(ch), value);
                    if ((flags & kAlt) != 0 && precision == 0)
                    {
                        if (g_cfltcvt_tab[3] == kFptrap)
                        {
                            std::exit(2);
                        }
                        force_decimal(buffer);
                    }
                    if (ch == 'g' && (flags & kAlt) == 0)
                    {
                        if (g_cfltcvt_tab[1] == kFptrap)
                        {
                            std::exit(2);
                        }
                        crop_zeros(buffer);
                    }
                    if (buffer[0] == '-')
                    {
                        flags |= kNeg;
                        narrow = buffer + 1;
                    }
                    else
                    {
                        narrow = buffer;
                    }
                    text_len = counted_length(narrow);
                    emit();
                }
                else if (ch == 'c' || ch == 'C')
                {
                    const int value = va_arg(argptr, int);
                    const unsigned unit = static_cast<unsigned>(value) & 0xffffu;
                    wide_text = 1;
                    wide = wchar_buf;
                    text_len = 1;
                    if ((flags & kShort) == 0)
                    {
                        wchar_buf[0] = static_cast<char16_t>(unit);
                    }
                    else
                    {
                        char bytes[2];
                        bytes[0] = static_cast<char>(unit);
                        bytes[1] = 0;
                        const int produced = crt_byte_to_wide_character(wchar_buf, bytes, g_crt_mb_cur_max);
                        if (produced < 0)
                        {
                            no_output = 1;
                        }
                    }
                    emit();
                }
                else if (ch == 's' || ch == 'S')
                {
                    int limit = precision == -1 ? 0x7fffffff : precision;
                    if ((flags & kShort) == 0)
                    {
                        char16_t* units = va_arg(argptr, char16_t*);
                        if (units == nullptr)
                        {
                            units = const_cast<char16_t*>(kNullWide);
                        }
                        char16_t* start = units;
                        while (limit != 0)
                        {
                            limit -= 1;
                            if (*units == 0)
                            {
                                break;
                            }
                            units += 1;
                        }
                        wide = start;
                        text_len = static_cast<int>(units - start);
                        wide_text = 1;
                    }
                    else
                    {
                        char* str = va_arg(argptr, char*);
                        if (str == nullptr)
                        {
                            str = const_cast<char*>(kNullText);
                        }
                        narrow = str;
                        wide_text = 0;
                        text_len = 0;
                        if (limit > 0)
                        {
                            const unsigned char* cursor = reinterpret_cast<const unsigned char*>(str);
                            while (text_len < limit)
                            {
                                const unsigned char byte = *cursor;
                                if (byte == 0)
                                {
                                    break;
                                }
                                if ((g_crt_pctype[byte] & 0x8000u) != 0)
                                {
                                    cursor += 1;
                                }
                                cursor += 1;
                                text_len += 1;
                            }
                        }
                    }
                    emit();
                }
                else if (ch == 'Z')
                {
                    const auto* block = static_cast<const char*>(va_arg(argptr, void*));
                    const char* buffer = nullptr;
                    if (block != nullptr)
                    {
                        std::memcpy(&buffer, block + 4, sizeof(buffer));
                    }
                    if (block == nullptr || buffer == nullptr)
                    {
                        narrow = kNullText;
                        text_len = counted_length(narrow);
                        wide_text = 0;
                    }
                    else
                    {
                        std::int16_t raw = 0;
                        std::memcpy(&raw, block, sizeof(raw));
                        text_len = raw;
                        if ((flags & kWide) != 0)
                        {
                            text_len /= 2;
                            wide = reinterpret_cast<const char16_t*>(buffer);
                            wide_text = 1;
                        }
                        else
                        {
                            narrow = buffer;
                            wide_text = 0;
                        }
                    }
                    emit();
                }
                else if (ch == 'n')
                {
                    void* dest = va_arg(argptr, void*);
                    if ((flags & kShort) != 0)
                    {
                        *static_cast<std::uint16_t*>(dest) = static_cast<std::uint16_t>(charsout);
                    }
                    else
                    {
                        *static_cast<int*>(dest) = charsout;
                    }
                    no_output = 1;
                }
                else
                {
                    emit();
                }
                if (heap != nullptr)
                {
                    crt_heap_free(heap);
                    heap = nullptr;
                }
                break;
            default:
                break;
            }
        }

        ch = *format;
        if (ch == 0)
        {
            break;
        }
    }
    return charsout;
}


// Xbox 0x0016A17C
int crt_scan_wide_input(void* stream, const char16_t* format, std::va_list argptr)
{
    // Xbox 0x0016A17C
    // _winput. swscanf (0x00166A72) builds a string FILE and calls here:
    // ptr and base point at the wchar buffer, cnt is wcslen (0x00166ECB)
    // times two, flag is 0x49 (_IOREAD|_IOMYBUF|_IOSTRG). The return is the
    // number of assigned fields, or -1 when the stream is at EOF and no
    // conversion and no suppressed field has matched. %n stores the wide
    // count and does not add to that total.
    //
    // The host FILE matches _woutput: ptr, cnt, base, flag, then the handle
    // fields. A string stream (flag 0x40) reads a wchar and subtracts 2 from
    // cnt. A negative cnt is WEOF; retail would call _filwbuf (0x0016F419).
    // Pushback is _ungetwc_nolock (0x0016E1A5) through the helper at
    // 0x0016A14E, which ignores WEOF. On a string stream the previous wchar
    // has to equal the pushed character. A byte stream with room reads one
    // byte. __mb_cur_max is 1 and the C ctype table has no lead bytes, so
    // that byte is the wchar. A full byte buffer returns WEOF instead of
    // calling _filbuf (0x00168A14).
    //
    // Whitespace in the format skips wchar whitespace via iswctype
    // (0x00166BF9) and _pwctype at 0x0020998A. Bytes at or above 0x100, and
    // WEOF, are not whitespace. 0x09..0x0D, 0x20, and 0xA0 are. %s stops on
    // 0x09..0x0D and 0x20 only. Digits and hex use _pctype (0x00338180), the
    // low byte when __mb_cur_max is at most 1, otherwise the whole word.
    //
    // %I64 selects the 64-bit store and %I32 is a 32-bit no-op. %I immediately
    // before d, i, o, x, or X is ignored. A bare %I is %i. %n with I64 stores
    // the zeroed 64-bit accumulator. Other integers store a short when the
    // size count is 0 ('h') and an int otherwise. %p forces that count back
    // to 1 and reads hex without a 0x prefix. %i and %x accept 0x. A leading
    // 0 on %i selects octal. %c, %s, and %[ write wchars when the size count
    // is positive. %C and %S with no size are bytes, expanded with wctomb
    // (0x0016DF8A): one byte, or -1 and errno EILSEQ (42) above 0xFF.
    //
    // Floats (%e, %f, %g and the capitals) build a wchar image and call
    // __fassign, the slot _fpmath writes at 0x003381A8 (g_cfltcvt_tab[2]).
    // While that slot is still _fptrap (0x0016F08C) the process exits with
    // code 2. A size count of 1 ('f', the default) stores a float. Any other
    // count stores a double. The decimal point is the C-locale '.' from
    // mbtowc (0x0016E07A) of the byte at 0x0033818C. A scanset bitmap is
    // 0x2000 bytes. Retail alloca-probes that (0x00166F70) and mallocs
    // (0x00167F1C) if the probe raises; this frees the block on the way out.
    // A ']' missing before the format terminator ends the scan.

    using ScanFile = CrtOutputFile;

    auto* file = static_cast<ScanFile*>(stream);
    const char16_t* fmt = format;
    int charcount = 0;
    int assigned = 0;
    unsigned char matched = 0;
    unsigned ch = 0;
    unsigned char* scan = nullptr;

    auto is_wspace = [&](unsigned unit) -> bool
    {
        return crt_wide_character_type(static_cast<u16>(unit), 8u) != 0;
    };

    auto narrow_class = [&](unsigned unit, unsigned mask) -> bool
    {
        if ((unit & 0xff00u) != 0)
        {
            return false;
        }
        const unsigned byte = unit & 0xffu;
        if (g_crt_mb_cur_max > 1)
        {
            return crt_character_type(static_cast<int>(byte), mask) != 0;
        }
        return (static_cast<unsigned>(static_cast<u8>(g_crt_pctype[byte])) & mask) != 0;
    };

    auto hex_adjust = [&](unsigned unit) -> unsigned
    {
        if ((unit & 0xff00u) == 0 && narrow_class(unit, 4u))
        {
            return unit;
        }
        if (crt_wide_character_type(static_cast<u16>(unit), 2u) != 0)
        {
            return unit - 0x27u;
        }
        return unit - 7u;
    };

    auto get_unit = [&]() -> unsigned
    {
        if ((file->flag & 0x40) != 0)
        {
            file->cnt -= 2;
            if (file->cnt < 0)
            {
                return 0xffffu;
            }
            const auto unit = *reinterpret_cast<char16_t*>(file->ptr);
            file->ptr += 2;
            return unit;
        }
        file->cnt -= 1;
        if (file->cnt < 0)
        {
            return 0xffffu;
        }
        const auto byte = static_cast<unsigned char>(*file->ptr);
        file->ptr += 1;
        return byte;
    };

    auto unget_unit = [&](unsigned unit)
    {
        if ((unit & 0xffffu) == 0xffffu)
        {
            return;
        }
        const int flags = file->flag;
        if ((flags & 1) == 0 && ((flags & 0x80) == 0 || (flags & 2) != 0))
        {
            return;
        }
        if (file->base == nullptr)
        {
            return;
        }
        if ((flags & 0x40) != 0)
        {
            if (file->ptr < file->base + 2)
            {
                if (file->cnt != 0 || file->bufsiz < 2)
                {
                    return;
                }
                file->ptr = file->base + 2;
            }
            file->ptr -= 2;
            if (*reinterpret_cast<char16_t*>(file->ptr) != static_cast<char16_t>(unit))
            {
                file->ptr += 2;
                return;
            }
            file->cnt += 2;
            file->flag = (file->flag & ~0x10) | 1;
            return;
        }
        if (file->ptr <= file->base)
        {
            return;
        }
        file->ptr -= 1;
        *file->ptr = static_cast<char>(unit);
        file->cnt += 1;
        file->flag = (file->flag & ~0x10) | 1;
    };

    auto skip_white = [&]() -> unsigned
    {
        unsigned next = 0;
        do
        {
            charcount += 1;
            next = get_unit();
        } while (is_wspace(next));
        return next;
    };

    auto wctomb_byte = [&](char* dest, unsigned unit) -> int
    {
        if (dest == nullptr)
        {
            return 0;
        }
        if (unit > 0xffu)
        {
            errno = 42;
            return -1;
        }
        *dest = static_cast<char>(unit);
        return 1;
    };

    auto assign_float = [&](int kind, void* dest, const char16_t* text)
    {
        if (g_cfltcvt_tab[2] == 0x0016F08Cu)
        {
            std::exit(2);
        }
        char narrow[360];
        int length = 0;
        while (text[length] != 0 && length < 358)
        {
            narrow[length] = static_cast<char>(text[length] & 0xff);
            length += 1;
        }
        narrow[length] = 0;
        if (kind == 0)
        {
            float value = 0.0f;
            std::from_chars(narrow, narrow + length, value);
            *static_cast<float*>(dest) = value;
        }
        else
        {
            double value = 0.0;
            std::from_chars(narrow, narrow + length, value);
            *static_cast<double*>(dest) = value;
        }
    };

    std::va_list args;
    va_copy(args, argptr);

    auto finish_field = [&]() -> bool
    {
        matched = static_cast<unsigned char>(matched + 1);
        fmt += 1;
        if ((ch & 0xffffu) == 0xffffu && (*fmt != '%' || fmt[1] != 'n'))
        {
            return false;
        }
        return true;
    };

    while (*fmt != 0)
    {
        if (is_wspace(*fmt))
        {
            charcount -= 1;
            ch = skip_white();
            unget_unit(ch);
            do
            {
                fmt += 1;
            } while (is_wspace(*fmt));
            continue;
        }

        if (*fmt != '%')
        {
            charcount += 1;
            ch = get_unit();
            const char16_t literal = *fmt;
            fmt += 1;
            if (literal != static_cast<char16_t>(ch))
            {
                unget_unit(ch);
                break;
            }
            if ((ch & 0xffffu) == 0xffffu && (*fmt != '%' || fmt[1] != 'n'))
            {
                break;
            }
            continue;
        }

        int width = 0;
        int widthset = 0;
        signed char suppress = 0;
        signed char size_count = 1;
        signed char wide_count = 0;
        int i64 = 0;
        bool spec_done = false;
        while (!spec_done)
        {
            fmt += 1;
            const unsigned unit = *fmt;
            if ((unit & 0xff00u) == 0 && narrow_class(unit, 4u))
            {
                widthset += 1;
                width = static_cast<int>(unit - 0x30u) + width * 10;
                continue;
            }
            if (unit == 'N' || unit == 'F')
            {
                continue;
            }
            if (unit == '*')
            {
                suppress = static_cast<signed char>(suppress + 1);
                continue;
            }
            if (unit == 'I')
            {
                const char16_t next = fmt[1];
                if (next == '6' && fmt[2] == '4')
                {
                    i64 += 1;
                    fmt += 2;
                    continue;
                }
                if (next == '3' && fmt[2] == '2')
                {
                    fmt += 2;
                    continue;
                }
                if (next == 'd' || next == 'i' || next == 'o' || next == 'x' || next == 'X')
                {
                    continue;
                }
                spec_done = true;
                continue;
            }
            if (unit == 'L')
            {
                size_count = static_cast<signed char>(size_count + 1);
                continue;
            }
            if (unit == 'h')
            {
                size_count = static_cast<signed char>(size_count - 1);
                wide_count = static_cast<signed char>(wide_count - 1);
                continue;
            }
            if (unit == 'l')
            {
                size_count = static_cast<signed char>(size_count + 1);
                wide_count = static_cast<signed char>(wide_count + 1);
                continue;
            }
            if (unit == 'w')
            {
                wide_count = static_cast<signed char>(wide_count + 1);
                continue;
            }
            spec_done = true;
        }

        if (wide_count == 0)
        {
            if (*fmt == 'S' || *fmt == 'C')
            {
                wide_count = -1;
            }
            else
            {
                wide_count = 1;
            }
        }

        const unsigned spec = static_cast<unsigned>(*fmt) | 0x20u;
        if (spec != 'n')
        {
            if (spec == 'c' || spec == '{')
            {
                charcount += 1;
                ch = get_unit();
            }
            else
            {
                ch = skip_white();
            }
        }
        if (widthset != 0 && width == 0)
        {
            unget_unit(ch);
            break;
        }

        const bool is_float = spec == 'e' || spec == 'f' || spec == 'g';
        const bool is_int = spec == 'd' || spec == 'i' || spec == 'o' || spec == 'u' || spec == 'x' || spec == 'p';
        if (spec == 'n')
        {
            if (suppress == 0)
            {
                void* dest = va_arg(args, void*);
                if (i64 != 0)
                {
                    *reinterpret_cast<std::uint64_t*>(dest) = 0;
                }
                else if (size_count != 0)
                {
                    *reinterpret_cast<int*>(dest) = charcount;
                }
                else
                {
                    *reinterpret_cast<u16*>(dest) = static_cast<u16>(charcount);
                }
            }
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (is_int)
        {
            int started = 0;
            bool done = false;
            signed char negative = 0;
            u32 bits = 0;
            std::uint64_t wide_bits = 0;
            const bool radix_prefix = spec == 'x' || spec == 'i';
            unsigned number_spec = spec;
            if (spec == 'p')
            {
                size_count = 1;
            }
            if (spec == 'i')
            {
                number_spec = 'd';
            }
            if (ch == '-' || ch == '+')
            {
                if (ch == '-')
                {
                    negative = 1;
                }
                width -= 1;
                if (width == 0 && widthset != 0)
                {
                    done = true;
                }
                else
                {
                    charcount += 1;
                    ch = get_unit();
                }
            }
            if (radix_prefix && ch == '0')
            {
                charcount += 1;
                ch = get_unit();
                if (ch == 'x' || ch == 'X')
                {
                    charcount += 1;
                    ch = get_unit();
                    if (widthset != 0)
                    {
                        width -= 2;
                        if (width < 1)
                        {
                            done = true;
                        }
                    }
                    number_spec = 'x';
                }
                else
                {
                    started = 1;
                    if (number_spec == 'x')
                    {
                        charcount -= 1;
                        unget_unit(ch);
                        ch = '0';
                    }
                    else
                    {
                        if (widthset != 0)
                        {
                            width -= 1;
                            if (width == 0)
                            {
                                done = true;
                            }
                        }
                        number_spec = 'o';
                    }
                }
            }
            while (!done)
            {
                const bool hex = number_spec == 'x' || number_spec == 'p';
                bool take = false;
                if (hex)
                {
                    if (narrow_class(ch, 0x80u))
                    {
                        if (i64 != 0)
                        {
                            wide_bits <<= 4;
                        }
                        else
                        {
                            bits <<= 4;
                        }
                        ch = hex_adjust(ch);
                        take = true;
                    }
                }
                else if (narrow_class(ch, 4u))
                {
                    if (number_spec == 'o')
                    {
                        if (ch < 0x38u)
                        {
                            if (i64 != 0)
                            {
                                wide_bits <<= 3;
                            }
                            else
                            {
                                bits <<= 3;
                            }
                            take = true;
                        }
                    }
                    else
                    {
                        if (i64 != 0)
                        {
                            wide_bits *= 10u;
                        }
                        else
                        {
                            bits *= 10u;
                        }
                        take = true;
                    }
                }
                if (!take)
                {
                    done = true;
                    charcount -= 1;
                    unget_unit(ch);
                    break;
                }
                started += 1;
                const unsigned digit = (ch & 0xffffu) - 0x30u;
                if (i64 != 0)
                {
                    wide_bits += digit;
                }
                else
                {
                    bits += digit;
                }
                if (widthset != 0)
                {
                    width -= 1;
                    if (width == 0)
                    {
                        done = true;
                        break;
                    }
                }
                charcount += 1;
                ch = get_unit();
            }
            if (number_spec == 0x46u)
            {
                started = 0;
            }
            if (started == 0)
            {
                break;
            }
            if (negative != 0)
            {
                if (i64 != 0)
                {
                    wide_bits = static_cast<std::uint64_t>(-static_cast<std::int64_t>(wide_bits));
                }
                else
                {
                    bits = static_cast<u32>(-static_cast<int>(bits));
                }
            }
            if (suppress == 0)
            {
                void* dest = va_arg(args, void*);
                assigned += 1;
                if (i64 != 0)
                {
                    *reinterpret_cast<std::uint64_t*>(dest) = wide_bits;
                }
                else if (size_count != 0)
                {
                    *reinterpret_cast<u32*>(dest) = bits;
                }
                else
                {
                    *reinterpret_cast<u16*>(dest) = static_cast<u16>(bits);
                }
            }
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (is_float)
        {
            int started = 0;
            int float_len = 0;
            char16_t float_buf[352];
            if (ch == '-' || ch == '+')
            {
                if (ch == '-')
                {
                    float_buf[float_len] = '-';
                    float_len += 1;
                }
                width -= 1;
                charcount += 1;
                ch = get_unit();
            }
            if (widthset == 0 || width > 0x15d)
            {
                width = 0x15d;
            }
            auto take_digits = [&]()
            {
                while ((ch & 0xff00u) == 0 && narrow_class(ch, 4u))
                {
                    const int before = width;
                    width -= 1;
                    if (before == 0)
                    {
                        break;
                    }
                    started += 1;
                    float_buf[float_len] = static_cast<char16_t>(ch);
                    float_len += 1;
                    charcount += 1;
                    ch = get_unit();
                }
            };
            take_digits();
            char16_t decimal = 0;
            crt_byte_to_wide_character(&decimal, &g_crt_decimal_point, g_crt_mb_cur_max);
            if (ch == static_cast<unsigned>(decimal))
            {
                const int before = width;
                width -= 1;
                if (before != 0)
                {
                    charcount += 1;
                    ch = get_unit();
                    float_buf[float_len] = decimal;
                    float_len += 1;
                    take_digits();
                }
            }
            if (started != 0 && (ch == 'e' || ch == 'E'))
            {
                const int before = width;
                width -= 1;
                if (before != 0)
                {
                    float_buf[float_len] = 'e';
                    float_len += 1;
                    charcount += 1;
                    ch = get_unit();
                    if (ch == '-' || ch == '+')
                    {
                        const unsigned sign = ch;
                        const int sign_before = width;
                        width -= 1;
                        if (sign_before != 0)
                        {
                            if (sign == '-')
                            {
                                float_buf[float_len] = '-';
                                float_len += 1;
                            }
                            charcount += 1;
                            ch = get_unit();
                        }
                        else
                        {
                            width = 0;
                            if (sign == '-')
                            {
                                float_buf[float_len] = '-';
                                float_len += 1;
                            }
                        }
                    }
                    take_digits();
                }
            }
            charcount -= 1;
            unget_unit(ch);
            if (started == 0)
            {
                break;
            }
            if (suppress != 0)
            {
                if (!finish_field())
                {
                    break;
                }
                continue;
            }
            assigned += 1;
            float_buf[float_len] = 0;
            const int bytes = float_len * 2 + 2;
            void* block = std::malloc(static_cast<std::size_t>(bytes));
            if (block == nullptr)
            {
                break;
            }
            std::memcpy(block, float_buf, static_cast<std::size_t>(bytes));
            void* dest = va_arg(args, void*);
            assign_float(static_cast<int>(size_count) - 1, dest, static_cast<char16_t*>(block));
            crt_heap_free(block);
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (spec == 'c' || spec == 's' || spec == '{')
        {
            signed char wide_out = 0;
            unsigned char invert = 0;
            if (spec == 'c' && widthset == 0)
            {
                widthset = 1;
                width += 1;
            }
            if (wide_count > 0)
            {
                wide_out = 1;
            }
            if (spec == '{')
            {
                if (scan == nullptr)
                {
                    scan = static_cast<unsigned char*>(std::malloc(0x2000));
                    if (scan == nullptr)
                    {
                        break;
                    }
                }
                std::memset(scan, 0, 0x2000);
                const char16_t* cursor = fmt + 1;
                unsigned prev = 0;
                if (*cursor == '^')
                {
                    invert = 0xff;
                    cursor += 1;
                }
                if (*cursor == ']')
                {
                    prev = ']';
                    scan[0x5du >> 3] = static_cast<unsigned char>(scan[0x5du >> 3] | (1u << (0x5du & 7u)));
                    cursor += 1;
                }
                bool closed = false;
                while (*cursor != 0)
                {
                    const unsigned cur = *cursor;
                    if (cur == ']')
                    {
                        closed = true;
                        break;
                    }
                    cursor += 1;
                    if (cur == '-' && prev != 0 && *cursor != ']' && *cursor != 0)
                    {
                        const unsigned end = *cursor;
                        cursor += 1;
                        unsigned low = prev;
                        unsigned high = end;
                        if (low > high)
                        {
                            const unsigned swap = low;
                            low = high;
                            high = swap;
                        }
                        for (unsigned unit = low;; unit += 1)
                        {
                            scan[unit >> 3] = static_cast<unsigned char>(scan[unit >> 3] | (1u << (unit & 7u)));
                            if (unit == high)
                            {
                                break;
                            }
                        }
                        prev = 0;
                        continue;
                    }
                    prev = cur;
                    scan[cur >> 3] = static_cast<unsigned char>(scan[cur >> 3] | (1u << (cur & 7u)));
                }
                if (!closed)
                {
                    break;
                }
                fmt = cursor;
            }

            char* write = nullptr;
            char* mark = nullptr;
            if (suppress == 0)
            {
                write = static_cast<char*>(va_arg(args, void*));
            }
            mark = write;
            charcount -= 1;
            unget_unit(ch);
            while (true)
            {
                if (widthset != 0)
                {
                    const int before = width;
                    width -= 1;
                    if (before == 0)
                    {
                        break;
                    }
                }
                charcount += 1;
                ch = get_unit();
                if ((ch & 0xffffu) == 0xffffu)
                {
                    charcount -= 1;
                    unget_unit(ch);
                    break;
                }
                if (spec != 'c')
                {
                    if (spec == 's')
                    {
                        if ((ch >= 9u && ch <= 0x0du) || ch == 0x20u)
                        {
                            charcount -= 1;
                            unget_unit(ch);
                            break;
                        }
                    }
                    else
                    {
                        const unsigned bit = 1u << (ch & 7u);
                        const int table_byte = static_cast<signed char>(scan[ch >> 3]);
                        const int flipped = table_byte ^ static_cast<signed char>(invert);
                        if ((flipped & static_cast<int>(bit)) == 0)
                        {
                            charcount -= 1;
                            unget_unit(ch);
                            break;
                        }
                    }
                }
                if (suppress != 0)
                {
                    mark += 2;
                    continue;
                }
                if (wide_out == 0)
                {
                    const int produced = wctomb_byte(write, ch);
                    write += produced;
                }
                else
                {
                    *reinterpret_cast<char16_t*>(write) = static_cast<char16_t>(ch);
                    write += 2;
                }
            }
            if (write == mark)
            {
                break;
            }
            if (suppress == 0)
            {
                assigned += 1;
                if (spec != 'c')
                {
                    if (wide_out == 0)
                    {
                        *write = 0;
                    }
                    else
                    {
                        *reinterpret_cast<char16_t*>(write) = 0;
                    }
                }
            }
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (*fmt != static_cast<char16_t>(ch))
        {
            unget_unit(ch);
            break;
        }
        matched = static_cast<unsigned char>(matched - 1);
        if (!finish_field())
        {
            break;
        }
    }

    if (scan != nullptr)
    {
        crt_heap_free(scan);
    }
    va_end(args);
    if ((ch & 0xffffu) == 0xffffu && assigned == 0 && matched == 0)
    {
        return -1;
    }
    return assigned;
}

} // namespace scene_detail

} // namespace gbtg
