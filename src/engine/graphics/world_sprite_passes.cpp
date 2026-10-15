#include "world_sprite_passes.h"
#include "../camera/runtime_display_setup.h"
#include "../frontend/dialogue_draw_outer.h"
#include "gbtg/model_render.h"

#include <array>

namespace gbtg
{
namespace
{
u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}
}

// Xbox 0x0005CF20
void begin_world_sprite_draw(RuntimeDisplaySetupContext& c)
{
    auto& draw = c.draw;
    bind_display_projection(c);
    reset_dialogue_sprite_state(draw);
    set_dialogue_alpha_state(draw);
    set_dialogue_texture_state(draw);
    draw.d3d.simple_state(0x40354, 0x203);
    deferred(draw, 0x181C54) = 0x203;
    draw.d3d.simple_state(0x4035C, 1);
    deferred(draw, 0x181C70) = 1;
    draw.d3d.depth(1);
    ++draw.override_count;
    draw.fog_disabled = 1;
    set_runtime_render_fog_enabled(draw.renderer, 0);
}

// Xbox 0x000EC470
void draw_effect_debris(std::span<const u8> particles, RuntimeDisplaySetupContext& c)
{
    if (particles.data() == nullptr)
    {
        return;
    }
    auto& draw = c.draw;
    begin_world_sprite_draw(c);
    draw.d3d.simple_state(0x40354, 0x203);
    deferred(draw, 0x181C54) = 0x203;
    draw.d3d.simple_state(0x4035C, 0);
    deferred(draw, 0x181C70) = 0;
    draw.d3d.depth(1);
    u32 offset = 0;
    do
    {
        if (i32(read_u32(particles, offset + 0x68)) == -2)
        {
            // The size (+0x20 * +0x18), the color from four 0x001673D8 conversions and the quad 0x0005CF90
            // (immediate-mode Begin/SetVertexData/End over the DAT_0050DD70 world matrix) are not translated.
            throw Error("debris quad draw 0x0005CF90 is not translated");
        }
        offset += 0x74;
    } while (offset < 0x16A8);
    bind_display_projection(c);
    if (--draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
}

// Xbox 0x000615E0
void draw_screen_effects(const RuntimeScreenEffects& effects, RuntimeDialogueOuterDrawContext& c)
{
    std::array<float, 8> corners;
    dialogue_viewport_corners(corners.data(), c.draw.fitted_viewport);
    if (effects.flash != 0)
    {
        throw Error("screen flash draw 0x000615E0 (DAT_00504478) is not translated");
    }
    if (effects.capture != 0 && effects.transition_guard < 1)
    {
        throw Error("screen capture overlay 0x000615E0 (DAT_005044A4) is not translated");
    }
    if (effects.lines != 0)
    {
        throw Error("retained line draw 0x00061250 from 0x000615E0 (DAT_005044B0) is not bound");
    }
}
}
