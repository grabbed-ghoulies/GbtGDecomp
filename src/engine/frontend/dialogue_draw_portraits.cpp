#include "dialogue_draw_context.h"
#include "gbtg/anim.h"
#include "../audio/listener_settings.h"

namespace gbtg
{
// Xbox 0x000575F0
void draw_dialogue_speaker(RuntimeDialogueDrawContext& c)
{
    auto& r = c.dialogue.measurement.tags.trophies.resources;
    if (!r.speaker_resource.empty())
    {
        float scale = c.dialogue.state.progress;
        if ((c.dialogue.state.mode == 1 && r.previous_speaker == r.speaker)
            || (c.dialogue.state.mode == 3 && r.next_speaker == r.speaker))
        {
            scale = 1.f;
        }
        c.state.speaker_position = {320.f, 80.f};
        c.state.speaker_scale = {scale, scale};
        begin_dialogue_sprite_draw(c);
        draw_dialogue_texture(static_cast<i32>(r.speaker_frame), 0,
            const_cast<u8*>(r.speaker_resource.data()), c.state.speaker_scale.data(),
            nullptr, 0.f, c.white, c.state.speaker_position.data(), c);
        --c.override_count;
        if (c.override_count == 0)
        {
            c.fog_disabled = 0;
        }
    }
}
// Xbox 0x00057A80
u8 dialogue_icon_frame(float time, RuntimeDialogueDrawContext& c)
{
    const auto texture = c.dialogue.measurement.tags.trophies.resources.icon_resource;
    if (texture[0x11] == 0)
    {
        return 0;
    }
    const long double remainder = animation_remainder(time,
        static_cast<float>(texture[0x11]) / static_cast<float>(texture[0x10]));
    if (texture[0x10] != 0)
    {
        const u32 frame = static_cast<u32>(truncate_runtime_float(
            static_cast<long double>(texture[0x10]) * remainder));
        return static_cast<u8>(frame % texture[0x11]);
    }
    return 0;
}
// Xbox 0x000577E0
void draw_dialogue_icon(float& x, float y, float scale_x, float scale_y, u8 frame,
    RuntimeDialogueDrawContext& c)
{
    auto& r = c.dialogue.measurement.tags.trophies.resources;
    const auto texture = r.icon_resource;
    if (!texture.empty() && r.icon > 0x36)
    {
        const float progress = c.dialogue.state.progress;
        scale_x = progress * scale_x;
        scale_y = progress * scale_y;
        const float width = static_cast<float>(read_u16(texture, 8));
        const float height = static_cast<float>(read_u16(texture, 10));
        const float extent_x = width * scale_x;
        const float extent_y = height * scale_y;
        if (width == 128.f)
        {
            x -= scale_x * 32.f;
        }
        const float position_x = extent_x * .5f + x;
        float offset_y;
        float sx = scale_x;
        float sy = scale_y;
        if (width == 64.f)
        {
            offset_y = progress * 8.f;
            sy *= 1.25f;
            sx *= 1.25f;
        }
        else
        {
            offset_y = height * .25f * (1.f - progress);
        }
        const float scales[2]{sx, sy};
        const float position[2]{position_x, offset_y + extent_y * .5f + y};
        draw_dialogue_texture(frame, 0, const_cast<u8*>(texture.data()), scales,
            nullptr, 0.f, c.white, position, c);
        if (static_cast<float>(read_u16(r.icon_resource, 8)) == 128.f)
        {
            x -= scale_x * 32.f;
        }
        x += extent_x;
    }
}
}
