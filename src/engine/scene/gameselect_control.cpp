#include "gameselect_control.h"
#include "../saves/save_games.h"
#include "../transitions/native_book.h"
#include "../cutscenes/instance_construction.h"
#include "../frontend/frontend_state.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../frontend/frontend_sounds.h"
#include "../saves/save_summary.h"
#include "../scene/scene_types.h"
#include "scene_control_fields.h"
#include "scene_controls.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{

constexpr u32 kAid = 0x5F646961u; // "aid_"

// The frame step: nothing while frozen (DAT_004312DC > 0), else DAT_005096C0 * DAT_004312C4.
float frame_step(const GameSelectControlServices& services)
{
    if (services.freeze <= 0.f)
    {
        return services.dt * services.scene_scale;
    }
    return 0.f;
}

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

// The book's place in its clip: time over duration (+0x2C / +0x34 of its cutscene).
float book_position(const GameSelectControlState& control)
{
    const auto* cutscene = control.book->cutscene;
    return cutscene->time / cutscene->duration;
}

// Xbox 0x00083E20, ECX the control: whether the menu book lets the requested widget open. Pages 2 and 3 wait
// for the book past its clip's first threshold.
bool book_allows_widget(const GameSelectControlState& control,
    const GameSelectControlServices& services)
{
    const u32 book = control.book_turn;
    if (book == 1)
    {
        return true;
    }
    if (book != 2 && book != 3)
    {
        return true;
    }
    const float position = book_position(control);
    const float threshold = transition_book_threshold(*control.book, 0x104, services.image); // 0x0042217C
    return threshold <= position && position != threshold;
}

// Xbox 0x00083D90, ESI the control: a finished book is released; a running one ticks and asks for a page
// capture once it is past the clip's first or second threshold.
void advance_gameselect_book(GameSelectControlState& control, GameSelectControlServices& services)
{
    auto& books = services.books;
    if (control.book != nullptr && control.book->finished != 0)
    {
        release_transition_book(control.book, books); // 0x00048860
        control.book = nullptr;
    }
    if (control.book == nullptr)
    {
        return;
    }
    tick_transition_book(*control.book, books); // 0x000483E0
    auto& book = *control.book;
    float position = book_position(control);
    i32 page;
    if (position < transition_book_threshold(book, 0x104, services.image)
        || position == transition_book_threshold(book, 0x104, services.image)) // 0x0042217C
    {
        page = transition_book_row_word(book, 0x10C, services.image); // 0x00422184
    }
    else
    {
        position = book_position(control);
        if (position < transition_book_threshold(book, 0x108, services.image)
            || position == transition_book_threshold(book, 0x108, services.image)) // 0x00422180
        {
            return;
        }
        page = transition_book_row_word(book, 0x110, services.image); // 0x00422188
    }
    if (page != -1)
    {
        book.pages.at(size_t(page)).capture = 1; // +0x94 + page * 0x10
    }
}

// Xbox 0x00083C70 (EAX the control) and 0x00083D10 (ESI the control): the scene flags for the chosen start
// (+0x11C), the selected game becomes the current one and its script (0x00083C70) or +0x98's (0x00083D10) is
// requested.
void start_gameselect_game(const GameSelectControlState& control, bool saved_script,
    GameSelectControlServices& services)
{
    auto& bytes = services.profile.scene_flags->bytes;
    const u32 mode = control.start_mode;
    if (mode == 1)
    {
        bytes[0] |= 2;
        bytes[0] |= 4;
    }
    else if (mode == 2)
    {
        bytes[0] &= 0xFD;
        bytes[0] |= 4;
    }
    else if (mode == 3)
    {
        bytes[0] &= 0xFD;
        bytes[0] &= 0xFB;
    }
    std::copy(services.frontend.selected_name.begin(), services.frontend.selected_name.end(),
        services.profile.name.begin()); // lstrcpyW(DAT_00506C90, DAT_00429CC4)
    refresh_runtime_save_summary(services); // 0x000F2F00
    if (saved_script)
    {
        char script[0x80]{};
        save_game_script(services.frontend.selected_name.data(), script); // 0x0010DB10, EBX DAT_00429CC4
        services.request_script(script, 1); // 0x0011EB30
        return;
    }
    services.request_script(control.script.data(), 1); // 0x0011EB30, EAX +0x98
}
}

// Xbox 0x001234B0, EAX name: the held scene-order resource (DAT_00511B64) and its release (DAT_00511B68).
void hold_runtime_scene_order(const char* name, GameSelectControlServices& services)
{
    if (services.scene_order_release != 0 && services.scene_order_held.data() != nullptr)
    {
        release_held_scene_order(services);
    }
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        services.scene_order_held = {};
        services.scene_order_release = 0x00035940;
        return;
    }
    const auto held = acquire_animation_resource(services.resources, name);
    if (held.data() == nullptr)
    {
        resource_error(services.error, 2);
    }
    services.scene_order_held = held;
    services.scene_order_release = 0x00035940;
}

// The (*DAT_00511B68)(&DAT_00511B64) call: 0x00035940 releases a held resource, 0x000358F0 frees a heap block.
void release_held_scene_order(GameSelectControlServices& services)
{
    if (services.scene_order_release == 0x00035940)
    {
        // 0x00035940: 0x00035C00 over the held pointer, then the slot is cleared.
        release_animation_resource(services.resources, services.scene_order_held);
        services.scene_order_held = {};
        return;
    }
    if (services.scene_order_release == 0x000358F0)
    {
        // Xbox 0x000358F0: HeapFree on the block's heap (0x000F7100), then the slot is cleared.
        services.frontend.scene_order_heap.clear();
        services.scene_order_held = {};
        return;
    }
    untranslated("the scene-order release callback");
}

// Xbox 0x00083B90
void queue_runtime_gameselect_script(GameSelectControlState& control, const char* script, u32 mode,
    GameSelectControlServices& services)
{
    if (control.pending_start == 2)
    {
        return;
    }
    control.start_mode = mode;
    std::strcpy(control.script.data(), script); // the byte copy to +0x98
    control.pending_start = 2;
    if (services.stream_busy(2) == 0) // 0x00039D50(2, 0, 0)
    {
        control.start_ready = 1;
        return;
    }
    services.frontend.word_005065bc = 0; // DAT_005065BC
    services.frontend.word_005065c0 = 1; // DAT_005065C0
    auto& owner = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170, EDX 0x9A
    owner.requested_row = 0x12;
    owner.book_turn = 1;
    if (owner.book != nullptr)
    {
        release_transition_book(owner.book, services.books); // 0x00048860
        owner.book = nullptr;
    }
}

// Xbox 0x000835F0
void gameselect_control_object_sizes(ObjectSizes& output)
{
    output.instance = 300;
    output.parameters = 8;
}

// Xbox 0x00083660
bool construct_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services)
{
    char name[0x80]{};
    std::memcpy(name, "aid_loctext_ghoulies_gameselect_gameselecttext", 47);
    auto& profile = services.profile;
    clear_runtime_profile_block({profile.scene_flags->bytes, size_t(profile.scene_flags->size)});
    profile.scene_flags->bytes[0] |= 2;
    profile.times = {};
    profile.scene_flags->bytes[0] |= 4;
    refresh_runtime_save_summary(services); // 0x000F2F00
    services.flags |= 0x100000u;
    control.current_row = 0;
    control.requested_row = 0;
    control.book_turn = 1;
    control.start_mode = 1;
    control.start_ready = 0;
    control.texture = create_runtime_screen_texture(0x280, 0x1E0, 1, 0x1E, 0, services.textures);
    control.pending_start = 0;
    std::span<const u8> text;
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        text = acquire_animation_resource(services.resources, name);
        if (text.data() == nullptr)
        {
            resource_error(services.error, 2);
        }
    }
    services.text = text;
    control.book = nullptr;
    control.row = nullptr;
    select_runtime_gameselect_row(control.row, services);
    hold_runtime_scene_order(services.scene_order, services);
    control.widget = create_gameselect_widget(*control.row, services);
    open_gameselect_widget(*control.row, state, control, *control.widget, services);
    initialize_camera_view(control.camera, "gameselect", 8); // 0x003241E0
    control.camera.aspect_slot = 0;
    i32 fit_w = 0;
    i32 fit_x = 0;
    i32 fit_h = 0;
    i32 fit_y = 0;
    float fov = 0.f;
    camera_fit_display_view(control.camera.focal, 0, fit_w, fit_x, fit_h, fit_y, fov, control.camera.pixel_aspect,
        control.camera.focal_pixels);
    link_camera_view(0, control.camera); // 0x00048C20, inline.
    ++services.pause_count;
    push_runtime_input_entry(services.frontend_input, "game select", services.input); // 0x003241D4
    return true;
}

// Xbox 0x00083850
void destroy_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services)
{
    stop_pause_ditty(services.ditty, services.streams);
    // A widget whose creation raised (an untranslated row) was never opened; the host only frees the control.
    if (control.widget != nullptr)
    {
        close_gameselect_widget(*control.row, *control.widget, services);
        destroy_runtime_gameselect_widget(*control.row, control.widget);
    }
    camera_unlink_view(&control.camera);
    release_animation_resource(services.resources, services.text);
    if (control.book != nullptr)
    {
        release_transition_book(control.book, services.books); // 0x00048860
        control.book = nullptr;
    }
    --services.pause_count;
    release_runtime_screen_texture(control.texture, services.textures);
    control.texture = {};
    pop_runtime_input_entry(services.input);
    services.flags &= 0xFFEFFFFFu;
    (void)state;
}

// Xbox 0x000838D0
void tick_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services)
{
    SceneControlBase& base = scene_control_view<SceneControlBase>(state);
    base.elapsed = base.elapsed + frame_step(services);
    refresh_runtime_input_device(services.frontend_input, services.frame, services.input, services.input_release);
    if (control.start_ready != 0)
    {
        const u32 pending = control.pending_start;
        if (pending == 1)
        {
            start_gameselect_game(control, true, services); // 0x00083C70
        }
        if (pending == 2)
        {
            start_gameselect_game(control, false, services); // 0x00083D10
        }
        if (pending == 1 || pending == 2)
        {
            control.pending_start = 0;
        }
    }
    if (control.requested_row != 0 && book_allows_widget(control, services))
    {
        close_gameselect_widget(*control.row, *control.widget, services);
        destroy_runtime_gameselect_widget(*control.row, control.widget);
        const u32 next = control.requested_row;
        control.current_row = next;
        control.requested_row = 0;
        control.row = &gameselect_widget_row(next);
        control.widget = create_gameselect_widget(*control.row, services);
        open_gameselect_widget(*control.row, state, control, *control.widget, services);
    }
    tick_gameselect_widget(*control.row, *control.widget, services);
    const u32 book = control.book_turn;
    if (book == 2 || book == 3)
    {
        advance_gameselect_book(control, services); // 0x00083D90
    }
}

// Xbox 0x000839E0
void draw_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services)
{
    auto& display = services.display;
    if (select_display_camera(&control.camera, 0, display)) // 0x000489E0
    {
        rebuild_display_view(display); // 0x000F5E50
        extend_display_clip_limits(entity_detail::g_display_view->transform, display.scene_bounds,
            control.camera.blend, control.camera.clip_limit); // 0x0013C490, EAX DAT_005077A8 + 0x68
        entity_detail::g_display_view->near_clip = control.camera.clip_limit;
        entity_detail::g_display_view->far_clip = control.camera.blend;
        rebuild_display_projection(display); // 0x000F5FD0
        bind_display_projection(display); // 0x000F6170
    }
    draw_gameselect_widget(*control.row, *control.widget, services);
    (void)state;
}

// Xbox 0x00083C40
bool runtime_gameselect_book_busy(const GameSelectControlState& control,
    const GameSelectControlServices& services)
{
    if (services.transition != 0)
    {
        return true;
    }
    const i32 book = i32(control.book_turn);
    if (1 < book && book <= 3 && control.book != nullptr)
    {
        return true;
    }
    return false;
}

// Xbox 0x00083A50, EDI row, EBX turn
void request_runtime_gameselect_row(u32 row, u32 turn, i32 check_stream, GameSelectControlServices& services)
{
    auto& control = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170
    if (row == 0)
    {
        return;
    }
    if (check_stream == 0 || services.stream_busy(2) == 0) // 0x00039D50(2, 0, 0)
    {
        control.requested_row = row;
        control.book_turn = turn;
    }
    else
    {
        control.requested_row = 0x12;
        control.book_turn = 1;
        services.frontend.word_005065bc = i32(row); // DAT_005065BC
        services.frontend.word_005065c0 = i32(turn); // DAT_005065C0
    }
    switch (control.book_turn)
    {
    case 1:
        if (control.book != nullptr)
        {
            release_transition_book(control.book, services.books); // 0x00048860
            control.book = nullptr;
        }
        break;
    case 2:
        control.book = create_transition_book(0, 1, nullptr, services.books); // 0x00048260(0, 1, 0)
        break;
    case 3:
        control.book = create_transition_book(1, 1, nullptr, services.books); // 0x00048260(1, 1, 0)
        break;
    default:
        break;
    }
}

// Xbox 0x000E5AE0
void select_runtime_gameselect_row(const GameSelectWidgetRow*& row, GameSelectControlServices& services)
{
    if (services.stream_busy(2) == 0)
    {
        row = &gameselect_widget_row(u32(services.next_row));
        return;
    }
    row = &gameselect_widget_row(18); // PTR_LAB_0030A1B0
    if (services.stream_busy(2) == 0)
    {
        services.stream_wait = 0.f;
        return;
    }
    services.stream_wait = services.stream_wait_seconds;
}
}
