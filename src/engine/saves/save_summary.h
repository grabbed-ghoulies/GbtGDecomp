#pragma once

namespace gbtg
{
struct GameSelectControlServices;
// Refreshes the profile from the current game's save record (DAT_00506C90 names it): the five times
// DAT_00506C78..DAT_00506C88, the progress and DAT_00506C54 blocks, the unlock bits, then the save's tail.
void refresh_runtime_save_summary(GameSelectControlServices& services); // Xbox 0x000F2F00
}
