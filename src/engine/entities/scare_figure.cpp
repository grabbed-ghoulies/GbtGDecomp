#include "scare_figure.h"
#include "actor_frame_render.h"
#include "actor_render_registration.h"
#include "actor_scare_requests.h"
#include "actor_shape_push.h"
#include "actor_transform.h"
#include "ghouly_spawn_trigger.h"
#include "player_mind_feedback.h"
#include "../graphics/glow_pass.h"
#include "../scene/actor_loading.h"
#include "gbtg/entity.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
ScareFigureState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<ScareFigureState>(bytes);
}

const ScareFigureState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<ScareFigureState>(bytes);
}

namespace parameter = scare_figure_parameter;

void copy_name(std::span<const u8> source, std::span<u8> target)
{
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

// Message 0xB2 to the players: {0xB2, ended, +0xB7C}.
void tell_players(u32 ended, RuntimeScareFigure& context)
{
    const std::array<u32, 3> message{0xB2, ended, fields_of(context.bytes).scare_word};
    context.tell_players(message);
}

// Leaving state 0: message 0x37 with the position to the world (0x00122CE0), then script event 0xB8.
void announce_scare(RuntimeScareFigure& context)
{
    std::array<u32, 4> notice{0x37};
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    std::memcpy(notice.data() + 1, &position, 12);
    broadcast_actor_world_message(notice); // 0x00122CE0, EDI the message
    const u32 event = 0xB8;
    context.queue_event({reinterpret_cast<const u8*>(&event), 4}); // 0x00116810
}

// States 2 and 4's end: the players hear the scare end, then a figure that stays rests in its main animation and
// one that does not goes.
void finish_scare(RuntimeScareFigure& context)
{
    tell_players(1, context);
    if (fields_of(context.bytes).stays != 0)
    {
        play_runtime_scare_figure_animation(nullptr, fields_of(context.bytes).animation.data(), true, false, context);
        fields_of(context.bytes).state = 5;
        return;
    }
    activate_actor_bind_tree(context.actor); // 0x00112CC0, ESI this
    fields_of(context.bytes).state = 5;
}

// The fade's alpha toward the second bound, kept between the two in 0x0006F1C0's branch order.
float clamp_alpha(float value, float first, float second)
{
    if (first > second)
    {
        if (value > first)
        {
            return first;
        }
        if (!(second <= value))
        {
            return second;
        }
        return value;
    }
    if (first > value)
    {
        return first;
    }
    if (!(value <= second))
    {
        return second;
    }
    return value;
}
}

// Xbox 0x000423D0, ECX timer, EDX actor
bool tick_runtime_decay_timer(const Actor& actor, float& timer, const RuntimeActorClock& clock)
{
    if (!runtime_decay_enabled(clock))
    {
        return false;
    }
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(actor);
    if (0.f < timer)
    {
        timer = timer - rate * step;
        if (timer <= 0.f)
        {
            timer = 0.f;
            return true;
        }
    }
    return false;
}

// Xbox 0x0006EF90
void scare_figure_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xB84;
    sizes.parameters = 0x574;
}

// Xbox 0x0006F000
bool initialize_runtime_scare_figure(RuntimeScareFigure& context, std::span<const u8> parameters)
{
    auto bytes = context.bytes;
    copy_name(record(parameters, parameter::appear_animation), retail_bytes(fields_of(bytes).appear_animation));
    copy_name(record(parameters, parameter::appear_events), retail_bytes(fields_of(bytes).appear_events));
    copy_name(record(parameters, parameter::animation), retail_bytes(fields_of(bytes).animation));
    copy_name(record(parameters, parameter::play_events), retail_bytes(fields_of(bytes).play_events));
    fields_of(bytes).alpha_limit = get(parameters, parameter::alpha);
    fields_of(bytes).fade_in = get(parameters, parameter::fade_in);
    fields_of(bytes).fade_out = get(parameters, parameter::fade_out);
    fields_of(bytes).stays = get(parameters, parameter::stays);
    fields_of(bytes).scare_word = get(parameters, parameter::scare_word);
    fields_of(bytes).facing = get(parameters, parameter::facing);
    fields_of(bytes).timer = get(parameters, parameter::delay);
    fields_of(bytes).delay = get(parameters, parameter::delay);
    fields_of(bytes).reach = get(parameters, parameter::reach);
    fields_of(bytes).unk_958 = get(parameters, parameter::unk_340);
    fields_of(bytes).unk_95C = get(parameters, parameter::unk_344);
    fields_of(bytes).paused = 0u;
    fields_of(bytes).state = 5;
    fields_of(bytes).fade_in_plays = get(parameters, parameter::fade_in_plays);
    fields_of(bytes).facing_states = get(parameters, parameter::facing_states);
    fields_of(bytes).trigger = get(parameters, parameter::trigger);
    i32 state = 0;
    const float chance = get(parameters, parameter::chance);
    if (chance != 1.f) // DAT_0032B0F8.
    {
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        if (!(roll < chance))
        {
            if (fields_of(bytes).stays == 0)
            {
                return false;
            }
            state = 5;
        }
    }
    if (state != fields_of(bytes).state)
    {
        set_runtime_scare_figure_state(state, context); // 0x0006F7C0, EAX actor
    }
    return true;
}

// Xbox 0x0006F1C0
void update_runtime_scare_figure(RuntimeScareFigure& context)
{
    auto bytes = context.bytes;
    Actor& actor = context.actor;
    float timer = fields_of(bytes).timer;
    switch (fields_of(bytes).state)
    {
    case 0:
        if (fields_of(bytes).stays != 0)
        {
            notify_runtime_scare_watcher(context); // 0x0013D510
        }
        tick_runtime_decay_timer(actor, timer, context.clock); // 0x000423D0
        fields_of(bytes).timer = timer;
        if (timer == 0.f && runtime_scare_figure_triggered(context)) // DAT_0032B100, 0x0006FA40
        {
            set_runtime_scare_figure_state(fields_of(bytes).fade_in == 0.f ? 2 : 1, context);
        }
        break;
    case 1:
    {
        const bool done = actor_tick_timer(actor, timer, context.clock.freeze, context.clock.dt,
            context.clock.scale); // 0x00042360, EAX timer, ECX actor
        fields_of(bytes).timer = timer;
        if (done)
        {
            set_runtime_scare_figure_state(2, context);
            break;
        }
        const float duration = fields_of(bytes).fade_in;
        const float limit = float(fields_of(bytes).alpha_limit);
        float alpha = limit;
        if (duration != 0.f)
        {
            alpha = clamp_alpha((timer - duration) / (0.f - duration) * limit, limit, 0.f);
        }
        fields_of(bytes).alpha = i32(alpha);
        break;
    }
    case 2:
        if (!actor_animation_completed(context.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            break;
        }
        if (fields_of(bytes).fade_out != 0.f)
        {
            set_runtime_scare_figure_state(3, context);
            break;
        }
        if (fields_of(bytes).stays == 0)
        {
            activate_actor_bind_tree(actor); // 0x00112CC0, ESI this
            break;
        }
        finish_scare(context);
        break;
    case 3:
        if (!runtime_scare_blocker_present(context)) // 0x0006FC70
        {
            fields_of(bytes).timer = fields_of(bytes).fade_out;
            fields_of(bytes).state = 4;
        }
        break;
    case 4:
    {
        const bool done = actor_tick_timer(actor, timer, context.clock.freeze, context.clock.dt,
            context.clock.scale); // 0x00042360, EAX timer, ECX actor
        fields_of(bytes).timer = timer;
        if (done)
        {
            if (fields_of(bytes).state == 0)
            {
                announce_scare(context);
            }
            finish_scare(context);
            break;
        }
        const float duration = fields_of(bytes).fade_out;
        const float limit = float(fields_of(bytes).alpha_limit);
        float alpha = 0.f;
        if (duration != 0.f)
        {
            alpha = clamp_alpha((timer - duration) / (0.f - duration) * (0.f - limit) + limit, 0.f, limit);
        }
        fields_of(bytes).alpha = i32(alpha);
        break;
    }
    case 5:
        if (fields_of(bytes).stays != 0)
        {
            notify_runtime_scare_watcher(context); // 0x0013D510
        }
        break;
    default:
        break;
    }
    // Facing: states below +0x964 turn toward the player, +0xB80 added (0x00100DA0 wraps it to 2pi).
    if (!(fields_of(bytes).facing < 0.f) && fields_of(bytes).state < fields_of(bytes).facing_states
        && context.player() != nullptr)
    {
        const Vec3 point = actor_world_position(*context.player()); // 0x000412A0
        const float yaw = runtime_actor_direction_to_point(actor, point) + fields_of(bytes).facing; // 0x000419D0
        const Vec3 euler{0.f, float(animation_remainder(yaw, std::bit_cast<float>(0x40C90FDBu))), 0.f};
        normalize_euler_angles(actor.euler, euler); // 0x00105AA0, EDI actor +0x14
        actor.xform_valid = 0; // +0x2A8.
    }
}

// Xbox 0x0006F5C0
void dispatch_runtime_scare_figure_message(RuntimeScareFigure& context, std::span<u32> message)
{
    context.base_message(context.base, message); // 0x0003EF70
    if (message[0] != 0x23)
    {
        return;
    }
    if (context.actor.render_registration == nullptr)
    {
        throw Error("scare figure activation has no actor +0x908 view");
    }
    if (fields_of(context.bytes).stays == 0)
    {
        unregister_actor_render_bins(*context.actor.render_registration); // 0x0003CBB0
    }
    unregister_actor_segment_probes(*context.actor.render_registration); // 0x0003E850, the tail jump
}

// Xbox 0x0006F600
u32 draw_runtime_scare_figure(RuntimeScareFigure& context, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& draw, RuntimeGlowPass& glow)
{
    switch (fields_of(context.bytes).state)
    {
    case 0:
    case 5:
        return draw_runtime_actor(state, kind, attachments, draw); // 0x0003C9E0
    case 1:
    case 2:
    case 3:
    case 4:
    {
        if (kind != 2)
        {
            return 0;
        }
        // 0x0003DDC0: the actor's cylinder (0x0003DD50) on screen (0x0013BCC0).
        Vec3 position;
        float height = 0.f;
        float radius = 0.f;
        runtime_actor_cylinder_metrics(context.actor, position, height, radius);
        std::array<float, 2> minimum{};
        std::array<float, 2> maximum{};
        if (!runtime_screen_cylinder_rect(position, radius, height, minimum.data(), maximum.data(),
            *entity_detail::g_display_view))
        {
            return 0;
        }
        const i32 alpha = fields_of(context.bytes).alpha;
        if (alpha != 0xFF)
        {
            begin_runtime_glow_capture(minimum.data(), maximum.data(), glow); // 0x00062220
        }
        draw_runtime_actor(state, 1, attachments, draw); // 0x0003C9E0
        draw_runtime_actor(state, 2, attachments, draw); // 0x0003C9E0's pass-2 path, inlined
        if (alpha != 0xFF)
        {
            stamp_runtime_glow((u32(alpha) << 24) | 0x00FFFFFFu, 1, glow); // 0x00062490, EAX 1
        }
        return 1;
    }
    default:
        return 0;
    }
}

// Xbox 0x0006F7C0, EAX actor
void set_runtime_scare_figure_state(i32 state, RuntimeScareFigure& context)
{
    auto bytes = context.bytes;
    if (fields_of(bytes).state == 0)
    {
        announce_scare(context);
    }
    const bool appears = read_u32(bytes, offsetof(ScareFigureState, appear_animation)) == 0x5F646961; // "aid_".
    switch (state)
    {
    case 0:
        fields_of(bytes).timer = fields_of(bytes).delay;
        fields_of(bytes).alpha = fields_of(bytes).alpha_limit;
        play_runtime_scare_figure_animation(nullptr, fields_of(context.bytes).animation.data(), true, false, context);
        break;
    case 1:
        if (appears)
        {
            play_runtime_scare_figure_animation(fields_of(context.bytes).appear_events.data(),
                fields_of(context.bytes).appear_animation.data(), false, true, context);
        }
        else
        {
            play_runtime_scare_figure_animation(nullptr, fields_of(context.bytes).animation.data(),
                fields_of(bytes).fade_in_plays == 0, true, context);
        }
        if (fields_of(bytes).stays == 0)
        {
            register_actor_render_bins(*context.actor.render_registration); // 0x0003CAE0
        }
        if (fields_of(bytes).alpha_limit != 0xFF)
        {
            fields_of(bytes).alpha = 0;
        }
        fields_of(bytes).timer = fields_of(bytes).fade_in;
        tell_players(0, context); // 0x00122D10
        break;
    case 2:
        play_runtime_scare_figure_animation(fields_of(context.bytes).play_events.data(), fields_of(context.bytes).animation.data(), false,
            false, context);
        register_actor_render_bins(*context.actor.render_registration); // 0x0003CAE0
        fields_of(bytes).state = state;
        fields_of(bytes).alpha = fields_of(bytes).alpha_limit;
        return;
    case 3:
        if (appears)
        {
            play_runtime_scare_figure_animation(fields_of(context.bytes).appear_events.data(),
                fields_of(context.bytes).appear_animation.data(), false, true, context);
        }
        else
        {
            play_runtime_scare_figure_animation(nullptr, fields_of(context.bytes).animation.data(),
                fields_of(bytes).fade_in_plays == 0, true, context);
        }
        break;
    case 4:
        fields_of(bytes).state = state;
        fields_of(bytes).timer = fields_of(bytes).fade_out;
        return;
    case 5:
        tell_players(1, context); // 0x00122D10
        if (fields_of(bytes).stays != 0)
        {
            play_runtime_scare_figure_animation(nullptr, fields_of(context.bytes).animation.data(), true, false, context);
            break;
        }
        activate_actor_bind_tree(context.actor); // 0x00112CC0
        break;
    default:
        break;
    }
    fields_of(bytes).state = state;
}

// Xbox 0x0006FA40, EBX actor
bool runtime_scare_figure_triggered(RuntimeScareFigure& context)
{
    if (runtime_invisibility_active() != 0 || (context.runtime_flags & 8) != 0 || !actor_loading_complete()) // 0x001412F0
    {
        return false;
    }
    const float reach = fields_of(context.bytes).reach;
    if (reach <= 0.f)
    {
        return true;
    }
    const Actor* player = context.player();
    if (player == nullptr)
    {
        return false;
    }
    const Vec3 position = actor_world_position(*player); // 0x000412A0
    const Vec3 trigger = fields_of(context.bytes).trigger;
    const float dz = trigger.z - position.z;
    const float dx = trigger.x - position.x;
    const float dy = trigger.y - position.y;
    return dz * dz + dx * dx + dy * dy <= reach * reach;
}

// Xbox 0x0006FB10, EAX events
void play_runtime_scare_figure_animation(const char* events, const char* name, bool hold, bool continuous,
    RuntimeScareFigure& context)
{
    AnimationMix mix{}; // 'PxiM': no fades, DAT_0032B1FC, 0x21, DAT_0032B1AC, 0x31, DAT_0032B1AC.
    if (events != nullptr)
    {
        mix.events = events;
    }
    mix.mode = continuous ? 0 : 1;
    AnimationController& controller = *ensure_actor_animation_controller(context.animation); // 0x0003ACB0 inlined
    start_controller_animation(controller, 0, {}, name, mix, context.registry, context.error,
        context.random); // 0x00030880
    auto bytes = context.bytes;
    const auto hold_slot = [&controller](bool held)
    {
        if (!controller.slots.empty() && controller.slots[0].player != nullptr) // Player +0x125.
        {
            if (held)
            {
                ++controller.slots[0].player->paused;
            }
            else
            {
                --controller.slots[0].player->paused;
            }
        }
        controller.dirty = true; // +0x5C.
    };
    if (hold)
    {
        if (fields_of(bytes).paused == 0)
        {
            hold_slot(true);
            fields_of(bytes).paused = 1u;
        }
        return;
    }
    if (fields_of(bytes).paused != 0)
    {
        hold_slot(false);
        fields_of(bytes).paused = 0u;
    }
}

// Xbox 0x0006FC70
bool runtime_scare_blocker_present(RuntimeScareFigure& context)
{
    return context.first_tagged(0x67) != nullptr;
}

// Xbox 0x0013D510
void notify_runtime_scare_watcher(RuntimeScareFigure& context)
{
    Actor* watcher = context.first_tagged(6);
    if (watcher == nullptr)
    {
        return;
    }
    Vec3 position;
    float height = 0.f;
    float radius = 0.f;
    runtime_actor_cylinder_metrics(context.actor, position, height, radius); // 0x0003DD50
    std::array<u32, 0x42> message{};
    message[0] = 0x63;
    message[1] = 0x00012004u; // Shape bytes 4, 0x20, 1.
    message[2] = std::bit_cast<u32>(position.x);
    message[3] = std::bit_cast<u32>(position.y);
    message[4] = std::bit_cast<u32>(position.z);
    message[6] = std::bit_cast<u32>(height);
    message[8] = std::bit_cast<u32>(radius);
    context.message(*watcher, message); // Vtable +0xC.
}
}
