#include "body_walk_alert_tick.h"
#include "body_state_actions.h"
#include "body_message_helpers.h"
#include "../audio/body_sound_commands.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "body_state_fields.h"

#include <cstring>

namespace gbtg
{
namespace walk = walk_alert_field;
namespace fly = fly_state_field;
namespace row = walk_alert_row_field;
namespace base = body_state_field;

namespace
{
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The inlined surface-mode scale: DAT_00217EC0[mode] when body +0x878 has the mode's bit, otherwise 1.
float surface_mode_scale(std::span<const u8> body, std::span<const float> mode_scales)
{
    const u8 mode = get(body, body_field::surface_override_frames) == 0 ? get(body, body_field::surface)
                                                                        : get(body, body_field::surface_override);
    if ((get(body, body_field::scaled_surfaces) & (1u << (mode & 0x1F))) != 0)
    {
        if (mode >= mode_scales.size())
        {
            throw Error("surface mode lies outside the DAT_00217EC0 view");
        }
        return mode_scales[mode];
    }
    return 1.f; // DAT_0032B0F8.
}
}

// Xbox 0x000258D0
float runtime_body_turn_rate(std::span<const u8> body, const Actor& actor, const RuntimeBodyDamageTables& tables,
    std::span<const float> mode_scales)
{
    // The inline class row 2 lookup (DAT_00433CB4 bounds, statsheet column 2).
    return runtime_class_damage_value(2, actor.damage_class, tables) * surface_mode_scale(body, mode_scales);
}

// Xbox 0x00133D50
float runtime_body_turn_ratio(std::span<const u8> body, const Actor& actor, u32 fixed,
    const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales)
{
    const float turn = runtime_body_turn_rate(body, actor, tables, mode_scales);
    const float scale = surface_mode_scale(body, mode_scales);
    const float stride = runtime_body_step_speed(body, actor, tables) * scale;
    if (fixed != 0)
    {
        return 1.f / surface_mode_scale(body, mode_scales);
    }
    return turn / stride / surface_mode_scale(body, mode_scales);
}

// Xbox 0x00133C90
bool runtime_body_stride_duration(float speed, const Actor& actor, const ActorAnimationState& animation,
    float& duration)
{
    if (speed == 0.f) // DAT_0032B100.
    {
        return false;
    }
    const AnimationController* controller = animation.controller.get();
    if (controller == nullptr)
    {
        return false;
    }
    const MinaPlayer* player = controller->slots.at(0).player.get();
    if (player == nullptr)
    {
        return false;
    }
    // Player +8 is the root track 0x00031A30 sets when mina +0x1A is nonzero.
    if (player->mina.empty() || read_u16(player->mina, 0x1A) == 0)
    {
        return false;
    }
    // Player +0x108: the z of the root 0x0002D8E0 samples at position 1 into +0xF0.
    const float stride = animation_absolute(actor.scale_200 * actor.radius_xz
        * player->reference_root.translation.z);
    float value = stride / speed * actor.radius_xz;
    duration = value;
    if (0.1f > value) // DAT_0032B254.
    {
        value = 0.1f;
    }
    else if (value > 50.f) // DAT_0032B1E0.
    {
        value = 50.f;
    }
    duration = value;
    return true;
}

// Xbox 0x000A5FD0
i32 runtime_walk_alert_band(std::span<const u8> state, std::span<const u8> body, const Actor& actor,
    float speed, const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales)
{
    const i32 count = get(state, walk::row_count);
    for (i32 index = 0; index < count; ++index)
    {
        const float factor = runtime_class_damage_value(body[0x328], actor.damage_class, tables);
        const float scale = surface_mode_scale(body, mode_scales);
        if (speed >= 0.f
            && read_f32(body, 0x32C) * get(record(state, walk::rows, size_t(index)), row::top_speed) * factor * scale
                > speed)
        {
            return index;
        }
    }
    return count - 1;
}

// Xbox 0x000A5C70
void retime_runtime_walk_alert(std::span<const u8> state, float speed, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    float duration;
    if (!runtime_body_stride_duration(speed, context.actor, context.actions.animation, duration))
    {
        const size_t band = get(state, walk::band);
        // The band starts at the previous band's top speed.
        const float lower_limit = band == 0 ? 0.f : get(record(state, walk::rows, band - 1), row::top_speed);
        const auto current = record(state, walk::rows, band);
        const float upper_limit = get(current, row::top_speed);
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float base = runtime_body_step_speed(body, context.actor, context.tables) * scale;
        const float lower_rate = get(current, row::lower_duration);
        const float upper_rate = get(current, row::upper_duration);
        const float upper = base * upper_limit;
        const float lower = base * lower_limit;
        if (upper == lower)
        {
            duration = upper_rate;
        }
        else
        {
            const float value = (speed - lower) / (upper - lower) * (upper_rate - lower_rate) + lower_rate;
            if (upper_rate > lower_rate)
            {
                if (value > upper_rate)
                {
                    duration = upper_rate;
                }
                else if (lower_rate > value)
                {
                    duration = lower_rate;
                }
                else
                {
                    duration = value;
                }
            }
            else if (upper_rate > value)
            {
                duration = upper_rate;
            }
            else if (value > lower_rate)
            {
                duration = lower_rate;
            }
            else
            {
                duration = value;
            }
        }
        duration = runtime_body_turn_ratio(body, context.actor, get(state, walk::fixed_turn_ratio), context.tables,
            context.mode_scales) * duration;
    }
    if (context.actions.animation.controller != nullptr)
    {
        // EAX 0: controller slot 0.
        auto& controller = *context.actions.animation.controller;
        retime_mina_playback(controller.slots.at(0).player.get(), duration, controller.dirty);
    }
}

// Xbox 0x000A5DC0
void select_runtime_walk_alert_band(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = get(body, body_field::velocity).x;
    const float vz = get(body, body_field::velocity).z;
    const float speed = float(retail_square_root(x * x + z * z));
    static_cast<void>(retail_square_root(vz * vz + vx * vx)); // Popped unused.
    const i32 band = runtime_walk_alert_band(state, body, context.actor, speed, context.tables,
        context.mode_scales);
    if (u32(band) != get(state, walk::band))
    {
        context.actions.animation.retain_position = 1; // Actor +0x84.
        const AnimationRecord* record = runtime_walk_alert_record(state, u16(band));
        if (record == nullptr)
        {
            throw Error("walk-alert band record is null; 0x0001AFA0 reads its +0x117");
        }
        start_runtime_body_animation_record(*record, context.actions);
        set(state, walk::band, u16(band));
    }
    retime_runtime_walk_alert(state, speed, context);
}

// Xbox 0x000A5A60
void steer_runtime_walk_alert(std::span<const u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{};
    if (context.input == nullptr || get(body, body_field::move_holds) != 0 ||
        get(body, body_field::move_hold_time) != 0.f)
    {
        axes = {0.f, 0.f};
    }
    else
    {
        runtime_input_axes(*context.input, axes, context.input_state);
    }
    const long double root = retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]);
    float magnitude = float(root);
    if (!(1.0L > root)) // DAT_0032B0F8.
    {
        magnitude = 1.f;
    }
    float angle;
    if (axes[0] == 0.f && axes[1] == 0.f)
    {
        angle = 0.f;
    }
    else
    {
        angle = float(retail_atan2(axes[1], axes[0]));
    }
    const float limit = get(state, walk::stick_limit);
    if (magnitude > limit)
    {
        magnitude = limit;
    }
    float forward = 0.f;
    float side = 0.f;
    if (magnitude > 0.f)
    {
        set(body, body_field::euler, Vec3{0.f, angle, 0.f});
        float heading = angle;
        if (get(state, walk::settles) != 0)
        {
            context.orient();
        }
        if (get(state, walk::holds_heading) != 0 && magnitude > get(state, walk::hold_heading_above))
        {
            heading = context.actor.euler.y; // Actor +0x18.
        }
        put_float(body, 0x33C, heading);
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float speed = runtime_body_step_speed(body, context.actor, context.tables) * scale * magnitude;
        side = float(retail_sin(heading) * speed);
        forward = float(retail_cos(heading) * speed);
    }
    set(body, body_field::velocity, Vec3{side, 0.f, forward});
}

// Xbox 0x000A5EA0
void select_runtime_walk_alert_key(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = get(body, body_field::velocity).x;
    const float vz = get(body, body_field::velocity).z;
    if ((context.flags.bytes[7] & 8) == 0 && get(state, walk::alert_key) != 0)
    {
        const bool alert = context.mind_present && context.mind_alert != 0;
        if (get(state, walk::alert_when_calm) != 0 ? !alert : alert)
        {
            context.actions.animation.retain_position = 1; // Actor +0x84.
            set(state, base::next_key, get(state, walk::alert_key));
            return;
        }
    }
    const u16 key = get(state, walk::stop_key);
    if (key == 0)
    {
        return;
    }
    const float actor_speed = z * z + x * x;
    const float desired = vz * vz + vx * vx;
    const float scale = surface_mode_scale(body, context.mode_scales);
    const float threshold = runtime_body_step_speed(body, context.actor, context.tables) * scale * 0.01f;
    if (desired == 0.f) // DAT_0032B100; DAT_0032B124 above.
    {
        if (threshold * threshold >= actor_speed || get(state, walk::stops_at_once) != 0)
        {
            set(state, base::next_key, key);
        }
    }
}

// Xbox 0x000A59C0
void tick_runtime_walk_alert(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)
        + get(state, base::elapsed);
    set(state, base::elapsed, elapsed);
    steer_runtime_walk_alert(state, context);
    select_runtime_walk_alert_band(state, context);
    select_runtime_walk_alert_key(state, context);
}

// Xbox 0x00025870, EAX body
float runtime_body_scaled_step_speed(std::span<const u8> body, const Actor& actor,
    const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales)
{
    const float scale = surface_mode_scale(body, mode_scales);
    return runtime_body_step_speed(body, actor, tables) * scale; // 0x000259C0
}

// Xbox 0x00091210
void steer_runtime_fly_state(std::span<const u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{};
    if (context.input == nullptr || get(body, body_field::move_holds) != 0 ||
        get(body, body_field::move_hold_time) != 0.f)
    {
        axes = {0.f, 0.f};
    }
    else
    {
        runtime_input_axes(*context.input, axes, context.input_state); // 0x001085F0
    }
    const long double root = retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]);
    float magnitude = float(root);
    if (!(1.0L > root)) // DAT_0032B0F8.
    {
        magnitude = 1.f;
    }
    float angle;
    if (axes[0] == 0.f && axes[1] == 0.f)
    {
        angle = 0.f;
    }
    else
    {
        angle = float(retail_atan2(axes[1], axes[0])); // 0x001530E0
    }
    float rise;
    if (get(body, body_field::move_holds) != 0 || get(body, body_field::move_hold_time) != 0.f)
    {
        rise = 0.f;
    }
    else
    {
        if (context.input == nullptr)
        {
            throw Error("0x00091210 reads the third stick pair of input address zero");
        }
        std::array<float, 2> third{};
        runtime_input_tertiary_axes(*context.input, third, context.input_state); // 0x00108AB0, EAX input
        rise = third[1];
    }
    const float limit = get(state, fly::stick_limit);
    if (magnitude > limit)
    {
        magnitude = limit;
    }
    float side = 0.f;
    float forward = 0.f;
    if (magnitude > 0.f)
    {
        set(body, body_field::euler, Vec3{0.f, angle, 0.f});
        float heading = angle;
        if (get(state, fly::holds_heading) != 0 && magnitude > get(state, fly::hold_heading_above))
        {
            heading = context.actor.euler.y; // Actor +0x18.
        }
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float speed = runtime_body_step_speed(body, context.actor, context.tables) * scale * magnitude; // 0x000259C0
        side = float(retail_sin(heading) * speed);
        forward = float(retail_cos(heading) * speed);
    }
    const float scale = surface_mode_scale(body, context.mode_scales);
    const float lift = runtime_body_step_speed(body, context.actor, context.tables) * scale * rise; // 0x000259C0
    set(body, body_field::velocity, Vec3{side, lift, forward});
}

// Xbox 0x00091480, EAX body, EBX state
void retime_runtime_fly_state(std::span<const u8> state, float speed, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    float duration;
    if (!runtime_body_stride_duration(speed, context.actor, context.actions.animation, duration)) // 0x00133C90
    {
        const size_t band = get(state, fly::band);
        // The band starts at the previous band's top speed.
        const float lower_limit = band == 0 ? 0.f : get(record(state, fly::rows, band - 1), row::top_speed);
        const auto current = record(state, fly::rows, band);
        const float upper_limit = get(current, row::top_speed);
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float base = runtime_body_step_speed(body, context.actor, context.tables) * scale;
        const float lower_rate = get(current, row::lower_duration);
        const float upper_rate = get(current, row::upper_duration);
        const float upper = base * upper_limit;
        const float lower = base * lower_limit;
        if (upper == lower)
        {
            duration = upper_rate;
        }
        else
        {
            const float value = (speed - lower) / (upper - lower) * (upper_rate - lower_rate) + lower_rate;
            if (upper_rate > lower_rate)
            {
                if (value > upper_rate)
                {
                    duration = upper_rate;
                }
                else if (lower_rate > value)
                {
                    duration = lower_rate;
                }
                else
                {
                    duration = value;
                }
            }
            else if (upper_rate > value)
            {
                duration = upper_rate;
            }
            else if (value > lower_rate)
            {
                duration = lower_rate;
            }
            else
            {
                duration = value;
            }
        }
        duration = runtime_body_turn_ratio(body, context.actor, 0, context.tables, context.mode_scales)
            * duration; // 0x00133D50
    }
    if (context.actions.animation.controller != nullptr)
    {
        // EAX 0: controller slot 0 (0x00030DB0).
        auto& controller = *context.actions.animation.controller;
        retime_mina_playback(controller.slots.at(0).player.get(), duration, controller.dirty);
    }
}

// Xbox 0x000915D0
void select_runtime_fly_band(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = get(body, body_field::velocity).x;
    const float vz = get(body, body_field::velocity).z;
    const float speed = float(retail_square_root(x * x + z * z)); // 0x00152B30
    const float steered = float(retail_square_root(vz * vz + vx * vx));
    const i32 steered_band = runtime_walk_alert_band(state, body, context.actor, steered, context.tables,
        context.mode_scales); // 0x000A5FD0
    const i32 band = runtime_walk_alert_band(state, body, context.actor, speed, context.tables,
        context.mode_scales); // 0x000A5FD0
    if (band == steered_band && u32(steered_band) != get(state, fly::band))
    {
        context.actions.animation.retain_position = 1; // Actor +0x84.
        const AnimationRecord* record = runtime_walk_alert_record(state, u16(steered_band));
        if (record == nullptr)
        {
            throw Error("fly band record is null; 0x0001AFA0 reads its +0x117");
        }
        start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0
        set(state, fly::band, u16(steered_band));
    }
    retime_runtime_fly_state(state, speed, context); // 0x00091480
}

// Xbox 0x000916C0, ESI state
void select_runtime_fly_key(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = get(body, body_field::velocity).x;
    const float vy = get(body, body_field::velocity).y;
    const float vz = get(body, body_field::velocity).z;
    if (get(state, fly::alert_key) != 0 && context.mind_present && context.mind_alert != 0)
    {
        context.actions.animation.retain_position = 1; // Actor +0x84.
        set(state, base::next_key, get(state, fly::alert_key));
        return;
    }
    const u16 key = get(state, fly::stop_key);
    if (key == 0)
    {
        return;
    }
    const float threshold = runtime_body_scaled_step_speed(body, context.actor, context.tables,
        context.mode_scales) * 0.01f; // 0x00025870, DAT_0032B124.
    if (vz * vz + vy * vy + vx * vx == 0.f) // DAT_0032B100.
    {
        if (threshold * threshold >= z * z + x * x)
        {
            set(state, base::next_key, key);
        }
    }
}

// Xbox 0x00091190
void tick_runtime_fly_state(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)
        + get(state, base::elapsed);
    set(state, base::elapsed, elapsed);
    steer_runtime_fly_state(state, context); // 0x00091210
    select_runtime_fly_band(state, context); // 0x000915D0
    select_runtime_fly_key(state, context); // 0x000916C0
}

// Xbox 0x000A5A40
void message_runtime_walk_alert(std::span<const u8> message, const std::function<void()>& stop)
{
    if (read_u32(message, 0) == 0x53)
    {
        stop();
    }
}

// Xbox 0x00023890, EAX body, ECX key, ESI the message
void stop_runtime_walk_alert(std::span<u8> body, std::span<u8> message, u32 key,
    const std::function<u16()>& action_key, RuntimeBodyActionMessages& actions)
{
    const u32 asked = read_u32(message, 4);
    if (key != asked)
    {
        return;
    }
    if (get(body, body_field::energy) == 0)
    {
        const u32 none = 0;
        std::memcpy(message.data() + 8, &none, 4);
        return;
    }
    if (asked != 0)
    {
        notify_runtime_body_action_key(body, 0x117, actions); // 0x00024B20, EAX 0x117
    }
    else
    {
        notify_runtime_body_action_key(body, action_key(), actions); // body vtable +0x20, then 0x00024B20
    }
    const u32 taken = 1;
    std::memcpy(message.data() + 8, &taken, 4);
}
}
