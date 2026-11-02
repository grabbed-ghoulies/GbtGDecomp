#include "gameselect_scene_replay.h"
#include "dialogue_text_metrics.h"
#include <cstdio>
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
extern u8 g_scene_record[]; // DAT_00510E58, scene.cpp.

namespace
{
constexpr const char* kGameSelectScript = "aid_script_ghoulies_gameselect_gameselect"; // 0x00322B8C

GameSceneReplayWidget& replay(GameSelectWidget& widget)
{
    return static_cast<GameSceneReplayWidget&>(widget);
}

GameSceneReplayWidget& open_widget(GameSelectControlServices& s)
{
    return *s.frontend.scene_replay_widget; // DAT_005065D4
}

void add_replay_scenes(GameSelectControlServices& s);
void add_replay_chapters(GameSelectControlServices& s);
void add_bonus_book_scenes(GameSelectControlServices& s);
void add_story_books(GameSelectControlServices& s);

const ReplayPage kChapterPage{9, 0x004299B8, "replaychapter", "replaychapter_replaychapterbanner", 0x00309240, true,
    &RuntimeFrontendState::chapter_replay_widget, add_replay_chapters}; // 0x000E1E10, 0x000E2900
const ReplayPage kScenePage{10, 0x00429A10, "replayscene", "replayscene_replayscenebanner", 0x003095D0, true,
    &RuntimeFrontendState::scene_replay_widget, add_replay_scenes}; // 0x000E2B90, 0x000E3760
const ReplayPage kBonusBookPage{11, 0x00429A78, "replayscenechallenge", "replayscenechallenge_banner", 0x003098E0,
    true, &RuntimeFrontendState::bonus_book_replay_widget, add_bonus_book_scenes}; // 0x000E39F0, 0x000E4650

const ReplayPage kStoryBookPage{16, 0x00429AE0, "replaystorybook", "replaystorybook_banner", 0x00309C70, false,
    &RuntimeFrontendState::story_book_widget, add_story_books}; // 0x000E48A0, 0x000E57F0

GameSceneReplayWidget& page_widget(const ReplayPage& page, GameSelectControlServices& s)
{
    return *(s.frontend.*page.widget);
}

float layout_real(const ReplayPage& page, u32 offset, const GameSelectControlServices& s)
{
    return std::bit_cast<float>(read_u32(s.image.memory(page.layout + offset, 4), 0));
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

// Xbox 0x000E3130 (row 10) and 0x000E3F90 (row 11): replay scene `ordinal`. The held scene order becomes game
// select, every script of that scene in the order, and game select again, and the scene's first script starts
// (0x00083B90, mode 2). Row 10 marks the scene flag 0x20, row 11 0x40.
void replay_scene(i32 ordinal, const ReplayPage& page, GameSelectControlServices& s)
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
    auto& w = page_widget(page, s);
    s.next_row = i32(page.row); // DAT_00429BC0: 10, or 0xB
    copy_wide(s.selected_game, w.name); // lstrcpyW(DAT_00429BC4, +0x48C)
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    queue_runtime_gameselect_script(*w.owner, start.c_str(), 2, s); // 0x00083B90, ECX +0x494, EAX the script
    s.profile.scene_flags->bytes[0] |= 0x10; // *(DAT_00506C74 + 8)
    s.profile.scene_flags->bytes[0] |= page.row == 10 ? 0x20 : 0x40;
    set_runtime_game_scene_replay_state(w, 3, s); // 0x000E3760 / 0x000E4650(3)
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
                replay_scene(i32(context), kScenePage, s); // 0x000E3130 with the block
            },
            u32(ordinal)), s); // 0x00143320, 0x001439C0
    }
}
}

namespace
{
// The chapter table number (DAT_002FE698) of a script's chapter (0x0004B4D0), or 0 for chapter 0.
i32 script_chapter_number(const char* script, const GameSelectControlServices& s)
{
    const i32 chapter = gameselect_script_chapter(script, s); // 0x0004B4D0
    return chapter < 1 ? 0 : exe_integer(s, 0x002FE698 + u32(chapter) * 4);
}

// The held order's entry after `current`, the first after the last, or null when `current` is not in it.
const char* next_in_order(const GameSelectControlServices& s, const char* current)
{
    return current == nullptr ? nullptr : next_held_scene(s.scene_order_held, current);
}

// Xbox 0x000E23B0: replay chapter `number`. The held scene order becomes game select, every script of that chapter
// in the order (chapter 5 then game select and chapter5_scene13_1bookfail), and game select again, and the
// chapter's first script starts (0x00083B90, mode 2).
void replay_chapter(i32 number, GameSelectControlServices& s)
{
    const std::span<const u8> order = s.scene_order_held; // DAT_00511B64
    const char* first = order.empty() ? nullptr : reinterpret_cast<const char*>(order.data());
    while (first != nullptr && script_chapter_number(first, s) != number)
    {
        first = next_in_order(s, first);
    }
    std::vector<const char*> scripts;
    for (const char* entry = first; entry != nullptr && script_chapter_number(entry, s) == number;
         entry = next_in_order(s, entry))
    {
        scripts.push_back(entry);
    }
    const size_t extra = number == 5 ? 2 : 0;
    const size_t size = (scripts.size() + extra + 2) * 0x80; // 0x000F72D0((count + 2) << 7)
    std::vector<u8> heap(size + 0x80, 0);
    std::strcpy(reinterpret_cast<char*>(heap.data()), kGameSelectScript);
    size_t at = 1;
    for (const char* script : scripts)
    {
        std::strcpy(reinterpret_cast<char*>(heap.data() + at * 0x80), script);
        ++at;
    }
    if (number == 5)
    {
        std::strcpy(reinterpret_cast<char*>(heap.data() + at * 0x80), kGameSelectScript);
        std::strcpy(reinterpret_cast<char*>(heap.data() + (at + 1) * 0x80),
            "aid_script_ghoulies_chapter5_scene13_1bookfail"); // 0x003251B4
        at += 2;
    }
    std::strcpy(reinterpret_cast<char*>(heap.data() + at * 0x80), kGameSelectScript);
    // Retail queues the chapter's first script through its pointer into the old order after releasing it; the
    // host copies it.
    const std::string start = first == nullptr ? std::string() : std::string(first);
    if (s.scene_order_release != 0 && s.scene_order_held.data() != nullptr)
    {
        release_held_scene_order(s); // (*DAT_00511B68)(&DAT_00511B64)
    }
    s.frontend.scene_order_heap = std::move(heap);
    s.scene_order_held = s.frontend.scene_order_heap; // DAT_00511B64
    s.scene_order_release = 0x000358F0; // DAT_00511B68
    s.next_row = 9; // DAT_00429BC0
    auto& w = page_widget(kChapterPage, s);
    copy_wide(s.selected_game, w.name); // lstrcpyW(DAT_00429BC4, DAT_005065D0 +0x48C)
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    queue_runtime_gameselect_script(*w.owner, start.c_str(), 2, s); // 0x00083B90, mode 2
    s.profile.scene_flags->bytes[0] |= 0x80; // *(DAT_00506C74 + 8)
    set_runtime_game_scene_replay_state(w, 3, s); // 0x000E2900(DAT_005065D0, 3)
}

std::u16string chapter_heading(const char* group, i32 number, GameSelectControlServices& s)
{
    char key[0x100];
    std::snprintf(key, sizeof(key), "%d", number); // _sprintf
    std::array<char16_t, 0x400> text{};
    request_namespaced_localtext("aid_loctext_ghoulies_chapterheadings", group, key, text.data(),
        s.localtext); // 0x000FF0A0 into DAT_00507E78
    return std::u16string(text.data());
}

// Xbox 0x000E27A0: one item per chapter before the save's (five for a finished game), the last first.
void add_replay_chapters(GameSelectControlServices& s)
{
    auto& w = page_widget(kChapterPage, s);
    i32 count = save_game_word(w.name, 0x188) == 0 ? w.chapter_number - 1 : 5; // 0x0010DC60
    for (; 0 < count; --count)
    {
        // The item's context is an 8-byte heap block (0x0014E872, tagged 0x1234) holding the chapter.
        std::u16string label = chapter_heading("chapternumber", count, s);
        label += u" : "; // wsprintfW(L"%s : %s", ..)
        label += chapter_heading("chaptername", count, s);
        add_runtime_button_bat_menu_item(make_runtime_button_bat_menu_item(label.c_str(), {}, {}, {}, {},
            [&s](u32 context)
            {
                replay_chapter(i32(context), s); // 0x000E23B0 with the block
            },
            u32(count)), s); // 0x00143320, 0x001439C0
    }
}

// Xbox 0x000E43C0: the replay scenes as row 10 lists them (0x000E3560), each whose bonus book has been found (its
// DAT_00506C54 bit) with the bonus-book icon after its text.
void add_bonus_book_scenes(GameSelectControlServices& s)
{
    std::array<char16_t, 0x100> chapter_text{};
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "replayscene_chapter", chapter_text.data(),
            s.localtext); // 0x000FF120
    }
    const char16_t* name = page_widget(kBonusBookPage, s).name;
    char script[0x80]{};
    save_game_script(name, script); // 0x0010DB10
    i32 count;
    if (save_game_word(name, 0x188) != 0 && save_game_word(name, 0x260) != 0) // 0x0010DC60, 0x0010E120
    {
        count = 100;
    }
    else if (save_game_word(name, 0x188) == 0
        && exe_integer(s, 0x002FE698 + u32(gameselect_script_chapter(script, s)) * 4) < 5) // 0x0004B4D0
    {
        count = gameselect_scene_ordinal(script, s); // 0x0011ED30
    }
    else
    {
        count = i32(save_game_byte(name, 0x25D)) + 0x57; // 0x0010E0E0
    }
    const auto& books = s.profile.challenges.bytes; // DAT_00506C54 +8
    for (i32 ordinal = count - 1; 0 <= ordinal; --ordinal)
    {
        const std::wstring digits = std::to_wstring(replay_chapter_number(ordinal, s)); // 0x00166B36, radix 10
        std::u16string label(chapter_text.data()); // lstrcpyW
        label += u' '; // lstrcatW(L" ")
        label.append(digits.begin(), digits.end());
        std::array<char16_t, 0x800> scene{};
        label += u" : "; // wsprintfW(L"%s : %s", .., 0x0011ED80)
        label += gameselect_scene_name(ordinal, scene, s);
        const auto action = [&s](u32 context)
        {
            replay_scene(i32(context), kBonusBookPage, s); // 0x000E3F90 with the block
        };
        auto item = make_runtime_button_bat_menu_item(label.c_str(), {}, {}, {}, {}, action, u32(ordinal));
        if ((books[size_t(ordinal) >> 3] & (1u << (ordinal & 7))) != 0)
        {
            // 0x001433C0: the item with its icon, placed after the text (0x00060710 at DAT_00429AC0).
            const float width = runtime_text_line_width(s.metrics, 0, label.c_str(), exe_real(s, 0x00429AC0));
            const std::array<float, 2> offset{width * exe_real(s, 0x0032B1C4) + exe_real(s, 0x00429AD0),
                exe_real(s, 0x00429AD4)};
            const std::array<float, 2> full{exe_real(s, 0x00429AD8), exe_real(s, 0x00429ADC)};
            item->icon = std::make_unique<ButtonBatMenuSprite>(); // 0x000F72D0(0x2C)
            initialize_menu_sprite(*item->icon, s.image.string(0x00309BF0), offset, full, 0.5f,
                s); // 0x00144780, EAX the bonus-books icon
            item->icon_offset = offset; // +0x230
        }
        add_runtime_button_bat_menu_item(std::move(item), s); // 0x001439C0
    }
}

// The leave callbacks: back to the features with no book turn (rows 9 and 16), or to the challenges with the book
// turning back (row 11).
void leave_replay_page(const ReplayPage& page, u32 row, GameSelectControlServices& s)
{
    auto& control = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170
    control.requested_row = row;
    if (row == 6)
    {
        control.book_turn = 3;
        control.book = create_transition_book(1, 1, nullptr, s.books); // 0x00048260(1, 1, 0)
    }
    else
    {
        control.book_turn = 1;
        if (control.book != nullptr)
        {
            release_transition_book(control.book, s.books); // 0x00048860
            control.book = nullptr;
        }
    }
    set_runtime_game_scene_replay_state(page_widget(page, s), 4, s);
}

GameSceneReplayWidget& replay_of(GameSelectWidget& widget)
{
    return static_cast<GameSceneReplayWidget&>(widget);
}

void open_replay_page(const ReplayPage& page, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services);
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
    (void)owner; // the control's bytes; the widget reaches the control through +0x494
    open_replay_page(kScenePage, control, widget, services);
}

// Xbox 0x000E1E10
void open_runtime_game_chapter_replay(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    (void)owner;
    open_replay_page(kChapterPage, control, widget, services);
}

// Xbox 0x000E39F0
void open_runtime_game_bonus_book_replay(std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services)
{
    (void)owner;
    open_replay_page(kBonusBookPage, control, widget, services);
}

namespace
{
// The opens 0x000E1E10, 0x000E2B90, 0x000E39F0 and 0x000E48A0 over their page.
void open_replay_page(const ReplayPage& p, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = replay(widget);
    w.kind = &p;
    char page[0x80]{};
    std::memcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1", 42); // 0x00324C04
    w.owner = &control;
    w.name = s.frontend.selected_name.data(); // +0x48C, DAT_00429CC4
    w.state = 0;
    w.prompts = nullptr;
    s.profile.scene_flags->bytes[0] |= 2;
    s.frontend.*p.widget = &w; // DAT_005065D0..DAT_005065DC
    s.profile.scene_flags->bytes[0] |= 4;
    refresh_runtime_save_summary(s); // 0x000F2F00
    load_runtime_save_progress(w.name, s); // 0x000F3020, EAX +0x48C
    initialize_camera_view(w.camera, p.camera, 8); // 0x000496C0, EAX the camera, CL 8
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
    camera_bind_named_view(0, p.camera); // 0x00048CF0, EBX the camera
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    allocate_dialogue_text_pool(w.pool, 0, 1, 0x80, s.text_pools); // 0x0005F390, ESI +0x41C
    w.pool->origin = {layout_real(p, 0x30, s), layout_real(p, 0x34, s), std::bit_cast<float>(3u)};
    copy_wide(w.title.text.data(), w.name); // lstrcpyW(+4, +0x48C)
    w.title.x = layout_real(p, 0x10, s);
    w.title.y = layout_real(p, 0x14, s);
    w.title.record = 0;
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", p.banner, w.banner.text.data(),
            s.localtext); // 0x000FF120, EBX +0x210, EDI the banner key
    }
    w.banner.x = layout_real(p, 8, s);
    w.banner.y = layout_real(p, 0xC, s);
    w.banner.record = 0;
    if (p.chapter_number)
    {
        char script[0x80]{};
        save_game_script(w.name, script); // 0x0010DB10, EBX +0x48C
        w.chapter_number = exe_integer(s, 0x002FE698 + u32(gameselect_script_chapter(script, s)) * 4); // 0x0004B4D0
    }
    const std::array<float, 2> anchor{s.hud.center_x + layout_real(p, 0x38, s),
        s.hud.center_y + layout_real(p, 0x3C, s)}; // DAT_00509730, DAT_00509734
    create_runtime_button_bat_menu(anchor, i32(read_u32(s.image.memory(p.layout + 0x40, 4), 0)),
        layout_real(p, 0x44, s), layout_real(p, 0x48, s), 0, layout_real(p, 0x50, s), layout_real(p, 0x54, s),
        std::bit_cast<float>(3u), read_u32(s.image.memory(p.layout + 0x4C, 4), 0), 1, 1,
        s); // 0x00143550, EAX the anchor
    start_pause_ditty(s.ditty, s.streams); // The 0x00038910 call, while DAT_00429BB8 is clear.
    set_runtime_game_scene_replay_state(w, 1, s); // The page's state change (1)
}
}

// Xbox 0x000E2F20
void close_runtime_game_scene_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = replay(widget);
    destroy_runtime_button_bat_menu(s); // 0x00143680
    s.frontend.*w.kind->widget = nullptr; // DAT_005065D0..DAT_005065DC
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

// Xbox 0x000E21A0
void close_runtime_game_chapter_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    close_runtime_game_scene_replay(widget, services); // The 0x000E2F20 body over DAT_005065D0
}

// Xbox 0x000E3D80
void close_runtime_game_bonus_book_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    close_runtime_game_scene_replay(widget, services); // The 0x000E2F20 body over DAT_005065D8
}

// Xbox 0x000E2230
void tick_runtime_game_chapter_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    tick_runtime_game_scene_replay(widget, services); // The 0x000E2FB0 body with 0x000E2900
}

// Xbox 0x000E3E10
void tick_runtime_game_bonus_book_replay(GameSelectWidget& widget, GameSelectControlServices& services)
{
    tick_runtime_game_scene_replay(widget, services); // The 0x000E2FB0 body with 0x000E4650
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

// Xbox 0x000E3760 (row 10), 0x000E2900 (row 9), 0x000E4650 (row 11) and 0x000E57F0 (row 16): the same body over
// the page's floats, rows and items.
void set_runtime_game_scene_replay_state(GameSceneReplayWidget& w, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    const ReplayPage& p = *w.kind;
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
    if (w.state == 1)
    {
        const float icon = layout_real(p, 0x28, s);
        const float text = layout_real(p, 0x2C, s);
        const float spacing = layout_real(p, 0x20, s);
        const std::array<float, 2> anchor{s.hud.inner_left + layout_real(p, 0x18, s),
            s.hud.inner_bottom - layout_real(p, 0x1C, s)}; // DAT_00509740, DAT_0050974C
        const std::array<float, 2> row_step{icon * 0.f, 0.f - icon * spacing};
        const float half = spacing * exe_real(s, 0x0032B0F4) * icon;
        const std::array<float, 2> origin{half + layout_real(p, 0x24, s), 0.f - half};
        std::array<char16_t, 0x200> scratch{};
        const float width = measure_dialogue_widget_width(resource, s.image.memory(p.prompts, 0x310).data(), icon,
            text, "gameselect", scratch.data(), s.widgets); // 0x001428F0, EAX the resource, ECX the rows
        const std::array<float, 2> column{width, 0.f};
        // Both records' ids go to +0x20C: retail stores the banner's over the title's.
        w.title.record = create_dialogue_text_record(*w.pool, w.title.text.data(), w.title.x, w.title.y,
            layout_real(p, 0, s), layout_real(p, 0, s), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random); // 0x0005F5C0, EAX 1
        w.title.record = create_dialogue_text_record(*w.pool, w.banner.text.data(), w.banner.x, w.banner.y,
            layout_real(p, 4, s), layout_real(p, 4, s), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random);
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return;
        }
        create_dialogue_widget(w.prompts, 1, s.image.memory(p.prompts, 0x310).data(), anchor, row_step, origin,
            &column, icon, text, "gameselect", s.text, 0x400C0, s.widget_allocation); // 0x00141E20, EAX +0x498
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {layout_real(p, 0x30, s), layout_real(p, 0x34, s), std::bit_cast<float>(3u)};
        p.add_items(s); // 0x000E3560, 0x000E27A0, 0x000E43C0 or 0x000E5430
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

// Xbox 0x000E2360: B goes back to the features (row 8).
void leave_runtime_game_chapter_replay(GameSelectControlServices& services)
{
    leave_replay_page(kChapterPage, 8, services); // 0x000E2900(DAT_005065D0, 4)
}

// Xbox 0x000E3F40: B goes back to the challenges (row 6), the book turning back.
void leave_runtime_game_bonus_book_replay(GameSelectControlServices& services)
{
    leave_replay_page(kBonusBookPage, 6, services); // 0x000E4650(DAT_005065D8, 4)
}

namespace
{
const char* current_script()
{
    return reinterpret_cast<const char*>(g_scene_record); // DAT_00510E58, game select's own script
}

// 0x00035960 over the held order (EAX DAT_00511B64) with wrap 1.
const char* next_wrapped(const GameSelectControlServices& s, const char* current)
{
    return next_scene_order_name(s.scene_order_held, current, 1);
}

bool is_book(const char* script)
{
    return std::strstr(script, "book") != nullptr; // _strstr(.., 0x00322634)
}

// Xbox 0x0011EE30, EAX the script: its book's name (booknames, keyed from "chapter" on), or the empty string.
std::u16string book_name(const char* script, GameSelectControlServices& s)
{
    char copy[0x100];
    std::strncpy(copy, script, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = 0;
    const char* key = std::strstr(copy, "chapter");
    if (key == nullptr || *key == 0)
    {
        return std::u16string(); // DAT_0030C9D0
    }
    std::array<char16_t, 0x400> text{};
    request_namespaced_localtext("aid_loctext_ghoulies_script_booknames", "booknames", key, text.data(),
        s.localtext); // 0x000FF0A0 into DAT_00507E78
    return std::u16string(text.data());
}

// Xbox 0x000E5360: the name of the `number`-th book after game select in the held order, or the empty string.
std::u16string story_book_name(u32 number, GameSelectControlServices& s)
{
    u32 books = 0;
    for (const char* entry = next_wrapped(s, current_script()); entry != nullptr;
         entry = next_in_order(s, entry))
    {
        if (transition_aid_equal(current_script(), entry)) // 0x000357D0, EAX DAT_00510E58
        {
            break;
        }
        if (is_book(entry))
        {
            ++books;
        }
        if (number == (books & 0xFFFF))
        {
            return book_name(entry, s); // The tail jump to 0x0011EE30
        }
    }
    return std::u16string(); // DAT_0030C9D0
}

// Xbox 0x000E5120: the books the game has passed: those after game select up to chapter5_scene1_1playcam, before
// the save's scene for an unfinished game (counting the save's own when it is a book).
u16 count_story_books(GameSelectControlServices& s)
{
    const auto& w = page_widget(kStoryBookPage, s);
    char script[0x80]{};
    save_game_script(w.name, script); // 0x0010DB10
    const bool finished = save_game_word(w.name, 0x188) != 0; // 0x0010DC60
    const char* limit = "aid_script_ghoulies_chapter5_scene1_1playcam"; // 0x00325314
    u16 books = 0;
    for (const char* entry = next_wrapped(s, current_script()); entry != nullptr; entry = next_in_order(s, entry))
    {
        if (transition_aid_equal(limit, entry) || (!finished && transition_aid_equal(script, entry))
            || transition_aid_equal(current_script(), entry)) // 0x000357D0
        {
            break;
        }
        if (is_book(entry))
        {
            ++books;
        }
    }
    if (!finished && is_book(script))
    {
        ++books;
    }
    return books;
}

// Xbox 0x000E5680: replay one book. The held order becomes game select, the book and game select again, and the
// book starts (0x00083B90, mode 2).
void play_story_book(const char* book, GameSelectControlServices& s)
{
    std::vector<u8> heap(0x200, 0); // 0x000F72D0(0x180) copied into 0x000F72D0(0x200), byte 0x180 cleared
    std::strcpy(reinterpret_cast<char*>(heap.data()), kGameSelectScript);
    std::strcpy(reinterpret_cast<char*>(heap.data() + 0x80), book);
    std::strcpy(reinterpret_cast<char*>(heap.data() + 0x100), kGameSelectScript);
    const std::string start(book); // EBP the book, read after the old order is released
    if (s.scene_order_release != 0 && s.scene_order_held.data() != nullptr)
    {
        release_held_scene_order(s); // (*DAT_00511B68)(&DAT_00511B64)
    }
    s.frontend.scene_order_heap = std::move(heap);
    s.scene_order_held = s.frontend.scene_order_heap; // DAT_00511B64
    s.scene_order_release = 0x000358F0; // DAT_00511B68
    auto& w = page_widget(kStoryBookPage, s);
    s.next_row = 0x10; // DAT_00429BC0
    copy_wide(s.selected_game, w.name); // lstrcpyW(DAT_00429BC4, DAT_005065DC +0x48C)
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    queue_runtime_gameselect_script(*w.owner, start.c_str(), 2, s); // 0x00083B90, ECX +0x494, EAX the book
    s.profile.scene_flags->bytes[1] |= 1; // *(DAT_00506C74 + 8) + 1
    set_runtime_game_scene_replay_state(w, 3, s); // 0x000E57F0(DAT_005065DC, 3)
}

// Xbox 0x000E4EE0: the `number`-th book after game select.
void play_numbered_story_book(i32 number, GameSelectControlServices& s)
{
    std::string book;
    i32 books = 0;
    for (const char* entry = next_wrapped(s, current_script()); entry != nullptr && books < number;
         entry = next_in_order(s, entry))
    {
        if (transition_aid_equal(current_script(), entry)) // 0x000357D0, EAX DAT_00510E58
        {
            break;
        }
        if (is_book(entry))
        {
            ++books;
            book = entry;
        }
    }
    play_story_book(book.c_str(), s); // The 0x000E5680 body inline
}

// Xbox 0x000E5080 / 0x000E50D0: a bonus book, played with the unlockable scenes' order held first.
void play_unlockable_book(const char* book, GameSelectControlServices& s)
{
    hold_runtime_scene_order("aid_aidlist_ghoulies_sceneorder_gameselect_unlockablescenes", s); // 0x001234B0
    play_story_book(book, s); // 0x000E5680
}

void add_book(const std::u16string& name, std::function<void(u32)> action, u32 context, GameSelectControlServices& s)
{
    add_runtime_button_bat_menu_item(make_runtime_button_bat_menu_item(name.c_str(), {}, {}, {}, {},
        std::move(action), context), s); // 0x00143320, 0x001439C0
}

std::u16string bonus_book_name(const char* key, GameSelectControlServices& s)
{
    std::array<char16_t, 0x400> text{};
    request_namespaced_localtext("aid_loctext_ghoulies_script_booknames", "booknames", key, text.data(),
        s.localtext); // 0x000FF0A0
    return std::u16string(text.data());
}

// Xbox 0x000E5430: the books passed, the last first, with chapter 5's ending books for a game that reached them
// and, once challenge 21 is platinum (DAT_004E199C 6), the two bonus books.
void add_story_books(GameSelectControlServices& s)
{
    auto& w = page_widget(kStoryBookPage, s);
    const u16 count = count_story_books(s); // 0x000E5120
    w.chapter_number = i32(count); // +0x490, a word here
    const char16_t* name = w.name;
    const auto play = [&s](const char* book)
    {
        return [&s, book](u32)
        {
            play_story_book(book, s); // 0x000E5680
        };
    };
    if (save_game_word(name, 0x188) == 0) // 0x0010DC60
    {
        if (save_game_byte(name, 0x25D) != 0 && save_game_word(name, 0x264) != 0) // 0x0010E0E0, 0x0010E160
        {
            add_book(story_book_name(0x15, s), play("aid_script_ghoulies_chapter5_scene13_1book"), 0, s); // 0x000E5000
        }
    }
    else if (save_game_word(name, 0x260) != 0) // 0x0010E120
    {
        add_book(story_book_name(0x16, s), play("aid_script_ghoulies_chapter5_scene13_3book"), 0, s); // 0x000E4FC0
        add_book(story_book_name(0x15, s), play("aid_script_ghoulies_chapter5_scene13_1book"), 0, s); // 0x000E5000
    }
    else
    {
        add_book(story_book_name(0x17, s), play("aid_script_ghoulies_chapter5_scene13_1bookfail"), 0,
            s); // 0x000E5040
    }
    const i32 challenge21 = i32(read_u32(g_game_progress.words_194c, 0x50)); // DAT_004E199C
    for (u32 number = u16(w.chapter_number); 0 < i32(number); --number)
    {
        // The item's context is an 8-byte heap block (0x0014E872, tagged 0x1234) holding the number.
        add_book(story_book_name(number, s), [&s](u32 context)
        {
            play_numbered_story_book(i32(context), s); // 0x000E4EE0 with the block
        }, number, s);
        if (number == 3 && challenge21 == 6)
        {
            add_book(bonus_book_name("chapter1_scene1_3book", s), [&s](u32)
            {
                play_unlockable_book("aid_script_ghoulies_chapter1_scene1_3book", s); // 0x000E5080
            }, 0, s);
        }
    }
    if (challenge21 == 6)
    {
        add_book(bonus_book_name("challenges_originalvideo", s), [&s](u32)
        {
            play_unlockable_book("aid_script_ghoulies_challenges_originalvideo", s); // 0x000E50D0
        }, 0, s);
    }
}
}

// Xbox 0x000E48A0
void open_runtime_game_story_books(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    (void)owner;
    open_replay_page(kStoryBookPage, control, widget, services);
}

// Xbox 0x000E4BF0
void close_runtime_game_story_books(GameSelectWidget& widget, GameSelectControlServices& services)
{
    close_runtime_game_scene_replay(widget, services); // The 0x000E2F20 body over DAT_005065DC
}

// Xbox 0x000E4C80
void tick_runtime_game_story_books(GameSelectWidget& widget, GameSelectControlServices& services)
{
    tick_runtime_game_scene_replay(widget, services); // The 0x000E2FB0 body with 0x000E57F0
}

// Xbox 0x000E4E70: B goes back to the features (row 8).
void leave_runtime_game_story_books(GameSelectControlServices& services)
{
    leave_replay_page(kStoryBookPage, 8, services); // 0x000E57F0(DAT_005065DC, 4)
}
}
