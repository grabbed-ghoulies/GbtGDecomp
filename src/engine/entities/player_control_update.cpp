#include "player_control_update.h"
#include "player_mind_control.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0007D130
void update_runtime_player_control(RuntimePlayerControlUpdate& context)
{
    auto mind = context.control.mind;
    const float step = runtime_frame_step(context.control.clock);
    const float timer = player_mind_record(mind).unk_12C;
    if (timer > 0.f)
    {
        const float remaining = timer - step;
        player_mind_record(mind).unk_12C = remaining;
        if (0.f >= remaining)
        {
            const u32 zero = 0;
            player_mind_record(mind).unk_12C = 0.f;
        }
    }
    auto& avoidance = context.avoidance;
    transform_runtime_player_mind_input(context.actions.body.body, context.invert,
        context.control.input, context.control.input_state, avoidance.writable,
        avoidance.keyboard, context.camera_direction);
    update_runtime_player_mind_action_control(context.actions);
    update_runtime_player_pending_attachment(context.pending);
    update_runtime_player_mind_gaze(context.gaze, 1, 30.f, 100.f);
    update_runtime_player_mind_inactivity(context.inactivity, context.actions.body,
        context.animation, context.random);
    update_runtime_player_mind_hold_timer(context.control);
    bool retained = false;
    if (update_runtime_player_boost_timer(context.boost, context.control,
        &context.boost_preset, 2.5f))
    {
        retained = avoid_runtime_player_navigation(avoidance);
    }
    if (!retained)
    {
        const u32 zero = 0;
        player_mind_record(mind).unk_194 = zero;
    }
    release_runtime_player_mind_input_attachment(context.control);
}
}
