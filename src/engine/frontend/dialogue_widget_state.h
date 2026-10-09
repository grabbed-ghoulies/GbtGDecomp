#pragma once

#include "dialogue_text_metrics.h"
#include "../../gbtg/text/localtext_resources.h"
#include <array>

namespace gbtg
{
struct Actor;
struct GhoulySpawnPool;
struct RuntimeDialogueWidgetLabel
{
    std::array<char16_t, 64> text; // row +0, widget +0xAF8
    u32 id; // row +0x80
};
struct RuntimeDialogueWidget
{
    i32 mode; // +0
    i32 layout_mode; // +4
    float progress; // +8
    float timer; // +0xC
    float duration; // +0x10
    float scale; // +0x14
    std::array<u8, 0x310> old_parameters; // +0x18
    std::array<u8, 0x310> new_parameters; // +0x328
    std::array<u8, 0x310> requested_parameters; // +0x638
    const u8* old_set; // +0x948
    const u8* current_set; // +0x94C
    const u8* requested_set; // +0x950
    i32 count; // +0x954
    std::array<std::array<float, 2>, 10> positions; // +0x958
    std::array<std::span<const u8>, 10> icons; // +0x9A8
    std::array<float, 2> origin; // +0x9D0
    std::array<float, 2> row_step; // +0x9E0
    std::array<float, 2> column_step; // +0x9E8
    std::array<char, 256> group; // +0x9F0
    std::span<const u8> text_resource; // +0xAF0
    std::array<RuntimeDialogueWidgetLabel, 10> labels; // +0xAF8, stride 0x84
    RuntimeDialogueTextPool* pool; // +0x1020
    u32 owns_text_resource; // +0xAF4
    Actor* bat; // +0x1024
    float icon_scale; // +0x9D8
    u32 transition_flags; // +0x9DC
    i32 selected; // +0x1028
    float bat_scale; // +0x102C
    u32 sound_enabled; // +0x1030
    std::array<char, 128> sound_name; // +0x1034
    u32 flags; // +0x10B4
};
struct RuntimeDialogueWidgetContext
{
    LocalTextResourceContext& text;
    const RuntimeTextMetrics& metrics;
    std::span<const char* const> icon_names;
    const u8* fallback_texture;
    float viewport_width;
    i32& next_id;
    u32& random_state;
};
float measure_dialogue_widget_width(const char* resource, const u8* parameters,
    float icon_scale, float text_scale, const char* group, char16_t* scratch,
    RuntimeDialogueWidgetContext& context); // Xbox 0x001428F0
void position_dialogue_widget(RuntimeDialogueWidget& widget,
    const std::array<float, 2>& anchor); // Xbox 0x001421C0
void layout_dialogue_widget(RuntimeDialogueWidget& widget,
    RuntimeDialogueWidgetContext& context); // Xbox 0x00142AF0
void set_dialogue_widget_mode(RuntimeDialogueWidget& widget, i32 mode,
    RuntimeDialogueWidgetContext& context); // Xbox 0x00142C20
void set_dialogue_widget_prompt(RuntimeDialogueWidget& widget, const u8* parameters,
    RuntimeDialogueWidgetContext& context); // Xbox 0x00142180
void destroy_dialogue_widget(RuntimeDialogueWidget* widget,
    AnimationResourceRegistry& resources, GhoulySpawnPool& text_pools); // Xbox 0x00142070
}
