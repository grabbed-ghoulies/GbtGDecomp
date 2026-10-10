#include "script_table_lookup.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00027510
const u8* find_runtime_script_table_row(const RuntimeScriptLookupTable* table,
    u16 key, u32& output)
{
    if (table == nullptr)
    {
        return nullptr;
    }
    i32 high = std::bit_cast<i32>(static_cast<u32>(table->count) - 1);
    i32 low = 0;
    if (high >= 0)
    {
        do
        {
            const i32 middle = std::bit_cast<i32>(static_cast<u32>(low) + static_cast<u32>(high)) >> 1;
            const u8* row = table->rows + static_cast<u32>(middle) * 0x108u;
            u16 row_key;
            std::memcpy(&row_key, row, 2);
            if (row_key < key)
            {
                low = middle + 1;
            }
            else
            {
                if (row_key <= key)
                {
                    output = 0;
                    return row;
                }
                high = middle - 1;
            }
        }
        while (low <= high);
    }
    output = 0;
    return nullptr;
}
}
