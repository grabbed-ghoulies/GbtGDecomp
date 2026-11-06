#include "app_error_screen.h"
#include "dialogue_draw_outer.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_state.h"
#include "gameselect_rows.h"
#include "hud_layout.h"
#include "input_activation.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/event_runtime.h"
#include "../saves/save_games.h"
#include "../scene/executable_resources.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <string>
#include <string_view>

namespace gbtg
{
AppErrorScreen g_app_error;
GameSelectControlServices* g_app_error_services = nullptr;

namespace
{
constexpr u32 kOneButtonRows = 0x00339F98; // "AppMainErrorDialogMenu_1": A (0x00032F10)
constexpr u32 kTwoButtonRows = 0x0033A2A8; // "AppMainErrorDialogMenu_2": B (0x00032FA0), A (0x00032F10)

GameSelectControlServices& services(const char* reader)
{
    if (g_app_error_services == nullptr)
    {
        throw Error(std::string("the app error screen (") + reader + ") ran outside a native scene");
    }
    return *g_app_error_services;
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

// A row's key (+4 of each 0x48-byte row after the 0x40-byte name): the strcpy into the set.
void copy_row_key(std::array<u8, 0x310>& rows, size_t row, const char* key)
{
    char* target = reinterpret_cast<char*>(rows.data() + 0x40 + row * 0x48 + 4);
    std::strcpy(target, key);
}

// Leaving phase 3 frees the page's text, prompts, cutscene and view.
void release_screen(GameSelectControlServices& s)
{
    auto& e = g_app_error;
    release_dialogue_text_pool(e.pool, *s.text_pools); // 0x0005F460, EBX DAT_004DEA10
    e.pool = nullptr;
    if (e.widget != nullptr)
    {
        destroy_dialogue_widget(e.widget, s.resources, *s.text_pools); // 0x00142070
        e.widget = nullptr;
    }
    release_runtime_cutscene(e.page, s.cutscenes); // 0x0004D720, EAX DAT_004DE9AC
    e.page = nullptr;
    camera_unlink_view(&e.camera); // 0x00048C70, EAX DAT_004DE9B0
}

// Phase 3's entry: the prompts close (0x00142C20(3)) and the text records end (0x0005F970).
void close_screen(GameSelectControlServices& s)
{
    auto& e = g_app_error;
    e.widget->requested_set = nullptr; // +0x950
    set_dialogue_widget_mode(*e.widget, 3, s.widgets); // 0x00142C20, ESI DAT_004DE9A0, EAX 3
    end_all_dialogue_text_records(*e.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
}

void open_screen(GameSelectControlServices& s)
{
    auto& e = g_app_error;
    char resource[0x80]{};
    std::memcpy(resource, "aid_loctext_ghoulies_apperror", 30); // 0x0031C2E4
    e.input = create_runtime_keyboard_input(s.input_release); // 0x00107EF0, ESI DAT_004DE99C
    e.input_address = s.map_input(e.input);
    initialize_camera_view(e.camera, "AppMainErrorDialog", 8); // 0x000496C0, EAX 0x0031C2D0, CL 8
    e.camera.aspect_slot = 0; // DAT_004DE9E0
    i32 fit_w = 0;
    i32 fit_x = 0;
    i32 fit_h = 0;
    i32 fit_y = 0;
    float fov = 0.f;
    camera_fit_display_view(e.camera.focal, 0, fit_w, fit_x, fit_h, fit_y, fov, e.camera.pixel_aspect,
        e.camera.focal_pixels); // 0x000F57F0, DAT_004DE9D4, DAT_004DEA08, DAT_004DEA0C
    link_camera_view(0, e.camera); // 0x00048C20, EDX DAT_004DE9B0, ESI 0
    e.page = load_runtime_cutscene("aid_cutscene_ghoulies_dialogs_dialogpage1", 6, &e.camera,
        s.cutscenes); // 0x0004D680, 0x002180E0
    camera_bind_named_view(0, "AppMainErrorDialog"); // 0x00048CF0, EBX 0x0031C2D0
    const float scale = exe_real(s, 0x00339F88); // 0.8
    const float half = exe_real(s, 0x00339F80) * 0.5f * scale; // DAT_0032B0F4.
    const std::array<float, 2> anchor{s.hud.inner_left + exe_real(s, 0x00339F78),
        s.hud.inner_bottom - exe_real(s, 0x00339F7C)}; // DAT_00509740, DAT_0050974C
    const std::array<float, 2> row_step{scale * 0.f, 0.f - scale * exe_real(s, 0x00339F80)};
    const std::array<float, 2> origin{half + exe_real(s, 0x00339F84), 0.f - half};
    create_resource_dialogue_widget(e.widget, 0, e.rows, anchor, row_step, origin, nullptr, scale,
        exe_real(s, 0x00339F8C), "apperror", resource, 0x80, s.widget_allocation); // 0x00141DA0, ESI DAT_004DE9A0
    auto& widget = *e.widget;
    const float fade = 0.2f; // DAT_0032B1AC.
    widget.duration = fade; // +0x10
    if (fade < widget.progress) // +8
    {
        widget.progress = fade;
    }
    widget.pool->end_time = fade; // +0x1020 +0x20
    widget.pool->origin = {exe_real(s, 0x00339F90), exe_real(s, 0x00339F94), std::bit_cast<float>(3u)};
    allocate_dialogue_text_pool(e.pool, 0, 1, 0xC0, s.text_pools); // 0x0005F390, ESI DAT_004DEA10
    e.pool->origin = {exe_real(s, 0x00339F90), exe_real(s, 0x00339F94), std::bit_cast<float>(3u)};
    e.title_record = create_dialogue_text_record(*e.pool, e.title.data(), exe_real(s, 0x00339F60),
        exe_real(s, 0x00339F64), exe_real(s, 0x00339F68), exe_real(s, 0x00339F68), 1.f, 1, 0, 1, s.metrics,
        s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
    e.message_record = create_dialogue_text_record(*e.pool, e.message.data(), exe_real(s, 0x00339F6C),
        exe_real(s, 0x00339F70), exe_real(s, 0x00339F74), exe_real(s, 0x00339F74), 1.f, 1, 0, 1, s.metrics,
        s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
    e.pool->wrap_width = exe_real(s, 0x00339F5C); // +0x10, 540
}
}

// Xbox 0x00032990
void open_app_error_screen(const AppErrorRequest& request)
{
    auto& s = services("0x00032990");
    auto& e = g_app_error;
    if ((s.flags & 0x40000u) != 0) // DAT_0050EA9C
    {
        return;
    }
    s.flags |= 0x40000u;
    e.choice = -1; // DAT_004DEA1C
    std::copy(request.title.begin(), request.title.end(), e.title.begin()); // lstrcpyW
    std::copy(request.message.begin(), request.message.end(), e.message.begin()); // lstrcpyW(+0x80)
    e.a_key = request.a_key;
    e.callbacks[0] = request.a;
    e.arguments[0] = request.a_argument;
    e.b_key = request.b_key;
    e.callbacks[1] = request.b;
    e.arguments[1] = request.b_argument;
    if (e.callbacks[0] && e.callbacks[1])
    {
        // B (row 0) and A (row 1).
        const auto block = s.image.memory(kTwoButtonRows, 0x310);
        std::copy(block.begin(), block.end(), e.two_button_rows.begin());
        e.rows = e.two_button_rows.data(); // DAT_004DE9A4
        e.row_count = 2; // DAT_004DE9A8
        copy_row_key(e.two_button_rows, 0, e.b_key); // 0x0033A2EC
        copy_row_key(e.two_button_rows, 1, e.a_key); // DAT_004DE9A4 + 0x8C
        set_app_error_phase(1); // 0x00032BA0, EAX 1
        return;
    }
    const auto block = s.image.memory(kOneButtonRows, 0x310);
    std::copy(block.begin(), block.end(), e.one_button_rows.begin());
    e.rows = e.one_button_rows.data();
    e.row_count = 2;
    copy_row_key(e.one_button_rows, 0, e.a_key); // 0x00339FDC
    set_app_error_phase(1);
}

// Xbox 0x00032AB0
void tick_app_error_screen()
{
    auto& e = g_app_error;
    if (e.page != nullptr)
    {
        auto& s = services("0x00032AB0");
        if (e.page->duration <= e.page->time) // +0x34, +0x2C
        {
            seek_runtime_cutscene(*e.page, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0(0, 0, 0)
        }
        tick_runtime_cutscene(*e.page, s.dt, &e.camera, s.cutscenes); // 0x0004DA50(DAT_005096C0), ESI DAT_004DE9B0
    }
    if (e.widget != nullptr && 0 < e.phase && e.phase < 4 && e.input.data() != nullptr)
    {
        auto& s = services("0x00032AB0");
        s.update_widget(*e.widget, e.input_address); // 0x00142460, EDI DAT_004DE9A0, EAX DAT_004DE99C
    }
    RuntimeDialogueWidget* widget = e.widget;
    if (e.phase == 1)
    {
        if (widget != nullptr)
        {
            auto& s = services("0x00032AB0");
            if (dialogue_text_record_animating(*e.pool, e.title_record, s.fonts, s.fallback_texture) == 0
                && dialogue_text_record_animating(*e.pool, e.message_record, s.fonts, s.fallback_texture) == 0
                && widget->current_set != nullptr) // 0x0005FFA0 twice, +0x94C
            {
                e.phase = 2;
            }
        }
    }
    else if (e.phase == 3 && e.pool->allocation->cursor <= e.pool->allocation->begin // *DAT_004DEA10 +8, +4
        && widget->current_set == nullptr) // +0x94C
    {
        set_app_error_phase(4); // 0x00032BA0, EAX 4
    }
}

// Xbox 0x00032BA0
void set_app_error_phase(i32 phase)
{
    auto& s = services("0x00032BA0");
    auto& e = g_app_error;
    if (e.phase == 3)
    {
        release_screen(s);
    }
    e.phase = phase;
    if (phase == 1)
    {
        open_screen(s);
    }
    else if (phase == 3)
    {
        close_screen(s);
    }
    else if (phase == 4)
    {
        s.flags &= 0xFFFBFFFFu; // DAT_0050EA9C
        e.callbacks[size_t(e.choice)](e.arguments[size_t(e.choice)]); // (*DAT_004DE98C[n])(DAT_004DE994[n])
        if (e.input.data() != nullptr)
        {
            release_runtime_input_device(e.input, s.input_release); // 0x00108230, EAX DAT_004DE99C
            s.unmap_input(e.input_address);
            e.input = {};
            e.input_address = 0;
        }
    }
}

// Xbox 0x00032F10
void choose_app_error_a()
{
    set_app_error_phase(3); // The 0x00032BA0(3) inline.
    g_app_error.choice = 0;
}

// Xbox 0x00032FA0
void choose_app_error_b()
{
    set_app_error_phase(3);
    g_app_error.choice = 1;
}

void draw_app_error_page(i32 view)
{
    auto& s = services("0x0011C240");
    auto& e = g_app_error;
    if (e.page == nullptr)
    {
        return;
    }
    setup_explicit_camera_display(&e.camera, 0, s.display); // 0x00048BC0, ESI DAT_004DE9B0, EAX 0
    draw_runtime_cutscene(*e.page, view, s.cutscenes); // 0x0004DBD0, EBX DAT_004DE9AC
}

void draw_app_error_prompts()
{
    auto& s = services("0x0011C240");
    if (g_app_error.widget != nullptr)
    {
        draw_runtime_dialogue_widget(*g_app_error.widget, 0, s.outer); // 0x00142720(0)
    }
}

[[noreturn]] void launch_memory_dashboard(u32 blocks)
{
    // DAT_0054E9A8 = 2 (XLD_LAUNCH_DASHBOARD_MEMORY), +4 = 0, +8 = 0x55 ('U'), +0xC = the blocks.
    throw Error("0x00032570 (XLaunchNewImage to the dashboard's memory area for " + std::to_string(blocks)
        + " blocks) has no host boundary");
}

// Xbox 0x000F32F0 / 0x000E1D50
void show_corrupt_game(const char16_t* name, const char* message, const char* a_key, const char* b_key)
{
    AppErrorRequest request{};
    if (name != nullptr)
    {
        std::u16string_view text(name);
        std::copy(text.begin(), text.end(), request.title.begin()); // lstrcpyW
    }
    request_app_error_message(request, message); // 0x000FF0A0
    request.a_key = a_key;
    request.a = play_on_without_saving; // 0x000F33F0
    auto* copy = new char16_t[0x80]{}; // 0x000F72D0(0x100)
    if (name != nullptr)
    {
        std::u16string_view text(name);
        std::copy(text.begin(), text.end(), copy); // lstrcpyW
    }
    request.a_argument = copy;
    request.b_key = b_key;
    request.b = leave_for_dashboard_with_name; // 0x000F33A0
    request.b_argument = copy;
    open_app_error_screen(request); // 0x00032990
}

// Xbox 0x000F32F0
void show_runtime_corrupt_game(const char16_t* name)
{
    show_corrupt_game(name, "gamesystem_corruptgamemessage", "gamesystem_corruptgameabutton",
        "gamesystem_corruptgamebbutton"); // 0x003260D0, 0x003260B0, 0x00326090
}

// Xbox 0x000F33F0: the save is shown as not written (0x0010E1A0) and the name copy freed.
void play_on_without_saving(void* name)
{
    auto* copy = static_cast<char16_t*>(name);
    mark_save_game_loading(copy); // 0x0010E1A0, EBX the copy
    if (copy != nullptr)
    {
        delete[] copy; // The tail jump to 0x000663E0.
    }
}

// Xbox 0x000F33A0
[[noreturn]] void leave_for_dashboard_with_name(void* name)
{
    auto* copy = static_cast<char16_t*>(name);
    if (copy != nullptr)
    {
        delete[] copy; // 0x000663E0
    }
    launch_memory_dashboard(0);
}

void request_app_error_message(AppErrorRequest& request, const char* key)
{
    auto& s = services("0x000FF0A0");
    request_namespaced_localtext("aid_loctext_ghoulies_apperror", "apperror", key, request.message.data(),
        s.localtext); // 0x000FF0A0, 0x0031C2E4, 0x0031C2C4
}
}
