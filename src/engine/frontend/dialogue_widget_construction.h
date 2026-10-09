#pragma once
#include "dialogue_widget_state.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct DialogueWidgetAllocationContext
{
    RuntimeDialogueWidgetContext& widget;
    GhoulySpawnPool*& pools;
    void* (*allocate)(i32) = engine_heap_allocate; // retail heap boundary 0x000F72D0
};
void create_dialogue_widget(RuntimeDialogueWidget*& output, i32 layout,
    const u8* parameters, const std::array<float, 2>& anchor,
    const std::array<float, 2>& row_step, const std::array<float, 2>& origin,
    const std::array<float, 2>* column_step, float icon_scale, float text_scale,
    const char* group, std::span<const u8> text, u32 flags,
    DialogueWidgetAllocationContext& context); // Xbox 0x00141E20
void create_resource_dialogue_widget(RuntimeDialogueWidget*& output, i32 layout,
    const u8* parameters, const std::array<float, 2>& anchor,
    const std::array<float, 2>& row_step, const std::array<float, 2>& origin,
    const std::array<float, 2>* column_step, float icon_scale, float text_scale,
    const char* group, const char* resource, u32 flags,
    DialogueWidgetAllocationContext& context); // Xbox 0x00141DA0
}
