#pragma once
#include "crt_mantissa.h"
namespace gbtg::scene_detail
{
u32 crt_parse_extended(CrtMantissa& output, const char*& end, const char* text,
    i32 preserve_tail, i32 scale, i32 decimal_position, i32 implicit_exponent); // Xbox 0x0016E7CA
struct CrtDecimalResult
{
    u32 flags;
    i32 consumed;
    u32 retained[2];
    double value;
};
CrtDecimalResult* crt_parse_decimal_result(CrtDecimalResult& output, const char* text); // Xbox 0x0016AD9A
double crt_ascii_to_double(const char* text); // Xbox 0x00166B9D
}
