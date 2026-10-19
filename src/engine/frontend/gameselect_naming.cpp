#include "gameselect_naming.h"
#include "dialogue_draw_outer.h"
#include "frontend_state.h"
#include "virtual_keyboard.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/event_runtime.h"
#include "../scene/attract_control.h"
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

GameNamingWidget& naming(GameSelectWidget& widget)
{
    return static_cast<GameNamingWidget&>(widget);
}

// The frame step: nothing while frozen (DAT_004312DC > 0), else DAT_005096C0 * DAT_004312C4.
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
    while ((*output++ = *input++) != 0)
    {
    }
}
}

namespace
{
// 0x000DD2F0, 0x000DD3A0, 0x000DD420 and 0x000DD4A0: the save errors, each an apperror message for the name
// shown by the app error screen (0x00032990).
SaveCallback naming_error(const char* key)
{
    return [key](const void*)
    {
        throw Error(std::string("0x00032990 (the app error screen, apperror ") + key + ") is not translated");
    };
}

// The DD210 / DD6C0 save of the widget's name in slot DAT_0042A0C4 (0 when DAT_0042A0C8 is clear).
i32 save_new_game(GameNamingWidget& w, GameSelectControlServices& s)
{
    const u8 slot = s.frontend.word_0042a0c8 != 0 ? u8(s.frontend.word_0042a0c4) : u8(0);
    const SaveCreateCallbacks callbacks{naming_error("failnaming_toomanysavesmessage"), // 0x000DD4A0 in EAX
        naming_error("failnaming_createspacemessage"), // 0x000DD2F0
        naming_error("failnaming_runagainmessage"), // 0x000DD3A0
        naming_error("failnaming_corruptgamemessage")}; // 0x000DD420
    SaveGameContext saves{s.files, s.profile, s.scene_order_held};
    return create_save_game(w.name.data(), slot, callbacks, s.save_image, saves); // 0x0010D430, ECX +0x74
}
}

// Xbox 0x000DD6C0
void set_runtime_game_naming_state(GameNamingWidget& widget, i32 state, GameSelectControlServices& services)
{
    auto& w = widget;
    w.state = state;
    if (state == 2)
    {
        if (w.keyboard == nullptr)
        {
            w.keyboard = create_virtual_keyboard(w.name.data(), services); // 0x000110D0
        }
    }
    else if (state == 3)
    {
        if (save_new_game(w, services) != 0)
        {
            finish_runtime_game_naming(services); // 0x000DD270
        }
    }
}

// Xbox 0x000DD210
void commit_runtime_game_name(const char16_t* name, GameSelectControlServices& services)
{
    auto& w = *services.frontend.naming_widget; // DAT_00506594
    copy_wide(w.name.data(), name); // lstrcpyW(+0x74, name)
    w.state = 3;
    if (save_new_game(w, services) != 0)
    {
        finish_runtime_game_naming(services); // 0x000DD270
    }
}

// Xbox 0x000DD270: the new game becomes the selected one and slot select comes back.
void finish_runtime_game_naming(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.naming_widget; // DAT_00506594
    copy_wide(s.frontend.selected_name.data(), w.name.data()); // lstrcpyW(DAT_00429CC4, +0x74)
    const u8 slot = save_game_slot(w.name.data()); // 0x0010DB60, EBX +0x74
    copy_wide(s.frontend.slot_names[slot].data(), w.name.data()); // DAT_00429DC4 + slot * 0x100
    SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170
    set(control->state, control_field::requested_row, 1);
    set(control->state, control_field::book_turn, 1);
    if (control->gameselect->book != nullptr)
    {
        release_transition_book(control->gameselect->book, s.books); // 0x00048860
        control->gameselect->book = nullptr;
    }
    set_runtime_game_naming_state(w, 4, s); // 0x000DD6C0
}

// Xbox 0x000DD550
void leave_runtime_game_naming(GameSelectControlServices& services)
{
    auto& s = services;
    s.frontend.selected_name[0] = 0; // lstrcpyW(DAT_00429CC4, 0x0030C9D0), an empty string.
    if (s.frontend.word_0042a0c8 != 0)
    {
        s.frontend.slot_names[size_t(s.frontend.word_0042a0c4)][0] = 0;
    }
    SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170, EDX 0x9A
    set(control->state, control_field::requested_row, 1);
    set(control->state, control_field::book_turn, 1);
    if (control->gameselect->book != nullptr)
    {
        release_transition_book(control->gameselect->book, s.books); // 0x00048860
        control->gameselect->book = nullptr;
    }
    set_runtime_game_naming_state(*s.frontend.naming_widget, 4, s); // 0x000DD6C0, DAT_00506594
}

// Xbox 0x000DCEB0
GameSelectWidgetHandle create_runtime_game_naming(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameNamingWidget>(); // 0x000F72D0(0x178)
}

// Xbox 0x000DCEC0
void open_runtime_game_naming(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = naming(widget);
    char page[0x80]{};
    std::memcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1", 42); // 0x00324C04
    w.owner = &control;
    w.owner_state = owner;
    s.frontend.naming_widget = &w; // DAT_00506594
    w.state = 0;
    w.keyboard = nullptr;
    w.name[0] = 0;
    initialize_camera_view(w.camera, "gameslotnaming", 8); // 0x00324F20
    w.camera.aspect_slot = 0;
    i32 fit_w = 0;
    i32 fit_x = 0;
    i32 fit_h = 0;
    i32 fit_y = 0;
    float fov = 0.f;
    camera_fit_display_view(w.camera.focal, 0, fit_w, fit_x, fit_h, fit_y, fov, w.camera.pixel_aspect,
        w.camera.focal_pixels); // 0x000F57F0
    link_camera_view(0, w.camera); // 0x00048C20, inline.
    w.page = load_runtime_cutscene(page, 6, &w.camera, s.cutscenes); // 0x0004D680
    seek_runtime_cutscene(*w.page, s.frontend.real_0042a0cc, 0.f, 0, s.cutscenes); // 0x0004E4A0, ESI the page
    camera_bind_named_view(0, "gameslotnaming"); // 0x00048CF0, EBX the name
    w.plate_flags = {s.plate_flags[0], s.plate_flags[1]};
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    const char16_t* text = s.lookup_text("aid_loctext_ghoulies_savedgame", "supportsavedgame",
        "defaultnewgamename"); // 0x000FF0A0 into DAT_00507E78
    if (text == nullptr)
    {
        text = u"ERROR";
    }
    copy_wide(w.name.data(), text);
    w.state = 1;
    push_runtime_attract_input(s.attract); // 0x00143070
}

// Xbox 0x000DD030
void close_runtime_game_naming(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = naming(widget);
    s.frontend.real_0042a0cc = w.page->time / w.page->duration; // +0x2C over +0x34
    s.frontend.naming_widget = nullptr;
    release_runtime_cutscene(w.page, s.cutscenes); // 0x0004D720, EAX the page
    w.page = nullptr;
    camera_unlink_view(&w.camera); // 0x00048C70, EAX +8
    if (w.keyboard != nullptr)
    {
        release_virtual_keyboard(*w.keyboard, s); // 0x000113C0
        destroy_virtual_keyboard(w.keyboard); // 0x000663E0
        w.keyboard = nullptr;
    }
    s.plate_flags[0] = w.plate_flags[0];
    s.plate_flags[1] = w.plate_flags[1];
    pop_runtime_attract_input(s.attract); // 0x001430E0
}

// Xbox 0x000DD0A0
void tick_runtime_game_naming(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = naming(widget);
    refresh_runtime_input_device(g_attract_input.address, s.frame, s.input, s.input_release); // 0x00109470, EAX
    // DAT_005461A0
    if (w.page != nullptr)
    {
        if (!(w.page->time < w.page->duration))
        {
            seek_runtime_cutscene(*w.page, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0
        }
        tick_runtime_cutscene(*w.page, frame_step(s), &w.camera, s.cutscenes); // 0x0004DA50, ESI +8
    }
    switch (w.state)
    {
    case 1:
    {
        if (s.transition != 0)
        {
            return;
        }
        const i32 book = i32(get(w.owner_state, control_field::book_turn));
        if (1 < book && book <= 3 && w.owner->book != nullptr)
        {
            return;
        }
        w.state = 2;
        if (w.keyboard == nullptr)
        {
            w.keyboard = create_virtual_keyboard(w.name.data(), s); // 0x000110D0
        }
        return;
    }
    case 2:
    case 3:
        if (w.keyboard != nullptr)
        {
            tick_virtual_keyboard(*w.keyboard, s); // 0x00011800, ESI the keyboard
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000DD170
void draw_runtime_game_naming(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = naming(widget);
    auto& display = s.display;
    if (select_display_camera(&w.camera, 0, display)) // 0x000489E0
    {
        rebuild_display_view(display); // 0x000F5E50
        extend_display_clip_limits(entity_detail::g_display_view->transform, display.scene_bounds, w.camera.blend,
            w.camera.clip_limit); // 0x0013C490, EAX DAT_005077A8 + 0x68
        entity_detail::g_display_view->near_clip = w.camera.clip_limit;
        entity_detail::g_display_view->far_clip = w.camera.blend;
        rebuild_display_projection(display); // 0x000F5FD0
        bind_display_projection(display); // 0x000F6170
    }
    if (s.dialogue_page_hold() == 0 && (s.flags & 0x40040u) == 0 && w.page != nullptr)
    {
        const i32 view = i32(s.outer.current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        draw_runtime_cutscene(*w.page, view, s.cutscenes); // 0x0004DBD0, EBX the page
    }
    if ((s.flags & 0x40000u) == 0 && w.state != 4 && w.keyboard != nullptr)
    {
        draw_virtual_keyboard(*w.keyboard, s); // 0x00011110, ESI the keyboard
    }
}
}
