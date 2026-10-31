#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct GameSelectControlServices;
struct RuntimeProfile;
struct RuntimeProfileBlock;
struct SaveGameContext;
// What the scene saves (0x000F2C40, 0x000F2CB0, 0x000F2D10, 0x000F2E80) read beside the profile: the save code's
// context with the held scene order (DAT_00511B64), the chapter lookups' executable tables and the scene's globals.
struct RuntimeSceneSave
{
    RuntimeProfile& profile;
    SaveGameContext& saves;
    GameSelectControlServices& gameselect; // 0x0004B4D0, 0x0011EC70, DAT_002FE698 and the held order (0x001234B0).
    const char* scene; // DAT_00510E58, the scene's script.
    u32 gold; // DAT_00506C5C, the chapter 5 countdown beaten.
    u32 runtime_flags; // DAT_0050EA9C
    float scene_time; // DAT_004312D0
};
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
// A challenge game's (scene flag 8 or DAT_0050EA9C 0x200000) steps: DAT_004E1740 the scene's held index, at most
// chapter 5 scene 1's, outside game select.
void clamp_runtime_challenge_steps(RuntimeSceneSave& context); // Xbox 0x00049D40
// A challenge game given up: its steps clamped (0x00049D40), scene flag 8 cleared, challenge 21's order
// (DAT_004E1748) emptied and the boot scene order (*DAT_00430CF8) held again.
void end_runtime_challenge_game(RuntimeSceneSave& context); // Xbox 0x00049C30
// The scene's save as it starts: with scene flag 2 its script and times (0x000F3080), the challenges, chapter 5's
// book scene, the progress outside chapter 5, then the save queued for writing.
void save_runtime_scene_start(RuntimeSceneSave& context); // Xbox 0x000F2C40
// The pause's quit (0x0011E1C0): as 0x000F2C40 with 0x000F3130's three times and no book scene.
void save_runtime_quit_scene(RuntimeSceneSave& context); // Xbox 0x000F2CB0
// A restarted scene (DAT_00430D0C 1): the times without the script (0x000F31C0), no book scene.
void save_runtime_restart_scene(RuntimeSceneSave& context); // Xbox 0x000F2D10
// A challenge game: its steps and time (+0x1A0, +0x1A4), progress and book scene.
void save_runtime_challenge_scene(RuntimeSceneSave& context); // Xbox 0x000F2E80
// 0x00117000's tail: the save its scene start chooses (DAT_00430D0C the chapter mode).
void save_runtime_scene(RuntimeSceneSave& context, i32 chapter_mode);
}
