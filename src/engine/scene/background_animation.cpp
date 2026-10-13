#include "background_animation.h"
#include "../entities/actor_matrix_setup.h"
#include "../graphics/model_points.h"
#include "../graphics/matrix_inverse.h"

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
}
