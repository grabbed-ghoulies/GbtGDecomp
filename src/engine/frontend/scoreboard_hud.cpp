#include "scoreboard_hud.h"
#include "dialogue_widget_state.h"
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

// Xbox 0x00130420
void draw_runtime_health_display(RuntimeScoreboardHud& context)
{
    const auto& globals = context.globals;
    if ((context.difficulty < 1 || context.difficulty > 2) && !context.area_dialogue_active()
        && (globals.health_shown != 0 || globals.health_pending != 0))
    {
        // The heart (or its override) and the broken-heart flag are picked before the placement 0x00131F00.
        untranslated("0x00131F00", "the health display draw 0x00130420");
    }
}

// Xbox 0x00130F20
void draw_runtime_scoreboard(RuntimeScoreboard* board, RuntimeScoreboardHud&)
{
    // The kind counts and the placements only feed the per-row draw, whose first call is 0x00131F00.
    if (!board->messages.empty())
    {
        untranslated("0x00131F00", "the scoreboard row draw 0x00130F20");
    }
}

// Xbox 0x0012DAF0
void draw_runtime_damage_counter(RuntimeScoreboardHud& context)
{
    if (context.globals.damage_caption != -1)
    {
        untranslated("0x00131F00", "the damage counter draw 0x0012DAF0");
    }
}

// Xbox 0x0012DDB0
void update_runtime_score_captions(RuntimeScoreboardHud& context)
{
    u32 next;
    if (context.score_refresh == 0)
    {
        for (const u32 points : context.score_points)
        {
            if (points != 0)
            {
                untranslated("0x0012DF30", "the score caption refresh 0x0012DDB0");
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
        // The pool's row count test, then the "Total Damage: $" caption (0x000FF0A0).
        untranslated("0x000FF0A0", "the score caption refresh 0x0012DDB0 over a live DAT_00433B4C pool");
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
    untranslated("0x00131F00", "the meter draw 0x0012EA50");
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
