#include "gameselect_delete.h"
#include "dialogue_draw_outer.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_state.h"
#include "frontend_state.h"
#include "hud_layout.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/event_runtime.h"
#include "../saves/save_games.h"
#include "../scene/executable_resources.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace control_field = gameselect_control_field;

constexpr u32 kDeletionRows = 0x003077B0; // "GameSlotDeletionMenu"

namespace field = game_deletion_field;
namespace text_line = gameselect_text_line;

GameDeletionWidget& deletion(GameSelectWidget& widget)
{
    return static_cast<GameDeletionWidget&>(widget);
}

// The game being deleted (+0x48C).
char16_t* save_name(GameDeletionWidget& w)
{
    return field_data(w.bytes, field::save_name);
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

void copy_wide(char16_t* output, const char16_t* input)
{
    while ((*output++ = *input++) != 0) // lstrcpyW
    {
    }
}

// 0x0005F5C0 with EAX 1: a centred, sticky text record for a text line.
u32 text_record(GameDeletionWidget& w, std::span<u8> line, float scale, GameSelectControlServices& s)
{
    return create_dialogue_text_record(*w.pool, field_data(line, text_line::text), get(line, text_line::x),
        get(line, text_line::y), scale, scale, 1.f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id, s.random);
}

// The control's widget request: row 1 (slot select) on the next book threshold, the book released.
void request_slot_select(GameSelectControlServices& s)
{
    SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170, EDX 0x9A
    set(control->state, control_field::requested_row, 1);
    set(control->state, control_field::book_turn, 1);
    if (control->gameselect->book != nullptr)
    {
        release_transition_book(control->gameselect->book, s.books); // 0x00048860
        control->gameselect->book = nullptr;
    }
}

// The 0x00083C40-style hold every state waits on: a scene transition, or the menu book still turning.
bool book_turning(const GameDeletionWidget& w, const GameSelectControlServices& s)
{
    if (s.transition != 0) // DAT_0054632C
    {
        return true;
    }
    const i32 book = i32(get(w.owner_state, control_field::book_turn));
    return 1 < book && book <= 3 && w.owner->book != nullptr;
}

void close_prompts(GameDeletionWidget& w, GameSelectControlServices& s)
{
    w.prompts->requested_set = nullptr; // +0x950
    set_dialogue_widget_mode(*w.prompts, 3, s.widgets); // 0x00142C20, EAX 3
}

// Xbox 0x000DBCD0: the app error screen with apperror "faildeletemessage" and its A and B buttons
// (faildeleteabutton 0x000DC0A0, faildeletebbutton 0x000DC070).
[[noreturn]] void show_delete_failure(const char16_t* name)
{
    (void)name;
    throw Error("0x00032990 (the app error screen, apperror faildeletemessage) is not translated");
}
}

// Xbox 0x000DBDC0
void set_runtime_game_deletion_state(GameDeletionWidget& widget, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = widget;
    const i32 previous = get(w.bytes, field::state);
    const auto title = record(w.bytes, field::title);
    const auto prompt = record(w.bytes, field::prompt);
    if (previous == 1)
    {
        const std::array<float, 2> anchor{s.hud.inner_left + exe_real(s, 0x0032B604),
            s.hud.inner_bottom - exe_real(s, 0x0032B1F4)}; // DAT_00509740, DAT_0050974C
        const std::array<float, 2> row_step{0.f, exe_real(s, 0x0032B600)};
        const std::array<float, 2> origin{exe_real(s, 0x0032B234), exe_real(s, 0x0032B5FC)};
        set(title, text_line::record, text_record(w, title, 1.35f, s));
        set(prompt, text_line::record, text_record(w, prompt, 1.5f, s));
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return;
        }
        create_dialogue_widget(w.prompts, 0, s.image.memory(kDeletionRows, 0x310).data(), anchor, row_step, origin,
            nullptr, 1.f, 0.75f, "gameselect", s.text, 0x400C0, s.widget_allocation); // 0x00141E20, EAX +0x590
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    }
    else if (previous == 2)
    {
        remove_dialogue_text_record(*w.pool, get(prompt, text_line::record)); // 0x0005FB10
    }
    set(w.bytes, field::state, state);
    if (state == 3)
    {
        field_data(prompt, text_line::text)[0] = 0;
        if (s.text.data() != nullptr)
        {
            lookup_borrowed_localtext(s.text, "gameselect", "gameslotdeletion_deleting",
                field_data(prompt, text_line::text), s.localtext); // 0x000FF120, EDI 0x00324D1C
        }
        set(prompt, text_line::record, text_record(w, prompt, 1.5f, s));
    }
    else if (state == 4)
    {
        end_all_dialogue_text_records(*w.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
        if (w.prompts != nullptr)
        {
            close_prompts(w, s);
        }
    }
}

// Xbox 0x000DB720
GameSelectWidgetHandle create_runtime_game_deletion(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameDeletionWidget>(); // 0x000F72D0(0x594)
}

// Xbox 0x000DB730
void open_runtime_game_deletion(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = deletion(widget);
    char page[0x80]{};
    std::memcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1", 42); // 0x00324C04
    w.owner = &control;
    w.owner_state = owner;
    s.frontend.deletion_widget = &w; // DAT_0050658C
    set(w.bytes, field::state, 0);
    w.prompts = nullptr;
    copy_wide(save_name(w), s.frontend.selected_name.data()); // lstrcpyW(+0x48C, DAT_00429CC4)
    initialize_camera_view(w.camera, "gameslotdeletion", 8); // 0x000496C0, EAX 0x00324D94
    w.camera.aspect_slot = 0; // +0x30
    i32 fit_w = 0;
    i32 fit_x = 0;
    i32 fit_h = 0;
    i32 fit_y = 0;
    float fov = 0.f;
    camera_fit_display_view(w.camera.focal, 0, fit_w, fit_x, fit_h, fit_y, fov, w.camera.pixel_aspect,
        w.camera.focal_pixels); // 0x000F57F0
    link_camera_view(0, w.camera); // The inline 0x00048C20 link and its 0x00048CF0 when slot 0 has no view.
    w.page = load_runtime_cutscene(page, 6, &w.camera, s.cutscenes); // 0x0004D680
    seek_runtime_cutscene(*w.page, s.frontend.real_0042a0cc, 0.f, 0, s.cutscenes); // 0x0004E4A0, ESI the page
    camera_bind_named_view(0, "gameslotdeletion"); // 0x00048CF0, EBX 0x00324D94
    set(w.bytes, field::saved_plate_flag_0, s.plate_flags[0]);
    set(w.bytes, field::saved_plate_flag_1, s.plate_flags[1]);
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    allocate_dialogue_text_pool(w.pool, 0, 1, 0x40080, s.text_pools); // 0x0005F390, ESI +0x41C
    w.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    const auto title = record(w.bytes, field::title);
    const auto prompt = record(w.bytes, field::prompt);
    copy_wide(field_data(title, text_line::text), save_name(w));
    set(title, text_line::x, exe_real(s, 0x0032B564));
    set(title, text_line::y, exe_real(s, 0x0032B12C));
    set(title, text_line::record, 0);
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "gameslotdeletion_deleteprompt",
            field_data(prompt, text_line::text), s.localtext); // 0x000FF120, EBX +0x210, EDI 0x00324D74
    }
    set(prompt, text_line::x, exe_real(s, 0x0032B564));
    set(prompt, text_line::y, exe_real(s, 0x0032B680));
    set(prompt, text_line::record, 0);
    set_runtime_game_deletion_state(w, 1, s); // 0x000DBDC0, EAX 1
}

// Xbox 0x000DB950
void close_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = deletion(widget);
    s.frontend.deletion_widget = nullptr; // DAT_0050658C
    release_dialogue_text_pool(w.pool, *s.text_pools); // 0x0005F460, EBX +0x41C
    w.pool = nullptr;
    if (w.prompts != nullptr)
    {
        destroy_dialogue_widget(w.prompts, s.resources, *s.text_pools); // 0x00142070
        w.prompts = nullptr;
    }
    s.frontend.real_0042a0cc = w.page->time / w.page->duration; // +0x2C over +0x34
    release_runtime_cutscene(w.page, s.cutscenes); // 0x0004D720, EAX the page
    w.page = nullptr;
    camera_unlink_view(&w.camera); // 0x00048C70, EAX +0x424
    s.plate_flags[0] = get(w.bytes, field::saved_plate_flag_0);
    s.plate_flags[1] = get(w.bytes, field::saved_plate_flag_1);
}

// Xbox 0x000DB9E0
void tick_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = deletion(widget);
    if (w.page != nullptr)
    {
        if (!(w.page->time < w.page->duration))
        {
            seek_runtime_cutscene(*w.page, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0
        }
        tick_runtime_cutscene(*w.page, frame_step(s), &w.camera, s.cutscenes); // 0x0004DA50, ESI +0x424
    }
    const i32 state = get(w.bytes, field::state);
    if (w.prompts != nullptr && u32(state) <= 5)
    {
        // The jump table at 0x000DBB74: state 2 passes the front-end device (DAT_00429BBC), the others 0.
        s.update_prompts(*w.prompts, state == 2); // 0x00142460
    }
    switch (get(w.bytes, field::state))
    {
    case 1:
        if (!book_turning(w, s))
        {
            set_runtime_game_deletion_state(w, 2, s);
        }
        return;
    case 3:
        if (!save_game_name_exists(save_name(w))) // 0x0010F530
        {
            s.frontend.selected_name[0] = 0; // lstrcpyW(DAT_00429CC4, 0x0030C9D0)
            request_slot_select(s);
            set_runtime_game_deletion_state(w, 4, s);
        }
        return;
    case 4:
        if (!book_turning(w, s))
        {
            set_runtime_game_deletion_state(w, 5, s);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000DBB90
void draw_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = deletion(widget);
    auto& display = s.display;
    if (select_display_camera(&w.camera, 0, display)) // 0x000489E0
    {
        rebuild_display_view(display); // 0x000F5E50
        extend_display_clip_limits(entity_detail::g_display_view->transform, display.scene_bounds, w.camera.blend,
            w.camera.clip_limit); // 0x0013C490
        entity_detail::g_display_view->near_clip = w.camera.clip_limit;
        entity_detail::g_display_view->far_clip = w.camera.blend;
        rebuild_display_projection(display); // 0x000F5FD0
        bind_display_projection(display); // 0x000F6170
    }
    if ((s.flags & 0x40040u) == 0 && w.page != nullptr)
    {
        const i32 view = i32(s.outer.current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        draw_runtime_cutscene(*w.page, view, s.cutscenes); // 0x0004DBD0, EBX the page
    }
    if (w.prompts != nullptr)
    {
        draw_runtime_dialogue_widget(*w.prompts, 0, s.outer); // 0x00142720
    }
}

// Xbox 0x000DBD50: B goes back to slot select.
void leave_runtime_game_deletion(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.deletion_widget; // DAT_0050658C
    close_prompts(w, s);
    request_slot_select(s);
    set_runtime_game_deletion_state(w, 4, s); // 0x000DBDC0, EAX 4
}

// Xbox 0x000DBC20: A deletes the game, then slot select comes back.
void confirm_runtime_game_deletion(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.deletion_widget; // DAT_0050658C
    const char16_t* name = save_name(w);
    if (save_game_loading(name) != 0 || save_game_name_exists(name)) // 0x0010DBA0, 0x0010F530
    {
        SaveGameContext saves{s.files, s.profile, s.scene_order_held};
        if (delete_save_game(name, saves) != 0) // 0x0010D5E0
        {
            s.frontend.selected_name[0] = 0; // lstrcpyW(DAT_00429CC4, 0x0030C9D0)
            request_slot_select(s);
            set_runtime_game_deletion_state(w, 4, s); // 0x000DBDC0, EAX 4
            close_prompts(w, s);
            return;
        }
    }
    show_delete_failure(name); // 0x000DBCD0
}
}
