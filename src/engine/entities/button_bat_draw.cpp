#include "button_bat_draw.h"
#include "gbtg/anim.h"

#include <bit>

namespace gbtg
{
namespace
{
// One coordinate of the flight between the start and target points, held inside them.
float button_bat_flight_coordinate(float from, float target, float fraction)
{
    const float value = (target - from) * fraction + from;
    bool inside;
    if (target <= from)
    {
        if (!(target <= value))
        {
            return target;
        }
        inside = value < from;
    }
    else
    {
        if (!(value <= target))
        {
            return target;
        }
        inside = from < value;
    }
    if (!inside && from != value)
    {
        return from;
    }
    return value;
}
}

// Xbox 0x0006D6C0
u32 draw_runtime_button_bat(ButtonBatScreen& bat, const ActorAnimationState& animation, RuntimeModelInstance& model,
    u32 model_cpu, u32 pass, RuntimeButtonBatDraw& context)
{
    const float extent = actor_model_radius(bat.actor);
    if (bat.state == 2)
    {
        const MinaPlayback* playback = nullptr;
        if (animation.controller != nullptr)
        {
            if (animation.controller->slots.empty())
            {
                throw Error("button bat animation controller has no slot array");
            }
            playback = animation.controller->slots[0].player.get();
        }
        // DAT_0032B1AC is 0.2, DAT_0032B5E0 0.92 and DAT_0032B5DC 1/0.72.
        if (playback == nullptr || playback->position < 0.2f)
        {
            bat.current = bat.from;
        }
        else if (std::bit_cast<float>(0x3F6B851Fu) <= playback->position)
        {
            bat.current = bat.target;
        }
        else
        {
            const float fraction = (playback->position - 0.2f) * std::bit_cast<float>(0x3FB1C71Cu);
            const float y = button_bat_flight_coordinate(bat.from.y, bat.target.y, fraction);
            const float x = button_bat_flight_coordinate(bat.from.x, bat.target.x, fraction);
            bat.current.x = x;
            bat.current.y = y;
        }
    }
    const std::array<i32, 4> rectangle{i32(bat.current.x - extent), i32(bat.current.y - extent),
        i32(bat.current.x + extent), i32(bat.current.y + extent)};
    context.d3d.clear(1, rectangle.data(), 3, 0xFF000000u, 1.f, 0);
    const std::array<float, 2> position{bat.current.x, bat.current.y};
    const std::array<float, 3> rotation{context.rotation.x, context.rotation.y, context.rotation.z};
    draw_runtime_screen_model(model, model_cpu, position.data(), rotation.data(),
        bat.actor.scale_200 * bat.actor.radius_xz, pass, context.screen);
    return 1;
}
}
