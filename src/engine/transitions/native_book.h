#pragma once
#include "gbtg/entity.h"
#include "gbtg/types.h"

#include <array>
#include <functional>
#include <span>

namespace gbtg
{
struct Actor;
struct CutsceneRuntimeContext;
struct RuntimeCutsceneInstance;
struct RuntimeModelInstance;
struct RuntimeShaderInstance;
struct AnimationResourceRegistry;
struct ResourceErrorState;
struct RuntimeDisplaySetupContext;
struct RuntimeModelRenderContext;
struct RuntimeDialogueDrawContext;
class SceneExecutableResources;
// A shader parameter a book page binds its texture to (0x18 bytes, 0x000F8540): the model, the shader's
// instance (model +0x2A4 + the node's offset), the parameter's texture index, the texture and two words.
struct BookShaderParameter
{
    RuntimeModelInstance* model = nullptr; // +0
    RuntimeShaderInstance* shader = nullptr; // +4
    u32 index = 0; // +8
    u32 texture = 0; // +0xC, a texture object's native address.
    u32 word_10 = 0; // +0x10
    u32 word_14 = 0; // +0x14
};
// The transition book (0x1E2A4 bytes, 0x00048260): a row of the clip table at 0x00422078 (stride 0x124), its
// cutscene and camera, the book model's four page shaders and the map state 0x00046F40 fills.
struct RuntimeTransitionBook
{
    i32 clip = 0; // +0
    u32 flags = 0; // +4, bit 0: the capture book (DAT_005462A0).
    i32 paused = 0; // +8
    i32 holding = 0; // +0xC, the book stops at the clip's first page threshold.
    i32 finished = 0; // +0x10
    i32 route = 0; // +0x14, a map route to run at the threshold.
    i32 route_argument = 0; // +0x18
    std::span<const u8> texture; // +0x1C
    Actor* model = nullptr; // +0x20, the clip's book model actor (0x0004EA50).
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x24
    EntityCamNode camera{}; // +0x28
    float real_88 = 0.f; // +0x88
    struct Page
    {
        BookShaderParameter* parameter = nullptr; // +0x8C
        u32 texture = 0; // +0x90, DAT_005047E4 or DAT_005047D8.
        u32 capture = 0; // +0x94, copy the back buffer into the page at the next draw.
        u32 copy = 0; // +0x98
    };
    std::array<Page, 4> pages{};
    // The map state (0x00046F40, 0x000473D0, 0x000475B0).
    i32 map_state = 0; // +0xCC
    float map_timer_d4 = 0.f; // +0xD4
    float map_timer_d8 = 0.f; // +0xD8
    u32 map_ready = 0; // +0xE4
    u32 map_menu = 0; // +0xE8
    float map_timer_ec = 0.f; // +0xEC
    std::span<const u8> map_runner; // +0x100
    std::span<const u8> map_runner_back; // +0x104
    std::span<const u8> map_spot; // +0x10C
    void* map_text = nullptr; // +0x114
    u32 map_points = 0; // +0x158
    std::span<u8> map_input; // +0x15C
    void* map_widget = nullptr; // +0x160
    i16 voice = -1; // +0x168
    std::array<std::span<const u8>, 6> map_markers{}; // +0x1E25C, stride 0xC.
};
// What the book reaches outside itself.
struct TransitionBookServices
{
    CutsceneRuntimeContext& cutscenes;
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    const SceneExecutableResources& image;
    RuntimeDisplaySetupContext& display;
    RuntimeModelRenderContext& renderer;
    std::span<u32> node_states; // 0x000F96F0's shared node states.
    const float& freeze; // DAT_004312DC
    const float& dt; // DAT_005096C0
    const float& scene_scale; // DAT_004312C4
    u32& flags; // DAT_0050EA9C
    std::array<u32, 2> page_textures; // DAT_005047D8, DAT_005047E4: the texture objects 0x000620C0 makes.
    std::function<void(Actor&)> update_actor; // 0x000420D0
    std::function<u32(std::span<const u8>)> texture_address; // A texture object's native address.
    std::function<void(u32 texture)> copy_back_buffer; // D3DDevice_CopyRects(back buffer, .., texture level 0)
    // D3DDevice_Clear(count, rectangles, flags, color, z, stencil).
    std::function<void(u32, const void*, u32, u32, float, u32)> clear;
    std::function<void(u8 enabled)> fog; // 0x00156EA0
    std::function<i32()> view; // The current display view index (DAT_005077A8).
    RuntimeDialogueDrawContext& draw; // DAT_00509B5C (override count), DAT_00509B00 (fog off)
    std::function<u8*(u32 texture)> texture_bytes; // A texture object's bytes.
    std::function<RuntimeModelInstance*(Actor&)> secondary_model; // Actor +0x310.
    u8 capture_configuration; // DAT_00509703
    u32 clear_color; // DAT_005462A4
    u32 page_clear_color; // DAT_004231C4
};
extern RuntimeTransitionBook* g_capture_book; // DAT_005462A0
extern u32 g_book_viewport_reset; // DAT_005077FC, read by the present (0x000F6F70).
RuntimeTransitionBook* create_transition_book(i32 clip, u32 flags, const char* texture,
    TransitionBookServices& services); // Xbox 0x00048260
void tick_transition_book(RuntimeTransitionBook& book, TransitionBookServices& services); // Xbox 0x000483E0
void release_transition_book(RuntimeTransitionBook* book, TransitionBookServices& services); // Xbox 0x00048860
void draw_transition_book(RuntimeTransitionBook& book, TransitionBookServices& services); // Xbox 0x00048550
float transition_book_threshold(const RuntimeTransitionBook& book, size_t offset,
    const SceneExecutableResources& image); // The clip row's real at +offset (0x0042217C, 0x00422180).
i32 transition_book_row_word(const RuntimeTransitionBook& book, size_t offset,
    const SceneExecutableResources& image); // The clip row's word at +offset.
}
