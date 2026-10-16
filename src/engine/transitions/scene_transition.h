#pragma once
#include "gbtg/entity.h"
#include "gbtg/types.h"

#include <array>
#include <functional>
#include <span>

namespace gbtg
{
class SceneExecutableResources;
struct AnimationResourceRegistry;
struct ResourceErrorState;
struct GhoulySpawnPool;
struct RuntimeTextMetrics;
struct RuntimeTextFont;
struct LocalTextResourceContext;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueDrawContext;
struct RuntimeDisplaySetupContext;
struct RuntimeTransitionBook;
struct TransitionBookServices;
struct AudioEventVoices;
// The scene transition (DAT_00546328, 0x3054 bytes): a row of the 18 rows at 0x00431478 (0x110 bytes each) and the
// state its handler (0x003129A8, 4 records of 6 slots) keeps. Words that hold pointers are typed members.
struct RuntimeSceneTransition
{
    i32 row = 0; // +0, DAT_00546328
    i32 phase = 0; // +4, DAT_0054632C: 1 entering, 2 waiting for the load, 3 leaving.
    float time = 0.f; // +8, DAT_00546330
    float duration = 0.f; // +0xC, DAT_00546334
    i32 clip = 0; // +0x10, DAT_00546338: the row's +0x108 (a book clip, or the slime's colour).
    i32 route = 0; // +0x14, DAT_0054633C: the directory's map route or chapter.
    const char* aid = nullptr; // +0x18, DAT_00546340: the row's entering (+4) or leaving (+0x84) aid, or null.
    i32 loading_frames = 0; // +0x1C, DAT_00546344
    i32 flag_delay = 0; // +0x20, DAT_00546348: frames until DAT_0050EA9C gets 0x2000.
    RuntimeDialogueTextPool* loading_pool = nullptr; // +0x24, DAT_0054634C
    std::array<char16_t, 0x800> loading_text{}; // +0x28, DAT_00546350: gameselect "loading".
    std::span<const u8> slime; // +0x1028, the slime's texture (handler 2).
    u32 slime_color = 0; // +0x102C
    i32 slime_captured = 0; // +0x1030
    RuntimeTransitionBook* book = nullptr; // +0x1034 (handler 1)
    i32 titled = 0; // +0x1038, the chapter title shows.
    RuntimeDialogueTextPool* title_pool = nullptr; // +0x103C
    std::array<char16_t, 0x800> chapter_number{}; // +0x1040
    std::array<char16_t, 0x800> chapter_name{}; // +0x2040
    i32 title_ready = 0; // +0x3040
    void* title_widget = nullptr; // +0x3044
    void* title_input = nullptr; // +0x3048
    i32 title_released = 0; // +0x304C
    i32 fade_captured = 0; // +0x3050 (handler 3)
};
extern RuntimeSceneTransition g_scene_transition; // DAT_00546328
extern EntityCamNode g_transition_camera; // DAT_0054937C, "transition"
// What the transitions reach outside their block. The boot-owned scene services bind it.
struct SceneTransitionServices
{
    const SceneExecutableResources& image;
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    GhoulySpawnPool*& text_pools; // DAT_005043CC
    const RuntimeTextMetrics& metrics;
    float viewport_width;
    i32& next_text_id;
    u32& random; // DAT_0054DD7C
    std::span<const RuntimeTextFont* const> fonts;
    const u8* fallback_texture; // DAT_004E1C58
    LocalTextResourceContext& localtext;
    const std::array<u16, 2>& screen; // DAT_005096E8 / DAT_005096EA
    const float& dt; // DAT_005096C0
    TransitionBookServices& books;
    AudioEventVoices& voices; // DAT_004E0CE8
    RuntimeDialogueDrawContext& draw; // 0x0005CDC0 / 0x0005CA60 and DAT_00509B5C / DAT_00509B00
    RuntimeDisplaySetupContext& display;
    std::function<i32()> dialogue_mode; // DAT_004EE1EC
    std::span<u8> page; // DAT_005047D8, the back buffer's copy (0x000620C0).
    std::function<void(u32 texture)> copy_back_buffer; // D3DDevice_CopyRects into the texture's surface level 0
    std::function<u32(const u8*)> address_of; // A texture object's native address.
};
extern SceneTransitionServices* g_transition_services;
void initialize_scene_transition(); // Xbox 0x0011C3F0's loop over each handler's +0 slot
void tick_scene_transition(); // Xbox 0x0012C340
void draw_scene_transition_view(); // Xbox 0x0012C490, from 0x0011C240
void end_scene_transition(); // Xbox 0x0012C4E0
void begin_scene_transition(const char* scene); // Xbox 0x0012C5D0, the scene record's +0x80
void leave_scene_transition(); // Xbox 0x0012C770
}
