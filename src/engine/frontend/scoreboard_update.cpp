#include "scoreboard_update.h"
#include "../camera/camera.h"
#include "../entities/bounded_channels.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>

namespace gbtg
{
using scene_detail::g_block_decay; // DAT_0050E768.
using scene_detail::g_decay_arm; // DAT_0030F144[DAT_0051248C].
using scene_detail::g_sim_dt; // DAT_005096C0.
using scene_detail::g_sim_freeze; // DAT_004312DC.
using scene_detail::g_sim_scale; // DAT_004312C4.

namespace
{
// DAT_00512278 is BSS no retail code writes; the meters read it as 0 and approach 0, not 1.0.
constexpr float kMeterLevelTarget = 0.f;

float frame_step()
{
    return g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f;
}

float& as_float(u32& word)
{
    return *reinterpret_cast<float*>(&word);
}

float preset_float(std::span<const u8> preset, size_t offset)
{
    return read_f32(preset, offset);
}

// 0x0012B2A0 with ECX = EAX = the timer: an ungated countdown.
void tick_timer(float& timer)
{
    const float step = frame_step();
    if (0.f < timer)
    {
        timer -= step;
        if (timer <= 0.f)
        {
            timer = 0.f;
        }
    }
}

RuntimeHudFade row_fade(RuntimeScoreboardMessage& row)
{
    return {row.word_8, row.float_c, row.float_10, row.word_14, row.word_18};
}

RuntimeHudPulse row_pulse(RuntimeScoreboardMessage& row)
{
    return {row.scale_x, row.scale_y, row.float_28, row.float_2c, row.float_30, row.float_34, row.float_38};
}

void stop_voice(u16& voice, RuntimeScoreboardUpdate& context)
{
    context.hud.stop_voice(voice); // 0x00039690 with stack 0
    voice = 0xFFFF;
}

// The voice idle test shared by 0x001308A0 / 0x001309F0 / 0x0012EE00: a released or finished cue.
bool voice_idle(u16 voice, RuntimeScoreboardUpdate& context)
{
    const auto& record = context.voices.voices.at(size_t(i16(voice)));
    if (record.cue == nullptr) // DAT_004E0D08
    {
        return true;
    }
    const u32 state = record.state; // DAT_004E0CE8
    return state != 2 && state != 3 && state != 5 && state != 6 && state != 7;
}
}

// Xbox 0x00131FC0
void set_runtime_hud_fade(RuntimeHudFade fade, u32 state, const RuntimeScoreboardUpdate& context)
{
    if (1 < fade.state && fade.state < 4)
    {
        fade.first = 0.f;
    }
    fade.state = state;
    switch (state)
    {
    case 1:
        fade.second = context.fade_in; // DAT_0032B248
        break;
    case 2:
    case 3:
        fade.second = context.hold_time; // DAT_004312CC
        break;
    case 4:
    case 6:
        fade.second = context.fade_out; // DAT_0032B244
        break;
    case 5:
        fade.second = context.leave; // DAT_0032B210
        break;
    default:
        break;
    }
}

// Xbox 0x0012B2F0
bool tick_runtime_hud_timer(float& timer)
{
    if (g_block_decay == 0 && g_decay_arm != 0)
    {
        const float step = frame_step();
        if (0.f < timer)
        {
            timer -= step;
            if (timer <= 0.f)
            {
                timer = 0.f;
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00131C50
bool step_runtime_hud_fade(RuntimeHudFade fade, i32 timed, const RuntimeScoreboardUpdate& context)
{
    const auto clear_first = [&fade]()
    {
        if (1 < fade.state && fade.state < 4)
        {
            fade.first = 0.f;
        }
    };
    const auto ended = [&]()
    {
        switch (fade.mode)
        {
        case 0:
            clear_first();
            fade.state = 4;
            fade.second = context.fade_out; // DAT_0032B244
            break;
        case 1:
            clear_first();
            fade.state = 5;
            fade.second = context.leave; // DAT_0032B210
            break;
        default:
            clear_first();
            fade.state = 0;
            break;
        }
    };
    switch (fade.state)
    {
    case 0:
        if (0.f < fade.first) // DAT_0032B100
        {
            set_runtime_hud_fade(fade, 1, context); // 0x00131FC0, EAX block, EDX 1
        }
        break;
    case 1:
        tick_timer(fade.second); // 0x0012B2A0
        if (fade.second == 0.f)
        {
            clear_first();
            fade.state = fade.hold != 0 ? 3u : 2u;
            fade.second = context.hold_time; // DAT_004312CC
        }
        break;
    case 2:
        if (timed == 0)
        {
            tick_runtime_hud_timer(fade.first); // 0x0012B2F0
        }
        else
        {
            tick_timer(fade.first); // 0x0012B2A0
        }
        tick_timer(fade.second);
        if (fade.first != 0.f)
        {
            if (fade.hold != 0)
            {
                clear_first();
                fade.state = 3;
                fade.second = context.hold_time;
            }
            break;
        }
        ended();
        break;
    case 3:
        tick_runtime_hud_timer(fade.first); // 0x0012B2F0
        tick_timer(fade.second);
        if (fade.first != 0.f)
        {
            if (fade.hold == 0)
            {
                clear_first();
                fade.state = 2;
                fade.second = context.hold_time;
            }
            break;
        }
        ended();
        break;
    case 4:
    case 6:
        tick_timer(fade.second);
        if (fade.second == 0.f)
        {
            clear_first();
            fade.state = 0;
        }
        break;
    case 5:
        tick_timer(fade.second);
        if (fade.second == 0.f)
        {
            clear_first();
            fade.state = 6;
            fade.second = context.fade_out;
        }
        break;
    default:
        break;
    }
    return fade.state != 0;
}

// Xbox 0x00104DF0
bool animate_runtime_hud_pulse(RuntimeHudPulse pulse, float dt, const std::function<void()>& callback)
{
    const float left = pulse.step_left - dt;
    const float rate = 1.f / pulse.rate; // DAT_0032B0F8
    pulse.step_left = left;
    bool stepped = false;
    if (!(0.f < left))
    {
        pulse.step_left = left + rate;
        stepped = true;
        pulse.left = pulse.length;
        if (callback)
        {
            callback();
        }
    }
    if (0.f < pulse.left)
    {
        const float angle = float(static_cast<long double>(pulse.left) / pulse.length
            * std::bit_cast<float>(0x40490FDBu)); // DAT_0032B148
        const float sine = float(retail_sin(angle)); // 0x00153320; 0x00153230's result is popped
        pulse.value = float(static_cast<long double>(sine) * pulse.amplitude * pulse.base + pulse.base);
    }
    else
    {
        pulse.value = pulse.base;
    }
    const float next = pulse.left - dt;
    pulse.left = next;
    if (next < 0.f)
    {
        pulse.left = 0.f;
    }
    return stepped;
}

// Xbox 0x0012EF60
void swell_runtime_hud_counter(RuntimeHudCounter& counter, std::span<const u8> preset)
{
    float& swell = as_float(counter.word_2c); // +0x2C
    float& time = as_float(counter.timer); // +0x30
    const float base = preset_float(preset, 0);
    if (time == 0.f) // DAT_0032B100
    {
        swell = base;
        return;
    }
    const float angle = float(static_cast<long double>(time) / preset_float(preset, 8)
        * std::bit_cast<float>(0x40490FDBu) - std::bit_cast<float>(0x3FC90FDBu)); // DAT_0032B148, DAT_0032B140
    const long double sine = retail_sin(angle); // 0x00153320
    swell = float(static_cast<long double>(preset_float(preset, 4)) * base * (sine + 1.0L) * 0.5L + base);
    const float step = frame_step();
    if (0.f < time)
    {
        time -= step;
        if (time <= 0.f)
        {
            time = 0.f;
        }
    }
}

// Xbox 0x00131B70
void settle_runtime_scoreboard_counters(RuntimeScoreboard& board, float& amplitude,
    RuntimeScoreboardUpdate& context)
{
    for (auto& row : board.messages)
    {
        const bool settled = row.counter.value == row.counter.target;
        amplitude = settled ? 0.5f : 0.f; // DAT_0032B0F4
        if (!settled)
        {
            row.counter.value = row.counter.target;
            update_runtime_hud_counter_digits(row.counter, context.row_preset); // 0x0012F990, ESI 0x00314170
        }
    }
}

// Xbox 0x00130E10
void update_runtime_scoreboard(RuntimeScoreboard& board, RuntimeScoreboardUpdate& context)
{
    for (size_t index = 0; index < board.messages.size(); ++index)
    {
        auto& row = board.messages[index];
        const bool active = (row.word_8 == 2 && context.paused != 0)
            || step_runtime_hud_fade(row_fade(row), i32(board.word_50), context); // 0x00131C50, ESI row+8
        if (active)
        {
            // DAT_00545AB4 names the board while the callback 0x00131B70 runs.
            const auto pulse = row_pulse(row);
            animate_runtime_hud_pulse(pulse, context.hud_time, [&]()
            {
                settle_runtime_scoreboard_counters(board, pulse.amplitude, context);
            }); // 0x00104DF0, ESI row+0x20, EAX 0x00131B70
            swell_runtime_hud_counter(board.messages[index].counter, context.row_preset); // 0x0012EF60
            continue;
        }
        if (row.word_1c != 0)
        {
            if (row.texture.data() != nullptr)
            {
                release_animation_resource(context.hud.registry, row.texture); // 0x00035C00, EBX +0x88
            }
            row.texture = {};
            board.messages.erase(board.messages.begin() + std::ptrdiff_t(index));
            --index;
        }
    }
}

// Xbox 0x001308A0
void release_runtime_health_voice(RuntimeScoreboardUpdate& context)
{
    auto& voice = context.hud.globals.health_voice; // DAT_00433BE4
    if (voice != 0xFFFF && voice_idle(voice, context))
    {
        stop_voice(voice, context);
    }
}

// Xbox 0x001301A0
void update_runtime_health_display(RuntimeScoreboardUpdate& context)
{
    auto& hud = context.hud;
    auto& globals = hud.globals;
    auto& health = hud.health;
    bool scary = false;
    if (hud.health_style == 1)
    {
        if (globals.health_drawn == 0) // DAT_00433C6C
        {
            if (globals.health_second_voice != 0xFFFF) // 0x001309C0
            {
                stop_voice(globals.health_second_voice, context);
            }
        }
        else
        {
            throw Error("untranslated Xbox 0x00130900 (the super-scary health loop, style 1)");
        }
    }
    else if (hud.health_style == 2)
    {
        // 0x001309F0: the second voice still playing holds the style.
        bool idle = true;
        auto& voice = globals.health_second_voice;
        if (voice != 0xFFFF)
        {
            if (!voice_idle(voice, context))
            {
                idle = false;
            }
            else
            {
                stop_voice(voice, context);
            }
        }
        if (idle)
        {
            hud.health_style = 0;
        }
    }
    bool pulse_shown = false;
    if (health.mode != 0) // DAT_00433C70
    {
        pulse_shown = true;
    }
    else if (health.active != 0 && ((hud.flags & 0x4020u) != 0 || scary)) // DAT_00433BE0
    {
        pulse_shown = true;
    }
    if (pulse_shown)
    {
        health.intensity = context.pulse; // DAT_00433C14
        acquire_runtime_player_health_textures(health); // 0x0012FE10
    }
    if (scary)
    {
        as_float(globals.health_c28[0]) = context.pulse; // DAT_00433C28
    }
    step_runtime_hud_fade({globals.health_pending, as_float(globals.health_c28[0]), as_float(globals.health_c28[1]),
        globals.health_c28[2], globals.health_c28[3]}, 0, context); // 0x00131C50, ESI DAT_00433C24
    if (!step_runtime_hud_fade({globals.health_shown, health.intensity, as_float(globals.health_c18[0]),
        globals.health_c18[1], globals.health_c18[2]}, 0, context)) // ESI DAT_00433C10
    {
        release_runtime_health_voice(context); // 0x001308A0
        globals.health_drawn = 0;
        return;
    }
    std::span<const u8> row;
    runtime_health_display_band(row, context); // 0x00130A50, EDX the row
    auto& pulse = globals.health_bf4; // DAT_00433BF4..DAT_00433C0C
    pulse[1] = read_f32(row, 8); // DAT_00433BF8
    pulse[4] = context.band_length; // DAT_00433C04 = DAT_0032B254
    pulse[6] = read_f32(row, 4); // DAT_00433C0C
    if (0 < health.counter.value) // DAT_00433C38
    {
        animate_runtime_hud_pulse({pulse[0], pulse[1], pulse[2], pulse[3], pulse[4], pulse[5], pulse[6]},
            frame_step(), [&]()
            {
                beat_runtime_health_display(pulse[2], context);
            }); // 0x00104DF0, ESI DAT_00433BF4, EAX 0x00130AC0
    }
    swell_runtime_hud_counter(health.counter, health.mode != 0 ? health.active_preset : health.normal_preset);
    globals.health_drawn = 0; // 0x0012EF60 above, ESI DAT_00433C38, EDI 0x00313A2C / 0x00313A18
}

// Xbox 0x00130A50
i32 runtime_health_display_band(std::span<const u8>& row, RuntimeScoreboardUpdate& context)
{
    auto& health = context.hud.health;
    if (health.override_health == 0 && context.hud.health_style != 1) // DAT_00512230, DAT_00433BE8
    {
        for (i32 band = 0; band < 5; ++band)
        {
            if (health.counter.value <= std::bit_cast<i32>(read_u32(health.thresholds, size_t(band) * 0x18)))
            {
                row = health.thresholds.subspan(size_t(band) * 0x18, 0x18);
                return band;
            }
        }
    }
    row = health.thresholds.subspan(4 * 0x18, 0x18); // DAT_00313AA0
    return 4;
}

// Xbox 0x00130830
void play_runtime_health_voice(const char* name, RuntimeScoreboardUpdate& context)
{
    auto& voice = context.hud.globals.health_voice; // DAT_00433BE4
    if (voice != 0xFFFF)
    {
        stop_voice(voice, context);
    }
    const i16 index = allocate_audio_event_voice(0, context.voices); // 0x00039830
    if (index != -1)
    {
        context.voices.voices.at(size_t(index)).stop = 0; // DAT_004E0CEC
        prepare_audio_event_voice(index, name, context.voices); // 0x00039A20
    }
    voice = u16(index);
}

// Xbox 0x00130AC0
void beat_runtime_health_display(float& amplitude, RuntimeScoreboardUpdate& context)
{
    auto& hud = context.hud;
    auto& health = hud.health;
    const auto preset = health.mode != 0 ? health.active_preset : health.normal_preset; // 0x00313A2C / 0x00313A18
    bool changed = false;
    if (health.counter.value == health.counter.target) // DAT_00433C38, DAT_00433C3C
    {
        if (health.mode != 0)
        {
            health.counter.timer = read_u32(preset, 8); // DAT_00433C68
        }
    }
    else
    {
        health.counter.value = health.counter.target;
        update_runtime_hud_counter_digits(health.counter, preset); // 0x0012F990, ECX DAT_00433C38, ESI preset
        changed = true;
    }
    std::span<const u8> row;
    const i32 band = runtime_health_display_band(row, context); // 0x00130A50
    if ((changed || band == 0) && hud.globals.health_drawn != 0) // DAT_00433C6C
    {
        play_runtime_health_voice(band == 0 ? "XACT_SOUNDBANK_GSUPERSCARYSTUF_HEARTBEATFAST"
            : "XACT_SOUNDBANK_GSUPERSCARYSTUF_HEARTBEAT", context); // 0x00130830
    }
    if (hud.health_style != 1)
    {
        // 0x00122D10 with EDI the stack message {0x90}: each DAT_00511B40 player's vtable +0xC.
        std::array<u32, 0x42> message{};
        message[0] = 0x90;
        context.message_players(message);
    }
    amplitude = read_f32(row, changed ? 0x10 : 0xC);
}

// Xbox 0x001323E0
void update_runtime_powerup_board(RuntimeScoreboardUpdate& context)
{
    auto& hud = context.hud;
    if (hud.powerup_board != nullptr)
    {
        for (auto& row : hud.powerup_board->messages)
        {
            if (row.kind == 3)
            {
                row.float_c = context.pulse; // DAT_0032B110
            }
        }
        update_runtime_scoreboard(*hud.powerup_board, context); // 0x00130E10
    }
    auto*& pool = hud.globals.award_pool; // DAT_00545AB8
    if (pool != nullptr && pool->records.empty())
    {
        hud.release_text_pool(pool); // 0x0005F460
        pool = nullptr;
    }
}

// Xbox 0x0012D690
void update_runtime_bonus_board(RuntimeScoreboardUpdate& context)
{
    auto& globals = context.hud.globals;
    const bool challenge = (context.scene_flags & 8) == 0 && (context.hud.flags & 0x200000u) == 0
        && context.challenge_mode == 2;
    if (globals.bonus_board != nullptr)
    {
        auto& board = *globals.bonus_board;
        update_runtime_scoreboard(board, context); // 0x00130E10
        for (size_t slot = 0; slot < 2; ++slot)
        {
            for (auto& row : board.messages)
            {
                if (row.id == globals.bonus_ids[slot])
                {
                    if (row.word_8 == 2 && globals.bonus_active[slot] != 0)
                    {
                        set_runtime_scoreboard_count(globals.bonus_ids[slot], globals.bonus_counts[slot], board,
                            context.row_preset); // 0x001318A0, ECX the id, EDX the count
                        globals.bonus_active[slot] = 0;
                        for (auto& again : board.messages)
                        {
                            if (again.id == globals.bonus_ids[slot])
                            {
                                again.word_1c = 1;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    if (challenge)
    {
        throw Error("the challenge bonus row (0x0012D690 with DAT_00506C58 == 2) is not translated");
    }
}

// Xbox 0x0012D290
void update_runtime_brew_prompt(RuntimeScoreboardUpdate& context)
{
    auto& globals = context.hud.globals;
    auto& brew = context.hud.brew;
    if (step_runtime_hud_fade({globals.brew_visible, brew.intensity, as_float(globals.brew_99c),
        globals.brew_9a0, globals.brew_9a4}, 0, context)) // 0x00131C50, ESI DAT_00545994
    {
        throw Error("untranslated Xbox 0x0012D540 / 0x00142460 (the brew prompt widget, 0x0012D290)");
    }
    if (brew.widget == nullptr) // DAT_005459F4
    {
        return;
    }
    context.hud.destroy_widget(brew.widget); // 0x00142070
    brew.widget = nullptr;
}

// Xbox 0x0012DA10
void update_runtime_damage_counter(RuntimeScoreboardUpdate& context)
{
    auto& globals = context.hud.globals;
    if (context.paused == 0)
    {
        if (globals.damage_6e0 != 0)
        {
            --globals.damage_6e0;
        }
    }
    else if (globals.damage_6e0 != 0)
    {
        globals.damage_690 = context.pulse; // DAT_00432690
    }
    if (!step_runtime_hud_fade({globals.damage_68c, globals.damage_690, as_float(globals.damage_694),
        globals.damage_698, globals.damage_69c}, 0, context)) // 0x00131C50, ESI DAT_0043268C
    {
        return;
    }
    globals.damage_6e4 = approach_runtime_scalar(globals.damage_6e4, kMeterLevelTarget,
        frame_step()); // 0x00101B90
    swell_runtime_hud_counter(globals.damage_counter, context.damage_preset); // 0x0012EF60, ESI DAT_004326A0
}

// Xbox 0x0012E070
void update_runtime_meter_view(RuntimeScoreboardUpdate& context)
{
    // 0x0012E710 returns false while DAT_00545A18 is clear.
    if (context.hud.globals.word_a18 == 0)
    {
        return;
    }
    throw Error("untranslated Xbox 0x0012E710 / 0x0005C1A0 (the meter view with DAT_00545A18 set, 0x0012E070)");
}

// Xbox 0x0012ECC0
void reset_runtime_meter(RuntimeScoreboardUpdate& context)
{
    auto& globals = context.hud.globals;
    globals.meter_b90 = 0; // DAT_00433B90
    globals.meter_b8c = 0; // DAT_00433B8C
    globals.meter_visible = 0; // DAT_00433B88
    globals.meter_b94 = 0; // DAT_00433B94
    globals.meter_b98 = 0; // DAT_00433B98
    globals.meter_level = 1.f; // DAT_00433B80, DAT_0032B0F8
    globals.meter_drawn = 0; // DAT_00433BCC
    globals.meter_bd0 = 0; // DAT_00433BD0
    if (globals.meter_voice != 0xFFFF)
    {
        stop_voice(globals.meter_voice, context); // 0x00039690
        globals.meter_voice = 0xFFFF;
    }
}

// Xbox 0x0012EE00
void release_runtime_meter_voice(RuntimeScoreboardUpdate& context)
{
    auto& voice = context.hud.globals.meter_voice; // DAT_00433B84
    if (voice != 0xFFFF && voice_idle(voice, context))
    {
        stop_voice(voice, context);
    }
}

// Xbox 0x0012E900
void update_runtime_meter(RuntimeScoreboardUpdate& context)
{
    auto& globals = context.hud.globals;
    if (context.paused == 0)
    {
        if (globals.meter_bc8 != 0)
        {
            --globals.meter_bc8;
        }
    }
    else if (globals.meter_bc8 != 0)
    {
        as_float(globals.meter_b8c) = context.pulse; // DAT_00433B8C
    }
    if (!step_runtime_hud_fade({globals.meter_visible, as_float(globals.meter_b8c), as_float(globals.meter_b90),
        globals.meter_b94, globals.meter_b98}, 0, context)) // 0x00131C50, ESI DAT_00433B88
    {
        release_runtime_meter_voice(context); // 0x0012EE00
        return;
    }
    throw Error("untranslated Xbox 0x0012ED30 path: the shown meter (0x0012E900)");
}

// Xbox 0x00132210
void update_runtime_scoreboard_hud(RuntimeScoreboardUpdate& context)
{
    auto& hud = context.hud;
    if (hud.enabled == 0)
    {
        return;
    }
    if (hud.prompt_owner == 0 && (hud.flags & 4) == 0)
    {
        update_runtime_health_display(context); // 0x001301A0
        if (hud.globals.score_board != nullptr)
        {
            update_runtime_scoreboard(*hud.globals.score_board, context); // 0x00130E10
        }
        update_runtime_powerup_board(context); // 0x001323E0
        update_runtime_bonus_board(context); // 0x0012D690
    }
    if (hud.enabled != 0 && hud.prompt_owner == 0 && hud.prompt_lock < 0 && (hud.flags & 0x400) == 0
        && (hud.flags & 4) == 0)
    {
        update_runtime_brew_prompt(context); // 0x0012D290
        update_runtime_damage_counter(context); // 0x0012DA10
        update_runtime_meter_view(context); // 0x0012E070
        update_runtime_meter(context); // 0x0012E900
    }
}

// Xbox 0x0012D8A0
void load_runtime_scoreboard_weapon_icons(RuntimeScoreboardGlobals& globals, AnimationResourceRegistry& registry,
    ResourceErrorState& error)
{
    if (globals.damage_texture.data() == nullptr)
    {
        const auto texture = acquire_animation_resource(registry,
            "aid_texture_ghoulies_scoreboard_eventicons_weapononly"); // 0x00035E90
        if (texture.data() == nullptr)
        {
            resource_error(error, 2); // 0x00032700
        }
        globals.damage_texture = texture;
        globals.damage_6d8 = float(read_u16(texture, 8)) * 0.5f; // DAT_0032B0F4.
    }
    if (globals.damage_flash_texture.data() == nullptr)
    {
        const auto texture = acquire_animation_resource(registry, "aid_texture_particles_impheadglow");
        if (texture.data() == nullptr)
        {
            resource_error(error, 2);
        }
        globals.damage_flash_texture = texture;
        globals.damage_6e4 = 0.f;
    }
}
}
