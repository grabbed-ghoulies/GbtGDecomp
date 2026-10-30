#include "gameselect_options.h"
#include "app_error_screen.h"
#include "button_bat_menu.h"
#include "dialogue_activation.h"
#include "dialogue_draw_outer.h"
#include "dialogue_measurement.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_parameters.h"
#include "dialogue_widget_state.h"
#include "frontend_state.h"
#include "hud_layout.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../camera/actor_camera_effects.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/event_runtime.h"
#include "../saves/runtime_profile.h"
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
constexpr u32 kLayout = 0x00429958; // The page's floats, laid out as the replay pages' blocks.
constexpr u32 kStandardRows = 0x00308C20; // "OptionsMenuStandard"
constexpr u32 kBrewRows = 0x00308F30; // "OptionsMenuButlersBrew"

GameOptionsWidget& options(GameSelectWidget& widget)
{
    return static_cast<GameOptionsWidget&>(widget);
}

GameOptionsWidget& open_widget(GameSelectControlServices& s)
{
    return *s.frontend.options_widget; // DAT_005065CC
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

float layout_real(u32 offset, const GameSelectControlServices& s)
{
    return exe_real(s, kLayout + offset);
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale; // DAT_005096C0 * DAT_004312C4
    }
    return 0.f;
}

void copy_wide(char16_t* output, const char16_t* input)
{
    while ((*output++ = *input++) != 0) // lstrcpyW
    {
    }
}

void gameselect_text(const char* key, char16_t* output, GameSelectControlServices& s)
{
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", key, output, s.localtext); // 0x000FF120, EDI the key
    }
}

u8& unlocks(GameSelectControlServices& s)
{
    return s.profile.unlocks.bytes[0]; // *(DAT_00506C70 + 8)
}

// One option's setting: its bit in the unlock block, and its texts with the bit set and clear.
struct Setting
{
    u8 bit;
    const char* set;
    const char* clear;
};
constexpr std::array<Setting, 3> kSettings{{
    {4, "options_rumbleinactive", "options_rumbleactive"}, // 0x00325154, 0x0032516C
    {2, "options_camrotationinverted", "options_camrotationnormal"}, // 0x00325138, 0x0032511C
    {8, "options_brewenabled", "options_brewdisabled"}, // 0x00325108, 0x003250F0
}};

u32 setting_record(GameOptionsWidget& w, GameOptionsWidget::Option& option, GameSelectControlServices& s)
{
    const float scale = exe_real(s, 0x004299A0);
    return create_dialogue_text_record(*w.pool, option.text.data(), option.x, option.y, scale, scale, 1.f, 1, 0, 1,
        s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
}

// Xbox 0x000E1320 / 0x000E1430 / 0x000E1620 (the item's +0x208): the setting's text beside the item.
void show_setting(size_t index, GameSelectControlServices& s)
{
    auto& w = open_widget(s);
    auto& option = w.options[index];
    if (option.item == nullptr)
    {
        return;
    }
    const float y = option.item->position[1]; // Item +0x228
    const Setting& setting = kSettings[index];
    gameselect_text((unlocks(s) & setting.bit) != 0 ? setting.set : setting.clear, option.text.data(), s);
    option.x = exe_real(s, 0x004299B0);
    option.y = y;
    option.record = setting_record(w, option, s);
}

// Xbox 0x000E1400 / 0x000E1510 / 0x000E1700 (the item's +0x20C): the setting's text ends (0x0005F7E0 inline).
void hide_setting(size_t index, GameSelectControlServices& s)
{
    auto& w = open_widget(s);
    if (w.options[index].item == nullptr)
    {
        return;
    }
    end_dialogue_text_record(*w.pool, w.options[index].record, s.fonts, s.fallback_texture, s.random);
}

// Xbox 0x000E1A10: the unlock block into the current game's save, and its write queued.
void save_settings(GameSelectControlServices& s)
{
    auto& w = open_widget(s);
    // 0x000F72D0(0xD): a block of kind 4 and one byte, the unlock block copied in (0x000F71F0), given to the save
    // (0x0010E610) and freed (0x000663E0).
    RuntimeProfileBlock copy{4, 1, {}};
    std::array<u8, 1> byte{};
    copy.bytes = byte;
    std::memcpy(copy.bytes.data(), s.profile.unlocks.bytes.data(), size_t(s.profile.unlocks.size));
    store_save_game_unlocks(w.name, copy); // 0x0010E610, EBX +0x48C
    SaveGameContext saves{s.files, s.profile, s.scene_order_held};
    queue_save_game(w.name, [](const void* argument)
    {
        // Xbox 0x000E1D50
        show_corrupt_game(static_cast<const char16_t*>(argument), "frontendoptions_corruptgamemessage",
            "frontendoptions_corruptgameabutton", "frontendoptions_corruptgamebbutton"); // 0x0032508C ..
    }, saves); // 0x0010DA80, EAX +0x48C
}

// Xbox 0x000E1220 / 0x000E1540 / 0x000E1730 (the item's +0x218): the setting flips, its text changes and the save
// is written. Rumble turned on plays the gunshot rumble.
void toggle_setting(size_t index, GameSelectControlServices& s)
{
    auto& w = open_widget(s);
    auto& option = w.options[index];
    const Setting& setting = kSettings[index];
    u8& bits = unlocks(s);
    if ((bits & setting.bit) == 0)
    {
        bits |= setting.bit;
        gameselect_text(setting.set, option.text.data(), s);
    }
    else
    {
        bits &= u8(~setting.bit);
        gameselect_text(setting.clear, option.text.data(), s);
        if (index == 0)
        {
            load_actor_camera_shake("aid_fxrumble_ghoulies_actor_player_gunshot", Vec3{}, 1.f, s.resources,
                s.error); // 0x000FB120(1.0), EAX 0x004298D8, ECX DAT_0050DD80
        }
    }
    end_dialogue_text_record(*w.pool, option.record, s.fonts, s.fallback_texture, s.random); // 0x0005F7E0
    option.record = setting_record(w, option, s);
    save_settings(s); // The tail jump to 0x000E1A10
}

// Xbox 0x000E1810 / 0x000E1850: Butler's Brew selected shows the brew prompts (with Y, its explanation), and the
// standard ones again when it is left.
void request_prompts(u32 rows, GameSelectControlServices& s)
{
    auto& prompts = *open_widget(s).prompts; // DAT_005065CC +0x494
    copy_dialogue_widget_parameters(prompts.requested_parameters, s.image.memory(rows, 0x310).data()); // 0x00142DF0
    prompts.requested_set = prompts.requested_parameters.data(); // +0x950 = +0x638
    set_dialogue_widget_mode(prompts, 3, s.widgets); // 0x00142C20, EAX 3
}

ButtonBatMenuItem* add_option(const char* key, std::function<void(u32)> show, std::function<void(u32)> hide,
    std::function<void(u32)> select, std::function<void(u32)> deselect, std::function<void(u32)> activate,
    GameSelectControlServices& s)
{
    std::array<char16_t, 0x100> text{};
    gameselect_text(key, text.data(), s);
    auto item = make_runtime_button_bat_menu_item(text.data(), std::move(show), std::move(hide), std::move(select),
        std::move(deselect), std::move(activate), 0); // 0x00143320
    ButtonBatMenuItem* added = item.get();
    add_runtime_button_bat_menu_item(std::move(item), s); // 0x001439C0
    return added;
}

// Xbox 0x000E18D0: Butler's Brew, rumble and the camera's rotation, each linked first, so the menu lists them in
// the other order.
void add_options(GameSelectControlServices& s)
{
    auto& w = open_widget(s);
    w.options[2].item = add_option("options_brew", [&s](u32)
    {
        show_setting(2, s); // 0x000E1620
    }, [&s](u32)
    {
        hide_setting(2, s); // 0x000E1700
    }, [&s](u32)
    {
        request_prompts(kBrewRows, s); // 0x000E1810
    }, [&s](u32)
    {
        request_prompts(kStandardRows, s); // 0x000E1850
    }, [&s](u32)
    {
        toggle_setting(2, s); // 0x000E1730
    }, s); // +0x8B8, EDI 0x003250D4
    w.options[0].item = add_option("options_rumble", [&s](u32)
    {
        show_setting(0, s); // 0x000E1320
    }, [&s](u32)
    {
        hide_setting(0, s); // 0x000E1400
    }, {}, {}, [&s](u32)
    {
        toggle_setting(0, s); // 0x000E1220
    }, s); // +0x498, EDI 0x003250C4
    w.options[1].item = add_option("options_camrotation", [&s](u32)
    {
        show_setting(1, s); // 0x000E1430
    }, [&s](u32)
    {
        hide_setting(1, s); // 0x000E1510
    }, {}, {}, [&s](u32)
    {
        toggle_setting(1, s); // 0x000E1540
    }, s); // +0x6A8, EDI 0x003250B0
}
}

// Xbox 0x000E0B60
GameSelectWidgetHandle create_runtime_game_options(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameOptionsWidget>(); // 0x000F72D0(0xAC8)
}

// Xbox 0x000E0B70
void open_runtime_game_options(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = options(widget);
    char page[0x80]{};
    std::memcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1", 42); // 0x00324C04
    (void)owner; // the control's bytes; the widget reaches the control through +0x490
    w.owner = &control;
    s.frontend.options_widget = &w; // DAT_005065CC
    w.state = 0;
    w.name = s.frontend.selected_name.data(); // +0x48C, DAT_00429CC4
    w.prompts = nullptr;
    for (auto& option : w.options)
    {
        option.item = nullptr;
        option.record = 0;
        option.x = 0.f;
        option.y = 0.f;
        option.text[0] = 0;
    }
    initialize_camera_view(w.camera, "options", 8); // The 0x000496C0 body inline, 0x0032519C, CL 8
    w.camera.aspect_slot = 0; // +0x30
    i32 fit_w = 0;
    i32 fit_x = 0;
    i32 fit_h = 0;
    i32 fit_y = 0;
    float fov = 0.f;
    camera_fit_display_view(w.camera.focal, 0, fit_w, fit_x, fit_h, fit_y, fov, w.camera.pixel_aspect,
        w.camera.focal_pixels); // 0x000F57F0
    link_camera_view(0, w.camera); // The inline 0x00048C20 link and its 0x00048CF0 when slot 0 has no view.
    if ((s.profile.progress.bytes[4] & 0x40) != 0)
    {
        // The gold dialogue bat replaces the plain one in the page's cutscene (DAT_004E1AC0, DAT_004E1A40).
        set_cutscene_actor_replacement(s.replacement_source, s.replacement_target, "aid_model_ghoulies_actor_dialogbat",
            "aid_model_ghoulies_actor_dialogbatgold");
    }
    w.page = load_runtime_cutscene(page, 6, &w.camera, s.cutscenes); // 0x0004D680
    seek_runtime_cutscene(*w.page, s.frontend.real_0042a0cc, 0.f, 0, s.cutscenes); // 0x0004E4A0
    camera_bind_named_view(0, "options"); // 0x00048CF0
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    allocate_dialogue_text_pool(w.pool, 0, 1, 0x400C0, s.text_pools); // 0x0005F390, ESI +0x41C
    w.pool->origin = {layout_real(0x30, s), layout_real(0x34, s), std::bit_cast<float>(3u)};
    copy_wide(w.title.text.data(), w.name); // lstrcpyW(+4, +0x48C)
    w.title.x = layout_real(0x10, s);
    w.title.y = layout_real(0x14, s);
    w.title.record = 0;
    gameselect_text("options_optionsbanner", w.banner.text.data(), s); // 0x000FF120, EDI 0x00325184
    w.banner.x = layout_real(8, s);
    w.banner.y = layout_real(0xC, s);
    w.banner.record = 0;
    const std::array<float, 2> anchor{s.hud.center_x + layout_real(0x38, s),
        s.hud.center_y + layout_real(0x3C, s)}; // DAT_00509730, DAT_00509734
    create_runtime_button_bat_menu(anchor, i32(read_u32(s.image.memory(kLayout + 0x40, 4), 0)), layout_real(0x44, s),
        layout_real(0x48, s), 0, layout_real(0x50, s), layout_real(0x54, s), std::bit_cast<float>(3u),
        read_u32(s.image.memory(kLayout + 0x4C, 4), 0), 0, 1, s); // 0x00143550: the widget keeps its items
    set_runtime_game_options_state(w, 1, s); // 0x000E1A80(1)
}

// Xbox 0x000E0F50
void close_runtime_game_options(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = options(widget);
    for (auto& option : w.options)
    {
        release_runtime_button_bat_menu_item(option.item, s); // 0x001434E0, the items the menu does not own
        option.item = nullptr;
    }
    destroy_runtime_button_bat_menu(s); // 0x00143680
    s.frontend.options_widget = nullptr; // DAT_005065CC
    release_dialogue_text_pool(w.pool, *s.text_pools); // 0x0005F460
    w.pool = nullptr;
    if (w.prompts != nullptr)
    {
        destroy_dialogue_widget(w.prompts, s.resources, *s.text_pools); // 0x00142070
        w.prompts = nullptr;
    }
    s.frontend.real_0042a0cc = w.page->time / w.page->duration; // +0x2C over +0x34
    release_runtime_cutscene(w.page, s.cutscenes); // 0x0004D720
    w.page = nullptr;
    camera_unlink_view(&w.camera); // 0x00048C70
    s.plate_flags[0] = w.saved_plate_flag_0;
    s.plate_flags[1] = w.saved_plate_flag_1;
}

// Xbox 0x000E1000
void tick_runtime_game_options(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = options(widget);
    if (w.page != nullptr)
    {
        if (w.page->duration <= w.page->time)
        {
            seek_runtime_cutscene(*w.page, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0
        }
        tick_runtime_cutscene(*w.page, frame_step(s), &w.camera, s.cutscenes); // 0x0004DA50
    }
    if (w.prompts != nullptr && u32(w.state) <= 4)
    {
        // State 2 passes the front-end device (DAT_00429BBC) while no dialogue page holds the screen; the others
        // pass none.
        if (w.state != 2)
        {
            s.update_prompts(*w.prompts, false); // 0x00142460, EAX 0
        }
        else if (s.dialogue_page_hold() == 0)
        {
            s.update_prompts(*w.prompts, true); // 0x00142460, EAX DAT_00429BBC
        }
    }
    const auto book_settled = [&w, &s]()
    {
        const i32 turn = i32(w.owner->book_turn);
        return s.transition == 0 && (turn < 2 || 3 < turn || w.owner->book == nullptr); // DAT_0054632C
    };
    if (w.state == 1)
    {
        if (book_settled())
        {
            set_runtime_game_options_state(w, 2, s);
        }
    }
    else if (w.state == 3 && book_settled())
    {
        set_runtime_game_options_state(w, 4, s);
    }
    tick_runtime_button_bat_menu(s); // The tail jump to 0x00143790.
}

// Xbox 0x000E1130
void draw_runtime_game_options(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = options(widget);
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
        draw_runtime_cutscene(*w.page, view, s.cutscenes); // 0x0004DBD0
    }
    if (w.prompts != nullptr && s.dialogue_page_hold() == 0)
    {
        draw_runtime_dialogue_widget(*w.prompts, 0, s.outer); // 0x00142720(0)
    }
    draw_runtime_button_bat_menu(s); // The tail jump to 0x00143840.
}

// Xbox 0x000E1A80, EBX the widget
void set_runtime_game_options_state(GameOptionsWidget& w, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
    if (w.state == 1)
    {
        const float icon = layout_real(0x28, s);
        const float text = layout_real(0x2C, s);
        const float spacing = layout_real(0x20, s);
        const std::array<float, 2> anchor{s.hud.inner_left + layout_real(0x18, s),
            s.hud.inner_bottom - layout_real(0x1C, s)}; // DAT_00509740, DAT_0050974C
        const std::array<float, 2> row_step{icon * 0.f, 0.f - icon * spacing};
        const float half = spacing * exe_real(s, 0x0032B0F4) * icon;
        const std::array<float, 2> origin{half + layout_real(0x24, s), 0.f - half};
        // The column is as wide as the wider of the two prompt sets.
        std::array<char16_t, 0x200> scratch{};
        float width = 0.f;
        const float standard = measure_dialogue_widget_width(resource, s.image.memory(kStandardRows, 0x310).data(),
            icon, text, "gameselect", scratch.data(), s.widgets); // 0x001428F0, ECX 0x00308C20
        if (exe_real(s, 0x0032B100) < standard)
        {
            width = standard;
        }
        const float brew = measure_dialogue_widget_width(resource, s.image.memory(kBrewRows, 0x310).data(), icon, text,
            "gameselect", scratch.data(), s.widgets); // 0x001428F0, ECX 0x00308F30
        if (width < brew)
        {
            width = brew;
        }
        const std::array<float, 2> column{width, 0.f};
        w.title.record = create_dialogue_text_record(*w.pool, w.title.text.data(), w.title.x, w.title.y,
            layout_real(0, s), layout_real(0, s), 1.f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id,
            s.random); // 0x0005F5C0, EAX 1
        w.banner.record = create_dialogue_text_record(*w.pool, w.banner.text.data(), w.banner.x, w.banner.y,
            layout_real(4, s), layout_real(4, s), 1.f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id, s.random);
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return;
        }
        create_dialogue_widget(w.prompts, 1, s.image.memory(kStandardRows, 0x310).data(), anchor, row_step, origin,
            &column, icon, text, "gameselect", s.text, 0x400C0, s.widget_allocation); // 0x00141E20, +0x494
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {layout_real(0x30, s), layout_real(0x34, s), std::bit_cast<float>(3u)};
        add_options(s); // 0x000E18D0
        open_runtime_button_bat_menu(s); // 0x00143B10
    }
    else if (w.state == 2)
    {
        close_runtime_button_bat_menu(s); // 0x00143B80
    }
    w.state = state;
    if (state == 3)
    {
        end_all_dialogue_text_records(*w.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
        if (w.prompts != nullptr)
        {
            w.prompts->requested_set = nullptr; // +0x950
            set_dialogue_widget_mode(*w.prompts, 3, s.widgets); // 0x00142C20, EAX 3
        }
    }
}

// Xbox 0x000E11D0: B goes back to the features (row 8).
void leave_runtime_game_options(GameSelectControlServices& services)
{
    auto& s = services;
    auto& control = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170
    control.requested_row = 8;
    control.book_turn = 1;
    if (control.book != nullptr)
    {
        release_transition_book(control.book, s.books); // 0x00048860
        control.book = nullptr;
    }
    set_runtime_game_options_state(open_widget(s), 4, s); // 0x000E1A80(DAT_005065CC, 4)
}

// Xbox 0x000E1890: Y on Butler's Brew explains it.
void explain_runtime_butlers_brew(GameSelectControlServices& services)
{
    auto& s = services;
    if (s.cutscenes.dialogue == nullptr)
    {
        throw Error("Butler's Brew's explanation: the scene dialogue (0x000582E0) is not bound");
    }
    auto& dialogue = *s.cutscenes.dialogue;
    if (i32(dialogue.state.count) + 1 < 9) // DAT_004EE1E8
    {
        request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", "brewexplain",
            dialogue.measurement.wide_scratch, dialogue.measurement.localtext); // 0x000FF0A0 into DAT_00507E78
        enqueue_runtime_dialogue(dialogue.measurement.wide_scratch, 0, dialogue); // 0x000582E0
    }
}
}
