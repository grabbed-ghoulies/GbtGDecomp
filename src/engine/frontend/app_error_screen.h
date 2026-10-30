#pragma once
#include "gbtg/entity.h"
#include "gbtg/types.h"

#include <array>
#include <functional>
#include <span>

namespace gbtg
{
struct GameSelectControlServices;
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;

// A button's callback with its argument (+0x1088.. / +0x1090..): retail calls callback(argument).
using AppErrorCallback = std::function<void(void* argument)>;

// The 0x1098-byte block a caller fills on its stack and passes in EAX to 0x00032990.
struct AppErrorRequest
{
    std::array<char16_t, 0x40> title{}; // +0, the game's name or empty
    std::array<char16_t, 0x800> message{}; // +0x80, the apperror message
    const char* a_key = nullptr; // +0x1080, the A button's apperror key
    const char* b_key = nullptr; // +0x1084, the B button's
    AppErrorCallback a; // +0x1088
    AppErrorCallback b; // +0x108C, none for a page with only A
    void* a_argument = nullptr; // +0x1090
    void* b_argument = nullptr; // +0x1094
};

// The app error screen's BSS state (DAT_004DD900 .. DAT_004DEA1C). While it is open DAT_0050EA9C carries 0x40000,
// which stops the scene pass; 0x00032AB0 ticks it every frame and 0x0011C240 draws its page and prompts.
struct AppErrorScreen
{
    i32 phase = 0; // DAT_004DD900: 1 opening, 2 shown, 3 closing, 4 closed
    std::array<char16_t, 0x40> title{}; // DAT_004DD904
    std::array<char16_t, 0x800> message{}; // DAT_004DD984
    const char* a_key = nullptr; // DAT_004DE984
    const char* b_key = nullptr; // DAT_004DE988
    std::array<AppErrorCallback, 2> callbacks; // DAT_004DE98C, DAT_004DE990: A, B
    std::array<void*, 2> arguments{}; // DAT_004DE994, DAT_004DE998
    std::span<u8> input; // DAT_004DE99C, the keyboard device (0x00107EF0)
    u32 input_address = 0; // The scene input registry's address for it.
    RuntimeDialogueWidget* widget = nullptr; // DAT_004DE9A0, "apperror" prompts
    const u8* rows = nullptr; // DAT_004DE9A4: AppMainErrorDialogMenu_1 (A) or _2 (B, A)
    i32 row_count = 0; // DAT_004DE9A8, 2 either way
    RuntimeCutsceneInstance* page = nullptr; // DAT_004DE9AC, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // DAT_004DE9B0, "AppMainErrorDialog"
    RuntimeDialogueTextPool* pool = nullptr; // DAT_004DEA10
    u32 title_record = 0; // DAT_004DEA14
    u32 message_record = 0; // DAT_004DEA18
    i32 choice = -1; // DAT_004DEA1C: 0 A, 1 B
    // The two row sets (0x00339F98, 0x0033A2A8), whose keys retail writes in place.
    std::array<u8, 0x310> one_button_rows{};
    std::array<u8, 0x310> two_button_rows{};
};
extern AppErrorScreen g_app_error;
// The scene binds the game select services the screen reaches while a native scene runs; null outside one.
extern GameSelectControlServices* g_app_error_services;

void open_app_error_screen(const AppErrorRequest& request); // Xbox 0x00032990, EAX the request
void tick_app_error_screen(); // Xbox 0x00032AB0
void set_app_error_phase(i32 phase); // Xbox 0x00032BA0, EAX the phase
void choose_app_error_a(); // Xbox 0x00032F10, "AppMainErrorDialogMenu" A
void choose_app_error_b(); // Xbox 0x00032FA0, "AppMainErrorDialogMenu_2" B
// 0x0011C240's draw: the page through the screen's view (0x00048BC0, 0x0004DBD0), then the prompts (0x00142720(0)).
void draw_app_error_page(i32 view);
void draw_app_error_prompts();
// The apperror message for `key` from aid_loctext_ghoulies_apperror (0x000FF0A0) into the request.
void request_app_error_message(AppErrorRequest& request, const char* key);
// The callbacks' dashboard launch: LD_LAUNCH_DASHBOARD at DAT_0054E9A8 (reason 2, the memory area, drive 'U' and
// the blocks wanted), then 0x00032570(0, it), whose XLaunchNewImage has no host boundary.
[[noreturn]] void launch_memory_dashboard(u32 blocks);
// A failed save write (EAX-less, the name on the stack): apperror `message` with A to play on without saving
// (0x000F33F0) and B to the dashboard (0x000F33A0), both given a copy of the name.
void show_corrupt_game(const char16_t* name, const char* message, const char* a_key,
    const char* b_key); // Xbox 0x000F32F0 (gamesystem_*) and 0x000E1D50 (frontendoptions_*)
void show_runtime_corrupt_game(const char16_t* name); // Xbox 0x000F32F0
void play_on_without_saving(void* name); // Xbox 0x000F33F0
[[noreturn]] void leave_for_dashboard_with_name(void* name); // Xbox 0x000F33A0
}
