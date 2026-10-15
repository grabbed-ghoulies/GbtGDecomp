#pragma once
#include "actor_frame_update.h"
#include "gbtg/actor_runtime.h"
#include "../graphics/scene_model_draw.h"
#include <unordered_map>

namespace gbtg
{
using ActorFrameDrawCallback = void (*)(ActorFrameAttachment&, Actor&, i32);
struct ActorFrameDrawContext
{
    const std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback>& draws; // Actual attachment vtable+14.
};
void draw_actor_frame_attachments(ActorFrameState& state, i32 kind,
    const ActorFrameDrawContext& context); // Xbox 0x0003DE00
struct ActorDrawModelView
{
    RuntimeModelInstance& instance;
    u32 cpu;
    u32& material_enabled; // model +0x1C0
    u32& material_first; // model +0x1C4
    u32& material_second; // model +0x1C8
};
struct ActorDrawState
{
    ActorFrameState& frame;
    ActorDrawModelView* model; // actor +0x204
    u8 hidden; // +0x197
    u8 matrices_ready; // +0x298
    u32 material_first; // +0x198
    u32 material_second; // +0x19C
    std::function<void(Actor&, i32, void*)> before; // genuine +0x1A0 indirect call
    std::function<void(Actor&, i32, void*)> after; // genuine +0x1A4 indirect call
    void* payload; // +0x1A8
    RuntimeActorMatrixState& matrices;
};
u32 draw_runtime_actor(ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& context); // Xbox 0x0003C9E0
// The background's vtable +0x14: nothing while a dialogue page holds DAT_004EE1EC, else the actor draw.
u32 draw_runtime_background(i32 dialogue_mode, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& context); // Xbox 0x00042C10
}
