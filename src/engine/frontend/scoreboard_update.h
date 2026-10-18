#pragma once
#include "scoreboard_hud.h"
#include "../audio/event_voices.h"

namespace gbtg
{
// A HUD element's fade block, five retail words: the state (0 off, 1 appearing, 2 shown, 3 held, 4/6 fading, 5
// leaving), timer A, timer B, the end mode and the hold flag. The scoreboard rows keep it at +8; the meters
// keep it in their globals.
struct RuntimeHudFade
{
    u32& state;
    float& first;
    float& second;
    u32& mode;
    u32& hold;
};
// The scale animator's block (0x00104DF0, ESI): the value, its base, the amplitude, the pulse time left, the pulse
// length, the step time left and the step rate.
struct RuntimeHudPulse
{
    float& value;
    float& base;
    float& amplitude;
    float& left;
    float& length;
    float& step_left;
    float& rate;
};

// The per-frame scoreboard update's views beyond the draw context.
struct RuntimeScoreboardUpdate
{
    RuntimeScoreboardHud& hud;
    RuntimeScoreboardResources& board_resources;
    AudioEventVoices& voices;
    const i32& paused; // DAT_00510CE8.
    const float& hud_time; // DAT_004312C8.
    const float& hold_time; // DAT_004312CC.
    const u8& scene_flags; // **(DAT_00506C74+8).
    const i32& challenge_mode; // DAT_00506C58.
    const u32& challenge_count; // DAT_00506C84.
    std::span<const u8> row_preset; // DAT_00314170.
    std::span<const u8> brew_preset; // DAT_00312A1C.
    std::span<const u8> damage_preset; // DAT_00312F8C.
    float pulse; // DAT_0032B110.
    float fade_in; // DAT_0032B248.
    float fade_out; // DAT_0032B244.
    float leave; // DAT_0032B210.
    float band_length; // DAT_0032B254.
    std::function<void(std::span<u32>)> message_players; // 0x00122D10: vtable +0xC of each DAT_00511B40 player.
};

void set_runtime_hud_fade(RuntimeHudFade fade, u32 state, const RuntimeScoreboardUpdate& context); // Xbox 0x00131FC0
bool step_runtime_hud_fade(RuntimeHudFade fade, i32 timed, const RuntimeScoreboardUpdate& context); // Xbox 0x00131C50
bool tick_runtime_hud_timer(float& timer); // Xbox 0x0012B2F0, ECX timer
// EAX callback (may be empty), ESI block: steps the pulse by `dt`; true when a step ended.
bool animate_runtime_hud_pulse(RuntimeHudPulse pulse, float dt,
    const std::function<void()>& callback); // Xbox 0x00104DF0
// ESI counter, EDI preset {base, amplitude, length}: the counter's +0x2C swell over its +0x30 time.
void swell_runtime_hud_counter(RuntimeHudCounter& counter, std::span<const u8> preset); // Xbox 0x0012EF60
// Message rows of `board` advance their fades; the rows that end and asked for it are removed.
void update_runtime_scoreboard(RuntimeScoreboard& board, RuntimeScoreboardUpdate& context); // Xbox 0x00130E10
void settle_runtime_scoreboard_counters(RuntimeScoreboard& board, float& amplitude,
    RuntimeScoreboardUpdate& context); // Xbox 0x00131B70
void update_runtime_health_display(RuntimeScoreboardUpdate& context); // Xbox 0x001301A0
void release_runtime_health_voice(RuntimeScoreboardUpdate& context); // Xbox 0x001308A0
// EDX output: the threshold row (DAT_00313A40, stride 0x18) the health counter falls in, and its index.
i32 runtime_health_display_band(std::span<const u8>& row, RuntimeScoreboardUpdate& context); // Xbox 0x00130A50
void play_runtime_health_voice(const char* name, RuntimeScoreboardUpdate& context); // Xbox 0x00130830
// The health pulse's step callback: the counter catches up, the heartbeat plays, players get 0x90.
void beat_runtime_health_display(float& amplitude, RuntimeScoreboardUpdate& context); // Xbox 0x00130AC0
void update_runtime_powerup_board(RuntimeScoreboardUpdate& context); // Xbox 0x001323E0
void update_runtime_bonus_board(RuntimeScoreboardUpdate& context); // Xbox 0x0012D690
void update_runtime_brew_prompt(RuntimeScoreboardUpdate& context); // Xbox 0x0012D290
void update_runtime_damage_counter(RuntimeScoreboardUpdate& context); // Xbox 0x0012DA10
void update_runtime_meter_view(RuntimeScoreboardUpdate& context); // Xbox 0x0012E070
void update_runtime_meter(RuntimeScoreboardUpdate& context); // Xbox 0x0012E900
void release_runtime_meter_voice(RuntimeScoreboardUpdate& context); // Xbox 0x0012EE00
void update_runtime_scoreboard_hud(RuntimeScoreboardUpdate& context); // Xbox 0x00132210
// The weapon-only icons of the damage counter (DAT_004326D4 and its half width DAT_004326D8) and the head glow
// (DAT_004326DC, its phase DAT_004326E4), loaded when first needed.
void load_runtime_scoreboard_weapon_icons(RuntimeScoreboardGlobals& globals, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x0012D8A0
}
