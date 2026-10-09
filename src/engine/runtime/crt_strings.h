#pragma once
#include "gbtg/types.h"
namespace gbtg::scene_detail
{
void crt_unsigned_to_string(u32 value, char* output, u32 radix, i32 negative); // Xbox 0x00167B2C
char* crt_integer_to_string(i32 value, char* output, i32 radix); // Xbox 0x00167B6A
char* crt_copy_string_count(char* destination, const char* source, u32 count); // Xbox 0x001665E0
char* crt_append_string_count(char* destination, const char* source, u32 count); // Xbox 0x001664A0
}
