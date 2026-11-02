#include "body_stand_alert_tick.h"
#include "player_mind_requests.h"
#include "body_state_actions.h"
#include "body_state_fields.h"

#include <bit>
#include <cmath>
#include "body_fields.h"

namespace gbtg
{
namespace field = stand_alert_field;
namespace base = body_state_field;

// Xbox 0x000A2A90
void repeat_runtime_stand_alert_animation(RuntimeStandAlertTick& context)
{
    const auto* controller = context.animation.controller.get();
    if (controller != nullptr && controller->slots.at(0).player != nullptr &&
        controller->slots[0].player->mode == 1)
    {
        const auto& player = *controller->slots[0].player;
        bool complete;
        if (player.speed < 0.f)
        {
            complete = player.position <= 0.f;
        }
        else
        {
            complete = player.position >= 1.f;
        }
        if (!complete)
        {
            return;
        }
        const u32 name_address = read_u32({context.actions.row, 0x108}, 0x104);
        if (name_address != 0)
        {
            select_runtime_body_animation(context.actions.names.at(name_address), context.actions);
        }
    }
}

// Xbox 0x000A2930
void tick_runtime_stand_alert(std::span<u8> action, RuntimeStandAlertTick& context)
{
    const float elapsed =
        runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + get(action, base::elapsed);
    set(action, base::elapsed, elapsed);
    if (get(action, field::moving_key) != 0)
    {
        const float z = get(context.body, body_field::motion).z;
        const float y = get(context.body, body_field::motion).y;
        const float x = get(context.body, body_field::motion).x;
        u16 key = 0;
        if ((z * z + y * y) + x * x > 0.f && (context.flags.bytes[0] & 2) != 0)
        {
            key = context.moving_action();
        }
        set(action, base::next_key, key);
    }
    if (get(action, field::holds) == 0)
    {
        if (get(action, field::settles) == 0)
        {
            const u32 zero = 0;
            std::memcpy(context.body.data() + 0x734, &zero, 4);
            std::memcpy(context.body.data() + 0x72C, &zero, 4);
        }
        else
        {
            context.orient();
        }
    }
    u16 key = get(action, field::alert_key);
    if ((context.flags.bytes[7] & 8) != 0 || key == 0 || !context.mind_present || context.mind_alert == 0)
    {
        repeat_runtime_stand_alert_animation(context);
        key = get(action, field::input_key);
        if (key == 0)
        {
            return;
        }
        std::array<float, 2> axes;
        runtime_input_axes(context.input, axes, context.input_state);
        if (!(axes[1] * axes[1] + axes[0] * axes[0] > context.input_threshold))
        {
            return;
        }
    }
    set(action, base::next_key, key);
}
}
