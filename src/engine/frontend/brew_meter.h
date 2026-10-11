#pragma once
#include "player_health_display.h"
#include "dialogue_widget_state.h"

namespace gbtg
{
// Butler's brew counter shown by the player strategy's brew prompt.
struct RuntimeBrewMeter
{
    u32& state; // DAT_00545990.
    float& intensity; // DAT_00545998.
    RuntimeHudCounter& counter; // DAT_005459A8.
    std::array<std::span<const u8>, 3>& textures; // DAT_005459DC..E4: brew, cross, tick.
    std::span<float, 3> half_widths; // DAT_005459E8..F0.
    RuntimeDialogueWidget*& widget; // DAT_005459F4.
    RuntimeDialogueWidgetContext& widget_context;
    std::span<const u8> preset; // DAT_00312A1C.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    std::span<const u8> address_zero; // A missing texture's width is read at address 8.
};
void acquire_runtime_brew_textures(RuntimeBrewMeter& meter); // Xbox 0x0012D090
void show_runtime_brew_count(i32 count, RuntimeBrewMeter& meter); // Xbox 0x0012D4A0, EAX count
void close_runtime_brew_meter(RuntimeBrewMeter& meter); // Xbox 0x0012D4E0
void accept_runtime_brew_meter(RuntimeBrewMeter& meter); // Xbox 0x0012D510
}
