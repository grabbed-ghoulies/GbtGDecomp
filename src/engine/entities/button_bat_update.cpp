#include "button_bat_update.h"
#include "../camera/display_view.h"

namespace gbtg
{
// Xbox 0x0006D5C0
void update_button_bat(ButtonBatScreen& bat, ActorFrameState& state,
    ActorFrameContext& context)
{
    update_actor_frame(state, context);
    const auto& display = *entity_detail::g_display_view;
    const Vec3 position
    {
        display.forward.x * 5.f + display.position.x,
        display.forward.y * 5.f + display.position.y,
        display.forward.z * 5.f + display.position.z
    }; // DAT_0032B184 = 0x40A00000
    actor_set_world_position(bat.actor, position);
    auto& animation = state.animation;
    auto& effects = context.effects;
    switch (bat.state)
    {
    case 1:
        if (actor_tick_timer(bat.actor, bat.idle_timer, context.freeze,
            context.dt, context.scene_scale))
        {
            if (animation.table != nullptr)
            {
                const AnimationRecord* record = find_actor_animation(animation.table,
                    "hop", nullptr, effects.random_state);
                if (record != nullptr)
                {
                    start_actor_animation(animation, *record, effects.models.registry,
                        effects.models.error, effects.random_state);
                }
            }
            bat.state = 3;
        }
        break;
    case 2:
    case 3:
    case 4:
        if (actor_animation_completed(animation, 0))
        {
            button_bat_set_target(bat, 1, animation, effects.models.registry,
                effects.models.error, effects.random_state);
        }
        break;
    }
}
}
