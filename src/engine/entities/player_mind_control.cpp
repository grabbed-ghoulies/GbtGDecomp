#include "player_mind_control.h"
#include "gbtg/anim.h"
#include "actor_live_attachments.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void decay_tracking_float(std::span<u8> bytes, size_t offset, float step)
{
    const float previous = read_f32(bytes, offset);
    if (previous > 0.f)
    {
        const float value = previous - step;
        put_float(bytes, offset, value);
        if (!(0.f < value) && !std::isnan(value))
        {
            put_float(bytes, offset, 0.f);
        }
    }
}
}

// Xbox 0x0007D5A0
void update_runtime_player_input_gesture(std::span<u8> tracking, float magnitude, float step)
{
    switch (read_u32(tracking, 0x1C))
    {
    case 0:
        if (magnitude > std::bit_cast<float>(0x3DE147AEu))
        {
            put_word(tracking, 0x1C, 1);
            put_float(tracking, 0x18, read_f32(tracking, 0x18) + step);
            put_float(tracking, 0x14, magnitude);
            return;
        }
        break;
    case 1:
        if (!(magnitude >= read_f32(tracking, 0x14) - 0.05f))
        {
            put_word(tracking, 0x1C, 2);
            put_float(tracking, 0x14, magnitude);
            return;
        }
        goto accumulate;
    case 2:
        if (0.1f > magnitude)
        {
            goto finish;
        }
accumulate:
        {
            const float value = read_f32(tracking, 0x18) + step;
            put_float(tracking, 0x18, value);
            if (!(value < 0.2f) && !std::isnan(value))
            {
                put_word(tracking, 0x1C, 3);
                put_float(tracking, 0x14, magnitude);
                return;
            }
        }
        break;
    case 3:
        if (!(0.1f > magnitude))
        {
            break;
        }
finish:
        if (0.1f > read_f32(tracking, 0x14))
        {
            put_float(tracking, 0x14, 0.f);
            put_word(tracking, 0x1C, 0);
            put_float(tracking, 0x18, 0.f);
            put_float(tracking, 0x14, magnitude);
            return;
        }
        {
            const u32 previous = read_u32(tracking, 4);
            put_word(tracking, 4, 2);
            if (previous == 0)
            {
                put_float(tracking, 8, 0.f);
                put_word(tracking, 0, 0);
            }
            put_word(tracking, 0, 1);
            put_float(tracking, 8, 0.2f);
            put_float(tracking, 0xC, 0.f);
        }
        break;
    default:
        break;
    }
    put_float(tracking, 0x14, magnitude);
}

// Xbox 0x0007D460
void update_runtime_player_input_direction(std::span<u8> tracking,
    const std::array<float, 2>& axes, float step, u32 special)
{
    const float magnitude = float(retail_square_root(axes[0] * axes[0] + axes[1] * axes[1]));
    if (read_u32(tracking, 4) == 1)
    {
        if (!(magnitude < std::bit_cast<float>(0x3DE147AEu)) && !std::isnan(magnitude))
        {
            put_word(tracking, 4, 2);
            put_word(tracking, 0, 1);
            put_float(tracking, 8, 0.2f);
            put_float(tracking, 0xC, 0.f);
        }
    }
    else if (read_u32(tracking, 4) == 2 && 0.1f > magnitude)
    {
        put_word(tracking, 4, 1);
        put_word(tracking, 0, 0);
    }
    update_runtime_player_input_gesture(tracking, special != 0 ? magnitude : 0.f, step);
    if (magnitude != 0.f)
    {
        float direction = 0.f;
        if (axes[0] != 0.f || axes[1] != 0.f)
        {
            direction = float(retail_atan2(axes[1], axes[0]));
        }
        put_float(tracking, 0x10, direction);
    }
    decay_tracking_float(tracking, 0xC, step);
    decay_tracking_float(tracking, 8, step);
}

// Xbox 0x00109810
void update_runtime_player_input_tracking(std::span<u8> tracking,
    const std::array<float, 2>& axes, std::span<const u8> preset,
    u32 accumulate, const RuntimeActorClock& clock)
{
    const float dx = read_f32(tracking, 0) - axes[0];
    const float dy = read_f32(tracking, 4) - axes[1];
    std::memcpy(tracking.data(), axes.data(), sizeof(axes));
    const long double distance = retail_square_root(dy * dy + dx * dx);
    if (!(distance > static_cast<long double>(read_f32(preset, 0))))
    {
        decay_tracking_float(tracking, 8, read_f32(preset, 8));
        decay_tracking_float(tracking, 0xC, read_f32(preset, 8));
    }
    else
    {
        if (1.f > read_f32(tracking, 8))
        {
            const float value = read_f32(preset, 4) + read_f32(tracking, 8);
            put_float(tracking, 8, value);
            if (!(value < 1.f) && !std::isnan(value))
            {
                put_float(tracking, 8, 1.f);
            }
        }
        if (accumulate != 0 && read_f32(preset, 0xC) > read_f32(tracking, 0xC))
        {
            const float value = read_f32(preset, 4) + read_f32(tracking, 0xC);
            put_float(tracking, 0xC, value);
            if (!(value < read_f32(preset, 0xC)) && !std::isnan(value)
                && !std::isnan(read_f32(preset, 0xC)))
            {
                put_float(tracking, 0xC, read_f32(preset, 0xC));
            }
        }
    }
    const u32 state = read_u32(tracking, 0x14);
    if ((state == 1 && read_f32(tracking, 8) == 1.f)
        || (state == 2 && read_f32(tracking, 8) == 0.f))
    {
        decay_tracking_float(tracking, 0x10, runtime_frame_step(clock));
        if (read_f32(tracking, 0x10) != 0.f)
        {
            return;
        }
        const u32 next = u32(read_u32(tracking, 0x14) != 2) + 1;
        std::memcpy(tracking.data() + 0x14, &next, 4);
        return;
    }
    put_float(tracking, 0x10, std::bit_cast<float>(0x3D89374Cu));
}

// Xbox 0x00137840
bool update_runtime_player_boost_timer(RuntimePlayerMindBoost& boost,
    RuntimePlayerMindControl& control, const std::span<const u8>* preset, float multiplier)
{
    const size_t group = (read_u16(boost.actor_bytes, 0xCDC) == 2
        || read_u32(boost.actor_bytes, 0x93C) != 0) ? 1 : 0;
    if (boost.resume.references[group] != 0
        || boost.resume.decay_enabled[boost.resume.decay_index] == 0)
    {
        return read_f32(boost.boost, 4) > 0.f;
    }
    float step = runtime_actor_rate(boost.actor) * runtime_frame_step(control.clock);
    if (read_f32(boost.boost, 4) > 0.f && preset != nullptr)
    {
        std::array<float, 2> axes;
        runtime_input_axes(control.input, axes, control.input_state);
        update_runtime_player_input_tracking(boost.boost.subspan(8), axes, *preset, 1, control.clock);
        runtime_input_secondary_axes(control.input, axes, control.input_state);
        update_runtime_player_input_tracking(boost.boost.subspan(0x20), axes, *preset, 1, control.clock);
        if (read_u32(boost.boost, 0x1C) == 2 || read_u32(boost.boost, 0x34) == 2)
        {
            step = step * multiplier;
        }
    }
    const float timer = read_f32(boost.boost, 4);
    if (timer > 0.f)
    {
        bool expired = false;
        if (timer > 0.f)
        {
            const float remaining = timer - step;
            put_float(boost.boost, 4, remaining);
            if (!(0.f < remaining) && !std::isnan(remaining))
            {
                put_float(boost.boost, 4, 0.f);
                expired = true;
            }
        }
        if (expired || (control.flags.bytes[4] & 0x10) != 0)
        {
            put_float(boost.boost, 4, 0.f);
            auto body = boost.attachment.body;
            if (read_u16(body, 0x94) != 0)
            {
                control.flags.bytes[5] &= 0xEF;
                --body[0x309];
                --body[0x30A];
                --body[0x30B];
                --body[0x30C];
                if (runtime_body_timer_is_idle(body))
                {
                    const u16 key = control.primary_action_key();
                    select_runtime_body_action(body, key, boost.attachment.actions);
                }
                const u32 zero = 0;
                std::memcpy(boost.boost.data(), &zero, 4);
                const std::array<i32, 2> message{0x78, i32(boost.actor_address)};
                broadcast_actor_pulse_message(message.data());
            }
        }
    }
    return read_f32(boost.boost, 4) > 0.f;
}

// Xbox 0x00140AE0
void rotate_runtime_player_input_axes(std::array<float, 2>& axes, const Vec3& camera_direction)
{
    const long double magnitude = retail_square_root(axes[0] * axes[0] + axes[1] * axes[1]);
    float length = float(magnitude);
    if (magnitude != 0.L)
    {
        if (length > 1.f)
        {
            length = 1.f;
        }
        float direction;
        if (axes[0] == 0.f && axes[1] == 0.f)
        {
            direction = 0.f;
        }
        else
        {
            direction = float(retail_atan2(axes[1], axes[0]));
        }
        float camera_angle;
        runtime_direction_angle(camera_direction, camera_angle);
        const float angle = float((static_cast<long double>(camera_angle)
            - std::bit_cast<float>(0x3FC90FDBu)) + static_cast<long double>(direction));
        const float vertical = float(retail_sin(angle) * static_cast<long double>(length));
        const float horizontal = float(retail_cos(angle) * static_cast<long double>(length));
        axes[1] = vertical;
        axes[0] = horizontal;
    }
}

// Xbox 0x0007EE40
void transform_runtime_player_mind_input(std::span<const u8> body, u32 invert,
    const RuntimeDialogueInputDevice& input, const RuntimeDialogueInputState& state,
    RuntimeInputAxisWriteDevice& writable, std::span<u8> keyboard, const Vec3& camera_direction)
{
    std::array<float, 2> axes;
    if (invert != 0)
    {
        runtime_input_axes(input, axes, state);
        axes[0] = 0.f - axes[0];
        axes[1] = 0.f - axes[1];
        set_runtime_input_axes(writable, axes, keyboard);
        runtime_input_secondary_axes(input, axes, state);
        axes[0] = 0.f - axes[0];
        axes[1] = 0.f - axes[1];
        set_runtime_input_secondary_axes(writable, axes, keyboard);
    }
    if (body[0x38] == 1)
    {
        runtime_input_axes(input, axes, state);
        rotate_runtime_player_input_axes(axes, camera_direction);
        set_runtime_input_axes(writable, axes, keyboard);
    }
    if (body[0x39] == 1)
    {
        runtime_input_secondary_axes(input, axes, state);
        rotate_runtime_player_input_axes(axes, camera_direction);
        set_runtime_input_secondary_axes(writable, axes, keyboard);
    }
}

// Xbox 0x0007D280
void update_runtime_player_mind_hold_timer(RuntimePlayerMindControl& context)
{
    if ((context.flags.bytes[6] & 2) == 0)
    {
        const u32 zero = 0;
        std::memcpy(context.mind.data() + 0x130, &zero, 4);
    }
    else
    {
        const float timer = read_f32(context.mind, 0x130) + runtime_frame_step(context.clock);
        std::memcpy(context.mind.data() + 0x130, &timer, 4);
        if (!(timer < 3.f) && !std::isnan(timer))
        {
            const u16 key = context.primary_action_key();
            select_runtime_body_action(context.attachment.body, key, context.attachment.actions);
            return;
        }
    }
}

// Xbox 0x0007D300
void release_runtime_player_mind_input_attachment(RuntimePlayerMindControl& context)
{
    if (read_u32(context.input.record, 4) != 0 && context.attachment.actions.attached != nullptr)
    {
        if (runtime_attachment_available(context.attachment.actions.attached->bytes, &context.attachment.host))
        {
            if (runtime_input_button_edge(context.input, 0x200, context.input_state) != 0
                && context.attachment.body[0x6A] == 0)
            {
                if (release_runtime_player_attachment(0, 0, context.attachment))
                {
                    const u16 key = context.primary_action_key();
                    select_runtime_body_action(context.attachment.body, key, context.attachment.actions);
                }
            }
        }
    }
}
}
