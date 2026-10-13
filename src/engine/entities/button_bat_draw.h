#pragma once
#include "../graphics/screen_model_draw.h"
#include "../frontend/dialogue_draw_outer.h"

namespace gbtg
{
struct RuntimeButtonBatDraw
{
    RuntimeScreenModelDraw& screen;
    DialogueOuterD3DBoundaries& d3d; // Clear, D3D 0x00176970.
    const Vec3& rotation; // DAT_0050DD80.
};
// Actor vtable +0x14 for the button bat (vtable 0x00504C34). The model is actor +0x204 and its .cpu address.
u32 draw_runtime_button_bat(ButtonBatScreen& bat, const ActorAnimationState& animation, RuntimeModelInstance& model,
    u32 model_cpu, u32 pass, RuntimeButtonBatDraw& context); // Xbox 0x0006D6C0
}
