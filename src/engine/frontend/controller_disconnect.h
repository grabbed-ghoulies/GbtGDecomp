#pragma once

#include "gbtg/types.h"

#include <span>

namespace gbtg
{
struct AudioBankStreams;
struct RuntimeDialogueInputState;
struct RuntimeDialogueTextPool;
struct RuntimeHudLayout;
struct TransitionBookServices;

// The controller prompt (DAT_004DEA20 .. DAT_004DEA48), ticked at the end of every frame (0x00033480). Phase 4
// waits for a pad to be connected; phase 1 watches the input stack's top device and opens the prompt when it is
// unplugged or 0x000335A0 asks; phase 2 shows "please reconnect" with the game paused and silenced until Start is
// pressed on the prompt's own device; phase 3 lets the line fade, then the game runs on (phase 1).
struct ControllerDisconnect
{
    i32 silenced = 0; // DAT_004DEA20
    i32 hold = 0; // DAT_004DEA24, nonzero skips phase 1's device check
    std::span<u8> input; // DAT_004DEA28, the prompt's input device
    u32 input_address = 0; // Host: the word the input entry (0x00109340) names the device by.
    RuntimeDialogueTextPool* text = nullptr; // DAT_004DEA2C
    u32 line = 0xFFFFFFFFu; // DAT_004DEA30, the prompt's text record
    i32 phase = 4; // DAT_004DEA34
    i32 requested = 0; // DAT_004DEA38, set by 0x000335A0
    float x = 0.f; // DAT_004DEA3C, the HUD centre (DAT_00509730) at boot
    float y = 0.f; // DAT_004DEA40, DAT_00509734 at boot
    i32 movies_held = 0; // DAT_004DEA44, 0x00106E90 paused the movie events
    i32 was_paused = 0; // DAT_004DEA48, DAT_0050EA9C bit 0x2000 when the prompt opened
};
extern ControllerDisconnect g_controller_disconnect;

// What the prompt reaches outside itself; the scene's actor services bind it.
struct ControllerDisconnectServices
{
    TransitionBookServices& books; // Text pools, fonts, localtext, resources and the input stack's contexts.
    AudioBankStreams& banks; // DAT_004DF370
    RuntimeDialogueInputState& input; // The device table (DAT_0032E3B4), DAT_0050E0E8, DAT_0050E0EC, DAT_0050E0F8.
    u32& flags; // DAT_0050EA9C
};

// fs_init 0x00033980's stores after its 0x00033030: no text, line -1, phase 4, the HUD centre.
void boot_controller_disconnect(const RuntimeHudLayout& hud);
void tick_controller_disconnect(ControllerDisconnectServices& services); // Xbox 0x00033480
void set_controller_disconnect_phase(i32 phase, ControllerDisconnectServices& services); // Xbox 0x000337B0, EAX
void silence_for_controller_prompt(ControllerDisconnectServices& services); // Xbox 0x00033720
void restore_after_controller_prompt(ControllerDisconnectServices& services); // Xbox 0x00033750
// EDI nonzero pauses each tracked bank of id 0x12, zero resumes them; nothing when no bank is tracked.
void hold_silenced_audio_banks(i32 hold, AudioBankStreams& banks); // Xbox 0x00038A70, EDI hold
// The prompt's line for the device's port: Port1 .. Port4 or PortAny of "noControllerText".
void write_controller_prompt_text(std::span<const u8> device, char16_t* output,
    ControllerDisconnectServices& services); // Xbox 0x00033610, ECX device, EAX output
u32 top_input_entry_device(const RuntimeDialogueInputState& input); // Xbox 0x001095F0
}
