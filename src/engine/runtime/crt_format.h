#pragma once

#include <cstdarg>

namespace gbtg::scene_detail
{
struct CrtOutputFile
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
int crt_format_bounded(char* output, int capacity, const char* format,
    std::va_list arguments); // Xbox 0x00166449
int crt_format_bounded_string(char* output, int capacity, const char* format,
    ...); // Xbox 0x001663B9
int crt_format_output(void* stream, const char* format, std::va_list arguments); // Xbox 0x00168FED
int crt_format_string(char* output, const char* format, ...); // Xbox 0x001661F9
int crt_format_arguments(char* output, const char* format,
    std::va_list arguments); // Xbox 0x001661A2
int crt_format_wide_output(void* stream, const char16_t* format, std::va_list arguments); // Xbox 0x0016993D
int crt_format_wide_string(char16_t* output, const char16_t* format, ...); // Xbox 0x00166A03
int crt_format_wide_arguments(char16_t* output, const char16_t* format,
    std::va_list arguments); // Xbox 0x00166FB1
int xapi_format_wide_arguments(char16_t* output, const char16_t* format,
    std::va_list arguments); // Xbox 0x0014BC66
int xapi_format_wide_string(char16_t* output, const char16_t* format, ...); // Xbox 0x0014BC7D
int crt_scan_wide_input(void* stream, const char16_t* format, std::va_list arguments); // Xbox 0x0016A17C
int crt_scan_wide_string(const char16_t* input, const char16_t* format, ...); // Xbox 0x00166A72
}
