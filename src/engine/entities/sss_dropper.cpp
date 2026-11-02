#include "sss_dropper.h"
#include "actor_frame_render.h"
#include "actor_render_registration.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "body_damage_motion.h"
#include "fx_anim_emitter.h"
#include "ghouly_spawn_trigger.h"
#include "player_state.h"
#include "../camera/camera_body_slots.h"
#include "../graphics/glow_pass.h"
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
namespace field = sss_dropper_field;
namespace parameter = sss_dropper_parameter;

template <u32 Offset>
const char* text(RuntimeSssDropper& context, RetailRecord<Offset, 0x80> name)
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

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The inlined 0x0003AAA0/0x0003A9E0 pair: the named clip, when the actor has an animation table.
void start_named_animation(RuntimeSssDropper& context, const char* name)
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

// {id, word, 0...} through the player's vtable +0xC.
void message_player(RuntimeSssDropper& context, const Actor& player, u32 id, u32 word)
{
    std::array<u32, 10> message{};
    message[0] = id;
    message[1] = word;
    context.message(context.address_of(player), message);
}

// Clamps `value` between `first` and `second`, whichever is lower (each COMISS keeps the earlier value on a tie).
float clamp_between(float value, float first, float second)
{
    if (second > first)
    {
        if (value > second)
        {
            return second;
        }
        return first > value ? first : value;
    }
    if (second > value)
    {
        return second;
    }
    return value > first ? first : value;
}
}

// Xbox 0x00071800
void sss_dropper_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x00071870
bool initialize_runtime_sss_dropper(RuntimeSssDropper& context, std::span<const u8> parameters)
{
    const float chance = context.restarting == 1 ? get(parameters, parameter::restart_chance)
                                                 : get(parameters, parameter::chance);
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f; // DAT_0032B0F8
    if (roll > chance)
    {
        return false;
    }
    auto bytes = context.bytes;
    set(bytes, field::reach, get(parameters, parameter::reach));
    set(bytes, field::target, get(parameters, parameter::target));
    set(bytes, field::start, actor_world_position(context.view.bat.actor)); // 0x000412A0, EDI +0x938
    copy_text(record(bytes, field::first_clip), record(parameters, parameter::first_clip).data());
    copy_text(record(bytes, field::second_clip), record(parameters, parameter::second_clip).data());
    set(bytes, field::phase, 0u);
    return true;
}

// Xbox 0x00071960
void update_runtime_sss_dropper(RuntimeSssDropper& context, const std::function<void()>& update_actor)
{
    auto bytes = context.bytes;
    Actor& actor = context.view.bat.actor;
    switch (get(bytes, field::phase))
    {
    case 0:
        if (runtime_sss_dropper_triggered(context)) // 0x00072100
        {
            set_runtime_sss_dropper_phase(context, 1); // 0x00071F30
        }
        break;
    case 1:
    {
        const float step = runtime_actor_step(actor, context.clock);
        const Actor* player = context.player();
        float trail = get(bytes, field::trail_timer);
        if (context.timer_elapsed(trail)) // 0x00042360, EAX +0x960
        {
            std::array<u8, 0x354> burst{};
            build_runtime_emitter_burst_parameters(burst, actor, context.address_of(actor), 0.1f,
                context.default_name); // 0x00069590, ESI actor
            burst[0x33C] = bytes[0x19B];
            put_word(burst, 0x350, 0);
            put_word(burst, 0x34C, 0);
            context.create_object(burst); // 0x00107520
            trail = 1.f / 15.f; // DAT_0032B128
        }
        set(bytes, field::trail_timer, trail);
        set(bytes, field::fall_speed, get(bytes, field::fall_speed) - step * 500.f); // DAT_0032B64C
        Vec3 position = actor_world_position(actor); // 0x000412A0
        float timer = get(bytes, field::timer);
        context.timer_elapsed(timer); // 0x00042360, EAX +0x95C
        set(bytes, field::timer, timer);
        const float fraction = (timer - 0.2f) * -5.f; // DAT_0032B1AC, DAT_0032B37C
        const Vec3 start = get(bytes, field::start);
        const Vec3 target = get(bytes, field::target);
        position.x = clamp_between((target.x - start.x) * fraction + start.x, start.x, target.x);
        position.y = get(bytes, field::fall_speed) * step + position.y;
        position.z = clamp_between((target.z - start.z) * fraction + start.z, start.z, target.z);
        if (target.y > position.y)
        {
            position.y = target.y;
            set_runtime_sss_dropper_phase(context, 2); // 0x00071F30
        }
        actor_set_world_position(actor, position); // 0x00041730, ESI position, EDI actor
        if (player != nullptr)
        {
            const Vec3 euler{0.f, runtime_actor_angle_toward(*player, actor), 0.f}; // 0x00041900, EAX player
            wrap_runtime_euler(euler, actor.euler); // 0x00105AA0
            actor.xform_valid = 0; // Actor +0x2A8.
        }
        break;
    }
    case 2:
        if (actor_animation_completed(context.view.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            start_named_animation(context, "scare"); // 0x0031C638
            set(bytes, field::phase, 3u);
        }
        break;
    case 3:
        break;
    case 4:
    {
        const float step = runtime_frame_step(context.clock);
        float timer = get(bytes, field::timer);
        if (timer > 0.f)
        {
            timer -= step;
            set(bytes, field::timer, timer);
            if (!(0.f < timer))
            {
                set(bytes, field::timer, 0.f);
                if (actor_animation_completed(context.view.animation, 0)) // 0x0003B290, EAX actor, ECX 0
                {
                    activate_actor_bind_tree(actor); // 0x00112CC0, ESI actor
                }
                break;
            }
        }
        // Back up toward the start over the 5 s (DAT_0032B184, DAT_0032B274).
        Vec3 position = actor_world_position(actor); // 0x000412A0
        const float top = get(bytes, field::start).y;
        const float bottom = get(bytes, field::target).y;
        const float fraction = (get(bytes, field::timer) - 5.f) * -0.2f;
        position.y = clamp_between((top - bottom) * fraction + bottom, bottom, top);
        actor_set_world_position(actor, position); // 0x00041730, ESI position, EDI actor
        break;
    }
    default:
        break;
    }
    update_actor(); // 0x000420D0
}

// Xbox 0x00071D50
void dispatch_runtime_sss_dropper_message(RuntimeSssDropper& context, std::span<u32> message)
{
    context.base_message(message); // 0x0003EF70
    if (message[0] == 0x23)
    {
        unregister_actor_render_bins(context.view.render); // 0x0003CBB0, EDI actor
        unregister_actor_segment_probes(context.view.render); // 0x0003E850, EAX actor
        clear_actor_shape_probe(context.view.bat.actor, context.view.shape); // 0x0003C700
        return;
    }
    if (message[0] == 0x8E && get(context.bytes, field::phase) == 3 && (message[1] == 7 || message[1] == 8))
    {
        change_runtime_actor_pause(u8(0xFFFF), 1, context.pause()); // 0x00117400
        start_named_animation(context, "leapoff"); // 0x0031C394
        set(context.bytes, field::timer, 5.f); // DAT_0032B184
        set(context.bytes, field::phase, 4u);
    }
}

// Xbox 0x00071DF0
u32 draw_runtime_sss_dropper(ActorDrawState& state, i32 kind, const ActorFrameDrawContext& attachments,
    SceneModelDrawContext& draw, RuntimeGlowPass& glow)
{
    if (kind != 2)
    {
        return 0;
    }
    begin_runtime_glow_capture(nullptr, nullptr, glow); // The inlined 0x00062220 over the whole view.
    draw_runtime_actor(state, 1, attachments, draw); // 0x0003C9E0
    stamp_runtime_glow(0xC4FFFFFFu, 1, glow); // 0x00062490, EAX 1
    return 1;
}

// Xbox 0x00071F30
void set_runtime_sss_dropper_phase(RuntimeSssDropper& context, u32 phase)
{
    auto bytes = context.bytes;
    switch (phase)
    {
    case 1:
    {
        change_runtime_actor_pause(u8(0xFFFF), 0, context.pause()); // 0x00117400
        if (const Actor* player = context.player(); player != nullptr)
        {
            message_player(context, *player, 0x4D, 7);
        }
        register_actor_render_bins(context.view.render); // 0x0003CAE0
        start_named_animation(context, "drop"); // 0x0031C3A4
        set(bytes, field::fall_speed, 0.f);
        set(bytes, field::timer, 0.2f); // DAT_0032B1AC
        set(bytes, field::trail_timer, 0.001f); // DAT_0032B1A4
        break;
    }
    case 2:
        start_named_animation(context, "land"); // 0x0031C39C
        if (const Actor* player = context.player(); player != nullptr)
        {
            message_player(context, *player, 0x32, 0);
        }
        start_runtime_sss_dropper_challenge(context); // 0x00072200, EDX actor
        break;
    case 3:
        start_named_animation(context, "scare"); // 0x0031C638
        break;
    case 4:
        change_runtime_actor_pause(u8(0xFFFF), 1, context.pause()); // 0x00117400
        start_named_animation(context, "leapoff"); // 0x0031C394
        set(bytes, field::timer, 5.f); // DAT_0032B184
        break;
    default:
        break;
    }
    set(bytes, field::phase, phase);
}

// Xbox 0x00072100, EBX actor
bool runtime_sss_dropper_triggered(RuntimeSssDropper& context)
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
    if ((context.runtime_flags & 8u) != 0 || runtime_invisibility_active() != 0 || context.clock.block_decay != 0
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
    const float squared = dz * dz + dx * dx + 0.f * 0.f;
    return reach * reach >= squared;
}

// Xbox 0x00072200, EDX actor
void start_runtime_sss_dropper_challenge(RuntimeSssDropper& context)
{
    std::array<u8, 0x1D8> parameters{};
    build_runtime_scene_challenge_parameters(parameters, text(context, field::first_clip),
        context.address_of(context.view.bat.actor), -1.f, -1.f, nullptr, nullptr, 0, nullptr, 0, 0, 0,
        text(context, field::second_clip), context.runtime_flags); // 0x000873A0
    context.create_object(parameters); // 0x00107520
}
}
