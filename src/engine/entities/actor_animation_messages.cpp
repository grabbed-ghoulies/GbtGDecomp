#include "gbtg/anim.h"

namespace gbtg
{
// Xbox 0x0003ACB0
AnimationController* ensure_actor_animation_controller(ActorAnimationState& actor)
{
    if (actor.controller == nullptr)
    {
        actor.controller = make_animation_controller(4);
        actor.controller->event_callback = [](std::span<const u8> event, u32 index,
            float position, void* owner, int phase)
        {
            animation_actor_event(event, index, position, *static_cast<ActorAnimationState*>(owner), phase);
        };
        actor.controller->event_owner = &actor;
    }
    return actor.controller.get();
}

// Xbox 0x00030B10
void start_controller_animation_mix(AnimationController& controller, u32 slot,
    const AnimationMix& mix, const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error, u32& random_state)
{
    if (mix.borrowed_animation.data() != nullptr)
    {
        start_controller_animation(controller, slot, mix.borrowed_animation, nullptr, mix,
            registry, error, random_state);
        return;
    }
    start_controller_animation(controller, slot, {}, name, mix, registry, error, random_state);
}
}
