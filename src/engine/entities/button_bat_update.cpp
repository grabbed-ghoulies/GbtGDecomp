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

// Xbox 0x0006DA00
void fly_runtime_button_bat(ButtonBatScreen& bat, ActorAnimationState& animation, const ButtonBatPoint& target,
    float scale, AnimationResourceRegistry& registry, ResourceErrorState& error, u32& random_state)
{
    bat.from = bat.current;
    if (animation.table != nullptr)
    {
        const AnimationRecord* record = find_actor_animation(animation.table, "flybetween", nullptr,
            random_state); // 0x0003AAA0, ECX 0x0031C3C8
        if (record != nullptr)
        {
            start_actor_animation(animation, *record, registry, error, random_state); // 0x0003A9E0
        }
    }
    if (animation.controller != nullptr)
    {
        retime_mina_playback(animation.controller->slots[0].player.get(), bat.flight_duration,
            animation.controller->dirty); // 0x00030DB0, XMM0 +0x93C
    }
    if (bat.state == 2)
    {
        set_actor_animation_position(animation, 0, 0.2f); // 0x0003B2E0, DAT_0032B1AC
    }
    bat.state = 2;
    bat.target = target;
    const float horizontal = scale * 2.f; // DAT_0032B110
    bat.target.y = bat.target.y - scale * 21.f; // DAT_0032B1BC
    bat.target.x = bat.target.x - horizontal;
}
}
