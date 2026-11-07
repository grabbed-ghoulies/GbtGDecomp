#include "gameselect_stream_wait.h"
#include "dialogue_draw_outer.h"
#include "dialogue_text_pool.h"
#include "frontend_sounds.h"
#include "frontend_state.h"
#include "hud_layout.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../entities/ghouly_spawn_record.h"
#include "../scene/executable_resources.h"
#include "../scene/gameselect_control.h"

#include <bit>

namespace gbtg
{
extern i32 g_transition_pause_count; // DAT_004DEA24, scene_transitions.cpp.

namespace
{
GameStreamWaitWidget& stream_wait(GameSelectWidget& widget)
{
    return static_cast<GameStreamWaitWidget&>(widget);
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale; // DAT_005096C0 * DAT_004312C4
    }
    return 0.f;
}

u32 line_record(GameStreamWaitWidget& w, GameSelectControlServices& s)
{
    return create_dialogue_text_record(*w.pool, w.line.text.data(), w.line.x, w.line.y, 1.5f, 1.5f, 1.f, 1, 0, 1,
        s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
}

// The pool's records go at once (its cursor back to its first row).
void clear_pool(GameStreamWaitWidget& w)
{
    w.pool->allocation->cursor = w.pool->allocation->begin; // *(+0x228) +8 = +4
}

// Xbox 0x000E0AD0, ESI the widget, EAX the state
void set_stream_wait_state(GameStreamWaitWidget& w, i32 state, GameSelectControlServices& s)
{
    if (w.state == 1)
    {
        clear_pool(w);
    }
    w.state = state;
    if (state == 1)
    {
        w.line.record = line_record(w, s);
        w.shown = 1;
        w.blink = 1.f; // DAT_0032B0F8.
        w.remaining = w.wait;
    }
}
}

// Xbox 0x000E0650
GameSelectWidgetHandle create_runtime_game_stream_wait(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameStreamWaitWidget>(); // 0x000F72D0(0x250)
}

// Xbox 0x000E0660
void open_runtime_game_stream_wait(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = stream_wait(widget);
    (void)owner;
    w.saved_row = s.frontend.word_005065bc; // DAT_005065BC
    w.wait = s.stream_wait; // DAT_005065C4
    w.saved_turn = s.frontend.word_005065c0; // DAT_005065C0
    w.state = 0;
    w.owner = &control;
    w.line.record = 0;
    w.line.x = 0.f;
    w.line.y = 0.f;
    w.line.text[0] = 0;
    s.frontend.stream_wait_widget = &w; // DAT_005065C8
    s.frontend.word_005065bc = 0;
    s.frontend.word_005065c0 = 0;
    s.stream_wait = 0.f;
    w.blink = 0.f;
    w.shown = 0;
    w.remaining = 0.f;
    w.picture.texture = acquire_animation_resource(s.resources, "aid_texture_ghoulies_gameselect_loading"); // 0x00035E90
    if (w.picture.texture.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700(2)
    }
    w.picture.color = 0xFFFFFFFFu; // DAT_00314F48.
    w.picture.position = {s.hud.center_x, s.hud.center_y}; // DAT_00509730, DAT_00509734
    const float one = 1.f; // DAT_0032B0F8.
    w.picture.scale = {one, one};
    allocate_dialogue_text_pool(w.pool, 0, 1, 0x1C0, s.text_pools); // 0x0005F390, ESI +0x228
    w.pool->origin = {3.f, 0.7f, std::bit_cast<float>(3u)}; // DAT_0032B260, DAT_0032B264.
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "loading", w.line.text.data(),
            s.localtext); // 0x000FF120, EBX +0x1C, EDI 0x0032503C
    }
    w.line.record = 0;
    w.line.x = s.hud.center_x;
    w.line.y = s.hud.center_y;
    w.line.y = s.hud.inner_bottom * 0.75f; // DAT_0050974C * 0.75, DAT_0032B2AC.
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    stop_pause_ditty(s.ditty, s.streams); // 0x000E5B70
    ++g_transition_pause_count; // DAT_004DEA24
    set_stream_wait_state(w, 1, s); // 0x000E0AD0(1)
}

// Xbox 0x000E0830
void close_runtime_game_stream_wait(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = stream_wait(widget);
    --g_transition_pause_count; // DAT_004DEA24
    if (w.saved_row != 0)
    {
        start_pause_ditty(s.ditty, s.streams); // 0x00038910(1) while DAT_00429BB8 is clear
    }
    s.frontend.stream_wait_widget = nullptr; // DAT_005065C8
    release_dialogue_text_pool(w.pool, *s.text_pools); // 0x0005F460
    w.pool = nullptr;
    if (w.picture.texture.data() != nullptr)
    {
        release_animation_resource(s.resources, w.picture.texture); // 0x00035C00
        w.picture.texture = {};
    }
    s.frontend.word_005065bc = 0;
    s.frontend.word_005065c0 = 0;
    s.plate_flags[0] = w.saved_plate_flag_0;
    s.plate_flags[1] = w.saved_plate_flag_1;
}

// Xbox 0x000E08C0
void tick_runtime_game_stream_wait(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = stream_wait(widget);
    if (w.state != 1)
    {
        return;
    }
    // The line blinks a second on and a second off.
    if (0.f < w.blink)
    {
        w.blink -= frame_step(s);
        if (w.blink <= 0.f)
        {
            w.blink = 0.f;
            if (w.shown == 0)
            {
                w.line.record = line_record(w, s);
                w.shown = 1;
            }
            else
            {
                end_dialogue_text_record(*w.pool, w.line.record, s.fonts, s.fallback_texture, s.random); // 0x0005F7E0
                w.shown = 0;
            }
            w.blink = 1.f; // DAT_0032B0F8.
        }
    }
    auto& open = *s.frontend.stream_wait_widget; // DAT_005065C8
    if (open.wait == 0.f)
    {
        if (s.stream_busy(2) == 0) // 0x00039D50(2, 0, 0)
        {
            open.owner->start_ready = 1; // +0x128
            set_stream_wait_state(open, 2, s);
        }
        return;
    }
    if (0.f < open.remaining)
    {
        open.remaining -= frame_step(s);
        if (open.remaining <= 0.f)
        {
            open.remaining = 0.f;
            request_runtime_gameselect_row(u32(s.next_row), 1, 0, s); // 0x00083A50(0), EDI DAT_00429BC0, EBX 1
            set_stream_wait_state(open, 2, s);
        }
    }
}

// Xbox 0x000E0A30
void draw_runtime_game_stream_wait(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = stream_wait(widget);
    if (-1 < w.state && w.state < 3 && w.picture.texture.data() != nullptr)
    {
        draw_runtime_dialogue_sprite(w.picture.texture.data(), w.picture.position.data(), w.picture.scale.data(), 0.f,
            w.picture.color, 0, s.draw); // 0x0005CE30
    }
}
}
