#include "body_walk_alert_tick.h"
#include "body_state_actions.h"
#include "../audio/body_sound_commands.h"
#include "gbtg/anim.h"

#include <cstring>

namespace gbtg
{
namespace
{
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The inlined surface-mode scale: DAT_00217EC0[mode] when body +0x878 has the mode's bit, otherwise 1.
float surface_mode_scale(std::span<const u8> body, std::span<const float> mode_scales)
{
    const u8 mode = read_u16(body, 0x880) == 0 ? body[0x874] : body[0x87E];
    if ((read_u32(body, 0x878) & (1u << (mode & 0x1F))) != 0)
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
    const i32 count = i16(read_u16(state, 0x2C));
    for (i32 index = 0; index < count; ++index)
    {
        const float factor = runtime_class_damage_value(body[0x328], actor.damage_class, tables);
        const float scale = surface_mode_scale(body, mode_scales);
        if (speed >= 0.f
            && read_f32(body, 0x32C) * read_f32(state, 0x3C + size_t(index) * 0x10) * factor * scale > speed)
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
        const size_t band = read_u16(state, 0x2E);
        const float lower_limit = band == 0 ? 0.f : read_f32(state, band * 0x10 + 0x2C);
        const float upper_limit = read_f32(state, band * 0x10 + 0x3C);
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float base = runtime_body_step_speed(body, context.actor, context.tables) * scale;
        const float lower_rate = read_f32(state, band * 0x10 + 0x34);
        const float upper_rate = read_f32(state, band * 0x10 + 0x38);
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
        duration = runtime_body_turn_ratio(body, context.actor, read_u32(state, 0x80), context.tables,
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
    const float vx = read_f32(body, 0x318);
    const float vz = read_f32(body, 0x320);
    const float speed = float(retail_square_root(x * x + z * z));
    static_cast<void>(retail_square_root(vz * vz + vx * vx)); // Popped unused.
    const i32 band = runtime_walk_alert_band(state, body, context.actor, speed, context.tables,
        context.mode_scales);
    if (u32(band) != read_u16(state, 0x2E))
    {
        context.actions.animation.retain_position = 1; // Actor +0x84.
        const AnimationRecord* record = runtime_walk_alert_record(state, u16(band));
        if (record == nullptr)
        {
            throw Error("walk-alert band record is null; 0x0001AFA0 reads its +0x117");
        }
        start_runtime_body_animation_record(*record, context.actions);
        const u16 stored = u16(band);
        std::memcpy(state.data() + 0x2E, &stored, 2);
    }
    retime_runtime_walk_alert(state, speed, context);
}

// Xbox 0x000A5A60
void steer_runtime_walk_alert(std::span<const u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{};
    if (context.input == nullptr || read_u16(body, 0x30) != 0 || read_f32(body, 0x34) != 0.f)
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
    const float limit = read_f32(state, 0x28);
    if (magnitude > limit)
    {
        magnitude = limit;
    }
    float forward = 0.f;
    float side = 0.f;
    if (magnitude > 0.f)
    {
        put_float(body, 0x72C, 0.f);
        put_float(body, 0x730, angle);
        put_float(body, 0x734, 0.f);
        float heading = angle;
        if (read_u32(state, 0x20) != 0)
        {
            context.orient();
        }
        if (read_u32(state, 0x74) != 0 && magnitude > read_f32(state, 0x78))
        {
            heading = context.actor.euler.y; // Actor +0x18.
        }
        put_float(body, 0x33C, heading);
        const float scale = surface_mode_scale(body, context.mode_scales);
        const float speed = runtime_body_step_speed(body, context.actor, context.tables) * scale * magnitude;
        side = float(retail_sin(heading) * speed);
        forward = float(retail_cos(heading) * speed);
    }
    put_float(body, 0x318, side);
    put_float(body, 0x31C, 0.f);
    put_float(body, 0x320, forward);
}

// Xbox 0x000A5EA0
void select_runtime_walk_alert_key(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const auto body = context.body;
    const float x = context.actor.anchor.x;
    const float z = context.actor.anchor.z;
    const float vx = read_f32(body, 0x318);
    const float vz = read_f32(body, 0x320);
    if ((context.flags.bytes[7] & 8) == 0 && read_u16(state, 0x1C) != 0)
    {
        const bool alert = context.mind_present && context.mind_alert != 0;
        if (read_u32(state, 0x7C) != 0 ? !alert : alert)
        {
            context.actions.animation.retain_position = 1; // Actor +0x84.
            std::memcpy(state.data() + 0xC, state.data() + 0x1C, 2);
            return;
        }
    }
    const u16 key = read_u16(state, 0x1A);
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
        if (threshold * threshold >= actor_speed || read_u32(state, 0x24) != 0)
        {
            std::memcpy(state.data() + 0xC, &key, 2);
        }
    }
}

// Xbox 0x000A59C0
void tick_runtime_walk_alert(std::span<u8> state, RuntimeWalkAlertTick& context)
{
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)
        + read_f32(state, 0x10);
    put_float(state, 0x10, elapsed);
    steer_runtime_walk_alert(state, context);
    select_runtime_walk_alert_band(state, context);
    select_runtime_walk_alert_key(state, context);
}

// Xbox 0x000A5A40
void message_runtime_walk_alert(std::span<const u8> message, const std::function<void()>& stop)
{
    if (read_u32(message, 0) == 0x53)
    {
        stop();
    }
}
}
