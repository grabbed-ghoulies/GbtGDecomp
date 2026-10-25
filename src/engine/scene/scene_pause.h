#pragma once
#include "gbtg/types.h"

#include <array>
#include <functional>
#include <span>

namespace gbtg
{
struct AudioBankStreams;
struct RuntimeDialogueActivationContext;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeHudLayout;
struct RuntimeTransitionBook;
struct TransitionBookServices;
// The scene's pause screen (DAT_00510CF0 .. DAT_00510E50). Start in a running scene asks for scene state 6
// (0x0011E7F0); entering it (0x0011CE10) opens the book on the scene's clip with "Paused", the chapter and scene
// line and the button prompts. Start again closes the book (0x0011DEE0), and once it has finished the scene runs on
// (0x0011EBA0, then 0x0011CE40 as the state leaves 6). The phase is 1 closed, 2 opening, 3 open and 6 closing;
// 4, 5 and 7 .. 9 have no native caller.
struct ScenePause
{
    i32 chapter = 0; // DAT_00510CF0, 0x0004B4D0 of the scene.
    i32 phase = 0; // DAT_00510CF4
    RuntimeDialogueTextPool* text = nullptr; // DAT_00510CF8
    std::span<u8> input; // DAT_00510CFC, the pause's input device (0x00107D60).
    u32 input_address = 0; // Host: the word the input entry (0x00109340) names the device by.
    std::array<char16_t, 0x80> title{}; // DAT_00510D00, the scene's name (0x0011DCF0, "No Name Available").
    u32 line_text = 0; // DAT_00510E00, the chapter and scene line.
    u32 paused_text = 0; // DAT_00510E04, "Paused".
    u32 third_text = 0; // DAT_00510E08, ended with the others; nothing native sets it.
    Vec3 volume_low{}; // DAT_00510E0C, the background volume (0x00118CB0) while paused.
    Vec3 volume_high{}; // DAT_00510E18
    RuntimeTransitionBook* book = nullptr; // DAT_00510E24
    i32 choice = 0; // DAT_00510E28, what the closed book leads to (1 resumes).
    i32 prompt = 0; // DAT_00510E2C, the prompt row (0x0030F948, stride 0x310), -1 for none.
    RuntimeDialogueWidget* widget = nullptr; // DAT_00510E30, the menu text prompts.
    float e38 = 0.f; // DAT_00510E38, cleared on opening.
    u32 seed = 0; // DAT_00510E3C, the random word the text and prompt pools keep at +0x1C.
    std::span<const u8> icon; // DAT_00510E40, the bonus book icon.
    std::array<float, 2> icon_position{}; // DAT_00510E44
    std::array<float, 2> icon_scale{}; // DAT_00510E4C
    std::array<char16_t, 0x100> paused{}; // DAT_00510AE8, "Paused" from aid_loctext_ghoulies_pause_pause.
};
extern ScenePause g_scene_pause;
// What the pause reaches outside itself.
struct ScenePauseServices
{
    TransitionBookServices& books; // The book, its menu widget, input, text pools and the executable.
    RuntimeDialogueActivationContext& dialogue; // 0x000582A0, 0x000584D0, DAT_004EE1EC, DAT_004E173C.
    AudioBankStreams& banks; // 0x00038910, 0x00038460 ..
    std::function<void(float)> audio_level; // 0x00038110
    std::function<void()> update_dialogue; // 0x00057E60
    std::function<i32(const char*)> chapter; // 0x0004B4D0
    std::function<i32(const char*)> script_scene; // 0x0011EBC0
    std::function<i32(const char*)> book_scene; // 0x0011EC70
    std::function<std::span<const u8>()> challenge_bits; // *(DAT_00506C54+8)
    const i32& event_state; // DAT_005064D8
    const i32& frontend_state; // DAT_004E17C8
    const RuntimeHudLayout& hud; // DAT_00509730 .. DAT_0050974C
    const char* scene; // DAT_00510E58, the scene record (its name at +0).
    i32& queued_state; // DAT_00510F60, scene record +0x108.
};
void request_scene_resume(ScenePauseServices& services); // Xbox 0x0011EBA0
void set_scene_pause_phase(i32 phase, ScenePauseServices& services); // Xbox 0x0011D490, EAX the phase
void select_scene_pause_prompt(i32 prompt, ScenePauseServices& services); // Xbox 0x0011DD10, EAX the row
void choose_scene_pause_prompt(ScenePauseServices& services); // Xbox 0x0011DD90
bool scene_pause_prompt_shown(); // Xbox 0x0011DEA0
void close_scene_pause(ScenePauseServices& services); // Xbox 0x0011DEE0
i32 scene_bonus_book_index(ScenePauseServices& services); // Xbox 0x0011ECF0, EAX the scene record
void enter_scene_pause(ScenePauseServices& services); // Xbox 0x0011CE10
void leave_scene_pause(ScenePauseServices& services); // Xbox 0x0011CE40
void tick_scene_pause(ScenePauseServices& services); // Xbox 0x0011CFB0
void draw_scene_pause(ScenePauseServices& services); // Xbox 0x0011DB50
void reset_scene_pause(ScenePauseServices& services); // Xbox 0x0011CD90
void name_scene(const char16_t* name); // Xbox 0x0011DCF0, EAX the name or null
// The prompt rows' buttons (row +0x84 code pointers, 0x0030F948 .. 0x00311AF8).
void confirm_scene_pause_quit(ScenePauseServices& services); // Xbox 0x0011DFA0, pause_Quit
void show_scene_pause_stats(ScenePauseServices& services); // Xbox 0x0011E110, pause_Stats
void show_scene_pause_map(ScenePauseServices& services); // Xbox 0x0011E140, pause_Map
void show_scene_pause_rules(ScenePauseServices& services); // Xbox 0x0011E150, pause_EventRules
void cancel_scene_pause_confirm(ScenePauseServices& services); // Xbox 0x0011E1A0, pause_No
void quit_scene_from_pause(ScenePauseServices& services); // Xbox 0x0011E1C0, pause_Yes (quit)
void restart_scene_from_pause(ScenePauseServices& services); // Xbox 0x0011E320, pause_Yes (restart)
void leave_scene_pause_map(ScenePauseServices& services); // Xbox 0x0011E3E0, pause_MapReturn
void confirm_scene_pause_restart(ScenePauseServices& services); // Xbox 0x0011E410, pause_RestartChallenge
}
