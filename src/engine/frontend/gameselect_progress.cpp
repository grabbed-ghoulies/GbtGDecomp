#include "gameselect_progress.h"
#include "../scene/executable_resources.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace gbtg
{
namespace
{
u32 exe_word(const GameSelectControlServices& s, u32 address)
{
    return read_u32(s.image.memory(address, 4), 0);
}
}

// Xbox 0x0004B4D0, EAX the script: its chapter (1..9 from the names at 0x004232F8), or 0.
i32 gameselect_script_chapter(const char* script, const GameSelectControlServices& s)
{
    if (std::strncmp("aid_script_ghoulies_chapter", script, 0x1B) != 0)
    {
        return 0;
    }
    for (i32 index = 1; index < 10; ++index)
    {
        const char* name = s.image.string(exe_word(s, 0x004232F4 + u32(index) * 4));
        if (std::strncmp(name, script + 0x1B, std::strlen(name)) == 0)
        {
            return index;
        }
    }
    return 0;
}

// Xbox 0x0004B530, EDX the chapter: the scenes of the chapters before it (DAT_002FE670 counts).
i32 gameselect_scenes_before(i32 index, const GameSelectControlServices& s)
{
    i32 total = 0;
    for (i32 count = 1; count < index; ++count)
    {
        total += i32(exe_word(s, 0x002FE670 + u32(count) * 4));
    }
    return total;
}

// Xbox 0x0011EBC0, EBX the script: the scene number after "aid_script_ghoulies_chapter<name>_scene".
i32 gameselect_script_scene(const char* script, const GameSelectControlServices& s)
{
    const i32 index = gameselect_script_chapter(script, s); // 0x0004B4D0
    if (index == 0)
    {
        return 0;
    }
    std::string prefix = s.image.string(exe_word(s, 0x00430CFC)); // "aid_script_ghoulies_chapter"
    prefix += s.image.string(exe_word(s, 0x004232F4 + u32(index) * 4));
    prefix += s.image.string(exe_word(s, 0x00430D00)); // "_scene"
    return i32(std::atol(script + prefix.size()));
}

// Xbox 0x0011EC70, EAX the script: the scene's number within its book (chapters 2..4 and 5..7 count on).
i32 gameselect_book_scene(const char* script, const GameSelectControlServices& s)
{
    const i32 index = gameselect_script_chapter(script, s); // 0x0004B4D0
    i32 scene = gameselect_script_scene(script, s); // 0x0011EBC0
    i32 first;
    switch (index)
    {
    case 2:
    case 3:
    case 4:
        first = 2;
        break;
    case 5:
    case 6:
    case 7:
        first = 5;
        break;
    default:
        return scene;
    }
    i32 total = 0;
    for (i32 count = first; count < index; ++count)
    {
        total += i32(exe_word(s, 0x002FE670 + u32(count) * 4));
    }
    return scene + total;
}

// Xbox 0x0011ED30, EAX the script: the scene's ordinal over all chapters, or -1.
i32 gameselect_scene_ordinal(const char* script, const GameSelectControlServices& s)
{
    const i32 scene = gameselect_script_scene(script, s); // 0x0011EBC0
    const i32 index = gameselect_script_chapter(script, s); // 0x0004B4D0
    if (index == 0)
    {
        return -1;
    }
    if (scene != 0)
    {
        return gameselect_scenes_before(index, s) - 1 + scene;
    }
    return gameselect_scenes_before(index, s); // 0x0004B530
}
}
