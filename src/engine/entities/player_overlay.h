#pragma once
#include "body_action_selection.h"
#include "../graphics/billboard_queue.h"
#include "../camera/display_view.h"
#include "actor_frame_render.h"

namespace gbtg
{
void normalize_vector_in_place(float* vector); // Xbox 0x0002AC70, ESI vector
void runtime_actor_overlay_anchor(const Actor& actor, Vec3& position, float& height,
    float& width); // Xbox 0x0003DD50
u32 runtime_body_health_color(std::span<const u8> body); // Xbox 0x0001E2A0
u32 runtime_player_uses_alternate_overlay(std::span<const u8> actor,
    std::span<const u8> attributes); // Xbox 0x0001E770
// Retail digit sprites DAT_00545A80; each sprite's u16 width is at +8.
void queue_runtime_overlay_digits(i32 value, const Vec3& position, u32 color, const u8* const* digits,
    const DisplayView& view, BillboardQueue& queue); // Xbox 0x0001E3E0
struct RuntimeBodyOverlay
{
    std::span<const u8> body; // Actor +0x95C.
    const Actor& owner; // Body +8 native actor.
    std::span<const u8> owner_bytes; // Body +8 retail bytes (+0x924, +0x964..+0x98C).
    std::span<const u8> attributes; // Owner +0x964.
    RuntimeBodyAttachedActor* attached; // Body +0x48.
    const u8* health_sprite; // Body +0x98.
    const u8* alternate_sprite; // Body +0x9C.
    const u8* active_sprite; // Body +0xA0.
    const u8* counter_sprite; // Body +0xD8.
    const u8* const* digits; // DAT_00545A80.
    const DisplayView& view; // DAT_005077A8.
    BillboardQueue& queue;
};
void queue_runtime_body_overlay(i32 kind, RuntimeBodyOverlay& context); // Xbox 0x0001DF70
struct RuntimePlayerDraw
{
    ActorDrawState& state;
    const ActorFrameDrawContext& attachments;
    SceneModelDrawContext& scene;
    const u32& runtime_flags; // DAT_0050EA9C.
    RuntimeBodyOverlay& overlay;
    void* body_effect; // Body +0x744.
    std::function<void(void*, i32)> draw_body_effect; // Actual body +0x744 vtable +0x18.
};
u32 draw_runtime_player(std::span<const u8> actor, i32 kind, RuntimePlayerDraw& context); // Xbox 0x00028050
}
