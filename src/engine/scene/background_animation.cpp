#include "background_animation.h"
#include "../entities/actor_matrix_setup.h"
#include "../graphics/model_points.h"
#include "../graphics/matrix_inverse.h"
#include "../graphics/point_graph_solver.h"
#include "gbtg/model_runtime.h"
#include <bit>

#include <cstring>

namespace gbtg
{
// Xbox 0x00044430
void publish_background_graph_matrices(RuntimeBackgroundAnimation& animation, std::span<u8> instance)
{
    const i32 count = animation.matrix_count;
    if (count != 0)
    {
        i32 index = 0;
        if (count > 0)
        {
            do
            {
                const PointGraphRecord& record = animation.rows[index].record();
                if (record.active != 0)
                {
                    const RuntimeShaderMatrix matrix = record.placed_matrix;
                    write_runtime_model_matrix(animation.matrix_node, instance, index, matrix);
                }
                ++index;
            }
            while (index < animation.matrix_count);
        }
    }
}

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
                        first->rows[index].record().edges_address_copy = 0;
                        first->rows[index].record().points_address_copy = 0;
                        first->rows[index].allocation = nullptr;
                    }
                }
                for (u32 index = 0; index < count; ++index)
                {
                    const u32 points = read_u32(table, index * 8);
                    const auto source = resolve(read_u32(table, index * 8 + 4));
                    Vec3& output = first->rows[index].record().center;
                    average_runtime_model_points(output,
                        {reinterpret_cast<const Vec3*>(source.data()), points});
                }
                if (first->matrix_count != 0 && first->matrix_count > 0)
                {
                    for (i32 index = 0; index < first->matrix_count; ++index)
                    {
                        const auto source = resolve(runtime_matrix_array_matrix_offset(first->matrix_node, index));
                        PointGraphRecord& row = first->rows[index].record();
                        for (auto* target : {&row.bind_matrix, &row.placed_matrix, &row.rest_matrix})
                        {
                            for (size_t word = 0; word < 16; ++word)
                            {
                                const u32 value = read_u32(source, word * 4);
                                std::memcpy(target->data() + word, &value, 4);
                            }
                        }
                        invert_runtime_graphics_matrix(row.placed_inverse, nullptr, row.placed_matrix);
                        invert_runtime_graphics_matrix(row.bind_inverse, nullptr, row.bind_matrix);
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
    for (u32 index = 0; index < row.header().points; ++index)
    {
        PointGraphPoint& point = row.point(index);
        point.previous.x = point.position.x - x;
        point.previous.y = point.position.y - y;
        point.previous.z = point.position.z - z;
    }
    context.random = context.random * 0x19660D + 0x3C6EF35F;
    const float unit = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000);
    const float share = (unit - 1.f) * 6.f + 2.f; // DAT_0032B0F8, DAT_0032B218, DAT_0032B110.
    x = x * share;
    y = y * share;
    z = z * share;
    Vec3& first = row.point(0).previous;
    first.x = first.x - x;
    first.y = first.y - y;
    first.z = first.z - z;
    const std::span<const u8> grid = context.resolve(read_u32(block, 0x14), 0x48);
    step_runtime_point_graph_collision(row, step * 6.f, grid, context.resolve);
    recover_runtime_point_graph_transform(row);
}
}
