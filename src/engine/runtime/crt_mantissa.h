#pragma once
#include "gbtg/types.h"
#include <array>
namespace gbtg::scene_detail
{
using CrtMantissa = std::array<u32, 3>;
void crt_shift_extended_right_one(CrtMantissa& value); // Xbox 0x0016F5B2
void crt_multiply_extended(CrtMantissa& value, std::span<const u8> factor); // Xbox 0x0016F6AD
void crt_scale_extended_decimal(CrtMantissa& value, i32 exponent, i32 preserve_tail); // Xbox 0x0016F8CF
void crt_decimal_digits_to_extended(const signed char* digits, u32 count,
    CrtMantissa& output); // Xbox 0x0016F5DF
u32 crt_add_word(u32 first, u32 second, u32& output); // Xbox 0x0016F505
void crt_add_mantissa(CrtMantissa& output, const CrtMantissa& value); // Xbox 0x0016F526
void crt_shift_mantissa_left(CrtMantissa& value); // Xbox 0x0016F584
u32 crt_mantissa_tail_zero(const CrtMantissa& value, i32 bit); // Xbox 0x0016E44C
u32 crt_increment_mantissa(CrtMantissa& value, i32 bit); // Xbox 0x0016E47E
u32 crt_round_mantissa(CrtMantissa& value, i32 bit); // Xbox 0x0016E4CB
void crt_copy_mantissa(CrtMantissa& output, const CrtMantissa& value); // Xbox 0x0016E53D
u32 crt_mantissa_zero(const CrtMantissa& value); // Xbox 0x0016E558
void crt_shift_mantissa_right(CrtMantissa& value, i32 bits); // Xbox 0x0016E571
u32 crt_convert_extended(std::span<const u8> value, u32* output,
    const std::array<i32, 6>& format); // Xbox 0x0016E5EC
u32 crt_extended_to_double(std::span<const u8> value, double& output); // Xbox 0x0016E744
}
