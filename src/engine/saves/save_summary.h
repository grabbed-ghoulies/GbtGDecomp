#pragma once

namespace gbtg
{
struct GameSelectControlServices;
struct RuntimeProfile;
// Refreshes the profile from the current game's save record (DAT_00506C90 names it): the five times
// DAT_00506C78..DAT_00506C88, the progress and DAT_00506C54 blocks, the unlock bits, then the save's tail.
void refresh_runtime_save_summary(GameSelectControlServices& services); // Xbox 0x000F2F00
void load_runtime_save_progress(const char16_t* name, GameSelectControlServices& services); // Xbox 0x000F3020, EAX
// The current game's challenge bits (DAT_00506C54) into its save (DAT_00506C90), through a copy of the block.
void store_runtime_save_challenges(RuntimeProfile& profile); // Xbox 0x000F3230
}
