#include "shape_copy.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x0010AA60
void copy_actor_shape(u8* output, const u8* source)
{
    u32 remaining = 0x20;
    if (output != source)
    {
        if ((reinterpret_cast<std::uintptr_t>(output) & 7) == 0 &&
            (reinterpret_cast<std::uintptr_t>(source) & 7) == 0)
        {
            i32 count = 4;
            do
            {
                u32 value;
                std::memcpy(&value, source, 4);
                std::memcpy(output, &value, 4);
                std::memcpy(&value, source + 4, 4);
                std::memcpy(output + 4, &value, 4);
                output += 8;
                source += 8;
                remaining -= 8;
                --count;
            }
            while (count != 0);
        }
        if ((reinterpret_cast<std::uintptr_t>(output) & 3) == 0 &&
            (reinterpret_cast<std::uintptr_t>(source) & 3) == 0 && remaining > 3)
        {
            u32 count = remaining >> 2;
            do
            {
                u32 value;
                std::memcpy(&value, source, 4);
                std::memcpy(output, &value, 4);
                output += 4;
                source += 4;
                remaining -= 4;
                --count;
            }
            while (count != 0);
        }
        if (remaining != 0)
        {
            do
            {
                *output = *source;
                ++output;
                ++source;
                --remaining;
            }
            while (remaining != 0);
        }
    }
}
}
