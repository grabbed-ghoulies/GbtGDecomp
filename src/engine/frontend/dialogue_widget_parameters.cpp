#include "dialogue_widget_parameters.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00142DF0
void copy_dialogue_widget_parameters(std::span<u8> destination, const u8* source)
{
    if (source != nullptr && *source != 0)
    {
        const u8* cursor = source;
        u8* output = destination.data();
        u8 value;
        do
        {
            value = *cursor;
            *output = value;
            ++cursor;
            ++output;
        }
        while (value != 0);
        u8* record = destination.data() + 0x40;
        const u8* text = source + 0x44;
        const u8* source_record = source + 0x40;
        i32 count = 10;
        do
        {
            std::memcpy(record, source_record, 4);
            std::memcpy(record + 0x44, text + 0x40, 4);
            cursor = text;
            output = record + 4;
            do
            {
                value = *cursor;
                *output = value;
                ++cursor;
                ++output;
            }
            while (value != 0);
            record += 0x48;
            source_record += 0x48;
            text += 0x48;
            --count;
        }
        while (count != 0);
    }
}
}
