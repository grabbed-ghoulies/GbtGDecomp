#pragma once
#include "scoreboard_update.h"
#include "dialogue_draw_outer.h"

namespace gbtg
{
// The views the scoreboard draws need beyond RuntimeScoreboardHud: the HUD sprite queue the views drain, the
// blink clock and the brew flag.
struct RuntimeScoreboardDraw
{
    RuntimeDialogueSpriteQueue& sprites; // DAT_00511210 / DAT_00511A10.
    const float& hold_time; // DAT_004312CC, the blink clock.
    const u32& brew_accepted; // DAT_00430D98.
    // The draw state 0x0012F8F0 draws a glyph with at once (0x0005CDC0 / 0x0005CA60) when its EAX is set.
    RuntimeDialogueDrawContext* immediate = nullptr;
};

// ECX scale, EDX position, XMM0 angle: one record on the HUD sprite queue.
void queue_runtime_hud_sprite(const u8* texture, const float* position, const float* scale, float angle, u32 color,
    u8 frame, RuntimeScoreboardDraw& draw); // Xbox 0x001221B0
// EAX fade block, EBX hidden point, ESI shown point, EDI output: false when the element is not drawn.
bool place_runtime_hud_element(const RuntimeHudFade& fade, const float* hidden, const float* shown,
    float* output); // Xbox 0x00131F00
long double ease_runtime_hud(float time); // Xbox 0x00103850, XMM0 time
// EAX now (0 queues), ESI position (advanced), EDI preset {spacing, -, -, gap, colour}.
void draw_runtime_hud_digit(u32 now, i32 digit, const float* scale, float* position, std::span<const u8> preset,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw); // Xbox 0x0012F8F0
void draw_runtime_hud_dollar(u32 now, i32 index, const float* scale, float* position, std::span<const u8> preset,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw); // Xbox 0x0012F850
// EAX counter, ECX position, EDX preset: the counter's digits from the first nonzero one.
void draw_runtime_hud_counter(RuntimeHudCounter& counter, float* position, std::span<const u8> preset, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw); // Xbox 0x0012F000
// EAX position, EDX counter, ECX preset: the counter's +0x28 as two digits, ones first.
void draw_runtime_hud_two_digits(float* position, RuntimeHudCounter& counter, std::span<const u8> preset, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw); // Xbox 0x0012F060
// EAX position, EBX counter, ECX preset: the dollar sign, then the counter.
void draw_runtime_hud_money(float* position, RuntimeHudCounter& counter, std::span<const u8> preset, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw); // Xbox 0x0012F0D0
// EAX counter, then preset, position and now: the counter's seconds as [H:]MM:SS.
void draw_runtime_hud_time(RuntimeHudCounter& counter, std::span<const u8> preset, float* position, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw); // Xbox 0x0012F110
}
