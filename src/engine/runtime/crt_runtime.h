#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
namespace scene_detail
{
// Xbox 0x00166ECB
int crt_wide_string_length(const u16* text); // Xbox 0x00166ECB

// Xbox 0x00338180. _pctype. The image pointer is 0x00209788.
extern const u16* g_crt_pctype;

// Xbox 0x00338188. __mb_cur_max. The image dword is 1, so isspace takes the
// single-byte path. A signed compare greater than 1 uses _isctype.
extern int g_crt_mb_cur_max;

// Xbox 0x0033818C. C-locale decimal point. The image byte is '.'. _winput
// widens it with mbtowc (0x0016E07A).
extern char g_crt_decimal_point;

// Xbox 0x00166BF9
int crt_wide_character_type(u16 c, u16 mask); // Xbox 0x00166BF9

// Xbox 0x0016AE1F
int crt_character_type(int c, unsigned mask); // Xbox 0x0016AE1F

// Xbox 0x003381A0. The image points every slot at _fptrap (0x0016F08C),
// which exits with code 2. _fpmath overwrites the six dwords.
extern u32 g_cfltcvt_tab[6];

// Xbox 0x0016DFB5
u32 crt_put_wide_character_unlocked(u32 unit, void* stream); // Xbox 0x0016DFB5

// Xbox 0x0016E07A
int crt_byte_to_wide_character(char16_t* dest, const char* src, int count); // Xbox 0x0016E07A

// Xbox 0x00167F2E
void crt_heap_free(void* block); // Xbox 0x00167F2E

// Win32 code stored by 0x0014B00C. 0x0014B03A maps the NTSTATUS, and
// 0x0014AFE4 reads this slot. XCreateSaveGame (0x0014C70F) treats 0xB7 as
// "the directory is already there".
extern u32 g_xapi_last_error;

} // namespace scene_detail
} // namespace gbtg
