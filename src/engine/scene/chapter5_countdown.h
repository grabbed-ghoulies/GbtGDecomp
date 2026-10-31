#pragma once
#include "../entities/actor_clock.h"
#include "gbtg/object_factory.h"

#include <functional>
#include <span>

namespace gbtg
{
struct AudioBankStreams;
struct RuntimeDialogueActivationContext;
struct RuntimeScoreboardHud;
struct RuntimeScoreboardResources;
// Chapter 5's countdown (DAT_00506C58 its mode, DAT_00506C60 the seconds left). Script type 0x89 sets the time and
// mode 1 (0x000F27C0); at 11 seconds the scoreboard clock warns (mode 2), and at none the chapter fails (mode 3:
// "chapter5_failchapter", the scene paused, then a second later chapter5_scene13_1bookfail). Mode 4 plays the gold
// challenge ditty. entitySceneControlChapter5Countdown (factory type 0x9D, getter 0x00082CB0, vtable 0x005055D8)
// shows the clock (0x000F2580) and runs it (0x000F25E0).
struct Chapter5CountdownServices
{
    const u8& scene_flags; // **(DAT_00506C74+8); bit 0x10 hides the countdown.
    i32& mode; // DAT_00506C58.
    float& time; // DAT_00506C60.
    float& fail_wait; // DAT_00506C64.
    u32& message; // DAT_00506C68, the clock's scoreboard message.
    u32& gold; // DAT_00506C5C.
    std::span<u8> story_bits; // **(DAT_00506C6C+8).
    u32& runtime_flags; // DAT_0050EA9C.
    i32& pause_requests; // DAT_0050E794.
    const RuntimeActorClock& clock;
    RuntimeScoreboardHud& hud; // 0x0012FA50 and DAT_00545AB0.
    RuntimeScoreboardResources& board_resources;
    std::span<const u8> preset; // 0x00314170, 0x001318A0's digits.
    RuntimeDialogueActivationContext& dialogue; // 0x000582A0.
    AudioBankStreams& streams; // 0x00038720.
    std::function<void(u8, i32)> pause; // 0x00117400.
    std::function<bool()> scene_end_flag; // 0x00049D10.
};
// DAT_00506C58, DAT_00506C5C, DAT_00506C64 and DAT_00506C68: game globals that outlive a scene, so the countdown
// runs on through chapter 5's scenes. DAT_00506C60 is the script's g_script_6c60.
extern i32 g_chapter5_mode;
extern u32 g_chapter5_gold;
extern float g_chapter5_fail_wait;
extern u32 g_chapter5_message;
// The scene binds it while it runs; null outside a native scene.
extern Chapter5CountdownServices* g_chapter5_countdown;
Chapter5CountdownServices& chapter5_countdown(const char* reader);
void show_runtime_chapter5_countdown(Chapter5CountdownServices& context); // Xbox 0x000F2580
void tick_runtime_chapter5_countdown(Chapter5CountdownServices& context); // Xbox 0x000F25E0
void run_runtime_chapter5_clock(Chapter5CountdownServices& context); // Xbox 0x000F2920
void set_runtime_chapter5_countdown_mode(i32 mode, Chapter5CountdownServices& context); // Xbox 0x000F27C0, EAX mode
// Script type 0x9A: the countdown beaten. Mode 4 (DAT_00506C5C set, the gold challenge ditty), then mode 0.
void finish_runtime_chapter5_countdown(Chapter5CountdownServices& context); // Xbox 0x000F2740
void stop_runtime_chapter5_countdown(Chapter5CountdownServices& context); // Xbox 0x000F2780
// The control's construct (0x000F2580, then 1) and update (0x000F25E0, then the base 0x00115E30's +0x20 time).
bool construct_runtime_chapter5_countdown_control(Chapter5CountdownServices& context); // Xbox 0x00082D00
void tick_runtime_chapter5_countdown_control(std::span<u8> state,
    Chapter5CountdownServices& context); // Xbox 0x00082D10
}
