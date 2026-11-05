#include "player_body_motion.h"
#include <cstddef>
#include "entity_heap.h"
#include "actor_transform.h"
#include "body_animation_setup.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <cmath>
#include <new>

namespace gbtg
{
// Xbox 0x00075120
void add_runtime_player_body_movement(std::span<u8> body, const Vec3& movement,
    const Actor& actor, const RuntimeActorClock& clock)
{
    const Vec3 previous = body_record(body).position;
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(actor);
    const float maximum = float(static_cast<long double>(rate) * step * 50.f); // DAT_0032B1E0.
    Vec3 delta = movement;
    const float squared = (movement.z * movement.z + movement.y * movement.y)
        + movement.x * movement.x;
    // FSQRT here is the processor operation, not the separate retail square-root helper.
    if (std::sqrt(static_cast<long double>(squared)) > static_cast<long double>(maximum)
        && squared != 0.f)
    {
        const long double scale = retail_reciprocal_sqrt(squared) * maximum;
        delta.x = float(static_cast<long double>(delta.x) * scale);
        delta.y = float(static_cast<long double>(delta.y) * scale);
        delta.z = float(static_cast<long double>(delta.z) * scale);
    }
    const Vec3 result{delta.x + previous.x, delta.y + previous.y, delta.z + previous.z};
    body_record(body).position.x = result.x;
    body_record(body).position.y = result.y;
    body_record(body).position.z = result.z;
}

// Xbox 0x000743E0
bool runtime_player_body_can_select_action(std::span<const u8> body,
    std::span<const u8> actor, const RuntimePlayerGoalRing* ring,
    const EntityBitBlock& flags, std::span<const u8> address_zero)
{
    if (i32(get(actor, player_field::pause_count)) > 0)
    {
        return false;
    }
    if (ring != nullptr)
    {
        const u16 kind = ring->tail == ring->capacity ? read_u16(address_zero, 0)
            : read_u16(ring->rows, size_t(ring->head) * ring->stride);
        if (kind != 0)
        {
            return false;
        }
    }
    if (body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f
        && body_record(body).holds == 0 && body_record(body).unk_6A == 0)
    {
        constexpr std::array<u32, 8> blockers{48, 44, 30, 49, 46, 42, 52, 54}; // DAT_002FFE80.
        for (const u32 bit : blockers)
        {
            if ((flags.bytes[bit >> 3] & (1u << (bit & 7))) != 0)
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

namespace
{
float joint_angle_delta(float angle)
{
    constexpr float circle = std::bit_cast<float>(0x40C90FDBu);
    const long double remainder = animation_remainder(angle, circle);
    float value = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        value += circle;
    }
    if (value >= std::bit_cast<float>(0x40490FDBu))
    {
        value -= circle;
    }
    return value;
}
}

// Xbox 0x00073C30
float select_runtime_player_joint_target(std::span<u8> body, size_t slot,
    const Vec3& joint_position, u32& tracking, RuntimePlayerJointTargets& context)
{
    constexpr std::array<size_t, 6> disable_offsets{2, 2, 1, 0, 3, 3}; // DAT_002FFE68.
    constexpr float release_time = std::bit_cast<float>(0x3E99999Au); // DAT_0032B268.
    constexpr float acquire_time = std::bit_cast<float>(0x3E4CCCCDu); // DAT_0032B1AC.
    constexpr float acquire_inverse = -5.f; // DAT_0032B37C.
    constexpr float release_inverse = std::bit_cast<float>(0xC0555555u); // DAT_0032B408.
    float pitch_delta;
    float yaw_delta;
    float stack_word = std::bit_cast<float>(context.body_address);
    tracking = 0;
    if (body_record(body).look_point_set != 0 && body[offsetof(BodyRecord, unk_309) + disable_offsets[slot]] == 0)
    {
        const Vec3 target{body_record(body).look_point.x, body_record(body).look_point.y,
            body_record(body).look_point.z};
        const Vec3 euler = actor_world_euler(context.actor);
        float yaw, pitch;
        direction_to_yaw_pitch(target.x - joint_position.x, target.y - joint_position.y,
            target.z - joint_position.z, yaw, pitch);
        pitch_delta = joint_angle_delta(pitch - euler.x);
        yaw_delta = joint_angle_delta(yaw - euler.y);
        stack_word = yaw_delta; // The second remainder reuses the first argument's stack word.
        const long double limit = static_cast<long double>(parent_body_record(body).joint_limit)
            + std::bit_cast<float>(0x3EB2B8C3u); // DAT_0032B160.
        tracking = !(std::fabs(static_cast<long double>(yaw_delta)) > limit);
    }
    PlayerBodyJoint& joint = parent_body_record(body).joints[slot];
    u8& state = joint.state;
    if (tracking == 0)
    {
        if (state == 1)
        {
            const float timer = joint.timer
                - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
            joint.timer = timer;
            if (timer <= 0.f)
            {
                joint.timer = 0.f;
                state = 0;
            }
        }
        else if (state == 2)
        {
            float timer = ((joint.timer - acquire_time) * acquire_inverse) * release_time;
            state = 1;
            if (timer > release_time)
            {
                timer = release_time;
            }
            else if (0.f > timer)
            {
                timer = 0.f;
            }
            joint.timer = timer;
        }
        else if (state == 3)
        {
            state = 1;
            joint.timer = release_time;
        }
    }
    else
    {
        if (state == 0)
        {
            state = 2;
            joint.timer = acquire_time;
        }
        else if (state == 1)
        {
            float timer = ((joint.timer - release_time) * release_inverse) * acquire_time;
            state = 2;
            if (timer > acquire_time)
            {
                timer = acquire_time;
            }
            else if (0.f > timer)
            {
                timer = 0.f;
            }
            joint.timer = timer;
        }
        else if (state == 2)
        {
            const float timer = joint.timer
                - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
            joint.timer = timer;
            if (timer <= 0.f)
            {
                joint.timer = 0.f;
                state = 3;
            }
        }
    }
    float weight = 0.f;
    switch (state)
    {
    case 0:
        break;
    case 1:
        weight = 1.f - (joint.timer - release_time) * release_inverse;
        if (0.f > weight)
        {
            weight = 0.f;
        }
        else if (weight > 1.f)
        {
            weight = 1.f;
        }
        break;
    case 2:
        weight = (joint.timer - acquire_time) * acquire_inverse;
        if (weight > 1.f)
        {
            weight = 1.f;
        }
        else if (0.f > weight)
        {
            weight = 0.f;
        }
        break;
    case 3:
        weight = 1.f;
        break;
    default:
        weight = stack_word;
        break;
    }
    if (tracking != 0)
    {
        joint.pitch_delta = pitch_delta;
        joint.yaw_delta = yaw_delta;
    }
    return weight;
}

// Xbox 0x00073820
void apply_runtime_player_joint_targets(std::span<u8> body, RuntimePlayerJointTargets& context)
{
    context.flags->bytes[4] &= 0xBF;
    if (body_record(body).unk_310 == 0)
    {
        for (size_t slot = 0; slot < 6; ++slot)
        {
            u8* record = context.records[slot];
            if (record != nullptr && read_u16({record, 0x30}, 0) != 0)
            {
                Vec3 position;
                actor_transform_joint_point(context.actor, read_u16({record, 0x30}, 0),
                    actor_transform_origin, position);
                u32 tracking;
                const float weight = select_runtime_player_joint_target(body, slot, position, tracking, context);
                if (tracking != 0)
                {
                    context.flags->bytes[4] |= 0x40;
                }
                record = context.records[slot];
                const u32 yaw = std::bit_cast<u32>(parent_body_record(body).joints[slot].yaw_delta);
                if (record != nullptr && read_u16({record, 0x30}, 0) != 0)
                {
                    const std::span<u8> bytes{record, 0x30};
                    if (read_f32(bytes, 0x2C) != -1.f)
                    {
                        const u32 pitch = std::bit_cast<u32>(parent_body_record(body).joints[slot].pitch_delta);
                        std::memcpy(record + 0x20, &pitch, 4);
                    }
                    if (read_f32(bytes, 0x1C) != -1.f)
                    {
                        std::memcpy(record + 0x10, &yaw, 4);
                    }
                    const float stored = weight;
                    std::memcpy(record + 0xC, &stored, 4);
                }
            }
        }
    }
}

// Xbox 0x00025EC0
void relay_runtime_parent_body(u32 message, RuntimePlayerBodyRelayChild*& action,
    RuntimePlayerBodyRelayChild*& attachment)
{
    if (action != nullptr)
    {
        action->relay(message);
    }
    if (attachment != nullptr)
    {
        attachment->relay(message);
    }
}

// Xbox 0x000744A0
void relay_runtime_player_body(std::span<u8> body, u32 message,
    RuntimePlayerBodyRelayChild*& action, RuntimePlayerBodyRelayChild*& attachment,
    RuntimePlayerJointTargets& context)
{
    if (action != nullptr)
    {
        action->relay(message);
    }
    if (attachment != nullptr)
    {
        attachment->relay(message);
    }
    ease_runtime_body_rotation(body, context.actor, context.clock);
    apply_runtime_player_joint_targets(body, context);
}

// Xbox 0x000265D0
void ease_runtime_body_rotation(std::span<u8> body, Actor& actor,
    const RuntimeActorClock& clock)
{
    if (body_record(body).unk_728 != 0)
    {
        const Vec3 current = actor_world_euler(actor);
        const Vec3 target = body_record(body).euler;
        const float elapsed = float(static_cast<long double>(runtime_actor_rate(actor)) * runtime_frame_step(clock));
        const float minimum = read_f32(body, offsetof(BodyRecord, turn_rates));
        const float maximum = body_record(body).turn_rates[1];
        const float gain = body_record(body).turn_rates[2];
        const Vec3 next{
            ease_runtime_body_angle(target.x, current.x, minimum, maximum, gain, elapsed),
            ease_runtime_body_angle(target.y, current.y, minimum, maximum, gain, elapsed),
            ease_runtime_body_angle(target.z, current.z, minimum, maximum, gain, elapsed)};
        normalize_euler_angles(actor.euler, next);
        actor.xform_valid = 0;
        if (actor.euler_out == nullptr || actor.euler_out->actor == nullptr)
        {
            throw Error("body rotation 0x000265D0 has no actor +0x95C body output view");
        }
        ActorEulerOut& output = *actor.euler_out;
        output.euler = actor_world_euler(*output.actor);
        std::memcpy(body.data() + offsetof(BodyRecord, euler.x), &output.euler, sizeof(Vec3));
    }
}

// Xbox 0x00101280
float ease_runtime_body_angle(float target, float current, float minimum,
    float maximum, float gain, float elapsed)
{
    constexpr float circle = std::bit_cast<float>(0x40C90FDBu);
    const long double remainder = animation_remainder(target - current, circle);
    float delta = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        delta += circle;
    }
    if (delta >= std::bit_cast<float>(0x40490FDBu))
    {
        delta -= circle;
    }
    float speed = delta * gain;
    if (speed < 0.f)
    {
        const float lower = 0.f - minimum;
        if (speed > lower)
        {
            speed = lower;
        }
        else
        {
            const float upper = 0.f - maximum;
            if (upper > speed)
            {
                speed = upper;
            }
        }
    }
    else if (speed >= 0.f)
    {
        if (minimum > speed)
        {
            speed = minimum;
        }
        else if (speed > maximum)
        {
            speed = maximum;
        }
    }
    const float step = float(static_cast<long double>(speed) * elapsed);
    if (!(static_cast<long double>(step) > std::fabs(static_cast<long double>(delta))))
    {
        const long double advanced = animation_remainder(step + current, circle);
        current = float(advanced);
        if (advanced < 0.L)
        {
            return current + circle;
        }
    }
    return current;
}

// Xbox 0x00073760
void release_runtime_player_joint_motion(std::span<u8> body, std::span<u8*, 6> records)
{
    u8* allocation = nullptr;
    const u32 zero = 0;
    for (size_t index = 6; index != 0;)
    {
        --index;
        if (records[index] != nullptr)
        {
            allocation = records[index];
            records[index] = nullptr;
            parent_body_record(body).joints[index].record_word = 0;
        }
    }
    if (allocation != nullptr)
    {
        engine_heap_free(allocation);
    }
}

// Xbox 0x000734B0
void destroy_runtime_player_body(std::span<u8> body, RuntimeBodyShapeStorage& shapes,
    std::span<u8*, 6> records)
{
    const u32 zero = 0;
    if (shapes.selected_state != nullptr)
    {
        engine_heap_free(shapes.selected_state);
        shapes.selected_state = nullptr;
        parent_body_record(body).selected_state_word = 0;
    }
    if (shapes.selected != nullptr)
    {
        engine_heap_free(shapes.selected);
        shapes.selected = nullptr;
        parent_body_record(body).selected_word = 0;
    }
    release_runtime_player_joint_motion(body, records);
}

// Xbox 0x00073A90
u8* append_runtime_player_joint_motion(std::span<const u8> parameters, u8*& cursor,
    float& first, float& second, float maximum, float acceleration, RuntimePlayerBodyMotion& context)
{
    const float a = read_f32(parameters, 4);
    const float b = read_f32(parameters, 8);
    if (a != -1.f)
    {
        first += a;
    }
    if (b != -1.f)
    {
        second += b;
    }
    if (read_u32(parameters, 0) != 0xFFFFFFFFu && context.model != nullptr && context.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(context.model->cpu, i16(read_u16(parameters, 0))) & 0xFFu;
        if (joint != 0xFFu && joint != 0xFFFFFFFFu)
        {
            if (context.controller == nullptr)
            {
                auto* allocation = engine_heap_allocate(sizeof(RuntimeBodyMotionController));
                context.controller = new (allocation) RuntimeBodyMotionController{};
                context.secondary = context.controller;
                context.selector = 0;
            }
            u8* record = cursor;
            const auto put = [&](size_t offset, float value)
            {
                std::memcpy(record + offset, &value, 4);
            };
            const float chosen_first = a != -1.f ? first : a;
            const float chosen_second = a != -1.f ? second : b;
            put(0x2C, chosen_first);
            put(4, maximum);
            put(0x28, 0.f);
            put(0x24, 0.f);
            put(0x20, 0.f);
            put(0x18, 0.f);
            put(0x14, 0.f);
            put(0x10, 0.f);
            put(0x1C, chosen_second);
            put(8, acceleration);
            const u16 bone = u16(joint);
            std::memcpy(record, &bone, 2);
            put(0xC, 0.f);
            cursor += 0x30;
            if (a != -1.f)
            {
                first = 0.f;
            }
            if (b != -1.f)
            {
                second = 0.f;
            }
            register_runtime_body_motion_callback(*context.controller, bone, update_runtime_body_joint_motion, record);
            return record;
        }
    }
    return nullptr;
}

// Xbox 0x000734F0
void initialize_runtime_player_joint_motion(std::span<u8> body, std::span<const u8> parameters,
    RuntimePlayerBodyMotion& context)
{
    const u32 zero = 0;
    for (size_t i = 0; i < 6; ++i)
    {
        parent_body_record(body).joints[i].state = 0;
        parent_body_record(body).joints[i].timer = 0.f;
    }
    const u8 count = count_runtime_player_body_joints(context.model, parameters);
    if (count != 0)
    {
        u8* cursor = static_cast<u8*>(engine_heap_allocate(count * 0x30));
        float first = 0.f, second = 0.f;
        const float sum = (((read_f32(parameters, 0x814) + read_f32(parameters, 0x808))
            + read_f32(parameters, 0x7FC)) + read_f32(parameters, 0x7F0)) + read_f32(parameters, 0x7E4);
        parent_body_record(body).joint_limit = sum;
        constexpr std::array<size_t, 6> offsets{0x800, 0x80C, 0x7F4, 0x7E8, 0x7D0, 0x7DC};
        constexpr std::array<u32, 6> multiplier_bits{0x3DCCCCCD, 0x3E4CCCCD, 0x3F333333, 0x3F333333, 0x3F800000, 0x3F800000};
        for (size_t i = 0; i < 6; ++i)
        {
            const float multiplier = std::bit_cast<float>(multiplier_bits[i]);
            const float maximum = i < 4 ? read_f32(parameters, 0x818) * multiplier : read_f32(parameters, 0x818);
            const float acceleration = i < 4 ? read_f32(parameters, 0x81C) * multiplier : read_f32(parameters, 0x81C);
            context.records[i] = append_runtime_player_joint_motion(parameters.subspan(offsets[i], 12),
                cursor, first, second, maximum, acceleration, context);
        }
    }
}

// Xbox 0x00073450
bool initialize_runtime_player_body(std::span<u8> body, std::span<const u8> parameters,
    RuntimePlayerBodyMotion& context)
{
    parent_body_record(body).collision_radius = read_f32(parameters, 0x7C8);
    parent_body_record(body).collision_height = read_f32(parameters, 0x7CC);
    initialize_runtime_player_joint_motion(body, parameters, context);
    collect_runtime_player_body_shapes(context.nodes, context.shapes);
    if (read_f32(parameters, 0x7C8) == 0.f)
    {
        const u32 counter = body_record(body).holds + 1u;
        body_record(body).holds = counter;
    }
    return true;
}
}
