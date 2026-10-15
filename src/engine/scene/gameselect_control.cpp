#include "gameselect_control.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../frontend/frontend_sounds.h"
#include "../saves/save_summary.h"
#include "../scene/scene_types.h"

#include <bit>
#include <cstring>

namespace gbtg
{
bool g_native_game_select = false;

namespace
{
constexpr u32 kAid = 0x5F646961u; // "aid_"

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

float real(std::span<const u8> bytes, size_t offset)
{
    return std::bit_cast<float>(read_u32(bytes, offset));
}

void put_real(std::span<u8> bytes, size_t offset, float value)
{
    put_word(bytes, offset, std::bit_cast<u32>(value));
}

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

// Xbox 0x001234B0, EAX name: the held scene-order resource (DAT_00511B64) and its release (DAT_00511B68).
void hold_scene_order(const char* name, GameSelectControlServices& services)
{
    if (services.scene_order_release != 0 && services.scene_order_held.data() != nullptr)
    {
        if (services.scene_order_release != 0x00035940)
        {
            untranslated("the scene-order release callback");
        }
        // 0x00035940: 0x00035C00 over the held pointer, then the slot is cleared.
        release_animation_resource(services.resources, services.scene_order_held);
        services.scene_order_held = {};
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

// 0x00083E20, ECX the control: whether the menu book lets the requested widget open.
bool book_allows_widget(std::span<const u8> state, const GameSelectControlState& control)
{
    const u32 book = word(state, 0x124);
    if (book == 1)
    {
        return true;
    }
    if (book == 2 || book == 3)
    {
        (void)control;
        untranslated("0x00083E20's menu book page threshold");
    }
    return true;
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
    put_word(state, 0x90, 0);
    put_word(state, 0x94, 0);
    put_word(state, 0x124, 1);
    put_word(state, 0x11C, 1);
    put_word(state, 0x128, 0);
    control.texture = create_runtime_screen_texture(0x280, 0x1E0, 1, 0x1E, 0, services.textures);
    put_word(state, 0x118, 0);
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
    hold_scene_order(services.scene_order, services);
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
        untranslated("0x00048860 (release the menu book)");
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
    put_real(state, 0x20, real(state, 0x20) + frame_step(services));
    refresh_runtime_input_device(services.frontend_input, services.frame, services.input, services.input_release);
    if (word(state, 0x128) != 0)
    {
        const u32 pending = word(state, 0x118);
        if (pending == 1)
        {
            untranslated("0x00083C70 (save and continue)");
        }
        if (pending == 2)
        {
            untranslated("0x00083D10 (save and quit)");
        }
        if (pending == 1 || pending == 2)
        {
            put_word(state, 0x118, 0);
        }
    }
    if (word(state, 0x94) != 0 && book_allows_widget(state, control))
    {
        close_gameselect_widget(*control.row, *control.widget, services);
        destroy_runtime_gameselect_widget(*control.row, control.widget);
        const u32 next = word(state, 0x94);
        put_word(state, 0x90, next);
        put_word(state, 0x94, 0);
        control.row = &gameselect_widget_row(next);
        control.widget = create_gameselect_widget(*control.row, services);
        open_gameselect_widget(*control.row, state, control, *control.widget, services);
    }
    tick_gameselect_widget(*control.row, *control.widget, services);
    const u32 book = word(state, 0x124);
    if (book == 2 || book == 3)
    {
        untranslated("0x00083D90 (advance the menu book)");
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
bool runtime_gameselect_book_busy(std::span<const u8> state, const GameSelectControlState& control,
    const GameSelectControlServices& services)
{
    if (services.transition != 0)
    {
        return true;
    }
    const i32 book = i32(word(state, 0x124));
    if (1 < book && book <= 3 && control.book != nullptr)
    {
        return true;
    }
    return false;
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
