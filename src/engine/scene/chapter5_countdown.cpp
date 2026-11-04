#include "chapter5_countdown.h"
#include "scene.h"
#include "scene_controls.h"
#include "../audio/bank_streams.h"
#include "../entities/body_timers.h"
#include "../frontend/dialogue_activation.h"
#include "../frontend/scoreboard_hud.h"
#include "../frontend/scoreboard_messages.h"
#include "../scripts/script_event_sequence.h"

#include <string>

namespace gbtg
{
i32 g_chapter5_mode = 0;
u32 g_chapter5_gold = 0;
float g_chapter5_fail_wait = 0.f;
u32 g_chapter5_message = 0;
Chapter5CountdownServices* g_chapter5_countdown = nullptr;

Chapter5CountdownServices& chapter5_countdown(const char* reader)
{
    if (g_chapter5_countdown == nullptr)
    {
        throw Error(std::string(reader) + " reads the chapter 5 countdown (DAT_00506C58) outside a native scene");
    }
    return *g_chapter5_countdown;
}

namespace
{
RuntimeScoreboard& board_of(Chapter5CountdownServices& context)
{
    RuntimeScoreboard* board = context.hud.globals.score_board; // DAT_00545AB0.
    if (board == nullptr)
    {
        throw Error("the chapter 5 countdown reads the score board (DAT_00545AB0) before it exists");
    }
    return *board;
}

// Modes 0 and 3, shared by 0x000F27C0 and 0x000F25E0.
void clear_countdown(Chapter5CountdownServices& context)
{
    fade_script_event_icon(context.message); // 0x0012FDC0, EDX DAT_00506C68
    context.time = -1.f; // DAT_0032B120.
    context.message = 0;
}

void fail_chapter(Chapter5CountdownServices& context)
{
    enqueue_runtime_dialogue_key("chapter5_failchapter", 0, context.dialogue); // 0x000582A0, ECX 0x00325F20
    ++context.pause_requests; // DAT_0050E794.
    context.fail_wait = 1.f; // DAT_0032B0F8.
    context.pause(u8(0xFFFF), 0); // 0x00117400
}
}

// Xbox 0x000F2580
void show_runtime_chapter5_countdown(Chapter5CountdownServices& context)
{
    if ((context.scene_flags & 0x10) != 0)
    {
        return;
    }
    create_runtime_score_board(context.hud); // 0x0012FA50
    context.message = add_runtime_scoreboard_message(5, 0x4C, 4, 0, 0, 0, 0, board_of(context),
        context.board_resources); // 0x001316F0, EBX DAT_00545AB0
    if (context.mode != 0)
    {
        set_runtime_scoreboard_count(context.message, i32(context.time), board_of(context),
            context.preset); // 0x001318A0, ECX the message, EDX the seconds (CVTTSS2SI)
    }
}

// Xbox 0x000F2920
void run_runtime_chapter5_clock(Chapter5CountdownServices& context)
{
    const RuntimeActorClock& clock = context.clock;
    if (clock.block_decay == 0 && clock.decay_enabled[clock.decay_index] != 0 && !(context.time < 0.f))
    {
        context.time = context.time - runtime_frame_step(clock);
    }
    set_runtime_scoreboard_count(context.message, i32(context.time), board_of(context),
        context.preset); // 0x001318A0, ECX DAT_00506C68, EDX the seconds (CVTTSS2SI)
}

// Xbox 0x000F25E0
void tick_runtime_chapter5_countdown(Chapter5CountdownServices& context)
{
    if ((context.scene_flags & 0x10) != 0)
    {
        return;
    }
    switch (context.mode)
    {
    case 1:
        run_runtime_chapter5_clock(context); // 0x000F2920
        if (context.time < 11.f && context.mode != 2) // DAT_0032B690.
        {
            context.mode = 2;
            set_runtime_scoreboard_message_word_74(context.message, 1, board_of(context)); // 0x0012FB40
        }
        return;
    case 2:
        run_runtime_chapter5_clock(context); // 0x000F2920
        if (context.time < 0.001f && context.mode != 3) // DAT_0032B1A4.
        {
            context.mode = 3;
            fail_chapter(context);
        }
        return;
    case 3:
        if ((context.runtime_flags & 0x40) != 0)
        {
            return;
        }
        tick_runtime_body_frame_timer(context.fail_wait, context.clock); // 0x0012B2A0, EAX DAT_00506C64
        if (!(context.fail_wait < 0.001f)) // DAT_0032B1A4.
        {
            return;
        }
        if (context.scene_end_flag()) // 0x00049D10
        {
            context.runtime_flags |= 0x200000;
        }
        else
        {
            request_scene_aid("aid_script_ghoulies_chapter5_scene13_1bookfail", 1); // 0x0011EB30, EAX 0x0030C1C8
        }
        if (context.mode != 0)
        {
            context.mode = 0;
            clear_countdown(context);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000F27C0
void set_runtime_chapter5_countdown_mode(i32 mode, Chapter5CountdownServices& context)
{
    while (context.mode != mode)
    {
        context.mode = mode;
        switch (mode)
        {
        case 0:
            clear_countdown(context);
            return;
        case 1:
            // Story bits 0x33..0x3C clear.
            context.story_bits[6] = u8(context.story_bits[6] & 0x07);
            context.story_bits[7] = u8(context.story_bits[7] & 0xE0);
            return;
        case 2:
            set_runtime_scoreboard_message_word_74(context.message, 1, board_of(context)); // 0x0012FB40
            return;
        case 3:
            fail_chapter(context);
            return;
        case 4:
            context.gold = 1; // DAT_00506C5C.
            request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_GOLD", 10, context.streams); // 0x00038720
            mode = 0;
            if (context.mode == 0)
            {
                return;
            }
            break;
        default:
            return;
        }
    }
}

// Xbox 0x000F2780: the countdown stops when it runs (0x0011CFB0's leaves).
void stop_runtime_chapter5_countdown(Chapter5CountdownServices& context)
{
    if (context.mode != 0) // DAT_00506C58
    {
        context.mode = 0;
        clear_countdown(context);
    }
}

// Xbox 0x000F2740
void finish_runtime_chapter5_countdown(Chapter5CountdownServices& context)
{
    if (context.mode == 4)
    {
        return;
    }
    context.mode = 4;
    context.gold = 1; // DAT_00506C5C.
    request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_GOLD", 10, context.streams); // 0x00038720
    set_runtime_chapter5_countdown_mode(0, context); // The tail jump into 0x000F27C0 with EAX 0.
}

// Xbox 0x00082D00
bool construct_runtime_chapter5_countdown_control(Chapter5CountdownServices& context)
{
    show_runtime_chapter5_countdown(context); // 0x000F2580
    return true;
}

// Xbox 0x00082D10
void tick_runtime_chapter5_countdown_control(SceneControlBase& state, Chapter5CountdownServices& context)
{
    tick_runtime_chapter5_countdown(context); // 0x000F25E0
    tick_runtime_scene_control(state, context.clock); // The inlined 0x00115E30.
}
}
