#pragma once
#include "gameselect_rows.h"

namespace gbtg
{
// The chapter and scene of a script name, as the game-select screens read a save's script.
i32 gameselect_script_chapter(const char* script, const GameSelectControlServices& services); // Xbox 0x0004B4D0, EAX
i32 gameselect_scenes_before(i32 chapter, const GameSelectControlServices& services); // Xbox 0x0004B530, EDX
i32 gameselect_script_scene(const char* script, const GameSelectControlServices& services); // Xbox 0x0011EBC0, EBX
i32 gameselect_book_scene(const char* script, const GameSelectControlServices& services); // Xbox 0x0011EC70, EAX
i32 gameselect_scene_ordinal(const char* script, const GameSelectControlServices& services); // Xbox 0x0011ED30, EAX
// The scene's localized name (aid_loctext_ghoulies_script_scenenames, "scenenames"), in retail into DAT_00507E78.
const char16_t* gameselect_scene_name(i32 ordinal, std::array<char16_t, 0x800>& output,
    GameSelectControlServices& services); // Xbox 0x0011ED80
}
