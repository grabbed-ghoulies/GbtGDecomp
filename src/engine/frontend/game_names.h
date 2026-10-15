#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct GameSelectControlServices;
bool game_name_is_blank(const char16_t* name); // Xbox 0x000FBE50, EBX the name: empty or all white space.
const char16_t* empty_slot_game_name(GameSelectControlServices& services); // Xbox 0x00141190
// 0: the name can be saved. 1 no name, 2 already a save, 3 empty, 4 white space, 5 the empty-slot name.
i32 check_new_game_name(const char16_t* name, GameSelectControlServices& services); // Xbox 0x001411C0, EAX
}
