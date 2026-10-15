#include "scene_view_present.h"
#include "gbtg/types.h"

namespace gbtg
{
// Xbox 0x0011C240
void present_runtime_scene_view(RuntimeSceneViewPresent& c)
{
    c.mirror();
    c.refresh_actors();
    c.bins();
    c.anchor_sprites();
    c.controls();
    c.effects();
    c.debris();
    c.trails();
    c.billboards();
    c.decals();
    c.shadow_volumes();
    c.refraction();
    c.screen_effects();
    c.dialogue();
    if (c.caption_camera != 0)
    {
        c.caption();
    }
    if (c.caption_widget != 0)
    {
        c.widget(c.caption_widget);
    }
    if (c.frontend_state == 6)
    {
        c.widget(c.frontend_widget);
    }
    c.sprites();
    c.hud();
    if ((c.scene_flags & 0x2000) != 0)
    {
        c.phase_overlay();
        if (c.capture_camera != nullptr)
        {
            c.capture(*c.capture_camera);
        }
        c.transition();
        c.text_pools();
        return;
    }
    c.text_pools();
    c.phase_overlay();
    if (c.capture_camera != nullptr)
    {
        c.capture(*c.capture_camera);
    }
    c.transition();
}
// Xbox 0x0011DB50
void draw_runtime_phase_overlay(i32 phase, const void* widget, const void* overlay, const void* capture)
{
    switch (phase)
    {
    case 1:
    case 2:
    case 3:
    case 6:
    case 7:
    case 8:
    case 9:
        if (widget != nullptr)
        {
            throw Error("phase widget draw 0x00142720 (DAT_00510E30) is not bound");
        }
        break;
    default:
        break;
    }
    if (overlay != nullptr)
    {
        switch (phase)
        {
        case 2:
        case 3:
        case 6:
        case 7:
        case 9:
            throw Error("phase sprite draw 0x0005CE30 (DAT_00510E40) is not bound");
        default:
            break;
        }
    }
    if (capture != nullptr)
    {
        throw Error("phase camera capture 0x00048550 (DAT_00510E24) is not translated");
    }
    if (phase == 8 && widget != nullptr)
    {
        throw Error("phase widget draw 0x00142720 (DAT_00510E30) is not bound");
    }
}
}
