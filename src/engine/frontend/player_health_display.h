#pragma once
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
struct RuntimeHudCounter
{
    i32 value;
    i32 target;
    std::array<i32, 8> digits; // +8 through +0x24, highest first.
    u32 word_28;
    u32 word_2c;
    u32 timer; // +0x30, bits of the preset's swell_time.
};
// A counter's digits' look: DAT_00314170, DAT_00314184, DAT_00312A1C, DAT_00312F8C, DAT_00313A18, DAT_00313A2C.
struct HudCounterPreset
{
    float scale; // +0x0, the digits' size and how far each advances.
    float swell_amplitude; // +0x4
    float swell_time; // +0x8
    i32 digit_gap; // +0xC
    u32 colour; // +0x10
};
static_assert(sizeof(HudCounterPreset) == 0x14);

// One band of the health display, DAT_00313A40: the first whose limit the health is at or under.
struct HealthBandRow
{
    i32 limit; // +0x0
    float pulse_6; // +0x4, the pulse's seventh word (DAT_00433C0C).
    float pulse_1; // +0x8, its second (DAT_00433BF8).
    float amplitude; // +0xC
    float changed_amplitude; // +0x10
    float unk_14; // +0x14
};
static_assert(sizeof(HealthBandRow) == 0x18);

struct RuntimePlayerHealthDisplay
{
    i32& active; // DAT_00433BE0.
    const i32& style; // DAT_00433BE8.
    float& intensity; // DAT_00433C14.
    RuntimeHudCounter& counter; // DAT_00433C38.
    u32& mode; // DAT_00433C70.
    std::array<std::span<const u8>, 5>& textures; // DAT_00433C7C..8C.
    std::span<u32, 5> widths; // DAT_00433C90..A0.
    const u32& override_health; // DAT_00512230.
    const u8& story_flags; // DAT_00506C6C -> bytes +4.
    std::span<const u8> thresholds; // DAT_00313A40, five HealthBandRow.
    std::span<const u8> normal_preset; // DAT_00313A18.
    std::span<const u8> active_preset; // DAT_00313A2C.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
};
void update_runtime_hud_counter_digits(RuntimeHudCounter& counter,
    std::span<const u8> preset); // Xbox 0x0012F990
void acquire_runtime_player_health_textures(RuntimePlayerHealthDisplay& context); // Xbox 0x0012FE10
void set_runtime_player_health_display(RuntimePlayerHealthDisplay& context,
    i32 health, u32 mode); // Xbox 0x00130710
}
