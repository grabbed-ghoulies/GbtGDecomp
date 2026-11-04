#include "player_strategy.h"
#include "strategy_fields.h"
#include "body_action_ids.h"
#include "../scripts/script_chapter.h"
#include "body_fields.h"
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
bool actor_tick_timer(const Actor& actor, float& timer, float freeze, float dt,
    float scene_scale); // Xbox 0x00042360

namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimePlayerStrategyFlow& context)
{
    return context.base.goals;
}

void move_state(u16 state, RuntimePlayerStrategyFlow& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    change_runtime_player_strategy_state(goals.strategy, state, goals.setup);
}

void move_state(u16 state, RuntimePlayerStrategyGoals& goals)
{
    change_runtime_player_strategy_state(goals.strategy, state, goals.setup);
}

// Row of DAT_0051108C whose word +6 matches; rows advance by their leading size dword.
const u8* find_script_record(const void* records, i16 id)
{
    const auto* table = static_cast<const u8*>(records);
    u32 count;
    std::memcpy(&count, table, 4);
    const u8* row = table + 8;
    for (u32 index = 0; index < count; ++index)
    {
        if (i16(read_u16({row, 8}, 6)) == id)
        {
            return row;
        }
        row += read_u32({row, 4}, 0);
    }
    return nullptr;
}

u16 axis_magnitude_exceeded(const std::array<float, 2>& axes)
{
    return axes[1] * axes[1] + axes[0] * axes[0] > 0.01f ? 1 : 0; // DAT_0032B22C.
}

Actor* nearest_threat(RuntimePlayerStrategyFlow& context)
{
    float distance;
    return find_nearest_runtime_script_actor(nullptr, goals_of(context).entry.actor, 0, 100.0f, &distance, 0x80241,
        nullptr, context.base.query);
}

void play_prompt_sound(const char* name, RuntimePlayerStrategyFlow& context)
{
    request_spatial_audio_event_voice(name, 1, 0.0f, context.voices);
}
}

// Xbox 0x000BEDB0
u32 runtime_brew_prompt_available(RuntimePlayerStrategyFlow& context)
{
    if (context.chapter_mode == 1 && (context.unlock_flags & 8) != 0 && (context.scene_flags & 8) == 0
        && (goals_of(context).runtime_flags & 0x204000) == 0)
    {
        if (script_chapter_index(context.scene_script) < 8)
        {
            return context.scene_kills >= context.brew_kills_required ? 1 : 0;
        }
    }
    return 0;
}

// Xbox 0x000BF4D0
void queue_runtime_scripted_goals(std::span<const u32> message, RuntimePlayerStrategyFlow& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    i16 id = i16(message[1]);
    if (id == 0)
    {
        return;
    }
    do
    {
        const u8* record = find_script_record(goals.script_records, id);
        // A missing record is read through address zero, as retail does.
        std::span<const u8> source = record != nullptr ? std::span<const u8>(record, 0x24)
                                                       : goals.entry.address_zero;
        u32 words[9]{};
        words[0] = 0x4D;
        for (size_t index = 0; index < 6; ++index)
        {
            words[2 + index] = read_u32(source, 0xC + 4 * index);
        }
        words[1] = 8;
        const u8* again = find_script_record(goals.script_records, id);
        const i16 following = again != nullptr ? i16(read_u16({again, 0xA}, 8)) : 0;
        words[8] = following != 0 ? 1 : 0;
        goals.cleanup.actor.message(words);
        const u8* current = find_script_record(goals.script_records, id);
        if (current == nullptr)
        {
            return;
        }
        id = i16(read_u16({current, 0xA}, 8));
    } while (id != 0);
}

// Xbox 0x0013A490
u32 check_runtime_player_knockout(RuntimePlayerStrategyGoals& context)
{
    std::span<const u8> body = context.entry.body;
    if (get(body, body_field::energy) == 0 && get(body, body_field::max_energy) != 0)
    {
        std::span<u8> strategy = context.entry.strategy;
        const u16 state = strategy_base(strategy).state;
        if (state != 1000 && state != 0x3FC)
        {
            drain_runtime_player_goals(context.cleanup);
            strategy_base(strategy).unk_18 = 0x3FC;
            strategy_base(strategy).resume_state = 0x3FC;
            strategy_base(strategy).flagged_resume_state = 0x3FC;
            move_state(0x3FC, context);
        }
        return 1;
    }
    return 0;
}

// Xbox 0x000D0690
u32 runtime_difficulty_allows_rest(const i32& difficulty)
{
    return difficulty > 0 && difficulty <= 2 ? 1 : 0;
}

// Xbox 0x000BF600
u32 runtime_player_idle_elapsed(RuntimePlayerStrategyFlow& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = goals.entry.strategy;
    if ((goals.body_bits->bytes[6] & 1) == 0 || nearest_threat(context) != nullptr)
    {
        put_float(strategy, 0x4C, 0.0f);
        return 0;
    }
    const float step = runtime_frame_step(goals.clock);
    const float rate = runtime_actor_rate(goals.entry.actor);
    const float elapsed = step * rate + read_f32(strategy, 0x4C);
    put_float(strategy, 0x4C, elapsed);
    return elapsed >= 30.0f ? 1 : 0; // DAT_0032B108.
}

// Xbox 0x000BF6A0
u32 runtime_player_wants_to_move(RuntimePlayerStrategyFlow& context)
{
    std::array<float, 2> axes;
    runtime_input_axes(context.input_device, axes, context.input_state);
    if (axis_magnitude_exceeded(axes) != 0)
    {
        return 1;
    }
    runtime_input_secondary_axes(context.input_device, axes, context.input_state);
    if (axis_magnitude_exceeded(axes) != 0)
    {
        return 1;
    }
    if (runtime_input_button_edge(context.input_device, 0xF00, context.input_state) != 0)
    {
        return 1;
    }
    return nearest_threat(context) != nullptr ? 1 : 0;
}

// Xbox 0x0013FF30
void unlock_runtime_brew_intro(RuntimePlayerStrategyFlow& context)
{
    const RuntimeIntroNotices notices{goals_of(context).runtime_flags, context.scene_flags, context.intro_pending,
        context.intro_name, context.cases, context.case_body_bits};
    if (runtime_intro_notice_ready(notices) == 0 || (context.story_flags & 4) != 0)
    {
        return;
    }
    context.story_flags |= 4;
    const char* name = "intros_brewaccept"; // 0x00315074.
    size_t index = 0;
    char value;
    do
    {
        value = name[index];
        context.intro_name[index] = value;
        ++index;
    } while (value != '\0');
    context.intro_pending = 1;
}

// Xbox 0x000BEE10
void dispatch_runtime_player_strategy_message(std::span<const u32> message, RuntimePlayerStrategyFlow& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = goals.entry.strategy;
    switch (message[0])
    {
    case 0x27:
        if (strategy_base(strategy).state == 0x3F1)
        {
            return;
        }
        break;
    case 0x2C:
    case 0x82:
        put(strategy, 0x3C, 1);
        break;
    case 0x6E:
        check_runtime_player_knockout(goals);
        break;
    case 0x7D:
        if (runtime_brew_prompt_available(context) != 0)
        {
            put(strategy, 0x40, 1);
        }
        break;
    case 0xB1:
        queue_runtime_scripted_goals(message, context);
        break;
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, context.base);
}

// Xbox 0x000BF2B0
void enter_runtime_player_strategy_state(u16 state, RuntimePlayerStrategyFlow& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = goals.entry.strategy;
    RuntimeMindGoalContext& mind_goals = goals.entry.goals;
    switch (state)
    {
    case 0x3F1:
        put_float(strategy, 0x50, 5.0f); // DAT_0032B184.
        ++strategy_base(strategy).pause_count;
        change_runtime_actor_pause(u8(0xFFFF), 0, context.pause);
        ++context.pause_count;
        break;
    case 0x3F2:
        change_runtime_actor_pause(1, 0, context.pause);
        break;
    case 0x3FC:
        put_float(strategy, 0x4C, 0.0f);
        allocate_runtime_mind_request(mind_goals.anchor, 0x16, mind_goals.sizes);
        if (read_u32(strategy, 0x48) == 0)
        {
            ++context.pause_count;
            put(strategy, 0x48, 1);
        }
        break;
    case 0x407:
        load_runtime_mind_goal(context.goal_idle, mind_goals);
        break;
    case 0x408:
        put_float(strategy, 0x4C, 0.0f);
        allocate_runtime_mind_request(mind_goals.anchor, 0x16, mind_goals.sizes);
        break;
    default:
        break;
    }
    enter_runtime_base_strategy_state(goals.entry, state);
}

// Xbox 0x000BEF40
void update_runtime_player_strategy(RuntimePlayerStrategyFlow& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = goals.entry.strategy;
    std::span<u8> body = goals.entry.body;
    const u16 state = strategy_base(strategy).state;
    if (read_u32(strategy, 0x38) != 0)
    {
        if ((goals.body_bits->bytes[6] & 1) != 0)
        {
            put_u16(body, 0x30, u16(get(body, body_field::move_holds) - 1));
            put(strategy, 0x38, 0);
        }
        if (read_u32(strategy, 0x38) != 0)
        {
            reset_runtime_input_device(goals.input, goals.input_release);
        }
    }
    update_runtime_base_strategy(goals);
    switch (state)
    {
    case 0x3F1:
    {
        static constexpr std::array<float, 2> kZeroAxes{}; // DAT_0030D390.
        set_runtime_input_trigger_axes(context.axis_device, kZeroAxes, context.keyboard);
        set_runtime_input_axes(context.axis_device, kZeroAxes, context.keyboard);
        set_runtime_input_secondary_axes(context.axis_device, kZeroAxes, context.keyboard);
        if (context.prompt_lock > -1 || context.prompt_owner != 0)
        {
            return;
        }
        i32 count = _mm_cvttss_si32(_mm_set_ss(read_f32(strategy, 0x50) + 1.0f));
        if (count > 5)
        {
            count = 5;
        }
        show_runtime_brew_count(count, context.brew);
        set_runtime_player_health_display(context.health, i32(get(body, body_field::energy)), 0);
        context.health.intensity = 2.0f; // DAT_0032B110.
        acquire_runtime_player_health_textures(context.health);
        if (runtime_input_button_edge(context.input_device, 0x100, context.input_state) != 0)
        {
            play_prompt_sound("XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON", context);
            dispatch_actor_resource_callout(context.resource, 1, nullptr, context.callout);
            ++context.brews_taken;
            context.brew_accepted = 1;
            accept_runtime_brew_meter(context.brew);
            put_u16(body, 0x94, u16(get(body, body_field::energy) << 1));
            move_state(0x408, context);
            change_runtime_actor_pause(u8(0xFFFF), 1, context.pause);
            unlock_runtime_brew_intro(context);
            return;
        }
        float timer = read_f32(strategy, 0x50);
        const bool expired = actor_tick_timer(goals.entry.actor, timer, goals.clock.freeze, goals.clock.dt,
            goals.clock.scale);
        put_float(strategy, 0x50, timer);
        if (!expired && runtime_input_button_edge(context.input_device, 0x200, context.input_state) == 0)
        {
            return;
        }
        if (runtime_input_button_edge(context.input_device, 0x200, context.input_state) != 0)
        {
            play_prompt_sound("XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON", context);
            dispatch_actor_resource_callout(context.resource, 0x13, nullptr, context.callout);
        }
        else
        {
            play_prompt_sound("XACT_SOUNDBANK_GSUPERSCARYSTUF_WRONGBUTTON", context);
        }
        context.brew.intensity = 2.0f; // DAT_0032B110.
        context.brew.counter.target = 0;
        context.brew.counter.value = 0;
        update_runtime_hud_counter_digits(context.brew.counter, context.brew.preset);
        close_runtime_brew_meter(context.brew);
        move_state(0x408, context);
        change_runtime_actor_pause(u8(0xFFFF), 1, context.pause);
        return;
    }
    case 0x3F2:
    case 0x3FC:
        reset_runtime_input_device(goals.input, goals.input_release);
        return;
    case 0x407:
        if (runtime_player_wants_to_move(context) != 0)
        {
            move_state(0x408, context);
            return;
        }
        if (goals.setup.mind->first == nullptr)
        {
            load_runtime_mind_goal(context.goal_idle, goals.entry.goals);
        }
        return;
    case 0x408:
        if (check_runtime_player_knockout(goals) != 0)
        {
            put(strategy, 0x40, 0);
            return;
        }
        if (read_u32(strategy, 0x40) != 0)
        {
            put(strategy, 0x40, 0);
            strategy_base(strategy).after_wait_state = 0x3F1;
            strategy_base(strategy).unk_18 = 0x3F1;
            move_state(0x3F1, context);
            return;
        }
        if (read_u32(strategy, 0x3C) != 0)
        {
            if (!runtime_body_timer_is_idle(body))
            {
                return;
            }
            if (context.difficulty != 3 && runtime_difficulty_allows_rest(context.difficulty) == 0)
            {
                move_state(0x3F2, context);
            }
            put(strategy, 0x3C, 0);
            return;
        }
        if (runtime_player_idle_elapsed(context) != 0)
        {
            move_state(0x407, context);
        }
        return;
    default:
        return;
    }
}
}
