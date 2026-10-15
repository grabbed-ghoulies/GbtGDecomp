#include "script_chapter.h"

namespace gbtg
{
// Xbox 0x0004B4D0. Suffixes at 0x004232F4 map through 0x002FE698.
int script_chapter_index(std::string_view script)
{
    constexpr std::string_view prefix = "aid_script_ghoulies_chapter";
    constexpr std::string_view suffixes[] = {"void", "1", "2a", "2b", "2c", "3a", "3b", "3c", "4", "5"};
    if (script.compare(0, 0x1b, prefix) == 0)
    {
        for (int i = 1; i < 10; ++i)
        {
            if (script.substr(0x1b).starts_with(suffixes[i]))
            {
                return i;
            }
        }
    }
    return 0;
}
}
