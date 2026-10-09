#include "dialogue_widget_construction.h"
#include "dialogue_widget_parameters.h"
#include <bit>
#include <cstring>
#include <new>

namespace gbtg
{
// Xbox 0x00141E20
void create_dialogue_widget(RuntimeDialogueWidget*& output, i32 layout,
    const u8* parameters, const std::array<float, 2>& anchor,
    const std::array<float, 2>& row_step, const std::array<float, 2>& origin,
    const std::array<float, 2>* column_step, float icon_scale, float text_scale,
    const char* group, std::span<const u8> text, u32 flags,
    DialogueWidgetAllocationContext& context)
{
    // Map the heap's untouched +0 word into the native mode field.
    // The retail constructor does not initialize it before 0x00142C20.
    void* storage = context.allocate(sizeof(RuntimeDialogueWidget));
    i32 allocated_mode;
    std::memcpy(&allocated_mode, storage, sizeof(allocated_mode));
    output = new (storage) RuntimeDialogueWidget;
    output->mode = allocated_mode;
    output->old_set = nullptr;
    output->current_set = nullptr;
    output->requested_set = nullptr;
    output->layout_mode = layout;
    output->progress = 0.f;
    output->timer = 0.f;
    output->icon_scale = icon_scale;
    output->scale = text_scale;
    output->text_resource = text;
    output->sound_enabled = 1;
    output->transition_flags = 0xFFFFFFFFu; // DAT_00314F48
    output->flags = flags;
    i32 index = 0;
    char unit;
    do
    {
        unit = group[index];
        output->group[index] = unit;
        ++index;
    }
    while (unit != 0);
    output->row_step = row_step;
    if (column_step == nullptr)
    {
        output->column_step[1] = 0.f;
        output->column_step[0] = 0.f;
    }
    else
    {
        output->column_step = *column_step;
    }
    allocate_dialogue_text_pool(output->pool, 0, 1, output->flags, context.pools);
    for (i32 slot = 0; slot < 10; ++slot)
    {
        output->icons[slot] = {};
    }
    const RuntimeTextFont& font = *context.widget.metrics.fonts[0];
    const u8* texture;
    if (font.first < 0x42 && 0x40 < font.last)
    {
        const RuntimeTextGlyph& glyph = font.glyphs[0x41 - font.first];
        texture = glyph.texture;
        // Retail zero-extends the marker before comparing against -1.
        if (texture == nullptr || u32(glyph.marker) == 0xFFFFFFFFu)
        {
            texture = context.widget.fallback_texture;
        }
    }
    else
    {
        texture = context.widget.fallback_texture;
    }
    if (texture == nullptr)
    {
        output->duration = 0.f;
    }
    else
    {
        output->duration = float(texture[0x11]) / float(texture[0x10]);
    }
    position_dialogue_widget(*output, anchor);
    output->origin = origin;
    output->owns_text_resource = 0;
    output->bat = nullptr;
    output->selected = 0;
    output->bat_scale = 0.f;
    const char* sound = "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON";
    index = 0;
    do
    {
        unit = sound[index];
        output->sound_name[index] = unit;
        ++index;
    }
    while (unit != 0);
    if (parameters != nullptr)
    {
        copy_dialogue_widget_parameters(output->requested_parameters, parameters);
        output->requested_set = output->requested_parameters.data();
        set_dialogue_widget_mode(*output, 3, context.widget);
        return;
    }
    output->requested_set = nullptr;
    set_dialogue_widget_mode(*output, 3, context.widget);
}

// Xbox 0x00141DA0
void create_resource_dialogue_widget(RuntimeDialogueWidget*& output, i32 layout,
    const u8* parameters, const std::array<float, 2>& anchor,
    const std::array<float, 2>& row_step, const std::array<float, 2>& origin,
    const std::array<float, 2>* column_step, float icon_scale, float text_scale,
    const char* group, const char* resource, u32 flags,
    DialogueWidgetAllocationContext& context)
{
    std::span<const u8> text;
    if (read_u32({reinterpret_cast<const u8*>(resource), 4}, 0) == 0x5F646961u)
    {
        text = acquire_animation_resource(context.widget.text.registry, resource);
        if (text.data() == nullptr)
        {
            resource_error(context.widget.text.error, 2);
        }
    }
    create_dialogue_widget(output, layout, parameters, anchor, row_step, origin,
        column_step, icon_scale, text_scale, group, text, flags, context);
    output->owns_text_resource = 1;
}
}
