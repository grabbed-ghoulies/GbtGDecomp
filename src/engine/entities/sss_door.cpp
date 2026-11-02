#include "sss_door.h"
#include "actor_render_registration.h"
#include "actor_scare_requests.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "ghouly_spawn_trigger.h"
#include "../camera/camera_body_slots.h"
#include "../cutscenes/door_registration.h"
#include "../scene/trophy_heads_control.h"
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "body_fields.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = sss_door_field;
namespace parameter = sss_door_parameter;

template <u32 Offset>
const char* text(RuntimeSssDoor& context, RetailRecord<Offset, 0x80> name)
{
    return reinterpret_cast<const char*>(record(context.bytes, name).data());
}

void copy_text(std::span<u8> target, const u8* source)
{
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

// The inlined 0x0003AAA0/0x0003A9E0 pair: the named clip, when the actor has an animation table (+0x48).
void start_named_animation(RuntimeSssDoor& context, const char* name)
{
    auto& animation = context.view.animation;
    if (animation.table == nullptr)
    {
        return;
    }
    const AnimationRecord* record = find_actor_animation(animation.table, name, nullptr, context.random);
    if (record != nullptr)
    {
        start_actor_animation(animation, *record, context.registry, context.error, context.random);
    }
}

// "out" from its start (slot 0's +0x14 and +0x10C cleared, the controller dirty), held there (0x0003B310): the
// figure waits out of sight.
void wait_out_of_sight(RuntimeSssDoor& context)
{
    start_named_animation(context, "out"); // 0x0031C38C
    auto& animation = context.view.animation;
    if (animation.controller != nullptr)
    {
        auto& controller = *animation.controller;
        if (MinaPlayer* player = controller.slots.at(0).player.get(); player != nullptr)
        {
            player->position = 0.f;
            player->previous = 0.f;
        }
        controller.dirty = true;
    }
    hold_runtime_actor_animation_slot(animation, 0); // 0x0003B310, ECX 0
}
}

// Xbox 0x00071180
void sss_door_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000711E0
bool initialize_runtime_sss_door(RuntimeSssDoor& context, std::span<const u8> parameters)
{
    auto bytes = context.bytes;
    set(bytes, field::reach, get(parameters, parameter::reach));
    copy_text(record(bytes, field::first_clip), record(parameters, parameter::first_clip).data());
    copy_text(record(bytes, field::second_clip), record(parameters, parameter::second_clip).data());
    set(bytes, field::reusable, get(parameters, parameter::reusable));
    set(bytes, field::door, get(parameters, parameter::door));
    set(bytes, field::face_player, get(parameters, parameter::face_player));
    set(bytes, field::move_player, get(parameters, parameter::move_player));
    const float chance = context.restarting == 1 ? get(parameters, parameter::restart_chance)
                                                 : get(parameters, parameter::chance);
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f; // DAT_0032B0F8
    if (roll > chance)
    {
        if (get(bytes, field::reusable) == 0)
        {
            return false;
        }
        wait_out_of_sight(context);
        set(bytes, field::phase, 4u);
        return true;
    }
    wait_out_of_sight(context);
    set(bytes, field::phase, 0u);
    return true;
}

// Xbox 0x00071380
void update_runtime_sss_door(RuntimeSssDoor& context, const std::function<void()>& update_actor)
{
    auto bytes = context.bytes;
    Actor& actor = context.view.bat.actor;
    switch (get(bytes, field::phase))
    {
    case 0:
        if (runtime_sss_door_triggered(context)) // 0x00071640, EBX actor
        {
            const Vec3 position = actor_world_position(actor); // 0x000412A0
            if (get(bytes, field::door) != 0)
            {
                close_runtime_door_nearest(position, context.doors()); // 0x0005C130, EDI position
            }
            const Vec3 where = actor_world_position(actor); // 0x000412A0
            const std::array<u32, 4> message{0x37, std::bit_cast<u32>(where.x), std::bit_cast<u32>(where.y),
                std::bit_cast<u32>(where.z)};
            broadcast_actor_world_message(message); // 0x00122CE0, EDI the message
            if (get(bytes, field::reusable) == 0)
            {
                register_actor_render_bins(context.view.render); // 0x0003CAE0
            }
            release_runtime_actor_animation_slot(context.view.animation, 0); // 0x0003B340, ECX 0
            start_named_animation(context, "out"); // 0x0031C38C
            set(bytes, field::timer, 0.001f); // DAT_0032B1A4
            set(bytes, field::phase, 1u);
        }
        break;
    case 1:
        if (const Actor* player = context.player(); player != nullptr && get(bytes, field::face_player) != 0)
        {
            const Vec3 euler{0.f, runtime_actor_angle_toward(*player, actor), 0.f}; // 0x00041900, EAX player
            wrap_runtime_euler(euler, actor.euler); // 0x00105AA0, EDI actor +0x14
            actor.xform_valid = 0; // Actor +0x2A8.
        }
        if (actor_animation_completed(context.view.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            start_runtime_sss_door_challenge(context); // 0x00071750, EAX actor
            start_named_animation(context, "scare"); // 0x0031C638
            set(bytes, field::phase, 2u);
        }
        break;
    case 3:
        if (actor_animation_completed(context.view.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            if (get(bytes, field::reusable) != 0)
            {
                wait_out_of_sight(context);
                set(bytes, field::phase, 4u);
                break;
            }
            activate_actor_bind_tree(actor); // 0x00112CC0, ESI actor
        }
        break;
    default:
        break;
    }
    update_actor(); // 0x000420D0
}

// Xbox 0x000715B0
void dispatch_runtime_sss_door_message(RuntimeSssDoor& context, std::span<u32> message)
{
    context.base_message(message); // 0x0003EF70
    if (message[0] == 0x23)
    {
        if (get(context.bytes, field::reusable) == 0)
        {
            unregister_actor_render_bins(context.view.render); // 0x0003CBB0, EDI actor
            unregister_actor_segment_probes(context.view.render); // 0x0003E850, EAX actor
            clear_actor_shape_probe(context.view.bat.actor, context.view.shape); // 0x0003C700
        }
        return;
    }
    if (message[0] == 0x8E && get(context.bytes, field::phase) == 2 && (message[1] == 7 || message[1] == 8))
    {
        start_named_animation(context, "in"); // 0x0031C390
        set(context.bytes, field::phase, 3u);
    }
}

// Xbox 0x00071640, EBX actor
bool runtime_sss_door_triggered(RuntimeSssDoor& context)
{
    const Actor* player = context.player();
    if (player == nullptr)
    {
        return false;
    }
    const auto body = context.player_body(*player); // Player +0x95C.
    if (get(body, body_field::move_holds) != 0 || get(body, body_field::move_hold_time) != 0.f)
    {
        return false;
    }
    if (runtime_invisibility_active() != 0 || (context.runtime_flags & 8u) != 0 || context.clock.block_decay != 0
        || context.clock.decay_enabled[context.clock.decay_index] == 0)
    {
        return false;
    }
    const float reach = get(context.bytes, field::reach);
    if (0.f >= reach)
    {
        return true;
    }
    const Vec3 player_position = actor_world_position(*player); // 0x000412A0
    const Vec3 position = actor_world_position(context.view.bat.actor); // 0x000412A0
    const float dz = position.z - player_position.z;
    const float dx = position.x - player_position.x;
    const float squared = dz * dz + dx * dx;
    if (!(reach * reach >= squared))
    {
        return false;
    }
    if (get(context.bytes, field::door) == 0)
    {
        return true;
    }
    return runtime_door_nearest_idle(position, context.doors()); // 0x0005C150, EAX position
}

// Xbox 0x00071750, EAX actor
void start_runtime_sss_door_challenge(RuntimeSssDoor& context)
{
    const Actor& actor = context.view.bat.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float heading = actor.euler.y; // Actor +0x18.
    const float across = float(retail_sin(heading) * 35.0L); // 0x00153320, DAT_0032B494
    const long double along = retail_cos(heading) * 35.0L; // 0x00153230
    const Vec3 spawn{float(static_cast<long double>(across) + position.x), position.y,
        float(static_cast<long double>(position.z) + along)};
    std::array<u8, 0x1D8> parameters{};
    build_runtime_scene_challenge_parameters(parameters, text(context, field::first_clip),
        context.address_of(actor), -1.f, -1.f, nullptr, &spawn, get(context.bytes, field::move_player), nullptr, 0, 0,
        0, text(context, field::second_clip), context.runtime_flags); // 0x000873A0
    context.create_object(parameters); // 0x00107520
}
}
