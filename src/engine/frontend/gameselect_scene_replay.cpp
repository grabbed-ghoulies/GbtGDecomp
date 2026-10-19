#include "gameselect_scene_replay.h"
#include "button_bat_menu.h"
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
#include "../runtime/name_lookup.h"
#include "../scene/executable_resources.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"

#include <bit>
#include <cstring>
#include <string>
#include <vector>

namespace gbtg
{
namespace
{
constexpr u32 kReplayRows = 0x003095D0; // "ReplayChapterMenu"
constexpr const char* kGameSelectScript = "aid_script_ghoulies_gameselect_gameselect"; // 0x00322B8C

GameSceneReplayWidget& replay(GameSelectWidget& widget)
{
    return static_cast<GameSceneReplayWidget&>(widget);
}

GameSceneReplayWidget& open_widget(GameSelectControlServices& s)
{
    return *s.frontend.scene_replay_widget; // DAT_005065D4
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

// The inline 0x00035960 (wrapping): the held order's entry after `current`, the first entry after the last, or
// null when `current` is not in the order. Entries are 0x80-byte AIDs; the order ends at one not starting "aid_".
const char* next_held_scene(std::span<const u8> order, const char* current)
{
    const auto entry = [&order](size_t offset) -> const char*
    {
        return reinterpret_cast<const char*>(order.data() + offset);
    };
    const auto is_aid = [&order, &entry](size_t offset)
    {
        return offset + 4 <= order.size() && std::memcmp(entry(offset), "aid_", 4) == 0;
    };
    if (order.data() == nullptr)
    {
        return nullptr;
    }
    size_t at = 0;
    while (!transition_aid_equal(entry(at), current)) // 0x000357D0, EAX the current, EDX the entry
    {
        if (!is_aid(at + 0x80))
        {
            return nullptr;
        }
        at += 0x80;
    }
    return is_aid(at + 0x80) ? entry(at + 0x80) : entry(0);
}

// The number in the chapter table (DAT_002FE698) for the chapter holding scene `ordinal`: the first chapter whose
// running scene count (DAT_002FE670) reaches ordinal + 1, or entry -1 when none does.
i32 replay_chapter_number(i32 ordinal, const GameSelectControlServices& s)
{
    i32 total = 0;
    i32 chapter = 1;
    for (; chapter < 10; ++chapter)
    {
        total += exe_integer(s, 0x002FE670 + u32(chapter) * 4);
        if (ordinal + 1 <= total)
        {
            break;
        }
    }
    if (chapter == 10)
    {
        chapter = -1;
    }
    return exe_integer(s, u32(0x002FE698 + chapter * 4));
}

// Xbox 0x000E3130: replay scene `ordinal`. The held scene order becomes game select, every script of that scene
// in the order, and game select again, and the scene's first script starts (0x00083B90, mode 2).
void replay_scene(i32 ordinal, GameSelectControlServices& s)
{
    const std::span<const u8> order = s.scene_order_held; // DAT_00511B64
    const char* first = order.empty() ? nullptr : reinterpret_cast<const char*>(order.data());
    while (first != nullptr && gameselect_scene_ordinal(first, s) != ordinal) // the inline 0x0011ED30
    {
        first = next_held_scene(order, first);
    }
    std::vector<const char*> scripts;
    for (const char* entry = first; entry != nullptr && gameselect_scene_ordinal(entry, s) == ordinal;
         entry = next_held_scene(order, entry)) // 0x0011ED30, then inline
    {
        scripts.push_back(entry);
    }
    const size_t size = (scripts.size() + 2) * 0x80; // 0x000F72D0((count + 2) << 7)
    // The block is copied into one 0x80 bytes longer (0x000F72D0, 0x000F71F0), whose byte at `size` is cleared,
    // and freed: the copy is the held order.
    std::vector<u8> heap(size + 0x80, 0);
    std::strcpy(reinterpret_cast<char*>(heap.data()), kGameSelectScript);
    for (size_t index = 0; index < scripts.size(); ++index)
    {
        std::strcpy(reinterpret_cast<char*>(heap.data() + (index + 1) * 0x80), scripts[index]);
    }
    std::strcpy(reinterpret_cast<char*>(heap.data() + (scripts.size() + 1) * 0x80), kGameSelectScript);
    // Retail queues the script through its pointer into the old order after releasing it; the host copies it.
    const std::string start = first == nullptr ? std::string() : std::string(first);
    if (s.scene_order_release != 0 && s.scene_order_held.data() != nullptr)
    {
        release_held_scene_order(s); // (*DAT_00511B68)(&DAT_00511B64)
    }
    s.frontend.scene_order_heap = std::move(heap);
    s.scene_order_held = s.frontend.scene_order_heap; // DAT_00511B64
    s.scene_order_release = 0x000358F0; // DAT_00511B68
    auto& w = open_widget(s);
    s.next_row = 10; // DAT_00429BC0
    copy_wide(s.selected_game, w.name); // lstrcpyW(DAT_00429BC4, +0x48C)
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    queue_runtime_gameselect_script(*w.owner, start.c_str(), 2, s); // 0x00083B90, ECX +0x494, EAX the script
    s.profile.scene_flags->bytes[0] |= 0x10; // *(DAT_00506C74 + 8)
    s.profile.scene_flags->bytes[0] |= 0x20;
    set_runtime_game_scene_replay_state(open_widget(s), 3, s); // 0x000E3760(DAT_005065D4, 3)
}

// Xbox 0x000E3560: one item per scene before the save's, added first, so the menu lists them in order. A
// finished game with +0x260 set has 100; an unfinished game in a chapter numbered below 5 has its scene's
// ordinal; any other has the save's +0x25D byte plus 0x57.
void add_replay_scenes(GameSelectControlServices& s)
{
    std::array<char16_t, 0x100> chapter_text{};
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "replayscene_chapter", chapter_text.data(),
            s.localtext); // 0x000FF120, EDI 0x00325218
    }
    const char16_t* name = open_widget(s).name; // DAT_005065D4 +0x48C
    char script[0x80]{};
    save_game_script(name, script); // 0x0010DB10
    i32 count;
    if (save_game_word(name, 0x188) != 0 && save_game_word(name, 0x260) != 0) // 0x0010DC60, 0x0010E120
    {
        count = 100;
    }
    else if (save_game_word(name, 0x188) == 0 // 0x0010DC60
        && exe_integer(s, 0x002FE698 + u32(gameselect_script_chapter(script, s)) * 4) < 5) // 0x0004B4D0
    {
        count = gameselect_scene_ordinal(script, s); // 0x0011ED30
    }
    else
    {
        count = i32(save_game_byte(name, 0x25D)) + 0x57; // 0x0010E0E0, AL
    }
    for (i32 ordinal = count - 1; 0 <= ordinal; --ordinal)
    {
        // The item's context is an 8-byte heap block (0x0014E872, tagged 0x1234 by the heap) holding the ordinal.
        const std::wstring digits = std::to_wstring(replay_chapter_number(ordinal, s)); // _itow(.., 10), 0x00166B36
        std::u16string label(chapter_text.data()); // lstrcpyW
        label += u' '; // lstrcatW(L" ")
        label.append(digits.begin(), digits.end()); // lstrcatW
        std::array<char16_t, 0x800> scene{};
        label += u" : "; // wsprintfW(L"%s : %s", .., 0x0011ED80)
        label += gameselect_scene_name(ordinal, scene, s);
        add_runtime_button_bat_menu_item(make_runtime_button_bat_menu_item(label.c_str(), {}, {}, {}, {},
            [&s](u32 context)
            {
                replay_scene(i32(context), s); // 0x000E3130 with the block
            },
            u32(ordinal)), s); // 0x00143320, 0x001439C0
    }
}
}

// Xbox 0x000E1E00
GameSelectWidgetHandle create_runtime_game_scene_replay(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameSceneReplayWidget>(); // 0x000F72D0(0x49C)
}

// Xbox 0x000E2B90
void open_runtime_game_scene_replay(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = replay(widget);
    char page[0x80]{};
    std::memcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1", 42); // 0x00324C04
    (void)owner; // the control's bytes; the widget reaches the control through +0x494
    w.owner = &control;
    w.name = s.frontend.selected_name.data(); // +0x48C, DAT_00429CC4
    w.state = 0;
    w.prompts = nullptr;
    s.profile.scene_flags->bytes[0] |= 2;
    s.frontend.scene_replay_widget = &w; // DAT_005065D4
    s.profile.scene_flags->bytes[0] |= 4;
    refresh_runtime_save_summary(s); // 0x000F2F00
    load_runtime_save_progress(w.name, s); // 0x000F3020, EAX +0x48C
    initialize_camera_view(w.camera, "replayscene", 8); // 0x000496C0, EAX 0x0032524C, CL 8
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
    camera_bind_named_view(0, "replayscene"); // 0x00048CF0, EBX 0x0032524C
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    allocate_dialogue_text_pool(w.pool, 0, 1, 0x80, s.text_pools); // 0x0005F390, ESI +0x41C
    w.pool->origin = {exe_real(s, 0x00429A40), exe_real(s, 0x00429A44), std::bit_cast<float>(3u)};
    copy_wide(w.title.text.data(), w.name); // lstrcpyW(+4, +0x48C)
    w.title.x = exe_real(s, 0x00429A20);
    w.title.y = exe_real(s, 0x00429A24);
    w.title.record = 0;
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "replayscene_replayscenebanner", w.banner.text.data(),
            s.localtext); // 0x000FF120, EBX +0x210, EDI 0x0032522C
    }
    w.banner.x = exe_real(s, 0x00429A18);
    w.banner.y = exe_real(s, 0x00429A1C);
    w.banner.record = 0;
    char script[0x80]{};
    save_game_script(w.name, script); // 0x0010DB10, EBX +0x48C
    w.chapter_number = exe_integer(s, 0x002FE698 + u32(gameselect_script_chapter(script, s)) * 4); // 0x0004B4D0
    const std::array<float, 2> anchor{s.hud.center_x + exe_real(s, 0x00429A48),
        s.hud.center_y + exe_real(s, 0x00429A4C)}; // DAT_00509730, DAT_00509734
    create_runtime_button_bat_menu(anchor, exe_integer(s, 0x00429A50), exe_real(s, 0x00429A54),
        exe_real(s, 0x00429A58), 0, exe_real(s, 0x00429A60), exe_real(s, 0x00429A64), std::bit_cast<float>(3u),
        read_u32(s.image.memory(0x00429A5C, 4), 0), 1, 1, s); // 0x00143550, EAX the anchor
    start_pause_ditty(s.ditty, s.streams); // The 0x00038910 call, while DAT_00429BB8 is clear.
    set_runtime_game_scene_replay_state(w, 1, s); // 0x000E3760(1)
}

// Xbox 0x000E2F20
void close_runtime_game_scene_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = replay(widget);
    destroy_runtime_button_bat_menu(s); // 0x00143680
    s.frontend.scene_replay_widget = nullptr; // DAT_005065D4
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

// Xbox 0x000E2FB0
void tick_runtime_game_scene_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = replay(widget);
    if (w.page != nullptr)
    {
        if (w.page->duration <= w.page->time)
        {
            seek_runtime_cutscene(*w.page, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0
        }
        tick_runtime_cutscene(*w.page, frame_step(s), &w.camera, s.cutscenes); // 0x0004DA50, ESI +0x424
    }
    if (w.prompts != nullptr && u32(w.state) <= 4)
    {
        // The jump table at 0x000E30C0: state 2 passes the front-end device (DAT_00429BBC); the others pass none.
        s.update_prompts(*w.prompts, w.state == 2); // 0x00142460
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
            set_runtime_game_scene_replay_state(w, 2, s);
        }
    }
    else if (w.state == 3 && book_settled())
    {
        set_runtime_game_scene_replay_state(w, 4, s);
    }
    tick_runtime_button_bat_menu(s); // The tail jump to 0x00143790.
}

// Xbox 0x000E4DB0, the draw rows 9, 10, 11 and 16 share.
void draw_runtime_game_scene_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = replay(widget);
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
    if (w.page != nullptr)
    {
        const i32 view = i32(s.outer.current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        draw_runtime_cutscene(*w.page, view, s.cutscenes); // 0x0004DBD0
    }
    if (w.prompts != nullptr)
    {
        draw_runtime_dialogue_widget(*w.prompts, 0, s.outer); // 0x00142720
    }
    draw_runtime_button_bat_menu(s); // The tail jump to 0x00143840.
}

// Xbox 0x000E3760
void set_runtime_game_scene_replay_state(GameSceneReplayWidget& w, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
    if (w.state == 1)
    {
        const float icon = exe_real(s, 0x00429A38);
        const float text = exe_real(s, 0x00429A3C);
        const float spacing = exe_real(s, 0x00429A30);
        const std::array<float, 2> anchor{s.hud.inner_left + exe_real(s, 0x00429A28),
            s.hud.inner_bottom - exe_real(s, 0x00429A2C)}; // DAT_00509740, DAT_0050974C
        const std::array<float, 2> row_step{icon * 0.f, 0.f - icon * spacing};
        const float half = spacing * exe_real(s, 0x0032B0F4) * icon;
        const std::array<float, 2> origin{half + exe_real(s, 0x00429A34), 0.f - half};
        std::array<char16_t, 0x200> scratch{};
        const float width = measure_dialogue_widget_width(resource, s.image.memory(kReplayRows, 0x310).data(), icon,
            text, "gameselect", scratch.data(), s.widgets); // 0x001428F0, EAX the resource, ECX the rows
        const std::array<float, 2> column{width, 0.f};
        // Both records' ids go to +0x20C: retail stores the banner's over the title's.
        w.title.record = create_dialogue_text_record(*w.pool, w.title.text.data(), w.title.x, w.title.y,
            exe_real(s, 0x00429A10), exe_real(s, 0x00429A10), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random); // 0x0005F5C0, EAX 1
        w.title.record = create_dialogue_text_record(*w.pool, w.banner.text.data(), w.banner.x, w.banner.y,
            exe_real(s, 0x00429A14), exe_real(s, 0x00429A14), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random);
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return;
        }
        create_dialogue_widget(w.prompts, 1, s.image.memory(kReplayRows, 0x310).data(), anchor, row_step, origin,
            &column, icon, text, "gameselect", s.text, 0x400C0, s.widget_allocation); // 0x00141E20, EAX +0x498
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {exe_real(s, 0x00429A40), exe_real(s, 0x00429A44), std::bit_cast<float>(3u)};
        add_replay_scenes(s); // 0x000E3560
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

// Xbox 0x000E30E0: B goes back to the features (row 8).
void leave_runtime_game_scene_replay(GameSelectControlServices& services)
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
    set_runtime_game_scene_replay_state(*s.frontend.scene_replay_widget, 4, s); // 0x000E3760(DAT_005065D4, 4)
}
}
