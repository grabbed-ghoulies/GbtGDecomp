#pragma once
#include "gbtg/types.h"
#include <functional>

namespace gbtg
{
struct RuntimeTransitionBook;
struct RuntimeCutsceneInstance;
struct RuntimeDialogueWidget;
// The per-view present 0x0011C240, in retail order. Each callee is the native owner of its pass; a pass whose
// body is not translated raises once its gate opens.
struct RuntimeSceneViewPresent
{
    std::function<void()> mirror; // 0x0011F690.
    std::function<void()> refresh_actors; // 0x00121DC0 (EBX the view).
    std::function<void()> bins; // DAT_0051120C = 0, 0x0012AB10, 0x00121730, 0x00121880, 0x00121900, 0x00121D40.
    std::function<void()> anchor_sprites; // 0x00121EC0.
    std::function<void()> controls; // DAT_0050E438: each control's vtable +0x14.
    std::function<void()> effects; // 0x000ED390 for each nonzero DAT_00506930..DAT_00506C50 word.
    std::function<void()> debris; // 0x000EC470.
    std::function<void()> trails; // 0x000EBCB0.
    std::function<void()> billboards; // 0x0005C250.
    std::function<void()> decals; // 0x0011AF90.
    std::function<void()> shadow_volumes; // 0x00119230.
    std::function<void()> refraction; // 0x0011BA50.
    std::function<void()> screen_effects; // 0x000615E0.
    std::function<void()> dialogue; // 0x00058150.
    RuntimeCutsceneInstance* const& error_page; // DAT_004DE9AC, the app error screen's page.
    std::function<void()> error_page_draw; // 0x00048BC0 (ESI DAT_004DE9B0, EAX 0), then 0x0004DBD0 (EBX DAT_004DE9AC).
    RuntimeDialogueWidget* const& error_widget; // DAT_004DE9A0, its prompts.
    std::function<void()> error_widget_draw; // 0x00142720(0), EAX DAT_004DE9A0.
    const i32& frontend_state; // DAT_004E17C8.
    RuntimeDialogueWidget* const& frontend_widget; // DAT_004E19A0, the challenge's restart menu.
    std::function<void(RuntimeDialogueWidget&)> frontend_draw; // 0x00142720(0), EAX DAT_004E19A0.
    std::function<void()> sprites; // 0x00122230.
    std::function<void()> hud; // 0x00132290.
    const u32& scene_flags; // DAT_0050EA9C.
    std::function<void()> phase_overlay; // 0x0011DB50.
    RuntimeTransitionBook* const& capture_camera; // DAT_005462A0.
    std::function<void(RuntimeTransitionBook&)> capture; // 0x00048550.
    std::function<void()> transition; // 0x0012C490.
    std::function<void()> text_pools; // 0x0005F330.
};
void present_runtime_scene_view(RuntimeSceneViewPresent& context); // Xbox 0x0011C240
// The scene phase (DAT_00510CF4) overlays: its widget (DAT_00510E30, 0x00142720), its transition sprite
// (DAT_00510E40, 0x0005CE30) and its camera capture (DAT_00510E24, 0x00048550). None of the three is bound to a
// native object yet, so a set pointer raises.
}
