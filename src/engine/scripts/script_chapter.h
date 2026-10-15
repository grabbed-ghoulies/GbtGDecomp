#pragma once
#include <string_view>

namespace gbtg
{
// Xbox 0x0004B4D0. Suffixes at 0x004232F4 map through 0x002FE698.
int script_chapter_index(std::string_view script); // Xbox 0x0004B4D0
}
