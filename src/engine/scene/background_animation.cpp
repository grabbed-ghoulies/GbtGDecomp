#include "background_animation.h"
#include "../entities/actor_matrix_setup.h"
#include "../graphics/model_points.h"
#include "../graphics/matrix_inverse.h"
#include "../graphics/point_graph_solver.h"
#include <bit>

#include <cstring>

namespace gbtg
{
// Xbox 0x00044490
void initialize_background_animation(RuntimeBackgroundAnimation* first,
    const std::function<std::span<const u8>(u32)>& resolve,
    const std::function<u32(u8*, u32)>& bind_allocation)
{
    if (first != nullptr && first->model_reference != nullptr &&
        first->model_reference->data() != nullptr)
    {
        const std::span<const u8> cpu = *first->model_reference;
        if (first->header.data() != nullptr)
        {
            const u32 point_table = runtime_model_animation_matrix_array(cpu);
            if (point_table != 0 && first->rows.data() != nullptr)
            {
                const u32 count = first->header[1];
                const auto table = resolve(point_table);
                if (read_u32(first->animation_block, 0x2C) == 0)
                {
                    for (u32 index = 0; index < count; ++index)
                    {
                        const u32 points = read_u32(table, index * 8);
                        const auto source = resolve(read_u32(table, index * 8 + 4));
                        build_runtime_point_graph(first->rows[index],
                            {reinterpret_cast<const Vec3*>(source.data()), points}, points, bind_allocation);
                    }
                }
                else
                {
                    for (u32 index = 0; index < count; ++index)
                    {
                        const u32 zero = 0;
                        std::memcpy(first->rows[index].fields.data() + 0x28, &zero, 4);
                        std::memcpy(first->rows[index].fields.data() + 0x20, &zero, 4);
                        first->rows[index].allocation = nullptr;
                    }
                }
                for (u32 index = 0; index < count; ++index)
                {
                    const u32 points = read_u32(table, index * 8);
                    const auto source = resolve(read_u32(table, index * 8 + 4));
                    auto& output = *reinterpret_cast<Vec3*>(first->rows[index].fields.data() + 0x1B0);
                    average_runtime_model_points(output,
                        {reinterpret_cast<const Vec3*>(source.data()), points});
                }
                if (first->matrix_count != 0 && first->matrix_count > 0)
                {
                    for (i32 index = 0; index < first->matrix_count; ++index)
                    {
                        const auto source = resolve(runtime_matrix_array_matrix_offset(first->matrix_node, index));
                        const auto row = first->rows[index].fields;
                        for (size_t target : {0x30u, 0xB0u, 0x70u})
                        {
                            for (size_t word = 0; word < 16; ++word)
                            {
                                const u32 value = read_u32(source, word * 4);
                                std::memcpy(row.data() + target + word * 4, &value, 4);
                            }
                        }
                        invert_runtime_graphics_matrix(
                            std::span<float, 16>{reinterpret_cast<float*>(row.data() + 0x170), 16}, nullptr,
                            std::span<const float, 16>{reinterpret_cast<const float*>(row.data() + 0xB0), 16});
                        invert_runtime_graphics_matrix(
                            std::span<float, 16>{reinterpret_cast<float*>(row.data() + 0x130), 16}, nullptr,
                            std::span<const float, 16>{reinterpret_cast<const float*>(row.data() + 0x30), 16});
                    }
                }
            }
        }
    }
}

// Xbox 0x00044A90
void push_background_graph(const Vec3& impulse, u32 graph, RuntimeBackgroundGraphPush& context)
{
    const std::span<const u8> block = context.animation.animation_block;
    if (block.data() == nullptr)
    {
        throw Error("background graph push reads the model block through address zero");
    }
    if (read_u32(block, 0x2C) != 0)
    {
        return;
    }
    constexpr float kLongestStep = std::bit_cast<float>(0x3C888889u); // DAT_0032B150, 1/60.
    float step = runtime_actor_rate(context.background) * runtime_frame_step(context.clock);
    if (step > kLongestStep)
    {
        step = kLongestStep;
    }
    RuntimePointGraph& row = context.animation.rows[graph];
    const float scale = step * 60.f; // DAT_0032B28C.
    float x = impulse.x * scale;
    float y = impulse.y * scale;
    float z = impulse.z * scale;
    for (u32 index = 0; index < read_u32(row.fields, 0); ++index)
    {
        u8* point = row.points.data() + size_t(index) * 0x30;
        float value;
        std::memcpy(&value, point, 4);
        value = value - x;
        std::memcpy(point + 0xC, &value, 4);
        std::memcpy(&value, point + 4, 4);
        value = value - y;
        std::memcpy(point + 0x10, &value, 4);
        std::memcpy(&value, point + 8, 4);
        value = value - z;
        std::memcpy(point + 0x14, &value, 4);
    }
    context.random = context.random * 0x19660D + 0x3C6EF35F;
    const float unit = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000);
    const float share = (unit - 1.f) * 6.f + 2.f; // DAT_0032B0F8, DAT_0032B218, DAT_0032B110.
    x = x * share;
    y = y * share;
    z = z * share;
    u8* first = row.points.data() + 0xC;
    float value;
    std::memcpy(&value, first, 4);
    value = value - x;
    std::memcpy(first, &value, 4);
    std::memcpy(&value, first + 4, 4);
    value = value - y;
    std::memcpy(first + 4, &value, 4);
    std::memcpy(&value, first + 8, 4);
    value = value - z;
    std::memcpy(first + 8, &value, 4);
    const std::span<const u8> grid = context.resolve(read_u32(block, 0x14), 0x48);
    step_runtime_point_graph_collision(row, step * 6.f, grid, context.resolve);
    recover_runtime_point_graph_transform(row);
}
}
