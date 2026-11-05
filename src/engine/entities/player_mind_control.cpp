#include "player_mind_control.h"
#include <cstddef>
#include "mind_fields.h"
#include "gbtg/anim.h"
#include "actor_live_attachments.h"
#include "shape_copy.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
// Xbox 0x0007EA20
std::span<u8> refresh_runtime_player_shape_target(RuntimePlayerMindShapeTarget& context,
    float angle, float distance)
{
    auto body = context.body;
    auto mind = context.mind;
    auto* attachment = context.attachment;
    if (attachment == nullptr || (body_record(body).unk_6A != 0
        && mind_record(mind).action_record != context.mind_address + 0xB8))
    {
        return {};
    }
    if ((body_record(body).unk_6B == 0 && body_record(body).unk_6A == 0 && !(body_record(body).unk_70 > 0.f)
        && body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f
        && (context.flags.bytes[3] & 8) == 0
        && !runtime_body_bit_set(context.flags, 0x2C)
        && !runtime_body_bit_set(context.flags, 0x2A)) || (context.flags.bytes[5] & 0x80) != 0)
    {
        std::array<u8, 0x20> first, second;
        copy_actor_shape(first.data(), attachment->shape.data());
        copy_actor_shape(second.data(), attachment->secondary_shape.data());
        Vec3 position = actor_world_position(context.actor);
        const float sine = float(retail_sin(angle));
        position.x = sine * distance + position.x;
        const float cosine = float(retail_cos(angle) * distance + position.z);
        position.z = cosine;
        std::memcpy(first.data() + 4, &position, sizeof(position));
        std::memcpy(second.data() + 4, &position, sizeof(position));
        copy_actor_shape(retail_bytes(player_mind_record(mind).own_target.shape).data(), first.data());
        copy_actor_shape(retail_bytes(player_mind_record(mind).own_target.secondary_shape).data(), second.data());
        player_mind_record(mind).own_target.direction = actor_transform_origin;
        player_mind_record(mind).own_target.floor = u8(context.floor);
    }
    return retail_bytes(player_mind_record(mind).own_target);
}

// Xbox 0x0007E9E0
std::span<const u8> redirect_runtime_player_shape_target(RuntimePlayerMindShapeTarget& context,
    const RuntimePlayerAttachmentEligibility& body, std::span<const u8> candidate, float angle)
{
    if (candidate[0x14] > 0 && candidate[0x14] <= 2)
    {
        if (!runtime_player_body_point_is_near(body, candidate))
        {
            return refresh_runtime_player_shape_target(context, angle, 80.f);
        }
    }
    return candidate;
}

// Xbox 0x0007E960
bool runtime_player_mind_accepts_target(const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> target_body, const Actor& target_actor,
    std::span<const u8> candidate, const LiveScript& events, std::span<const u8> address_zero)
{
    const bool near = runtime_player_body_target_is_near(body, target_body, target_actor);
    static_cast<void>(runtime_player_body_point_is_near(body, candidate));
    if (near)
    {
        auto* node = events.next;
        while (node != nullptr && u16(node->type) != 3)
        {
            node = node->next;
        }
        const auto bytes = node == nullptr ? address_zero : static_cast<const RuntimeMindEvent*>(node)->bytes;
        return read_f32(bytes, 0x18) >= std::bit_cast<float>(0x3F266666u);
    }
    return true;
}

// Xbox 0x0002A130
void set_runtime_mind_gaze_anchor(std::span<u8> mind, u32 anchor_address, u32& random_state)
{
    mind_record(mind).gaze_anchor = anchor_address;
    mind_record(mind).unk_34 = 0.f;
    float duration = 0.f;
    if (anchor_address != 0)
    {
        random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
        const float fraction = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        duration = fraction * 0.5f + 0.5f;
    }
    mind_record(mind).unk_38 = duration;
}

// Xbox 0x00137140
void update_runtime_player_mind_gaze(RuntimePlayerMindGaze& context,
    i32 check_path, float maximum_height, float radius)
{
    auto mind = context.mind;
    auto body = context.body;
    if (body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f)
    {
        if (mind_record(mind).gaze_point_set == 0)
        {
            const Vec3 origin = runtime_body_gaze_origin(body, context.body_actor);
            if (context.anchor == nullptr || mind_record(mind).unk_34 >= mind_record(mind).unk_38)
            {
                if (mind_record(mind).target_mode == 0 || context.target == nullptr)
                {
                    context.anchor = find_nearest_actor_model_anchor(context.actor, origin,
                        check_path, maximum_height, radius);
                }
                else
                {
                    context.anchor = select_actor_model_anchor(*context.target, context.random_state);
                }
                const u32 address = context.anchor == nullptr ? 0 : context.addresses.at(context.anchor);
                set_runtime_mind_gaze_anchor(mind, address, context.random_state);
            }
            if (context.anchor != nullptr)
            {
                std::memcpy(body.data() + offsetof(BodyRecord, look_point.x), &context.anchor->output, sizeof(Vec3));
                body_record(body).look_point_set = 1;
                return;
            }
            std::array<float, 2> axes;
            runtime_input_secondary_axes(context.input, axes, context.input_state);
            if (axes[0] != 0.f || axes[1] != 0.f)
            {
                const float facing = context.actor.euler.y;
                const float angle = float(runtime_input_axes_angle(axes));
                set_runtime_body_gaze_angle(body, context.body_actor, runtime_relative_angle(angle, facing));
            }
            return;
        }
        std::memcpy(body.data() + offsetof(BodyRecord, look_point.x), &mind_record(mind).gaze_point,
            sizeof(Vec3));
        body_record(body).look_point_set = 1;
    }
    mind_record(mind).gaze_point_set = 0u;
}

// Xbox 0x0007E3A0
bool request_runtime_player_idle_action(std::span<u8> mind,
    RuntimeBodyAttachmentRequests& body, u32& random_state)
{
    // DAT_002FFEE4, addressed backwards by the negative sampled index.
    constexpr std::array<u16, 6> keys{135, 136, 162, 165, 153, 154};
    random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    const i32 sampled = i32(fraction * -6.f); // DAT_0032B664.
    const u8* row = find_runtime_attachment_first_request(*body.tables, keys[size_t(-sampled)]);
    if (row != nullptr)
    {
        mind_record(mind).action_record = 0u;
        accept_runtime_attachment_request(body, row, 0);
        return true;
    }
    return false;
}

// Xbox 0x0007D210
void update_runtime_player_pending_attachment(RuntimePlayerMindPendingAttachment& context)
{
    auto* pending = context.pending;
    context.pending = nullptr;
    const u32 zero = 0;
    player_mind_record(context.mind).unk_134 = zero;
    if (!(player_mind_record(context.mind).control_lock > 0.f) && pending != nullptr)
    {
        if (runtime_player_can_attach(context.eligibility, *pending)
            && runtime_input_button_edge(context.input, 0x100, context.input_state) != 0)
        {
            context.action = pending;
            mind_record(context.mind).action_actor = pending->address;
            const u32 key = context.attach(*pending);
            select_runtime_body_action(context.body, u16(key), context.actions);
        }
    }
}

// Xbox 0x0007E160
void update_runtime_player_mind_inactivity(RuntimePlayerMindInactivity& control,
    RuntimeBodyAttachmentRequests& body, const ActorAnimationState& animation,
    u32& random_state)
{
    auto mind = control.mind;
    PlayerMindRecord& record = player_mind_record(mind);
    std::array<float, 2> axes;
    runtime_input_axes(control.input, axes, control.input_state);
    bool active = axes[1] * axes[1] + axes[0] * axes[0] > std::bit_cast<float>(0x3C23D70Bu);
    if (!active)
    {
        runtime_input_secondary_axes(control.input, axes, control.input_state);
        active = axes[1] * axes[1] + axes[0] * axes[0] > std::bit_cast<float>(0x3C23D70Bu);
    }
    if (active)
    {
        record.unk_148 = 0.f;
        record.unk_150 = 0;
        record.unk_154 = 0;
    }
    else
    {
        record.unk_148 = player_mind_record(mind).unk_148 + runtime_frame_step(control.clock);
    }
    if (player_mind_record(mind).unk_144 == 0)
    {
        return;
    }
    if (body_record(body.body).action_key == 0x2C
        && runtime_actor_animation_crossed_position(animation, 0, player_mind_record(mind).unk_14C)
        && mind_record(mind).target_mode != 0 && request_runtime_player_idle_action(mind, body, random_state))
    {
        record.unk_154 = 1;
    }
    if (body_record(body.body).move_holds != 0 || body_record(body.body).move_hold_time != 0.f)
    {
        return;
    }
    if ((control.flags.bytes[6] & 1) == 0)
    {
        return;
    }
    bool request = false;
    if (body_record(body.body).attached == 0 && body_record(body.body).unk_50 == 0)
    {
        request = player_mind_record(mind).unk_148 > 10.f && std::bit_cast<i32>(player_mind_record(mind).unk_150) > 2;
    }
    else
    {
        record.unk_154 = 0;
    }
    if (mind_record(mind).target_mode == 0)
    {
        record.unk_154 = 0;
    }
    if (request || player_mind_record(mind).unk_154 != 0)
    {
        if (body.actions.table != nullptr)
        {
            u32 optional;
            const u8* row = find_runtime_script_table_row(body.actions.table, 0x2C, optional);
            if (row != nullptr)
            {
                select_runtime_body_action_row(body.body, row, nullptr, body.actions);
            }
        }
        record.unk_148 = 0.f;
        record.unk_150 = 0;
        record.unk_154 = 1;
        random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
        const float fraction = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        const float position = fraction * std::bit_cast<float>(0x3EB33332u)
            + std::bit_cast<float>(0x3F19999Au);
        record.unk_14C = position;
    }
}

// Xbox 0x0007EBC0
u32 prepare_runtime_player_mind_request(std::span<u8> mind, Actor& actor,
    const Vec3* reference_position, std::span<u8> message, u32& random_state)
{
    const auto put = [](std::span<u8> bytes, size_t offset, u32 word)
    {
        std::memcpy(bytes.data() + offset, &word, 4);
    };
    const auto randomize = [&message, &random_state, &put](size_t range_offset, size_t output_offset)
    {
        const std::array<float, 2> range{read_f32(message, range_offset), read_f32(message, range_offset + 4)};
        float output;
        randomize_runtime_player_request_value(range, output, random_state);
        put(message, output_offset, std::bit_cast<u32>(output));
    };
    const u16 kind = read_u16(message, 8);
    if (kind == 0x29)
    {
        randomize(0x10, 0x18);
    }
    switch (kind)
    {
    case 0xF:
        randomize(0x10, 0x2C);
        break;
    case 0x10:
        randomize(0x1C, 0x38);
        break;
    case 0x11:
        randomize(0x10, 0x38);
        break;
    case 0x16:
        player_mind_record(mind).unk_130 = 0.f;
        player_mind_record(mind).unk_124 = 0;
        player_mind_record(mind).stick.pressed = 0;
        player_mind_record(mind).stick.gesture = 0;
        player_mind_record(mind).stick.unk_0C = 0.f;
        player_mind_record(mind).stick.direction = 0.f;
        player_mind_record(mind).stick.state = 1;
        player_mind_record(mind).stick.press_timer = 0.f;
        player_mind_record(mind).stick.magnitude = 0.f;
        player_mind_record(mind).stick.gesture_time = 0.f;
        break;
    case 0x24:
    {
        float angle;
        if (reference_position == nullptr || read_u32(message, 0x20) != 0)
        {
            angle = actor.euler.y;
        }
        else
        {
            angle = runtime_actor_direction_to_point(actor, *reference_position);
        }
        random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
        const float spread = read_f32(message, 0x14);
        const float negative = 0.0f - spread;
        const float fraction = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.0f;
        const float sampled = (((spread - negative) * fraction + read_f32(message, 0x10)) + negative) *
            std::bit_cast<float>(u32(0x3C8EFA35)) + angle;
        put(message, 0x28, std::bit_cast<u32>(sampled));
        break;
    }
    case 0x2D:
        randomize(0x10, 0x20);
        break;
    case 0x2E:
        randomize(0x1C, 0x2C);
        break;
    case 0x2F:
        randomize(0x10, 0x2C);
        break;
    }
    return 1;
}

// Xbox 0x0007EDF0
void randomize_runtime_player_request_value(std::span<const float, 2> range,
    float& output, u32& random_state)
{
    const float minimum = range[0];
    const float maximum = range[1];
    random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.0f;
    output = (maximum - minimum) * fraction + minimum;
}
namespace
{
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void decay_tracking_float(float& field, float step)
{
    const float previous = field;
    if (previous > 0.f)
    {
        const float value = previous - step;
        field = value;
        if (!(0.f < value) && !std::isnan(value))
        {
            field = 0.f;
        }
    }
}
}

// Xbox 0x0007D5A0
void update_runtime_player_input_gesture(std::span<u8> tracking, float magnitude, float step)
{
    switch (stick_record(tracking).gesture)
    {
    case 0:
        if (magnitude > std::bit_cast<float>(0x3DE147AEu))
        {
            stick_record(tracking).gesture = 1;
            stick_record(tracking).gesture_time = stick_record(tracking).gesture_time + step;
            stick_record(tracking).magnitude = magnitude;
            return;
        }
        break;
    case 1:
        if (!(magnitude >= stick_record(tracking).magnitude - 0.05f))
        {
            stick_record(tracking).gesture = 2;
            stick_record(tracking).magnitude = magnitude;
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
            const float value = stick_record(tracking).gesture_time + step;
            stick_record(tracking).gesture_time = value;
            if (!(value < 0.2f) && !std::isnan(value))
            {
                stick_record(tracking).gesture = 3;
                stick_record(tracking).magnitude = magnitude;
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
        if (0.1f > stick_record(tracking).magnitude)
        {
            stick_record(tracking).magnitude = 0.f;
            stick_record(tracking).gesture = 0;
            stick_record(tracking).gesture_time = 0.f;
            stick_record(tracking).magnitude = magnitude;
            return;
        }
        {
            const u32 previous = stick_record(tracking).state;
            stick_record(tracking).state = 2;
            if (previous == 0)
            {
                stick_record(tracking).press_timer = 0.f;
                stick_record(tracking).pressed = 0;
            }
            stick_record(tracking).pressed = 1;
            stick_record(tracking).press_timer = 0.2f;
            stick_record(tracking).unk_0C = 0.f;
        }
        break;
    default:
        break;
    }
    stick_record(tracking).magnitude = magnitude;
}

// Xbox 0x0007D460
void update_runtime_player_input_direction(std::span<u8> tracking,
    const std::array<float, 2>& axes, float step, u32 special)
{
    const float magnitude = float(retail_square_root(axes[0] * axes[0] + axes[1] * axes[1]));
    if (stick_record(tracking).state == 1)
    {
        if (!(magnitude < std::bit_cast<float>(0x3DE147AEu)) && !std::isnan(magnitude))
        {
            stick_record(tracking).state = 2;
            stick_record(tracking).pressed = 1;
            stick_record(tracking).press_timer = 0.2f;
            stick_record(tracking).unk_0C = 0.f;
        }
    }
    else if (stick_record(tracking).state == 2 && 0.1f > magnitude)
    {
        stick_record(tracking).state = 1;
        stick_record(tracking).pressed = 0;
    }
    update_runtime_player_input_gesture(tracking, special != 0 ? magnitude : 0.f, step);
    if (magnitude != 0.f)
    {
        float direction = 0.f;
        if (axes[0] != 0.f || axes[1] != 0.f)
        {
            direction = float(retail_atan2(axes[1], axes[0]));
        }
        stick_record(tracking).direction = direction;
    }
    decay_tracking_float(stick_record(tracking).unk_0C, step);
    decay_tracking_float(stick_record(tracking).press_timer, step);
}

// Xbox 0x00109810
void update_runtime_player_input_tracking(std::span<u8> tracking,
    const std::array<float, 2>& axes, std::span<const u8> preset,
    u32 accumulate, const RuntimeActorClock& clock)
{
    const auto previous = input_tracking_record(tracking).axes;
    const float dx = previous[0] - axes[0];
    const float dy = previous[1] - axes[1];
    input_tracking_record(tracking).axes = axes;
    const long double distance = retail_square_root(dy * dy + dx * dx);
    if (!(distance > static_cast<long double>(get(preset, input_tracking_preset_field::threshold))))
    {
        decay_tracking_float(input_tracking_record(tracking).activity, get(preset, input_tracking_preset_field::decay));
        decay_tracking_float(input_tracking_record(tracking).accumulation,
            get(preset, input_tracking_preset_field::decay));
    }
    else
    {
        if (1.f > input_tracking_record(tracking).activity)
        {
            const float value =
                get(preset, input_tracking_preset_field::rise) + input_tracking_record(tracking).activity;
            input_tracking_record(tracking).activity = value;
            if (!(value < 1.f) && !std::isnan(value))
            {
                input_tracking_record(tracking).activity = 1.f;
            }
        }
        if (accumulate != 0 &&
            get(preset, input_tracking_preset_field::cap) > input_tracking_record(tracking).accumulation)
        {
            const float value =
                get(preset, input_tracking_preset_field::rise) + input_tracking_record(tracking).accumulation;
            input_tracking_record(tracking).accumulation = value;
            if (!(value < get(preset, input_tracking_preset_field::cap)) && !std::isnan(value)
                && !std::isnan(get(preset, input_tracking_preset_field::cap)))
            {
                input_tracking_record(tracking).accumulation = get(preset, input_tracking_preset_field::cap);
            }
        }
    }
    const u32 state = input_tracking_record(tracking).state;
    if ((state == 1 && input_tracking_record(tracking).activity == 1.f)
        || (state == 2 && input_tracking_record(tracking).activity == 0.f))
    {
        decay_tracking_float(input_tracking_record(tracking).hold_timer, runtime_frame_step(clock));
        if (input_tracking_record(tracking).hold_timer != 0.f)
        {
            return;
        }
        const u32 next = u32(input_tracking_record(tracking).state != 2) + 1;
        input_tracking_record(tracking).state = next;
        return;
    }
    input_tracking_record(tracking).hold_timer = std::bit_cast<float>(0x3D89374Cu);
}

// Xbox 0x00137840
bool update_runtime_player_boost_timer(RuntimeMindBoost& boost,
    RuntimePlayerMindControl& control, const std::span<const u8>* preset, float multiplier)
{
    const size_t group = (player_record(boost.actor_bytes).mode == 2
        || player_record(boost.actor_bytes).case_link != 0) ? 1 : 0;
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
            if (body_record(body).energy != 0)
            {
                control.flags.bytes[5] &= 0xEF;
                --body_record(body).unk_309;
                --body_record(body).unk_30A;
                --body_record(body).unk_30B;
                --body_record(body).unk_30C;
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
    if (body_record(body).unk_38 == 1)
    {
        runtime_input_axes(input, axes, state);
        rotate_runtime_player_input_axes(axes, camera_direction);
        set_runtime_input_axes(writable, axes, keyboard);
    }
    if (body_record(body).unk_39 == 1)
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
        player_mind_record(context.mind).unk_130 = 0.f;
    }
    else
    {
        const float timer = player_mind_record(context.mind).unk_130 + runtime_frame_step(context.clock);
        player_mind_record(context.mind).unk_130 = timer;
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
                && body_record(context.attachment.body).unk_6A == 0)
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
