#pragma once

namespace gbtg
{
struct GameSelectControlServices;
struct RuntimeProfile;
struct RuntimeProfileBlock;
struct SaveGameContext;
// Refreshes the profile from the current game's save record (DAT_00506C90 names it): the five times
// DAT_00506C78..DAT_00506C88, the progress and DAT_00506C54 blocks, the unlock bits, then the save's tail.
void refresh_runtime_save_summary(GameSelectControlServices& services); // Xbox 0x000F2F00
void load_runtime_save_progress(const char16_t* name, GameSelectControlServices& services); // Xbox 0x000F3020, EAX
// The current game's challenge bits (DAT_00506C54) into its save (DAT_00506C90), through a copy of the block.
void store_runtime_save_challenges(RuntimeProfile& profile); // Xbox 0x000F3230
// EBX the block: cleared, then the current game's progress bits 0..0x3D (DAT_00506C6C).
void copy_runtime_progress_bits(RuntimeProfileBlock& block, const RuntimeProfile& profile); // Xbox 0x000F2A50
// The current game's progress bits into its save (record +0x1A8), through a kind-0x3E block.
void store_runtime_save_progress(RuntimeProfile& profile); // Xbox 0x000F3290
// The end of the game (the final score's construct): with a current game, scene flag 2 stores the five times
// (DAT_00506C78..DAT_00506C88) and the finished flag in its save; then its challenges and progress, the +0x184
// word cleared, and the save queued for writing.
void finish_runtime_game_save(RuntimeProfile& profile, SaveGameContext& saves); // Xbox 0x000F2D70
}
