#pragma once
#include "dialogue_draw_context.h"

namespace gbtg
{
struct RuntimeDisplaySetupContext;
using DialogueSurfaceHandle = std::uintptr_t;
struct DialogueRenderTargetHook
{
    std::function<DialogueSurfaceHandle(std::uintptr_t, std::uintptr_t)> function; // +0
    std::uintptr_t first; // +4
    std::uintptr_t second; // +8, retail hook stride 0x14
};
struct DialogueRenderTargetState
{
    DialogueSurfaceHandle back_buffer; // DAT_005047D0
    DialogueSurfaceHandle depth_buffer; // DAT_005047D4
    DialogueSurfaceHandle unused_surface; // DAT_005047FC
    DialogueSurfaceHandle color_texture; // DAT_00504834
    DialogueSurfaceHandle color_surface; // DAT_00504838
    DialogueSurfaceHandle depth_texture; // DAT_0050483C
    DialogueSurfaceHandle depth_surface; // DAT_00504840
    u32 active; // DAT_00504800
};
struct DialogueOuterD3DBoundaries
{
    std::function<DialogueSurfaceHandle(u32)> back_buffer;
    std::function<DialogueSurfaceHandle()> depth_buffer;
    std::function<DialogueSurfaceHandle(DialogueSurfaceHandle, u32)> surface;
    std::function<void(DialogueSurfaceHandle, DialogueSurfaceHandle)> render_target;
    std::function<void(DialogueSurfaceHandle)> release;
    std::function<void(u32, const void*, u32, u32, float, u32)> clear;
    std::function<void(float)> line_width;
    std::function<void(u32, const float*, u32)> pixel_constants;
};
struct RuntimeDialogueQueuedSprite
{
    const u8* texture; // +0
    std::array<float, 2> position; // +4
    std::array<float, 2> scale; // +0xC
    float angle; // +0x14
    u32 color; // +0x18
    u8 frame; // +0x1C, original record stride 0x20
};
struct RuntimeDialogueSpriteQueue
{
    std::span<RuntimeDialogueQueuedSprite> records; // DAT_00511210, 64 records
    i32& count; // DAT_00511A10
};
struct RuntimeDialogueOuterDrawContext
{
    RuntimeDialogueDrawContext& draw;
    DialogueRenderTargetState& targets;
    DialogueOuterD3DBoundaries& d3d;
    RuntimeDialogueSpriteQueue& sprites;
    std::span<const DialogueRenderTargetHook> hooks; // DAT_00545DC0
    RuntimeDisplaySetupContext& display;
    u32& text_draw_count; // DAT_004E61C8
    u32& capture_enabled; // DAT_005077F8
    u8 capture_configuration; // DAT_00509703
    u32 visibility_mask; // DAT_0050EA9C
    u32& current_display_address; // DAT_005077A8, retail address for view index
    u32 line_vertex_shader; // DAT_004271F4
    u32 line_pixel_shader; // DAT_00425E0C
    u32 line_shader_constants; // PTR_DAT_004271E8
    u32& line_retained; // DAT_005044B0
};
DialogueSurfaceHandle invoke_dialogue_render_target(u8 index,
    std::span<const DialogueRenderTargetHook> hooks); // Xbox 0x001417A0
void begin_runtime_dialogue_target(DialogueSurfaceHandle color, DialogueSurfaceHandle depth,
    RuntimeDialogueOuterDrawContext& context); // Xbox 0x000629D0
void end_runtime_dialogue_target(RuntimeDialogueOuterDrawContext& context); // Xbox 0x00062A90
void dialogue_viewport_corners(float* output, std::span<const u32, 4> viewport); // Xbox 0x000F5DB0
void draw_runtime_dialogue_lines(const float* positions, float width, u32 color,
    RuntimeDialogueOuterDrawContext& context); // Xbox 0x00061250
void draw_runtime_dialogue_border(float width, u32 color,
    RuntimeDialogueOuterDrawContext& context); // Xbox 0x000619D0
void draw_runtime_dialogue_sprite(const u8* texture, const float* position, const float* scale,
    float angle, u32 color, i32 frame, RuntimeDialogueDrawContext& context); // Xbox 0x0005CE30
void queue_runtime_dialogue_sprite(const float* scale, const float* position,
    const u8* texture, u32 color, u8 frame, float angle,
    RuntimeDialogueSpriteQueue& queue); // Xbox 0x001221B0
void draw_queued_runtime_dialogue_sprites(RuntimeDialogueSpriteQueue& queue,
    RuntimeDialogueDrawContext& context); // Xbox 0x00122230
void draw_runtime_dialogue_widget(RuntimeDialogueWidget& widget, i32 immediate,
    RuntimeDialogueOuterDrawContext& context); // Xbox 0x00142720
void render_runtime_dialogue(RuntimeDialogueOuterDrawContext& context); // Xbox 0x00058150
}
