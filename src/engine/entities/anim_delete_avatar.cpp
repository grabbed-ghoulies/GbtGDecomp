#include "anim_delete_avatar.h"
#include "actor_render_registration.h"
#include "fx_anim_emitter.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
AnimDeleteState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<AnimDeleteState>(bytes);
}

const AnimDeleteState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<AnimDeleteState>(bytes);
}

namespace parameter = anim_delete_parameter;

MinaPlayer* slot_player(const ActorAnimationState& animation)
{
    AnimationController* controller = animation.controller.get(); // Actor +0x50.
    if (controller == nullptr || controller->slots.empty())
    {
        return nullptr;
    }
    return controller->slots[0].player.get(); // +0x58
}
}

// Xbox 0x0006C9C0
void anim_delete_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(AnimDeleteState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0006CA20
bool initialize_runtime_anim_delete(std::span<const u8> parameters, RuntimeAnimDelete& context)
{
    ActorAnimationState& animation = context.animation;
    if (AnimationController* controller = animation.controller.get(); controller != nullptr)
    {
        if (MinaPlayer* player = slot_player(animation); player != nullptr)
        {
            player->mode = 1; // +0x2A, played once.
        }
        controller->dirty = true; // +0x5C.
    }
    const float start = get(parameters, parameter::start);
    if (AnimationController* controller = animation.controller.get(); controller != nullptr)
    {
        if (MinaPlayer* player = slot_player(animation); player != nullptr)
        {
            player->position = start; // +0x14.
            player->previous = start; // +0x10C.
        }
        controller->dirty = true;
    }
    const float duration = animation_absolute(get(parameters, parameter::speed)); // 0x00152BB0
    if (AnimationController* controller = animation.controller.get(); controller != nullptr)
    {
        retime_mina_playback(slot_player(animation), duration, controller->dirty); // 0x00030DB0, EAX slot 0
    }
    const u16 direction = 0.f > get(parameters, parameter::speed) ? 1 : 0;
    if (AnimationController* controller = animation.controller.get(); controller != nullptr)
    {
        if (MinaPlayer* player = slot_player(animation); player != nullptr)
        {
            set_mina_playback_direction(*player, direction); // 0x0002DD00, AX the direction, ESI slot 0
        }
        controller->dirty = true;
    }
    fields_of(context.bytes).fade = get(parameters, parameter::fade);
    return true;
}

// Xbox 0x0006CAF0
void update_runtime_anim_delete(RuntimeAnimDelete& context)
{
    if (!actor_animation_completed(context.animation, 0)) // The inlined 0x0003B290, slot 0.
    {
        return;
    }
    const float fade = fields_of(context.bytes).fade;
    if (fade > 0.f && 0.f < fade) // COMISS twice: JBE, then JNC, skip the after-image.
    {
        std::array<u8, 0x354> parameters{};
        build_runtime_emitter_burst_parameters(parameters, context.actor, context.address, fade,
            context.default_name); // 0x00069590, ESI actor
        parameters[0x33C] = context.bytes[0x19B];
        std::memset(parameters.data() + 0x350, 0, 4);
        std::memset(parameters.data() + 0x34C, 0, 4);
        context.keep(create_factory_object(context.factory, parameters)); // 0x00107520
    }
    activate_actor_bind_tree(context.actor); // 0x00112CC0, ESI actor
}
}
