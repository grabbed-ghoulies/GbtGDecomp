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

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
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
        throw Error("0x0010D430 (create the new game's save) is not translated");
    }
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
    const u32 one = 1;
    std::memcpy(control->state.data() + 0x94, &one, 4);
    std::memcpy(control->state.data() + 0x124, &one, 4);
    if (control->gameselect->book != nullptr)
    {
        throw Error("0x00048860 (release the menu book) is not translated");
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
        const i32 book = i32(read_u32(w.owner_state, 0x124));
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
