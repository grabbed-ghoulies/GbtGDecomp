#include "gameselect_features.h"
#include "button_bat_menu.h"
#include "dialogue_activation.h"
#include "dialogue_draw_outer.h"
#include "dialogue_measurement.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_state.h"
#include "frontend_sounds.h"
#include "frontend_state.h"
#include "gameselect_progress.h"
#include "hud_layout.h"
#include "input_activation.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/event_runtime.h"
#include "../saves/save_games.h"
#include "../saves/runtime_profile.h"
#include "../saves/save_summary.h"
#include "../saves/xdk_saves.h"
#include "../scene/executable_resources.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"

#include <bit>
#include <cstring>
#include <vector>

namespace gbtg
{
namespace
{
constexpr u32 kFeaturesRows = 0x00306570; // "FeaturesMenu"

GameFeaturesWidget& features(GameSelectWidget& widget)
{
    return static_cast<GameFeaturesWidget&>(widget);
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

i32 exe_integer(const GameSelectControlServices& s, u32 address)
{
    return i32(read_u32(s.image.memory(address, 4), 0));
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

GameFeaturesWidget& open_widget(GameSelectControlServices& s)
{
    return *s.frontend.features_widget; // DAT_00506588
}

// The control's next row with no book turn (+0x94 and +0x124 1), and its book released.
void request_row(u32 row, GameSelectControlServices& s)
{
    auto& control = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170, EDX 0x9A
    control.requested_row = row;
    control.book_turn = 1;
    if (control.book != nullptr)
    {
        release_transition_book(control.book, s.books); // 0x00048860
        control.book = nullptr;
    }
}

// An error page from aid_loctext_ghoulies_dialog_dialogs over the scene dialogue, while it has room.
void show_error(const char* key, GameSelectControlServices& s)
{
    if (s.cutscenes.dialogue == nullptr)
    {
        throw Error("the features menu's error page: the scene dialogue (0x000582E0) is not bound");
    }
    auto& dialogue = *s.cutscenes.dialogue;
    if (i32(dialogue.state.count) + 1 < 9) // DAT_004EE1E8
    {
        request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", key,
            dialogue.measurement.wide_scratch, dialogue.measurement.localtext); // 0x000FF0A0 into DAT_00507E78
        enqueue_runtime_dialogue(dialogue.measurement.wide_scratch, 0, dialogue); // 0x000582E0
    }
}

// Xbox 0x000D8130: the unlockable art, as a script the control starts.
void open_artwork(GameSelectControlServices& s)
{
    char script[0x80]{};
    std::memcpy(script, "aid_script_ghoulies_gameselect_unlockableart", 45); // 0x00324BB0
    hold_runtime_scene_order(s.image.string(0x00306880), s); // 0x001234B0, EAX ..._sceneorder_gameselect_unlockableart
    auto& w = open_widget(s);
    s.next_row = 8; // DAT_00429BC0
    copy_wide(s.selected_game, w.name); // lstrcpyW(DAT_00429BC4, +0x48C)
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    queue_runtime_gameselect_script(*w.owner, script, 3, s); // 0x00083B90, ECX +0x490, EAX the script
    set_runtime_game_features_state(open_widget(s), 3, s);
}

// Xbox 0x000D80E0: the options (row 17).
void open_options(GameSelectControlServices& s)
{
    request_row(0x11, s);
    set_runtime_game_features_state(open_widget(s), 3, s);
}

// Xbox 0x000D81D0: replay a scene (row 10), once the save is past the first scene.
void open_scene_replay(GameSelectControlServices& s)
{
    char script[0x80]{};
    save_game_script(open_widget(s).name, script); // 0x0010DB10, EBX +0x48C
    if (0 < gameselect_scene_ordinal(script, s)) // 0x0011ED30
    {
        request_row(10, s);
        set_runtime_game_features_state(open_widget(s), 3, s);
        return;
    }
    show_error("errormessages_notpastsceneone", s);
}

// Xbox 0x000D8290: replay a chapter (row 9), once the save is past the first chapter.
void open_chapter_replay(GameSelectControlServices& s)
{
    char script[0x80]{};
    save_game_script(open_widget(s).name, script); // 0x0010DB10, EBX +0x48C
    const i32 chapter = gameselect_script_chapter(script, s); // 0x0004B4D0
    if (1 < exe_integer(s, 0x002FE698 + u32(chapter) * 4))
    {
        request_row(9, s);
        set_runtime_game_features_state(open_widget(s), 3, s);
        return;
    }
    show_error("errormessages_notpastchapterone", s);
}

// Xbox 0x000D8360: the story books (row 16), once the save is not a new game.
void open_story_books(GameSelectControlServices& s)
{
    if (save_game_word(open_widget(s).name, 0x184) == 0) // 0x0010DC20, EBX +0x48C
    {
        request_row(0x10, s);
        set_runtime_game_features_state(open_widget(s), 3, s);
        return;
    }
    show_error("errormessages_savedgameisnewgame", s);
}

// Xbox 0x000D8400: the demos disc's launcher.
[[noreturn]] void open_demos(GameSelectControlServices&)
{
    // 0x000325C0 / 0x00032570 launch D:\cdx.xbe with launch data (XLaunchNewImage); there is no host image to start.
    throw Error("0x000D8400 launches D:\\cdx.xbe (0x00032570), which is not translated");
}

// Xbox 0x000D8420: the staff credits. The held order becomes game select, aid_script_ghoulies_credits_final and
// game select again, and the credits start (0x00083B90, mode 3).
void open_credits(GameSelectControlServices& s)
{
    char script[0x80]{};
    std::strcpy(script, "aid_script_ghoulies_credits_final");
    const char* gameselect = "aid_script_ghoulies_gameselect_gameselect"; // 0x00322B8C
    std::vector<u8> heap(0x200, 0); // 0x000F72D0(0x180) copied into 0x000F72D0(0x200), byte 0x180 cleared
    std::strcpy(reinterpret_cast<char*>(heap.data()), gameselect);
    std::strcpy(reinterpret_cast<char*>(heap.data() + 0x80), script);
    std::strcpy(reinterpret_cast<char*>(heap.data() + 0x100), gameselect);
    if (s.scene_order_release != 0 && s.scene_order_held.data() != nullptr)
    {
        release_held_scene_order(s); // (*DAT_00511B68)(&DAT_00511B64)
    }
    s.frontend.scene_order_heap = std::move(heap);
    s.scene_order_held = s.frontend.scene_order_heap; // DAT_00511B64
    s.scene_order_release = 0x000358F0; // DAT_00511B68
    s.next_row = 8; // DAT_00429BC0
    auto& w = open_widget(s);
    copy_wide(s.selected_game, w.name); // lstrcpyW(DAT_00429BC4, DAT_00506588 +0x48C)
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    queue_runtime_gameselect_script(*w.owner, script, 3, s); // 0x00083B90, ECX +0x490, EAX the script
    set_runtime_game_features_state(w, 3, s); // 0x000D8840(DAT_00506588, 3)
}

// Item text from the gameselect localtext group, or an empty string without the resource.
void feature_text(const char* key, std::array<char16_t, 0x100>& text, GameSelectControlServices& s)
{
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", key, text.data(), s.localtext); // 0x000FF120, EDI the key
    }
}

void add_feature(const char* key, void (*action)(GameSelectControlServices&), GameSelectControlServices& s)
{
    std::array<char16_t, 0x100> text{};
    feature_text(key, text, s);
    add_runtime_button_bat_menu_item(make_runtime_button_bat_menu_item(text.data(), {}, {}, {}, {},
        [&s, action](u32)
        {
            action(s);
        },
        0), s); // 0x00143320, 0x001439C0
}

// Xbox 0x000D85A0: the features, each added first, so the menu lists them last-added first.
void add_features(GameSelectControlServices& s)
{
    i32 finished = 0;
    for (size_t index = 0; index < 0x15; ++index) // DAT_004E194C .. DAT_004E19A4
    {
        if (read_u32(g_game_progress.words_194c, index * 4) == 6)
        {
            ++finished;
        }
    }
    if (0 < finished || (s.profile.progress.bytes[7] & 0x20) != 0)
    {
        add_feature("features_artwork", open_artwork, s); // EDI 0x00324B4C, LAB_000d8130
    }
    add_feature("features_options", open_options, s); // EDI 0x00324B38, LAB_000d80e0
    add_feature("features_replayscene", open_scene_replay, s); // EDI 0x00324B20, LAB_000d81d0
    add_feature("features_replaychapter", open_chapter_replay, s); // EDI 0x00324B08, LAB_000d8290
    add_feature("features_storybooks", open_story_books, s); // EDI 0x00324AF4, LAB_000d8360
    // GetFileAttributesA("D:\\cdxmedia\\cdx.inx"): 0xFFFFFFFF is GetLastError, a directory is 0x10B, else 0.
    u32 status = xbox_file_attributes("D:\\cdxmedia\\cdx.inx"); // 0x0014B280
    if (status == 0xFFFFFFFFu)
    {
        status = xbox_last_error(); // GetLastError (0x0014AFE4)
    }
    else
    {
        status = (status & 0x10) != 0 ? 0x10Bu : 0u;
    }
    if (status == 0)
    {
        add_feature("features_demos", open_demos, s); // EDI 0x00324AD0, LAB_000d8400
    }
    if (save_game_word(open_widget(s).name, 0x188) != 0) // 0x0010DC60, a finished game
    {
        add_feature("features_credits", open_credits, s); // EDI 0x00324ABC, LAB_000d8420
    }
}
}

// Xbox 0x000D4EA0
GameSelectWidgetHandle create_runtime_game_features(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameFeaturesWidget>(); // 0x000F72D0(0x498)
}

// Xbox 0x000D7A60
void open_runtime_game_features(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = features(widget);
    char page[0x80]{};
    std::memcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1", 42); // 0x00324C04
    (void)owner; // the control's bytes; the widget reaches the control through +0x490
    w.owner = &control;
    w.name = s.frontend.selected_name.data(); // +0x48C, DAT_00429CC4
    w.state = 0;
    w.prompts = nullptr;
    s.profile.scene_flags->bytes[0] |= 2;
    s.frontend.features_widget = &w; // DAT_00506588
    s.profile.scene_flags->bytes[0] |= 4;
    refresh_runtime_save_summary(s); // 0x000F2F00
    load_runtime_save_progress(w.name, s); // 0x000F3020, EAX +0x48C
    initialize_camera_view(w.camera, "features", 8); // 0x000496C0, EAX 0x00324BF8, CL 8
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
    seek_runtime_cutscene(*w.page, s.frontend.real_0042a0cc, 0.f, 0, s.cutscenes); // 0x0004E4A0, ESI the page
    camera_bind_named_view(0, "features"); // 0x00048CF0, EBX 0x00324BF8
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    allocate_dialogue_text_pool(w.pool, 0, 1, 0x400C0, s.text_pools); // 0x0005F390, ESI +0x41C
    w.pool->origin = {exe_real(s, 0x00428C58), exe_real(s, 0x00428C5C), std::bit_cast<float>(3u)};
    copy_wide(w.title.text.data(), w.name); // lstrcpyW(+4, +0x48C)
    w.title.x = exe_real(s, 0x00428C38);
    w.title.y = exe_real(s, 0x00428C3C);
    w.title.record = 0;
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "features_featuresbanner", w.banner.text.data(),
            s.localtext); // 0x000FF120, EBX +0x210, EDI 0x00324BE0
    }
    w.banner.x = exe_real(s, 0x00428C30);
    w.banner.y = exe_real(s, 0x00428C34);
    w.banner.record = 0;
    const std::array<float, 2> anchor{s.hud.center_x + exe_real(s, 0x00428C60),
        s.hud.center_y + exe_real(s, 0x00428C64)}; // DAT_00509730, DAT_00509734
    create_runtime_button_bat_menu(anchor, exe_integer(s, 0x00428C68), exe_real(s, 0x00428C6C),
        exe_real(s, 0x00428C70), 0, exe_real(s, 0x00428C78), exe_real(s, 0x00428C7C), std::bit_cast<float>(3u),
        read_u32(s.image.memory(0x00428C74, 4), 0), 1, 1, s); // 0x00143550, EAX the anchor
    start_pause_ditty(s.ditty, s.streams); // The 0x000E5B40 inline, while DAT_00429BB8 is clear.
    set_runtime_game_features_state(w, 1, s); // 0x000D8840(1)
}

// Xbox 0x000D7DB0
void close_runtime_game_features(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = features(widget);
    destroy_runtime_button_bat_menu(s); // 0x00143680
    s.frontend.features_widget = nullptr; // DAT_00506588
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

// Xbox 0x000D7E40
void tick_runtime_game_features(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = features(widget);
    if (s.dialogue_page_hold() == 0 && w.page != nullptr) // DAT_004EE1EC
    {
        if (w.page->duration <= w.page->time)
        {
            seek_runtime_cutscene(*w.page, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0
        }
        tick_runtime_cutscene(*w.page, frame_step(s), &w.camera, s.cutscenes); // 0x0004DA50, ESI +0x424
    }
    if (w.prompts != nullptr && u32(w.state) <= 4)
    {
        // The jump table at 0x000D7F64: state 2 passes the front-end device (DAT_00429BBC) while no dialogue page
        // holds the screen; the others pass none.
        if (w.state != 2)
        {
            s.update_prompts(*w.prompts, false); // 0x00142460, EAX 0
        }
        else if (s.dialogue_page_hold() == 0)
        {
            s.update_prompts(*w.prompts, true); // 0x00142460, EAX DAT_00429BBC
        }
    }
    // States 1 and 3 wait for the scene transition and the menu book's page turn.
    const auto book_settled = [&w, &s]()
    {
        const i32 turn = i32(w.owner->book_turn);
        return s.transition == 0 && (turn < 2 || 3 < turn || w.owner->book == nullptr); // DAT_0054632C
    };
    if (w.state == 1)
    {
        if (book_settled())
        {
            set_runtime_game_features_state(w, 2, s);
        }
    }
    else if (w.state == 3 && book_settled())
    {
        set_runtime_game_features_state(w, 4, s);
    }
    tick_runtime_button_bat_menu(s); // The tail jump to 0x00143790.
}

// Xbox 0x000D7F80
void draw_runtime_game_features(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = features(widget);
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
    if (s.dialogue_page_hold() == 0 && (s.flags & 0x40040u) == 0 && w.page != nullptr)
    {
        const i32 view = i32(s.outer.current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        draw_runtime_cutscene(*w.page, view, s.cutscenes); // 0x0004DBD0
    }
    if (w.prompts != nullptr && s.dialogue_page_hold() == 0)
    {
        draw_runtime_dialogue_widget(*w.prompts, 0, s.outer); // 0x00142720
    }
    draw_runtime_button_bat_menu(s); // The tail jump to 0x00143840.
}

// Xbox 0x000D8840
void set_runtime_game_features_state(GameFeaturesWidget& w, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
    if (w.state == 1)
    {
        const float icon = exe_real(s, 0x00428C50);
        const float text = exe_real(s, 0x00428C54);
        const float spacing = exe_real(s, 0x00428C48);
        const std::array<float, 2> anchor{s.hud.inner_left + exe_real(s, 0x00428C40),
            s.hud.inner_bottom - exe_real(s, 0x00428C44)}; // DAT_00509740, DAT_0050974C
        const std::array<float, 2> row_step{icon * 0.f, 0.f - icon * spacing};
        const float half = spacing * exe_real(s, 0x0032B0F4) * icon;
        const std::array<float, 2> origin{half + exe_real(s, 0x00428C4C), 0.f - half};
        std::array<char16_t, 0x200> scratch{};
        const float width = measure_dialogue_widget_width(resource, s.image.memory(kFeaturesRows, 0x310).data(), icon,
            text, "gameselect", scratch.data(), s.widgets); // 0x001428F0, EAX the resource, ECX the rows
        const std::array<float, 2> column{width, 0.f};
        // Both records' ids go to +0x20C: retail stores the banner's over the title's.
        w.title.record = create_dialogue_text_record(*w.pool, w.title.text.data(), w.title.x, w.title.y,
            exe_real(s, 0x00428C28), exe_real(s, 0x00428C28), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random); // 0x0005F5C0, EAX 1
        w.title.record = create_dialogue_text_record(*w.pool, w.banner.text.data(), w.banner.x, w.banner.y,
            exe_real(s, 0x00428C2C), exe_real(s, 0x00428C2C), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random);
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return;
        }
        create_dialogue_widget(w.prompts, 1, s.image.memory(kFeaturesRows, 0x310).data(), anchor, row_step, origin,
            &column, icon, text, "gameselect", s.text, 0x400C0, s.widget_allocation); // 0x00141E20, EAX +0x494
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {exe_real(s, 0x00428C58), exe_real(s, 0x00428C5C), std::bit_cast<float>(3u)};
        add_features(s); // 0x000D85A0
        open_runtime_button_bat_menu(s); // 0x00143B10
    }
    else if (w.state == 2)
    {
        close_runtime_button_bat_menu(s); // 0x00143B80
    }
    w.state = state;
}

// Xbox 0x000E4E40
void activate_runtime_feature(GameSelectControlServices& services)
{
    auto& s = services;
    auto& menu = *s.frontend.bat_menu; // DAT_005461A4
    if ((s.flags & 0x80000u) != 0 || menu.state == 2)
    {
        auto& entry = *menu.items.at(size_t(menu.selected));
        entry.on_activate(entry.context); // +0x218 with +0x21C
    }
}

// Xbox 0x000D8080
void move_runtime_features_cursor(GameSelectControlServices& services)
{
    auto& s = services;
    std::array<float, 2> direction{};
    s.frontend_direction(direction); // 0x001429E0, ESI DAT_00429BBC
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return;
    }
    const u16 pressed = gameselect_direction_code(direction, s); // 0x001080B0
    if (pressed == 1)
    {
        move_runtime_button_bat_menu_up(s); // 0x00143BF0
    }
    else if (pressed == 2)
    {
        move_runtime_button_bat_menu_down(s); // 0x00143CB0
    }
}

// Xbox 0x000D8030: B goes back to the contents (row 3).
void leave_runtime_game_features(GameSelectControlServices& services)
{
    auto& s = services;
    auto& control = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170
    control.requested_row = 3;
    control.book_turn = 1;
    if (control.book != nullptr)
    {
        release_transition_book(control.book, s.books); // 0x00048860
        control.book = nullptr;
    }
    set_runtime_game_features_state(*s.frontend.features_widget, 4, s); // 0x000D8840(DAT_00506588, 4)
}
}
