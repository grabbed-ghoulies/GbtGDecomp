#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct RuntimeScriptLookupTable
{
    i32 count; // +0.
    const u8* rows; // +4, retail stride 0x108.
};
const u8* find_runtime_script_table_row(const RuntimeScriptLookupTable* table,
    u16 key, u32& output); // Xbox 0x00027510
}
