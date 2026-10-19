#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// The deletion widget's scalar fields at their retail offsets (see docs/RETAIL_LAYOUTS.md). The words that hold
// pointers are GameDeletionWidget members, noted here by offset.
namespace game_deletion_field
{
constexpr u32 kSize = 0x594; // 0x000DB720 allocates 0x594 bytes
constexpr RetailField<i32, 0x000> state{}; // 0x000DBDC0 sets it; 0x000DB9E0 switches on it
constexpr RetailRecord<0x004, gameselect_text_line::kSize> title{}; // the game's name, from +0x48C
constexpr RetailRecord<0x210, gameselect_text_line::kSize> prompt{}; // deleteprompt, then deleting
// +0x41C pool, +0x420 page, +0x424 camera (EntityCamNode, to +0x484)
constexpr RetailField<i32, 0x484> saved_plate_flag_0{}; // DAT_00510F7C while the widget is open
constexpr RetailField<i32, 0x488> saved_plate_flag_1{}; // DAT_00510F80
constexpr RetailField<std::array<char16_t, 0x80>, 0x48C> save_name{}; // lstrcpyW from DAT_00429CC4
// +0x58C owner, +0x590 prompts
static_assert(retail_fits(kSize, save_name));
static_assert(retail_fits(0x41C, prompt));
} // namespace game_deletion_field

// Row 2 of the mode table: deleting a game (0x594 bytes, 0x000DB720). Its scalar fields stay at their retail
// offsets in `bytes` (game_deletion_field above); the words that hold pointers are the members below.
struct GameDeletionWidget : GameSelectWidget
{
    std::array<u8, 0x594> bytes{};
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    RuntimeCutsceneInstance* page = nullptr; // +0x420, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // +0x424, "gameslotdeletion"
    GameSelectControlState* owner = nullptr; // +0x58C
    std::span<u8> owner_state;
    RuntimeDialogueWidget* prompts = nullptr; // +0x590, "GameSlotDeletionMenu"
};
GameSelectWidgetHandle create_runtime_game_deletion(GameSelectControlServices& services); // Xbox 0x000DB720
void open_runtime_game_deletion(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000DB730
void close_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DB950
void tick_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DB9E0
void draw_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DBB90
void set_runtime_game_deletion_state(GameDeletionWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000DBDC0, EAX the state
// The "GameSlotDeletionMenu" prompts' callbacks (rows +0x84 at 0x003077B0).
void leave_runtime_game_deletion(GameSelectControlServices& services); // Xbox 0x000DBD50
void confirm_runtime_game_deletion(GameSelectControlServices& services); // Xbox 0x000DBC20
}
