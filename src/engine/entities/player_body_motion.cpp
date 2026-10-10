#include "player_body_motion.h"
#include "entity_heap.h"
#include <bit>
#include <cstring>
#include <new>

namespace gbtg
{
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
        body[0x8B0 + i * 0x14] = 0;
        std::memcpy(body.data() + 0x8B4 + i * 0x14, &zero, 4);
    }
    const u8 count = count_runtime_player_body_joints(context.model, parameters);
    if (count != 0)
    {
        u8* cursor = static_cast<u8*>(engine_heap_allocate(count * 0x30));
        float first = 0.f, second = 0.f;
        const float sum = (((read_f32(parameters, 0x814) + read_f32(parameters, 0x808))
            + read_f32(parameters, 0x7FC)) + read_f32(parameters, 0x7F0)) + read_f32(parameters, 0x7E4);
        std::memcpy(body.data() + 0x8A8, &sum, 4);
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
    const u32 first = read_u32(parameters, 0x7C8);
    const u32 second = read_u32(parameters, 0x7CC);
    std::memcpy(body.data() + 0x894, &first, 4);
    std::memcpy(body.data() + 0x8A4, &second, 4);
    initialize_runtime_player_joint_motion(body, parameters, context);
    collect_runtime_player_body_shapes(context.nodes, context.shapes);
    if (read_f32(parameters, 0x7C8) == 0.f)
    {
        const u32 counter = read_u32(body, 0x258) + 1u;
        std::memcpy(body.data() + 0x258, &counter, 4);
    }
    return true;
}
}
