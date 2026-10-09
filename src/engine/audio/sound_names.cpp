#include "sound_names.h"
#include "../runtime/crt_format.h"
#include <cstdarg>

namespace gbtg
{
// Xbox 0x00038070
void format_sound_name(char* output, const char* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    scene_detail::crt_format_bounded(output, 0x80, format, arguments);
    va_end(arguments);
    char value = *output;
    while (value != 0)
    {
        value = *output;
        if (value >= 'a' && value <= 'z')
        {
            *output = value - 0x20;
        }
        ++output;
        value = *output;
    }
}
}
