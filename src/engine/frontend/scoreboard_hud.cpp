#include "scoreboard_hud.h"
#include "../entities/powerup_flags.h"
#include "scoreboard_draw.h"
#include "../camera/camera.h"
#include "../entities/body_hit_state.h"
#include "../entities/bounded_channels.h"
#include "../graphics/color_blend.h"
#include "dialogue_widget_state.h"
#include "dialogue_text_metrics.h"
#include "dialogue_text_pool.h"
#include "../entities/ghouly_spawn_record.h"
#include "../../gbtg/text/localtext_resources.h"
#include <cstdio>
#include "../audio/listener_settings.h"
#include <algorithm>
#include <bit>

namespace gbtg
{
namespace
{
const char* texture_name(const RuntimeScoreboardHud& context, size_t index)
{
    return reinterpret_cast<const char*>(context.texture_names.data() + index * 0x80);
}

// 0x00035E90 on a name that starts "aid_", with error 2 (0x00032700) when it is missing; other names load nothing.
const u8* acquire_named_texture(const char* name, RuntimeScoreboardHud& context)
{
    u32 head;
    std::memcpy(&head, name, 4);
    if (head != 0x5F646961)
    {
        return nullptr;
    }
    const auto texture = acquire_animation_resource(context.registry, name);
    if (texture.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    return texture.data();
}

// 0x00035C00 on a held texture, then the slot clears.
void release_texture(std::span<const u8>& texture, RuntimeScoreboardHud& context)
{
    if (texture.data() != nullptr)
    {
        release_animation_resource(context.registry, texture);
        texture = {};
    }
}

void release_texture(const u8*& texture, RuntimeScoreboardHud& context)
{
    if (texture != nullptr)
    {
        release_animation_resource(context.registry, {texture, 0});
        texture = nullptr;
    }
}

void reset_counter(RuntimeHudCounter& counter)
{
    counter.value = 0;
    counter.target = 0;
    counter.digits = {};
    counter.word_28 = 0;
    counter.word_2c = std::bit_cast<u32>(0.4f); // DAT_0032B11C.
    counter.timer = 0;
}

[[noreturn]] void untranslated(const char* callee, const char* caller)
{
    throw Error(std::string("untranslated Xbox ") + callee + " reached from " + caller);
}
}

// Xbox 0x0012EE60
void load_runtime_scoreboard_digits(RuntimeScoreboardHud& context)
{
    for (size_t index = 0; index < 10; ++index)
    {
        context.digits[index] = acquire_named_texture(texture_name(context, index), context);
    }
    for (size_t index = 0; index < 2; ++index)
    {
        context.globals.dollars[index] = acquire_named_texture(texture_name(context, 10 + index), context);
    }
}

// Xbox 0x0012EF10
void release_runtime_scoreboard_digits(RuntimeScoreboardHud& context)
{
    for (const u8*& digit : context.digits)
    {
        release_texture(digit, context);
    }
    for (const u8*& dollar : context.globals.dollars)
    {
        release_texture(dollar, context);
    }
}

// Xbox 0x0012D180
void reset_runtime_brew_meter(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    globals.brew_visible = 0;
    globals.brew_99c = 0;
    context.brew.intensity = 0.f;
    globals.brew_9a0 = 0;
    globals.brew_9a4 = 0;
    reset_counter(context.brew.counter);
}

// Xbox 0x0012D200
void release_runtime_brew_meter(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    for (auto& texture : context.brew.textures)
    {
        release_texture(texture, context);
    }
    globals.brew_visible = 0;
    globals.brew_99c = 0;
    context.brew.intensity = 0.f;
    globals.brew_9a0 = 0;
    globals.brew_9a4 = 0;
    context.brew.state = 0;
    if (context.brew.widget != nullptr)
    {
        context.destroy_widget(context.brew.widget);
        context.brew.widget = nullptr;
    }
}

// Xbox 0x00130BB0
void create_runtime_scoreboard(RuntimeScoreboard*& output, const float* start, const float* end,
    const float* offset, RuntimeScoreboardHud& context)
{
    auto board = std::make_unique<RuntimeScoreboard>();
    // The pool header: stride 0x94, room for two rows.
    board->messages.reserve(2);
    board->next_id = 1;
    board->first_icon = {};
    board->second_icon = {};
    board->icon_scale = 1.f; // DAT_0032B0F8.
    board->spacing = 0.f;
    board->word_50 = 0;
    board->slots[0] = start[0];
    board->slots[1] = start[1];
    board->slots[2] = end[0];
    board->slots[3] = end[1];
    board->slot_scale_x = 0.5f; // DAT_0032B0F4.
    board->slot_scale_y = 0.5f;
    for (size_t slot = 0; slot < 8; slot += 4)
    {
        auto& slots = board->slots;
        if (offset != nullptr)
        {
            slots[slot + 4] = slots[slot] + offset[0];
            slots[slot + 5] = slots[slot + 1] + offset[1];
            slots[slot + 6] = slots[slot + 2] + offset[0];
            slots[slot + 7] = slots[slot + 3] + offset[1];
        }
        else
        {
            slots[slot + 4] = slots[0];
            slots[slot + 5] = slots[1];
            slots[slot + 6] = slots[2];
            slots[slot + 7] = slots[3];
        }
    }
    output = board.get();
    context.globals.boards.push_back(std::move(board));
}

// Xbox 0x00130CE0
void free_runtime_scoreboard(RuntimeScoreboard* board, RuntimeScoreboardHud& context)
{
    // Each row's texture (+0x88) goes, and the rows behind it move down.
    while (!board->messages.empty())
    {
        release_texture(board->messages.front().texture, context);
        board->messages.erase(board->messages.begin());
    }
    release_texture(board->first_icon, context);
    release_texture(board->second_icon, context);
    auto& boards = context.globals.boards;
    const auto owned = std::find_if(boards.begin(), boards.end(), [board](const auto& entry)
    {
        return entry.get() == board;
    });
    if (owned == boards.end())
    {
        throw Error("scoreboard freed by 0x00130CE0 was not allocated by 0x00130BB0");
    }
    boards.erase(owned);
}

// Xbox 0x0012D5F0
void create_runtime_bonus_board(RuntimeScoreboardHud& context)
{
    const float x = float(i32(context.width)) * 0.5f - 10.f; // DAT_0032B0F4, DAT_0032B10C.
    const float start[2]{x, context.layout.inner_bottom - 22.f}; // DAT_0032B300.
    const float end[2]{x, context.layout.inner_bottom + 160.f}; // DAT_0032B2FC.
    auto& globals = context.globals;
    create_runtime_scoreboard(globals.bonus_board, start, end, nullptr, context);
    globals.bonus_board->word_50 = 1;
    globals.bonus_ids = {};
    globals.bonus_active = {};
    globals.bonus_counts = {};
}

// Xbox 0x0012D930
void reset_runtime_damage_counter(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    globals.damage_68c = 0;
    globals.damage_694 = 0;
    globals.damage_690 = 0.f;
    globals.damage_698 = 0;
    globals.damage_69c = 0;
    reset_counter(globals.damage_counter);
}

// Xbox 0x0012D9B0
void release_runtime_damage_counter(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    release_texture(globals.damage_texture, context);
    release_texture(globals.damage_flash_texture, context);
    globals.damage_68c = 0;
    globals.damage_698 = 0;
    globals.damage_69c = 0;
    globals.damage_694 = 0;
    globals.damage_690 = 0.f;
    globals.damage_caption = -1;
}

// Xbox 0x0012FA50
void create_runtime_score_board(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    if (globals.score_board != nullptr)
    {
        return;
    }
    const auto& layout = context.layout;
    const float width = float(i32(context.width));
    const float x = layout.inner_left + 27.5f; // DAT_0032B2F8.
    const float y = layout.inner_top + 27.5f;
    const float start[2]{x, y};
    const float end[2]{x - width, y};
    const float offset[2]{0.f, 67.5f}; // DAT_0032B2F4.
    create_runtime_scoreboard(globals.score_board, start, end, offset, context);
    // The third placement moves to the bottom corner.
    auto& slots = globals.score_board->slots;
    const float left = layout.inner_left + 25.f; // DAT_0032B170.
    const float bottom = layout.inner_bottom - 25.f;
    slots[8] = left;
    slots[9] = bottom;
    slots[10] = left - width;
    slots[11] = bottom;
}

// Xbox 0x0012FF80
void reset_runtime_health_display(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    auto& health = context.health;
    // DAT_00433BF4..DAT_00433C0C.
    globals.health_bf4 = {1.f, 1.f, 0.05f, 0.f, 0.1f, 0.f, 1.2f};
    health.active = 0;
    health.mode = 0;
    globals.health_drawn = 0;
    globals.health_shown = 0;
    globals.health_c18[0] = 0;
    health.intensity = 0.f;
    globals.health_c18[1] = 0;
    globals.health_c18[2] = 0;
    globals.health_pending = 0;
    globals.health_c28 = {};
    reset_counter(health.counter);
}

// Xbox 0x00130080
void release_runtime_health_display(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    auto& health = context.health;
    for (auto& texture : health.textures)
    {
        release_texture(texture, context);
    }
    if (globals.health_voice != 0xFFFF)
    {
        context.stop_voice(globals.health_voice);
        globals.health_voice = 0xFFFF;
    }
    if (globals.health_second_voice != 0xFFFF)
    {
        context.stop_voice(globals.health_second_voice);
        globals.health_second_voice = 0xFFFF;
    }
    globals.health_shown = 0;
    globals.health_c18[1] = 0;
    globals.health_c18[2] = 0;
    globals.health_pending = 0;
    globals.health_c28[2] = 0;
    globals.health_c28[3] = 0;
    context.health_style = 0;
    globals.health_drawn = 0;
    globals.health_c18[0] = 0;
    health.intensity = 0.f;
    globals.health_c28[1] = 0;
    globals.health_c28[0] = 0;
}

// Xbox 0x0012E840
void release_runtime_meter_textures(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    for (auto& texture : globals.meter_textures)
    {
        release_texture(texture, context);
    }
    globals.meter_b90 = 0;
    globals.meter_b8c = 0;
    globals.meter_visible = 0;
    globals.meter_b94 = 0;
    globals.meter_b98 = 0;
    globals.meter_level = 1.f; // DAT_0032B0F8.
    globals.meter_drawn = 0;
    globals.meter_bd0 = 0;
    if (globals.meter_voice != 0xFFFF)
    {
        context.stop_voice(globals.meter_voice);
        globals.meter_voice = 0xFFFF;
    }
}

// Xbox 0x00132350
void create_runtime_powerup_board(RuntimeScoreboardHud& context)
{
    const auto& layout = context.layout;
    const float y = layout.inner_bottom - 16.f; // DAT_0032B1F4.
    const float start[2]{layout.inner_left, y};
    const float end[2]{layout.inner_left - float(i32(context.width)), y};
    create_runtime_scoreboard(context.powerup_board, start, end, nullptr, context);
    context.powerup_board->icon_scale = 0.5f; // DAT_0032B0F4.
    context.powerup_board->spacing = -23.4f; // DAT_0032B3F4.
}

// Xbox 0x00132040
void set_up_runtime_scoreboard(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    load_runtime_scoreboard_digits(context);
    reset_runtime_brew_meter(context);
    create_runtime_bonus_board(context);
    reset_runtime_damage_counter(context);
    globals.meter_b90 = 0;
    globals.meter_b8c = 0;
    globals.meter_scales = {1.f, 1.f, 1.f}; // DAT_0032B0F8.
    globals.meter_bb4 = 0;
    globals.meter_bbc = 0;
    globals.word_a18 = 0;
    globals.word_a24 = 0;
    globals.meter_visible = 0;
    globals.meter_b94 = 0;
    globals.meter_b98 = 0;
    globals.meter_drawn = 0;
    globals.meter_bb8 = 0.25f; // DAT_0032B104.
    globals.meter_bc0 = std::bit_cast<float>(0x4036DB6Eu); // DAT_0032B25C.
    create_runtime_score_board(context);
    reset_runtime_health_display(context);
    create_runtime_powerup_board(context);
    context.enabled = 1;
}

// Xbox 0x00132100
void tear_down_runtime_scoreboard(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    release_runtime_brew_meter(context);
    if (globals.bonus_board != nullptr)
    {
        free_runtime_scoreboard(globals.bonus_board, context);
        globals.bonus_board = nullptr;
    }
    globals.bonus_ids = {};
    globals.bonus_active = {};
    globals.bonus_counts = {};
    release_runtime_damage_counter(context);
    if (globals.caption_pool != nullptr)
    {
        context.release_text_pool(globals.caption_pool);
        globals.caption_pool = nullptr;
    }
    context.flags &= 0xFFFEFFFFu;
    globals.word_a20 = 0;
    globals.word_a18 = 0;
    globals.word_a24 = 0;
    if (globals.caption_tinted != 0)
    {
        globals.caption_tinted = 0;
        globals.caption_color = context.white;
    }
    release_texture(globals.texture_a7c, context);
    release_runtime_meter_textures(context);
    if (globals.score_board != nullptr)
    {
        free_runtime_scoreboard(globals.score_board, context);
        globals.score_board = nullptr;
    }
    release_runtime_health_display(context);
    if (context.powerup_board != nullptr)
    {
        free_runtime_scoreboard(context.powerup_board, context);
        context.powerup_board = nullptr;
    }
    if (globals.award_pool != nullptr)
    {
        context.release_text_pool(globals.award_pool);
        globals.award_pool = nullptr;
    }
    release_runtime_scoreboard_digits(context);
    context.enabled = 0;
}

namespace
{
RuntimeScoreboardDraw& draw_views(RuntimeScoreboardHud& context)
{
    if (context.draw == nullptr)
    {
        throw Error("scoreboard draw without the HUD sprite queue");
    }
    return *context.draw;
}
}

// Xbox 0x00130420
void draw_runtime_health_display(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    if (!((context.difficulty < 1 || context.difficulty > 2) && !context.area_dialogue_active() // 0x0005B830
        && (globals.health_shown != 0 || globals.health_pending != 0)))
    {
        return;
    }
    auto& draw = draw_views(context);
    auto& health = context.health;
    const auto& layout = context.layout;
    u32 backing = 0;
    const u8* heart;
    float width;
    float size;
    if (health.override_health != 0) // DAT_00512230
    {
        width = float(i32(health.widths[0])); // DAT_00433C90
        heart = health.textures[2].data(); // DAT_00433C84
        size = 0.85f; // DAT_0032B17C
    }
    else
    {
        backing = draw.brew_accepted; // DAT_00430D98
        if (health.mode != 0) // DAT_00433C70
        {
            heart = health.textures[1].data(); // DAT_00433C80
            width = float(i32(health.widths[1])); // DAT_00433C94
        }
        else
        {
            heart = health.textures[0].data(); // DAT_00433C7C
            width = float(i32(health.widths[0]));
        }
        size = 1.f; // DAT_0032B0F8
    }
    float shown[2]{layout.inner_right - width - 4.5f, 0.f}; // DAT_00509748, DAT_0032B3B0
    shown[1] = layout.inner_top + width - 36.f; // DAT_00509744, DAT_0032B5C4
    float hidden[2]{shown[0] + 160.f, shown[1]}; // DAT_0032B2FC
    float at[2];
    const RuntimeHudFade fade{globals.health_shown, health.intensity,
        *reinterpret_cast<float*>(&globals.health_c18[0]), globals.health_c18[1],
        globals.health_c18[2]}; // DAT_00433C10..DAT_00433C20
    if (place_runtime_hud_element(fade, hidden, shown, at)) // 0x00131F00, EDI at
    {
        const u32 color = 0xC0FFFFFFu;
        if (backing != 0)
        {
            const float scale[2]{globals.health_bf4[1] * 1.5f, globals.health_bf4[1] * 1.5f}; // DAT_00433BF8, DAT_0032B210
            queue_runtime_hud_sprite(health.textures[3].data(), at, scale, 0.f, color, 0, draw); // DAT_00433C88
        }
        const float beat = globals.health_bf4[0] * size; // DAT_00433BF4
        const float scale[2]{beat, beat};
        queue_runtime_hud_sprite(heart, at, scale, 0.f, color, 0, draw); // 0x001221B0
        const float offset = width - 20.f; // DAT_0032B234
        shown[0] += offset;
        hidden[0] += offset;
        float digits[2];
        place_runtime_hud_element(fade, hidden, shown, digits); // 0x00131F00, EDI the digits
        const auto preset = health.mode != 0 ? health.active_preset : health.normal_preset; // 0x00130B90
        if (context.health_style == 1)
        {
            draw_runtime_hud_two_digits(digits, health.counter, preset, 0, context, draw); // 0x0012F060
        }
        else
        {
            draw_runtime_hud_counter(health.counter, digits, preset, 0, context, draw); // 0x0012F000
        }
        globals.health_drawn = 1; // DAT_00433C6C
    }
    const float broken = float(i32(health.widths[4])); // DAT_00433CA0
    float pending_shown[2]{layout.inner_right - broken - 20.f, layout.inner_top + broken - 100.f}; // DAT_0032B12C
    float pending_hidden[2]{pending_shown[0], pending_shown[1] - 120.f}; // DAT_0032B400
    const RuntimeHudFade pending{globals.health_pending, *reinterpret_cast<float*>(&globals.health_c28[0]),
        *reinterpret_cast<float*>(&globals.health_c28[1]), globals.health_c28[2], globals.health_c28[3]}; // DAT_00433C24
    if (place_runtime_hud_element(pending, pending_hidden, pending_shown, at))
    {
        const float scale[2]{0.75f, 0.75f}; // DAT_0032B2AC
        queue_runtime_hud_sprite(health.textures[4].data(), at, scale, 0.f, 0xC0FFFFFFu, 0, draw); // DAT_00433C8C
    }
}

// Xbox 0x00130F20
void draw_runtime_scoreboard(RuntimeScoreboard* board, RuntimeScoreboardHud& context)
{
    if (board == nullptr)
    {
        return;
    }
    auto& draw = draw_views(context);
    i32 counts[3]{};
    for (const auto& row : board->messages)
    {
        if (row.word_70 >= 1 && row.word_70 <= 3)
        {
            ++counts[row.word_70 - 1];
        }
    }
    // Three placements of {shown x, shown y, hidden x, hidden y}: kind 4 rows use the second (the first again when
    // no row counts), kind 5 rows the third, the others the first.
    float first[4]{board->slots[0], board->slots[1], board->slots[2], board->slots[3]};
    float second[4];
    if (counts[0] == 0 && counts[1] == 0 && counts[2] == 0)
    {
        std::copy_n(first, 4, second);
    }
    else
    {
        std::copy_n(board->slots.begin() + 4, 4, second);
    }
    float third[4]{board->slots[8], board->slots[9], board->slots[10], board->slots[11]};
    const float blink_rate = 6.f; // DAT_0032B218
    for (size_t index = board->messages.size(); index-- > 0;)
    {
        auto& row = board->messages[index];
        float* shown = first;
        float* hidden = first + 2;
        if (row.word_70 == 4)
        {
            shown = second;
            hidden = second + 2;
        }
        else if (row.word_70 == 5)
        {
            shown = third;
            hidden = third + 2;
        }
        const bool wide = row.scaled_width == 128.f; // DAT_0032B368
        if (wide)
        {
            shown[0] += 32.f; // DAT_0032B458
            hidden[0] += 32.f;
        }
        const RuntimeHudFade fade{row.word_8, row.float_c, row.float_10, row.word_14, row.word_18};
        float at[2];
        if (!place_runtime_hud_element(fade, hidden, shown, at)) // 0x00131F00, EAX row+8
        {
            if (wide)
            {
                shown[0] -= 32.f;
                hidden[0] -= 32.f;
            }
            continue;
        }
        float size = board->icon_scale * row.scale_y; // board +0x48, row +0x24
        float scale[2]{size, size};
        const u32 base_color = 0xC0FFFFFFu;
        float factor = 0.f;
        const bool blinking = row.word_8 == 5 || row.word_8 == 3;
        const bool hidden_blink = blinking && (i32(draw.hold_time * blink_rate) & 1) == 0;
        if (!hidden_blink)
        {
            if (row.pulse != 0 && 0.f < row.pulse_scale) // +0x7C, +0x80
            {
                const float step = scene_detail::g_sim_freeze <= 0.f
                    ? scene_detail::g_sim_dt * scene_detail::g_sim_scale : 0.f;
                if (0.f < row.pulse_scale)
                {
                    row.pulse_scale -= step;
                    if (row.pulse_scale <= 0.f)
                    {
                        row.pulse_scale = 0.f;
                    }
                }
                float level = row.pulse_scale * 0.25f; // DAT_0032B104
                float clamped = level;
                if (clamped > 1.f)
                {
                    clamped = 1.f;
                }
                else if (0.f > clamped)
                {
                    clamped = 0.f;
                }
                const float glow = (clamped + 1.f) * size;
                const float glow_scale[2]{glow, glow};
                float alpha = level * 255.f; // DAT_0032B158
                if (alpha > 255.f)
                {
                    alpha = 255.f;
                }
                else if (0.f > alpha)
                {
                    alpha = 0.f;
                }
                const u8 byte = u8(static_cast<std::int64_t>(alpha)); // 0x001673D8
                const u32 color = (base_color & 0x00FFFFFFu) | (u32(byte) << 24);
                queue_runtime_hud_sprite(board->second_icon.data(), at, glow_scale, 0.f, color, 0, draw);
                if (row.pulse_scale < 0.001f) // DAT_0032B1A4
                {
                    row.pulse = 0;
                }
            }
            if (row.word_78 != 0)
            {
                const float back[2]{at[0] - 5.f, at[1]}; // DAT_0032B184
                queue_runtime_hud_sprite(board->first_icon.data(), back, scale, 0.f, base_color, 0, draw);
            }
            if (row.icon == 0x42)
            {
                size *= 1.5f; // DAT_0032B210
                scale[0] = size;
                scale[1] = size;
            }
            queue_runtime_hud_sprite(row.texture.data(), at, scale, 0.f, base_color, 0, draw); // +0x88
        }
        switch (row.word_8)
        {
        case 1:
            factor = 1.f - row.float_10 * (5.f / 3.f); // DAT_0032B118
            break;
        case 2:
        case 3:
        case 5:
            factor = 1.f;
            break;
        case 4:
        case 6:
            factor = row.float_10 * 1.25f; // DAT_0032B114
            break;
        default:
            factor = 0.f;
            break;
        }
        {
            if (!blinking || (i32(draw.hold_time * blink_rate) & 1) != 0)
            {
                const auto preset = context.counter_preset(row.word_74 != 0); // 0x00314184 / 0x00314170
                if (row.word_70 >= 2 && row.word_70 <= 5)
                {
                    const float offset = row.scale_y * row.scaled_width * 0.5f; // +0x24, +0x90, DAT_0032B0F4
                    shown[0] += offset;
                    hidden[0] += offset;
                    float digits[2];
                    if (place_runtime_hud_element(fade, hidden, shown, digits)) // 0x00131F00
                    {
                        if (row.word_70 == 2)
                        {
                            draw_runtime_hud_counter(row.counter, digits, preset, 0, context, draw); // 0x0012F000
                        }
                        else if (row.word_70 == 3)
                        {
                            draw_runtime_hud_money(digits, row.counter, preset, 0, context, draw); // 0x0012F0D0
                        }
                        else
                        {
                            draw_runtime_hud_time(row.counter, preset, digits, 0, context, draw); // 0x0012F110
                        }
                    }
                    shown[0] -= offset;
                    hidden[0] -= offset;
                }
            }
        }
        float spacing = board->spacing; // board +0x4C
        switch (row.word_70)
        {
        case 2:
            spacing += 89.f; // DAT_0032B258
            break;
        case 3:
            spacing += 79.f; // DAT_0032B5F8
            break;
        case 4:
        case 5:
            spacing += 144.f; // DAT_0032B5F4
            break;
        default:
            spacing += 74.f; // DAT_0032B5F0
            break;
        }
        shown[0] = spacing * factor + shown[0];
        hidden[0] = hidden[0] + spacing * factor;
        if (wide)
        {
            shown[0] += 32.f;
            hidden[0] += 32.f;
        }
    }
}

// Xbox 0x0012DAF0
void draw_runtime_damage_counter(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    if (globals.damage_caption == -1)
    {
        return;
    }
    auto& draw = draw_views(context);
    const auto& layout = context.layout;
    const float half = globals.damage_6d8; // Half the icon's width.
    const float y = layout.inner_bottom - half + 10.f; // DAT_0050974C, DAT_0032B10C
    float shown[2]{layout.inner_right - half - 15.5f, y}; // DAT_00509748, DAT_0032B57C
    float hidden[2]{float(i32(context.width)) + 100.f, y}; // DAT_005096E8, DAT_0032B12C
    const RuntimeHudFade fade{globals.damage_68c, globals.damage_690,
        *reinterpret_cast<float*>(&globals.damage_694), globals.damage_698, globals.damage_69c}; // DAT_0043268C..
    float at[2];
    if (!place_runtime_hud_element(fade, hidden, shown, at)) // 0x00131F00, EBX hidden, ESI shown, EDI at
    {
        return;
    }
    u32 color = 0x00FFFFFFu;
    if (runtime_weapon_boost_active() != 0) // DAT_00512278.
    {
        const float scale[2]{1.5f, 1.5f}; // DAT_0032B210
        const u32 alpha = u32(truncate_runtime_float(static_cast<long double>(globals.damage_6e4) * 192.f)); // DAT_0032B454
        color = (color & 0x00FFFFFFu) | (alpha << 24);
        queue_runtime_hud_sprite(globals.damage_flash_texture.data(), at, scale, 0.f, color, 0, draw); // 0x001221B0
    }
    color = (color & 0x00FFFFFFu) | 0xC0000000u;
    const float scale[2]{1.f, 1.f}; // DAT_0032B0F8
    queue_runtime_hud_sprite(globals.damage_texture.data(), at, scale, 0.f, color, 0, draw); // 0x001221B0
    const float offset = half + 10.f; // DAT_0032B10C
    shown[0] += offset;
    hidden[0] += offset;
    place_runtime_hud_element(fade, hidden, shown, at); // 0x00131F00, EDI the digits
    draw_runtime_hud_counter(globals.damage_counter, at, context.damage_preset, 0, context, draw); // 0x0012F000
}

namespace
{
RuntimeScoreCaptions& captions_of(RuntimeScoreboardHud& context)
{
    if (context.captions == nullptr)
    {
        throw Error("the score captions (DAT_00433B4C) are not bound");
    }
    return *context.captions;
}

void append_wide(char16_t* output, const char16_t* text)
{
    while (*output != 0)
    {
        ++output;
    }
    while ((*output++ = *text++) != 0) // lstrcatW
    {
    }
}

void format_wide(char16_t* output, const char* format, std::initializer_list<u32> values)
{
    char narrow[64];
    const auto it = values.begin();
    if (values.size() == 1)
    {
        std::snprintf(narrow, sizeof(narrow), format, it[0]);
    }
    else if (values.size() == 2)
    {
        std::snprintf(narrow, sizeof(narrow), format, it[0], it[1]);
    }
    else
    {
        std::snprintf(narrow, sizeof(narrow), format, it[0], it[1], it[2]);
    }
    size_t at = 0;
    for (; narrow[at] != 0; ++at)
    {
        output[at] = char16_t(u8(narrow[at]));
    }
    output[at] = 0;
}

// Xbox 0x0012DFD0, ESI the text: one caption centred under the HUD, a line higher when it wraps.
void add_score_caption(const char16_t* text, RuntimeScoreboardHud& context)
{
    auto& captions = captions_of(context);
    auto& globals = context.globals;
    const float scale = 0.97f; // DAT_0032B4C4
    const float x = context.layout.center_x; // DAT_00509730
    float y = context.layout.inner_bottom - globals.caption_line - 30.f; // DAT_0050974C, DAT_0032B108
    if (captions.wrap_width < runtime_text_line_width(captions.metrics, 0, text, scale)) // 0x00060710, EAX 0
    {
        y -= float(captions.fonts[0]->drop) * scale; // DAT_00504444 +0x10
    }
    create_dialogue_text_record(*globals.caption_pool, text, x, y, scale, scale, 1.5f, 1, 0, 0, captions.metrics,
        captions.viewport_width, captions.next_text_id, captions.random); // 0x0005F5C0, EAX 0
}

// Xbox 0x0012DF30, ECX the output, EAX the caption: "caption: $" and the points.
void format_score_caption(char16_t* output, const char16_t* caption, u32 points)
{
    size_t at = 0;
    for (; caption[at] != 0; ++at)
    {
        output[at] = caption[at]; // wsprintfW(output, L"%s: $", caption)
    }
    for (const char16_t c : {u':', u' ', u'$'})
    {
        output[at++] = c;
    }
    output[at] = 0;
    append_runtime_grouped_number(output, points); // 0x000FBD30
}
}

// Xbox 0x000FBD30, EAX value, EDI output
void append_runtime_grouped_number(char16_t* output, u32 value)
{
    char16_t text[0x20];
    const u32 thousands = (value / 1000u) % 1000u;
    const u32 millions = (value / 1000u / 1000u) % 1000u;
    if (millions != 0)
    {
        format_wide(text, "%u,%03u,%03u", {millions, thousands, value % 1000u}); // 0x0032613C
        append_wide(output, text);
        return;
    }
    if (thousands != 0)
    {
        format_wide(text, "%u,%03u", {thousands, value % 1000u}); // 0x0032612C
        append_wide(output, text);
        return;
    }
    format_wide(text, "%u", {value % 1000u}); // 0x00326124
    append_wide(output, text);
}

// Xbox 0x0012DCD0
void create_runtime_score_captions(RuntimeScoreboardHud& context)
{
    auto& captions = captions_of(context);
    auto& globals = context.globals;
    u32 seed = read_u32(captions.pool_setup, 8); // DAT_004326F4
    if (read_u32(captions.pool_setup, 0xC) != 0) // DAT_004326F8
    {
        captions.random = captions.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
        seed = captions.random;
    }
    allocate_dialogue_text_pool(globals.caption_pool, 0, 0, 0x4E4, captions.text_pools); // 0x0005F390, ESI DAT_00433B4C
    globals.caption_pool->origin = {read_f32(captions.pool_setup, 0), read_f32(captions.pool_setup, 4),
        std::bit_cast<float>(seed)};
    if (context.score_refresh != 2)
    {
        context.score_refresh = 2; // DAT_004326E8
    }
    globals.caption_line = float(captions.fonts[0]->drop) * 0.97f; // DAT_00504444 +0x10, DAT_0032B4C4
    context.flags |= 0x10000u; // DAT_0050EA9C
}

// Xbox 0x0012DDB0
void update_runtime_score_captions(RuntimeScoreboardHud& context)
{
    u32 next;
    if (context.score_refresh == 0)
    {
        for (size_t index = 0; index < context.score_points.size(); ++index)
        {
            u32& points = context.score_points[index];
            if (points != 0)
            {
                auto& captions = captions_of(context);
                format_score_caption(captions.scratch, captions.texts.data() + index * 0x100, points); // 0x0012DF30
                add_score_caption(captions.scratch, context); // 0x0012DFD0, ESI DAT_00507E78
                points = 0;
            }
        }
        next = 1;
    }
    else
    {
        if (context.score_refresh != 1)
        {
            return;
        }
        if (context.globals.caption_pool == nullptr)
        {
            context.score_refresh = 2;
            return;
        }
        const auto& pool = *context.globals.caption_pool->allocation; // The pool's rows, *DAT_00433B4C +4 / +8.
        if (pool.begin < pool.cursor) // A caption still shows.
        {
            return;
        }
        auto& captions = captions_of(context);
        captions.scratch[0] = 0;
        request_namespaced_localtext("aid_loctext_ghoulies_scoreboard_coreterms", "coreterms", "Total Damage: $",
            captions.scratch, captions.localtext); // 0x000FF0A0 into DAT_00507E78
        append_runtime_grouped_number(captions.scratch, captions.score_total); // 0x000FBD30, EAX DAT_0054597C
        add_score_caption(captions.scratch, context); // 0x0012DFD0
        next = 2;
    }
    if (context.score_refresh != next)
    {
        context.score_refresh = next;
    }
}

// Xbox 0x0012EA50
void draw_runtime_meter(RuntimeScoreboardHud& context)
{
    if (context.globals.meter_visible == 0)
    {
        return;
    }
    if (0 < context.difficulty && context.difficulty < 3)
    {
        return;
    }
    if (context.area_dialogue_active())
    {
        return;
    }
    auto& globals = context.globals;
    auto& draw = draw_views(context);
    const auto& layout = context.layout;
    const float half = globals.meter_bd4; // Half the gauge's width.
    const float y = layout.inner_bottom - half + 54.f; // DAT_0050974C, DAT_0032B4C0
    const float shown[2]{layout.inner_right - half + 4.5f, y}; // DAT_00509748, DAT_0032B3B0
    const float hidden[2]{float(i32(context.width)) + 100.f, y}; // DAT_005096E8, DAT_0032B12C
    const RuntimeHudFade fade{globals.meter_visible, *reinterpret_cast<float*>(&globals.meter_b8c),
        *reinterpret_cast<float*>(&globals.meter_b90), globals.meter_b94, globals.meter_b98}; // DAT_00433B88..
    float at[2];
    if (!place_runtime_hud_element(fade, hidden, shown, at)) // 0x00131F00, EBX hidden, ESI shown, EDI at
    {
        return;
    }
    u32 color = 0x00FFFFFFu;
    if (runtime_weapon_boost_active() != 0) // DAT_00512278.
    {
        const float scale[2]{1.8f, 1.8f}; // DAT_0032B4BC
        const u32 alpha = u32(truncate_runtime_float(static_cast<long double>(globals.meter_bd8) * 192.f)); // DAT_0032B454
        color = (color & 0x00FFFFFFu) | (u32(u8(alpha)) << 24);
        queue_runtime_hud_sprite(globals.meter_textures[1].data(), at, scale, 0.f, color, 0, draw); // 0x001221B0
    }
    color = (color & 0x00FFFFFFu) | 0xC0000000u;
    const float scale[2]{1.f, 1.f}; // DAT_0032B0F8
    queue_runtime_hud_sprite(globals.meter_textures[0].data(), at, scale, 0.f, color, 0, draw); // 0x001221B0
    const float level = globals.meter_level; // DAT_00433B80
    const float swing = std::bit_cast<float>(0x3FC6D3F2u); // DAT_0032B4B8, 89 degrees.
    const float turned = step_runtime_angle_toward(swing, std::bit_cast<float>(0xBFC6D3F2u), level); // 0x001013A0
    const float angle = float(static_cast<long double>(turned)
        - runtime_float_remainder(turned, std::bit_cast<float>(0x3DB2B8C2u))); // 0x00152BE0, 5 degrees; FSUBR
    if (level == 0.f) // UCOMISS DAT_0032B100.
    {
        color = (0xC00000C0u & 0x00FFFFFFu) | 0xC0000000u; // DAT_00313188.
    }
    else if (!(0.25f < level)) // DAT_0032B104; COMISS; JC keeps the gauge colour.
    {
        color = (menu_color(0xC00000C0u, 0xFFFFFFFFu, globals.meter_scales[0] - 1.f) & 0x00FFFFFFu)
            | 0xC0000000u; // 0x00101730, DAT_00313188, DAT_00314F48
    }
    queue_runtime_hud_sprite(globals.meter_textures[2].data(), at, scale, angle, color, 0, draw); // 0x001221B0
    globals.meter_drawn = 1;
}

// Xbox 0x0012D300
void draw_runtime_brew_meter(RuntimeScoreboardHud& context)
{
    if (context.globals.brew_visible != 0 && (context.difficulty < 1 || context.difficulty > 2))
    {
        untranslated("0x00131F00", "the brew meter draw 0x0012D300");
    }
}

// Xbox 0x00132290
void draw_runtime_scoreboard_hud(RuntimeScoreboardHud& context)
{
    auto& globals = context.globals;
    if (context.enabled == 0)
    {
        return;
    }
    if (context.prompt_owner == 0 && (context.flags & 4) == 0)
    {
        draw_runtime_health_display(context);
        if (globals.score_board != nullptr)
        {
            draw_runtime_scoreboard(globals.score_board, context);
        }
    }
    if (context.enabled == 0)
    {
        return;
    }
    if (context.prompt_owner == 0 && (context.flags & 0x400) == 0 && (context.flags & 4) == 0
        && globals.bonus_board != nullptr)
    {
        draw_runtime_scoreboard(globals.bonus_board, context);
    }
    // DAT_004E1C34 above 0x7FFFFFFF: no prompt holds the scoreboard.
    if (context.enabled != 0 && context.prompt_owner == 0 && context.prompt_lock < 0 && (context.flags & 0x400) == 0
        && (context.flags & 4) == 0)
    {
        draw_runtime_damage_counter(context);
        update_runtime_score_captions(context);
        draw_runtime_meter(context);
        if (context.powerup_board != nullptr)
        {
            draw_runtime_scoreboard(context.powerup_board, context);
        }
        draw_runtime_brew_meter(context);
    }
}
}
