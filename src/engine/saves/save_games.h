#pragma once
#include "save_scan.h"

#include <functional>
#include <span>

namespace gbtg
{
struct MovieFileContext;
// What the save code reaches outside DAT_005462E4: the asynchronous file rows (0x000E9E60's list), the profile
// blocks it saves (DAT_00506C6C, DAT_00506C54, DAT_00506C70) and the held scene order (DAT_00511B64).
struct SaveGameContext
{
    MovieFileContext& files;
    RuntimeProfile& profile;
    std::span<const u8> scene_order;
};
// 0x0010F320's save image: the random savegame texture (0x00035E90), released after (0x00035C00), written by
// XGWriteSurfaceOrTextureToXPR from its D3D header.
struct SaveImageBoundaries
{
    std::function<std::span<const u8>(const char* aid)> acquire;
    std::function<void(std::span<const u8> texture)> release;
    std::function<void(std::span<const u8> header, const char* path)> write_xpr;
    std::function<void()> missing; // 0x00032700(2)
    u32& random; // DAT_0054DD7C
};
// The callbacks of 0x0010D430: too many saves (EAX), no space, run again and a corrupt container.
struct SaveCreateCallbacks
{
    SaveCallback too_many;
    SaveCallback no_space;
    SaveCallback run_again;
    SaveCallback corrupt;
};
SaveSlot* find_save_slot(const char16_t* name); // The lstrcmpW walk every 0x0010Dxxx accessor opens with.
i32 create_save_game(const char16_t* name, u8 slot, const SaveCreateCallbacks& callbacks,
    SaveImageBoundaries& image, SaveGameContext& context); // Xbox 0x0010D430, ECX the name
i32 load_save_game(const char16_t* name, SaveCallback loaded, SaveCallback failed,
    SaveGameContext& context); // Xbox 0x0010D910, EBX the name
i32 unload_save_game(const char16_t* name, SaveGameContext& context); // Xbox 0x0010D9B0, EAX the name
i32 delete_save_game(const char16_t* name, SaveGameContext& context); // Xbox 0x0010D5E0
i32 queue_save_game(const char16_t* name, SaveCallback failed, SaveGameContext& context); // Xbox 0x0010DA80, EAX
void write_queued_save_games(SaveGameContext& context); // Xbox 0x0010F440
void tick_save_games(float dt, SaveGameContext& context); // 0x00033BF2..0x00033C1E in the frame tick
u8 save_game_slot(const char16_t* name); // Xbox 0x0010DB60, EBX the name
void save_game_script(const char16_t* name, char* output); // Xbox 0x0010DB10, EBX the name
i32 save_game_loading(const char16_t* name); // Xbox 0x0010DBA0, EBX the name
i32 save_game_writing(const char16_t* name); // Xbox 0x0010DBE0, EBX the name
i32 save_game_word(const char16_t* name, size_t offset); // 0x0010DC20..0x0010DDA0, 0x0010E050, 0x0010E120
u8 save_game_byte(const char16_t* name, size_t offset); // Xbox 0x0010E0E0 (+0x25D), EBX the name
RuntimeProfileBlock* save_game_progress(const char16_t* name); // Xbox 0x0010DDE0, EBX the name
RuntimeProfileBlock* save_game_challenges(const char16_t* name); // Xbox 0x0010DE30, EBX the name
RuntimeProfileBlock* save_game_unlocks(const char16_t* name); // Xbox 0x0010DE80, EBX the name
void load_save_game_words_1b4(const char16_t* name, std::span<u8> output); // Xbox 0x0010DED0, 0x54 bytes
void load_save_game_words_208(const char16_t* name, std::span<u8> output); // Xbox 0x0010DF90, 0x54 bytes
void set_save_game_slot(const char16_t* name, u8 slot); // Xbox 0x0010E270, EBX the name
void mark_save_game_dirty(const char16_t* name); // Xbox 0x0010F2D0, EBX the name
bool save_game_name_exists(const char16_t* name); // Xbox 0x0010F530
}
