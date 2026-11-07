#include "body_motion_controller.h"
#include "entity_heap.h"
#include "../frontend/dialogue_draw_context.h"
#include <bit>
#include <cstring>
#include <new>

namespace gbtg
{
// Xbox 0x0002EEB0
void adjust_runtime_body_pitch(float* matrix, float target, float weight)
{
    float roll, pitch, yaw;
    extract_runtime_body_angles(matrix, roll, pitch, yaw);
    const float angle = float((static_cast<long double>(pitch) + target) * weight);
    const float sine = float(retail_sin(angle));
    const float cosine = float(retail_cos(angle));
    std::array<float, 12> rotation;
    make_dialogue_axis_rotation(rotation.data(), 'X', sine, cosine);
    multiply_affine_matrices(matrix, matrix, rotation.data());
}

// Xbox 0x0002EF20
void adjust_runtime_body_yaw(float* matrix, float target, float weight)
{
    float roll, pitch, yaw;
    extract_runtime_body_angles(matrix, roll, pitch, yaw);
    const float angle = float((static_cast<long double>(target) - yaw) * weight);
    const float sine = float(retail_sin(angle));
    const float cosine = float(retail_cos(angle));
    std::array<float, 12> rotation;
    make_dialogue_axis_rotation(rotation.data(), 'Y', sine, cosine);
    multiply_affine_matrices(matrix, matrix, rotation.data());
}

namespace
{
GhoulySpawnPool* allocate_pool(u16 stride, u16 grow, size_t capacity)
{
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + capacity * stride)));
    pool->stride = stride;
    pool->grow = grow;
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->limit = pool->begin + capacity * stride;
    return pool;
}
float normalize_angle(float angle)
{
    constexpr float full = std::bit_cast<float>(0x40C90FDBu);
    constexpr float negative_pi = std::bit_cast<float>(0xC0490FDBu);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    const long double remainder = animation_remainder(angle, full);
    float value = float(remainder);
    if (remainder < negative_pi)
    {
        value += full;
    }
    if (value >= pi)
    {
        value -= full;
    }
    return value;
}
}

// Xbox 0x00066310
u8* insert_runtime_pool_element(GhoulySpawnPool*& pool, i32 index)
{
    const i32 used = i32(pool->cursor - pool->begin) / pool->stride;
    if (pool->cursor == pool->limit)
    {
        pool = grow_ghouly_spawn_pool(pool);
    }
    u8* result = pool->cursor;
    pool->cursor += pool->stride;
    if (index != used)
    {
        result = pool->begin + index * pool->stride;
        u8* destination = pool->begin + (index + 1) * pool->stride;
        std::memmove(destination, result, size_t(pool->cursor - destination));
    }
    return result;
}

// Xbox 0x0002F3C0
i32 initialize_runtime_body_motion_controller(RuntimeBodyMotionController& controller)
{
    controller.joints = allocate_pool(2, 2, 2);
    controller.dispatch = allocate_pool(u16(sizeof(RuntimeBodyMotionCallback)), 1, 1);
    controller.callbacks = allocate_pool(u16(sizeof(RuntimeBodyMotionCallbacks)), 1, 1);
    controller.count = 1;
    insert_runtime_pool_element(controller.joints, 0);
    insert_runtime_pool_element(controller.joints, 0);
    insert_runtime_pool_element(controller.dispatch, 0);
    auto* callbacks = insert_runtime_pool_element(controller.callbacks, 0);
    new (callbacks) RuntimeBodyMotionCallbacks{};
    const u16 sentinel = 0xFFFF;
    std::memcpy(controller.joints->begin + controller.joints->stride, &sentinel, 2);
    return 0;
}

// Xbox 0x0002F480
i32 find_or_insert_runtime_body_motion_joint(RuntimeBodyMotionController& controller, u16 joint)
{
    i32 index = 0;
    auto* cursor = static_cast<i32*>(engine_heap_allocate(4));
    *cursor = 0;
    const u8* row;
    for (;;)
    {
        row = controller.joints->begin + controller.joints->stride * *cursor;
        if (row == controller.joints->cursor)
        {
            row = nullptr;
            break;
        }
        ++*cursor;
        if (row == nullptr || joint <= read_u16({row, 2}, 0))
        {
            break;
        }
        ++index;
    }
    engine_heap_free(cursor);
    if (row == nullptr)
    {
        ++index;
    }
    else if (read_u16({row, 2}, 0) == joint)
    {
        return index;
    }
    insert_runtime_pool_element(controller.joints, index);
    insert_runtime_pool_element(controller.dispatch, index);
    auto* callbacks = insert_runtime_pool_element(controller.callbacks, index);
    new (callbacks) RuntimeBodyMotionCallbacks{};
    ++controller.count;
    return index;
}

// Xbox 0x0002F350
void register_runtime_body_motion_callback(RuntimeBodyMotionController& controller, u16 joint,
    RuntimeBodyMotionCallback callback, void* data)
{
    i32 index;
    if (controller.count == 0)
    {
        index = initialize_runtime_body_motion_controller(controller);
    }
    else
    {
        index = find_or_insert_runtime_body_motion_joint(controller, joint);
    }
    std::memcpy(controller.joints->begin + controller.joints->stride * index, &joint, 2);
    const RuntimeBodyMotionCallback dispatch = dispatch_runtime_body_motion_callbacks;
    std::memcpy(controller.dispatch->begin + controller.dispatch->stride * index, &dispatch, sizeof(dispatch));
    auto* callbacks = reinterpret_cast<RuntimeBodyMotionCallbacks*>(controller.callbacks->begin + controller.callbacks->stride * index);
    callbacks->first = callback;
    callbacks->first_data = data;
}

// Xbox 0x0002F6A0
void dispatch_runtime_body_motion_callbacks(float* output, const float* local, float* parent,
    u16 index, void* context, float elapsed)
{
    auto& controller = *static_cast<RuntimeBodyMotionController*>(context);
    const u8* joint = controller.joints->begin + controller.joints->stride * index;
    auto* callbacks = reinterpret_cast<RuntimeBodyMotionCallbacks*>(controller.callbacks->begin + controller.callbacks->stride * index);
    const auto first = callbacks->first;
    if (first != nullptr)
    {
        first(output, local, parent, read_u16({joint, 2}, 0), callbacks->first_data, elapsed);
    }
    if (callbacks->second != nullptr)
    {
        callbacks->second(output, local, parent, read_u16({joint, 2}, 0), callbacks->second_data, elapsed);
    }
}

// Xbox 0x00100F30
float runtime_body_angle_distance(float current, float target, float anchor)
{
    const float first = normalize_angle(anchor - current);
    const float second = normalize_angle(target - anchor);
    return second + first;
}

// Xbox 0x001010B0
void accelerate_runtime_body_angle(float& current, const float& target, float& velocity,
    float anchor, float maximum, float acceleration, float elapsed)
{
    const float distance = runtime_body_angle_distance(current, target, anchor);
    if (distance < 0.f)
    {
        const float braking = 0.f - ((velocity / acceleration) * velocity * 0.5f);
        if (!(distance > braking) || !(velocity > 0.f))
        {
            const float negative_maximum = 0.f - maximum;
            if (velocity > negative_maximum)
            {
                velocity -= acceleration * elapsed;
                if (negative_maximum > velocity)
                {
                    velocity = negative_maximum;
                }
            }
        }
        else if (velocity < 0.f)
        {
            velocity += acceleration * elapsed;
            if (velocity > 0.f)
            {
                velocity = 0.f;
            }
        }
        current = velocity * elapsed + current;
        if (runtime_body_angle_distance(current, target, anchor) > 0.f)
        {
            current = target;
            velocity = 0.f;
        }
        return;
    }
    if (distance > 0.f)
    {
        const float braking = (velocity / acceleration) * velocity * 0.5f;
        if (!(braking > distance) || !(velocity > 0.f))
        {
            if (maximum > velocity)
            {
                velocity = acceleration * elapsed + velocity;
                if (velocity > maximum)
                {
                    velocity = maximum;
                }
            }
        }
        else
        {
            velocity -= acceleration * elapsed;
            if (velocity < 0.f)
            {
                velocity = 0.f;
            }
        }
        current = velocity * elapsed + current;
        if (runtime_body_angle_distance(current, target, anchor) < 0.f)
        {
            current = target;
            velocity = 0.f;
        }
        return;
    }
    if (velocity != 0.f)
    {
        if (velocity < 0.f)
        {
            velocity = acceleration * elapsed + velocity;
            if (velocity > 0.f)
            {
                velocity = 0.f;
            }
        }
        else
        {
            velocity -= acceleration * elapsed;
            if (velocity < 0.f)
            {
                velocity = 0.f;
            }
        }
    }
}

// Xbox 0x0002F060
void update_runtime_body_joint_motion(float* local, const float* parent, float* output,
    u16, void* context, float elapsed)
{
    multiply_affine_matrices(output, local, parent);
    auto& motion = *static_cast<RuntimeBodyJointMotion*>(context);
    if (motion.bone != 0)
    {
        float roll, pitch, yaw;
        extract_runtime_body_angles(local, roll, pitch, yaw);
        if (motion.pitch_limit != -1.f)
        {
            motion.pitch_target = constrain_runtime_body_angle(motion.pitch_target, 0.f - pitch, motion.pitch_limit);
            if (motion.weight > 0.f)
            {
                accelerate_runtime_body_angle(motion.pitch, motion.pitch_target, motion.pitch_speed,
                    0.f, motion.max_speed, motion.acceleration, elapsed);
            }
            else
            {
                motion.pitch = motion.pitch_target;
                motion.pitch_speed = 0.f;
            }
            adjust_runtime_body_pitch(output, motion.pitch, motion.weight);
        }
        if (motion.yaw_limit != -1.f)
        {
            motion.yaw_target = constrain_runtime_body_angle(motion.yaw_target, yaw, motion.yaw_limit);
            if (motion.weight > 0.f)
            {
                accelerate_runtime_body_angle(motion.yaw, motion.yaw_target, motion.yaw_speed,
                    yaw, motion.max_speed, motion.acceleration, elapsed);
            }
            else
            {
                motion.yaw = motion.yaw_target;
                motion.yaw_speed = 0.f;
            }
            motion.yaw = constrain_runtime_body_angle(motion.yaw, yaw, motion.yaw_limit);
            adjust_runtime_body_yaw(output, motion.yaw, motion.weight);
        }
    }
}

// Xbox 0x0002EF90
float constrain_runtime_body_angle(float current, float target, float limit)
{
    float difference = normalize_angle(current - target);
    if (difference > 0.f && difference > limit)
    {
        difference -= limit;
        current -= difference;
    }
    else if (difference < 0.f && difference < 0.f - limit)
    {
        difference += limit;
        current -= difference;
    }
    return normalize_angle(current);
}

// Xbox 0x0002F1D0
void extract_runtime_body_angles(const float* matrix, float& roll, float& pitch, float& yaw)
{
    float value = matrix[6];
    while (value < -1.f)
    {
        value += 1.f;
    }
    while (value > 1.f)
    {
        value -= 1.f;
    }
    if (value >= 1.f)
    {
        pitch = std::bit_cast<float>(0x3FC90FDBu);
    }
    else if (-1.f >= value)
    {
        pitch = std::bit_cast<float>(0xBFC90FDBu);
    }
    else
    {
        pitch = float(retail_asin(value));
    }
    yaw = float(retail_atan2(0.f - matrix[8], matrix[10]));
    if (matrix[1] == 0.f && matrix[5] == 0.f)
    {
        roll = float(retail_atan2(0.f - matrix[4], matrix[0]));
        return;
    }
    roll = float(retail_atan2(0.f - matrix[1], matrix[5]));
}
}
