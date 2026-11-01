#include "scare_actor.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "entity.h"
#include "../camera/camera_body_slots.h"
#include "../camera/display_view.h"
#include "../camera/entity_camera.h"
#include "../graphics/shader_parameters.h"
#include "gbtg/actor_runtime.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
namespace parameter = scare_actor_parameter;
namespace field = scare_actor_field;

constexpr float kThird = std::bit_cast<float>(0x3EAAAAABu); // DAT_0032B3D4
constexpr float kEnd = std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8, 0.95

// The model's pose 0x00031530 reads: +0x168, else +0x164.
const ModelPoseBuffer* scare_pose(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->runtime == nullptr)
    {
        return nullptr;
    }
    const RuntimeModelInstance& model = *actor.model->runtime;
    return model.alternate != nullptr ? model.alternate : model.primary;
}

MinaPlayer* slot_player(const ActorAnimationState& animation)
{
    AnimationController* controller = animation.controller.get(); // Actor +0x50.
    if (controller == nullptr || controller->slots.empty())
    {
        return nullptr;
    }
    return controller->slots[0].player.get(); // +0x58
}
} // namespace

// Xbox 0x0006FCA0
void scare_actor_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0006FD20
bool initialize_runtime_scare_actor(RuntimeScareActor& context, std::span<const u8> parameters)
{
    auto bytes = context.bytes;
    set(bytes, field::effect, get(parameters, parameter::effect));
    set(bytes, field::pulse_time, 0.f);
    set(bytes, field::pulsing, 1u);
    set(bytes, field::pulses, 3);
    // 'PxiM': no duration, DAT_0032B1FC, 0x21, DAT_0032B1AC, mode 1, 0x31, DAT_0032B1AC, the parameters' events.
    const auto events = get(parameters, parameter::events);
    AnimationMix mix;
    mix.duration = 0.f;
    mix.weight = -10.f; // DAT_0032B1FC
    mix.blend_mode = 0x21;
    mix.fade_duration = 0.2f; // DAT_0032B1AC
    mix.position = 0.f;
    mix.mode = 1;
    mix.priority = 0x31;
    mix.old_pose_duration = 0.2f;
    mix.events = std::string_view(events.data());
    ActorAnimationState& animation = context.animation;
    if (animation.controller == nullptr)
    {
        animation.controller = make_animation_controller(4); // 0x00030740
        animation.controller->event_callback = [](std::span<const u8> event, u32 index, float position,
            void* owner, int phase)
        {
            animation_actor_event(event, index, position, *static_cast<ActorAnimationState*>(owner), phase); // 0x0003B3C0
        };
        animation.controller->event_owner = &animation;
    }
    const auto name = get(parameters, parameter::animation);
    start_controller_animation(*animation.controller, 0, {}, name.data(), mix, context.registry, context.error,
        context.random); // 0x00030880, its result unused
    if (AnimationController* controller = animation.controller.get(); controller != nullptr)
    {
        MinaPlayer* player = slot_player(animation);
        const float length = player != nullptr ? read_f32(player->mina, 8) : 0.f;
        retime_mina_playback(player, length / get(parameters, parameter::intensity),
            controller->dirty); // 0x00030DB0, EAX slot 0
    }
    const u32 texture = find_actor_shader_texture(context.actor.model != nullptr ? context.actor.model->runtime
        : nullptr, "shader_lambert3", "colour0", [&context](u32 address) -> const u8*
    {
        return context.texture_bytes(address);
    }); // 0x00040B70
    set(bytes, field::texture, texture);
    set(bytes, field::silence, get(parameters, parameter::silence));
    set(bytes, field::silenced, 0u);
    set(bytes, field::player_flag, context.player_flag()); // DAT_00511B40
    context.runtime_flags |= 1; // DAT_0050EA9C
    set(bytes, field::camera_inside, 0u);
    if (get(bytes, field::silence) != 0)
    {
        acquire_tracked_audio_bank("XACT_SILENCE", 4, context.streams); // 0x00038910, EAX 0x002FFCC0
        set(bytes, field::silenced, 1u);
    }
    return true;
}

// Xbox 0x0006FF20
void release_runtime_scare_actor(RuntimeScareActor& context)
{
    if (get(context.bytes, field::silenced) != 0)
    {
        release_tracked_audio_bank("XACT_SILENCE", context.streams); // 0x00038970 inlined
        set(context.bytes, field::silenced, 0u);
    }
    context.runtime_flags &= ~1u; // DAT_0050EA9C
}

// Xbox 0x00070410
float runtime_scare_actor_reach(const Actor& actor)
{
    i32 joint = -1;
    if (actor.model != nullptr && actor.model->runtime != nullptr && actor.model->runtime->source != nullptr)
    {
        const u32 index = actor_lookup_joint_index(actor.model->cpu, 0) & 0xFFu; // 0x00052FC0(0)
        joint = index == 0xFF ? -1 : i32(index);
    }
    const ModelPoseBuffer* pose = scare_pose(actor);
    if (pose == nullptr)
    {
        throw Error("0x00070410 reads joint 0's matrix of a scare with no model pose (address zero)");
    }
    const AnimationMatrix& matrix = model_pose_matrix(*pose, joint); // 0x00031530
    const float scale = actor.scale_200; // +0x200
    const float body = (actor.radius_xz * scale + actor.radius_y * scale + actor.scale_x * scale) * kThird;
    const float joint_scale = (matrix[10] + matrix[5] + matrix[0]) * kThird; // +0x28, +0x14, +0
    return joint_scale * body;
}

// Xbox 0x000703A0
void start_runtime_scare_actor_effect(RuntimeScareActor& context)
{
    g_actor_creation_options.flags = 0; // DAT_004E1C5C
    g_actor_creation_options.position = actor_world_position(context.actor); // 0x000412A0
    g_actor_creation_options.flags |= 1;
    const auto name = get(context.bytes, field::effect);
    load_actor_effect_stream(name.data(), &g_actor_creation_options, context.effects); // 0x00053170
}

// Xbox 0x0006FF90
void update_runtime_scare_actor(RuntimeScareActor& context)
{
    auto bytes = context.bytes;
    Actor& actor = context.actor;
    MinaPlayer* player = slot_player(context.animation);
    const float position = player != nullptr ? player->position : 0.f; // +0x14
    // The texture flickers through its frames with the clip: |sin(6 pi position)| of the last frame.
    if (const u32 texture = get(bytes, field::texture); texture != 0)
    {
        const float wave = float(retail_sin(position * std::bit_cast<float>(0x4196CBE4u))); // DAT_0032B608, 0x00153320
        u8* record = context.texture_bytes(texture);
        const u8 frames = record[0x11];
        const float swing = engine_absolute(wave); // 0x00152BB0
        const u32 frame = u32(static_cast<i32>(swing * (float(i32(frames)) - 1.f))) & 0xFFu; // 0x001673D8
        if (i32(frame) != -1 && frame < u32(record[0x11]))
        {
            record[0x13] = u8(frame);
        }
    }
    if (get(bytes, field::pulsing) != 0)
    {
        float time = get(bytes, field::pulse_time) - runtime_actor_step(actor, context.clock);
        set(bytes, field::pulse_time, time);
        if (!(0.f < time))
        {
            time = time + 0.5f; // DAT_0032B0F4
            set(bytes, field::pulse_time, time);
            const i32 pulses = get(bytes, field::pulses) - 1;
            set(bytes, field::pulses, pulses);
            if (pulses == 0)
            {
                set(bytes, field::pulsing, 0u);
            }
        }
    }
    const u32 previous = get(bytes, field::player_flag);
    const u32 flag = context.player_flag(); // DAT_00511B40
    set(bytes, field::player_flag, flag);
    if (flag != 0 && previous == 0)
    {
        set(bytes, field::unk_94C, 0.2f); // DAT_0032B1AC
    }
    const float reach = runtime_scare_actor_reach(actor); // 0x00070410
    set(bytes, field::reach, reach);
    const Vec3 at = actor_world_position(actor); // 0x000412A0
    const Vec3& eye = context.view->position; // DAT_005077A8
    const float x = at.x - eye.x;
    const float y = at.y - eye.y;
    const float z = at.z - eye.z;
    // x87: (z z + y y) + x x, its square root against the reach.
    const long double distance = std::sqrt(((long double)z * z + (long double)y * y) + (long double)x * x);
    const bool inside = !((long double)reach < distance);
    set(bytes, field::camera_inside, inside ? 1u : 0u);
    if (inside)
    {
        const u32 color = 0x800000FFu; // DAT_0042742C
        context.tint.rise = color;
        context.tint.peak = color;
        context.tint.fall = color;
        context.tint.total = -1.f; // DAT_0032B120
        context.tint.remaining = -1.f;
        context.tint.active = 1;
    }
    const std::array<u32, 1> message{0x5B};
    context.broadcast(message, at, reach); // 0x00122D80
    player = slot_player(context.animation);
    if (player == nullptr)
    {
        activate_actor_bind_tree(actor); // 0x00112CC0
        return;
    }
    // Past 0.95 of the clip in its direction of play (or wrapping past it), the scare's effect starts.
    const float now = player->position; // +0x14
    bool crossed = false;
    if (now != player->previous) // +0x10C
    {
        float from;
        float to;
        if (0.f > player->speed) // +0x1C
        {
            from = now;
            to = player->previous;
        }
        else
        {
            from = player->previous;
            to = now;
        }
        if (to > from)
        {
            crossed = !(kEnd < from) && to > kEnd;
        }
        else
        {
            crossed = kEnd >= from || to > kEnd;
        }
    }
    if (crossed)
    {
        start_runtime_scare_actor_effect(context); // 0x000703A0
    }
    // A clamped clip at its end retires the scare.
    bool done = false;
    if (player->mode == 1) // +0x2A
    {
        done = 0.f > player->speed ? !(0.f < player->position) : !(player->position < 1.f);
    }
    if (done)
    {
        activate_actor_bind_tree(actor); // 0x00112CC0
    }
}

// Xbox 0x000702C0
bool face_runtime_scare_actor(RuntimeScareActor& context)
{
    if (get(context.bytes, field::camera_inside) != 0)
    {
        return false;
    }
    Actor& actor = context.actor;
    const Vec3 at = actor_world_position(actor); // 0x000412A0
    const Vec3& eye = context.view->position; // DAT_005077A8
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(eye.x - at.x, eye.y - at.y, eye.z - at.z, yaw, pitch); // 0x00105E20
    wrap_runtime_euler(Vec3{pitch, yaw, 0.f}, actor.euler); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // +0x2A8
    return true;
}
}
