#include "brew_meter.h"
#include "dialogue_widget_construction.h"
#include "hud_layout.h"

namespace gbtg
{
namespace
{
void acquire_texture(const char* name, size_t index, RuntimeBrewMeter& meter)
{
    if (meter.textures[index].data() == nullptr)
    {
        const auto texture = acquire_animation_resource(meter.registry, name);
        if (texture.data() == nullptr)
        {
            resource_error(meter.error, 2);
        }
        meter.textures[index] = texture;
        const std::span<const u8> stored = texture.data() != nullptr ? texture : meter.address_zero;
        meter.half_widths[index] = float(i32(read_u16(stored, 8))) * 0.5f; // DAT_0032B0F4.
    }
}

void set_meter_widget(u32 state, RuntimeBrewMeter& meter)
{
    meter.state = state;
    RuntimeDialogueWidget* widget = meter.widget;
    if (widget != nullptr)
    {
        widget->requested_set = nullptr;
        set_dialogue_widget_mode(*widget, 3, meter.widget_context);
    }
}
}

// Xbox 0x0012D090
void acquire_runtime_brew_textures(RuntimeBrewMeter& meter)
{
    acquire_texture("aid_texture_ghoulies_scoreboard_energy_butlersbrew", 0, meter);
    acquire_texture("aid_texture_ghoulies_scoreboard_eventicons_cross", 1, meter);
    acquire_texture("aid_texture_ghoulies_scoreboard_eventicons_tick", 2, meter);
}

// Xbox 0x0012D4A0
void show_runtime_brew_count(i32 count, RuntimeBrewMeter& meter)
{
    if (count != 0)
    {
        acquire_runtime_brew_textures(meter);
    }
    meter.intensity = 2.0f; // DAT_0032B110.
    const i32 shown = count >= 0 ? count : 0;
    meter.counter.target = shown;
    meter.counter.value = shown;
    update_runtime_hud_counter_digits(meter.counter, meter.preset);
}

// Xbox 0x0012D4E0
void close_runtime_brew_meter(RuntimeBrewMeter& meter)
{
    set_meter_widget(1, meter);
}

// Xbox 0x0012D510
void accept_runtime_brew_meter(RuntimeBrewMeter& meter)
{
    set_meter_widget(2, meter);
}

// Xbox 0x0012D540
void create_runtime_brew_widget(RuntimeBrewMeter& meter, const RuntimeHudLayout& layout)
{
    const float inset = 16.f; // DAT_0032B1F4
    const std::array<float, 2> anchor{layout.inner_left + inset, layout.inner_bottom - inset};
    const std::array<float, 2> row_step{0.f, -32.f}; // DAT_0032B600
    const std::array<float, 2> origin{20.f, -16.f}; // DAT_0032B234, DAT_0032B5FC
    create_resource_dialogue_widget(meter.widget, 0, meter.rows, anchor, row_step, origin, nullptr, 1.f, 0.75f,
        "menutext", "aid_loctext_ghoulies_menu_menutext", 0x400C0, *meter.allocation); // 0x00141DA0, ESI DAT_005459F4
}
}
