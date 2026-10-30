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
        c.frontend_draw(*c.frontend_widget);
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
}
