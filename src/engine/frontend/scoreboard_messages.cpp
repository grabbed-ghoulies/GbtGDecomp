#include "scoreboard_messages.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00132620
void set_runtime_scoreboard_message_warning(u32 id, u32 warning,
    RuntimeScoreboard& board)
{
    for (auto& message : board.messages)
    {
        if (message.id == id)
        {
            message.word_18 = warning;
            return;
        }
    }
}
// Xbox 0x00131BD0
void acquire_runtime_scoreboard_icon(u32 index, std::span<const u8>& output, RuntimeScoreboardResources& context)
{
    const auto icons = acquire_animation_resource(context.registry, "aid_aidlist_ghoulies_scoreboard_icons");
    if (icons.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    if (read_u32(icons, index * 0x80) != 0x5F646961)
    {
        output = {};
        release_animation_resource(context.registry, icons);
        return;
    }
    const auto texture = acquire_animation_resource(context.registry,
        reinterpret_cast<const char*>(icons.data() + index * 0x80));
    if (texture.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    output = texture;
    release_animation_resource(context.registry, icons);
}

// Xbox 0x001316F0
u32 add_runtime_scoreboard_message(u32 kind, u32 icon, u32 word_70, i32 count, u32 word_78, u32 pulse, u32 word_14,
    RuntimeScoreboard& board, RuntimeScoreboardResources& context)
{
    if (icon == 0)
    {
        return 0;
    }
    auto& message = board.messages.emplace_back();
    message.id = board.next_id;
    message.word_78 = word_78;
    message.kind = kind;
    message.icon = icon;
    message.word_70 = word_70;
    message.word_74 = 0;
    message.word_1c = 0;
    float pulse_scale;
    if (pulse != 0)
    {
        pulse_scale = 4.f;
        message.pulse = 1;
    }
    else
    {
        pulse_scale = 0.f;
        message.pulse = 0;
    }
    message.pulse_scale = pulse_scale;
    acquire_runtime_scoreboard_icon(icon, message.texture, context);
    const u16 width = read_u16(message.texture, 8);
    message.width = width;
    message.scaled_width = float(i32(width)) * board.icon_scale;
    message.scale_x = 1.f;
    message.scale_y = 1.f;
    message.float_30 = 0.1f;
    message.float_28 = 0.f;
    message.float_2c = 0.f;
    message.float_34 = 0.f;
    message.float_38 = 4.f;
    if (board.first_icon.data() == nullptr)
    {
        acquire_runtime_scoreboard_icon(1, board.first_icon, context);
    }
    if (board.second_icon.data() == nullptr)
    {
        acquire_runtime_scoreboard_icon(2, board.second_icon, context);
    }
    message.word_8 = 0;
    message.float_10 = 0.f;
    message.float_c = 0.f;
    message.word_14 = 0;
    message.word_18 = 0;
    message.word_14 = word_14;
    auto& counter = message.counter;
    counter.value = 0;
    counter.target = 0;
    counter.word_28 = 0;
    counter.word_2c = std::bit_cast<u32>(0.4f);
    counter.timer = std::bit_cast<u32>(0.f);
    counter.digits.fill(0);
    if (count < 1)
    {
        counter.target = 0;
        counter.value = 0;
    }
    else
    {
        counter.target = count;
        counter.value = count;
    }
    update_runtime_hud_counter_digits(counter, context.counter_preset);
    ++board.next_id;
    if (board.next_id == 0xFFFFFFFF)
    {
        board.next_id = 1;
    }
    return message.id;
}

// Xbox 0x001319C0
bool remove_runtime_scoreboard_message(u32 id, RuntimeScoreboard& board, AnimationResourceRegistry& registry)
{
    auto message = board.messages.begin();
    while (message != board.messages.end())
    {
        if (message->id == id)
        {
            release_animation_resource(registry, message->texture);
            message->texture = {};
            // 0x00165AF0 moves the following rows down one stride, then the end shrinks.
            board.messages.erase(message);
            return true;
        }
        ++message;
    }
    return false;
}

// Xbox 0x00132450
u32 add_runtime_powerup_message(u32 kind, RuntimeScoreboard& board, RuntimeScoreboardResources& context)
{
    u32 icon = 0;
    switch (kind)
    {
    case 1:
    case 3:
    case 5:
    case 6:
        return 0;
    case 2:
        icon = 0x3A;
        break;
    case 4:
        icon = 0x3B;
        break;
    case 7:
        icon = 0x3C;
        break;
    case 8:
        icon = 0x3D;
        break;
    case 9:
        icon = 0x3E;
        break;
    case 0xA:
        icon = 0x3F;
        break;
    case 0xB:
        icon = 0x40;
        break;
    case 0xC:
        icon = 0x41;
        break;
    case 0xD:
        icon = 0x42;
        break;
    case 0xE:
        icon = 0x43;
        break;
    case 0xF:
        icon = 0x44;
        break;
    case 0x10:
        icon = 0x45;
        break;
    case 0x11:
        icon = 0x46;
        break;
    case 0x12:
        icon = 0x47;
        break;
    case 0x13:
        icon = 0x48;
        break;
    }
    return add_runtime_scoreboard_message(3, icon, 1, 0, 0, 0, 0, board, context);
}

// Xbox 0x00132540
void show_runtime_powerup_caption(u32 kind, RuntimePowerupCaption& context)
{
    if (kind == 1)
    {
        return;
    }
    context.scratch[0] = 0;
    if (request_namespaced_localtext("aid_loctext_ghoulies_scoreboard_powerups", "powerups",
        context.names[kind] + 10, context.scratch, context.text) == 0)
    {
        return;
    }
    if (context.pool == nullptr)
    {
        allocate_dialogue_text_pool(context.pool, 0, 0, 0x4E4, context.pools);
        context.pool->origin[0] = 2.5f;
        context.pool->origin[1] = 0.7f;
        context.pool->origin[2] = std::bit_cast<float>(0x80u);
    }
    create_dialogue_text_record(*context.pool, context.scratch, float(i32(context.hud_width)) * 0.5f,
        context.hud_bottom - 25.f, std::bit_cast<float>(0x3F7851ECu), std::bit_cast<float>(0x3F7851ECu), 1.5f,
        1, 0, 0, context.metrics, context.viewport_width, context.next_id, context.random_state);
}

// Xbox 0x0012D7E0
void show_runtime_bonus_counter(u32 slot, i32 count, RuntimeBonusCounters& counters,
    RuntimeScoreboardResources& context)
{
    if (counters.ids[slot] == 0)
    {
        // Any other slot passes the count itself as the icon.
        u32 icon = u32(count);
        u32 pulse = 0;
        if (slot == 0)
        {
            icon = 0x4A;
            if (count % 5 == 0)
            {
                pulse = 1;
            }
        }
        else if (slot == 1)
        {
            icon = 0x4B;
        }
        counters.ids[slot] = add_runtime_scoreboard_message(4, icon, 2, count - 1, 0, pulse, 0, *counters.board,
            context);
    }
    counters.active[slot] = 1;
    counters.counts[slot] = count;
    const u32 id = counters.ids[slot];
    if (id != 0)
    {
        for (auto& message : counters.board->messages)
        {
            if (message.id == id)
            {
                message.float_c = 2.f;
                return;
            }
        }
    }
}
}
