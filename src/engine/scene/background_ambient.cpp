#include "background_ambient.h"

#include <bit>

namespace gbtg
{
namespace
{
Vec3 transform_point(const float* matrix, const Vec3& point)
{
    return {((matrix[1] * point.y + matrix[2] * point.z) + matrix[0] * point.x) + matrix[3],
        ((matrix[5] * point.y + matrix[6] * point.z) + matrix[4] * point.x) + matrix[7],
        ((matrix[9] * point.y + matrix[10] * point.z) + matrix[8] * point.x) + matrix[11]};
}
}

// Xbox 0x000432B0
void initialize_background_ambient_nodes(const Actor& actor, u32& random,
    RuntimeBackgroundAmbient& state)
{
    for (u16 kind : {u16(8), u16(9), u16(0x1F)})
    {
        u32 previous = 0;
        while (true)
        {
            if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
            {
                break;
            }
            const auto cpu = actor.model->cpu;
            const u32 count = read_u32(cpu, 4);
            u32 block = 0;
            const u32 table = read_u32(cpu, 0);
            for (u32 index = 0; index < count; ++index)
            {
                if (read_u32(cpu, table + index * 8) == 2)
                {
                    block = read_u32(cpu, table + index * 8 + 4);
                    break;
                }
            }
            if (block == 0)
            {
                break;
            }
            u32 node = previous == 0 ? block + 4 : previous + 0x34;
            const u32 end = block + 4 + read_u32(cpu, block) * 0x34u;
            while (node < end && read_u16(cpu, node) != kind)
            {
                node += 0x34;
            }
            if (node >= end)
            {
                break;
            }
            previous = node;
            const EntityBonePose* pose = actor.model->alternate_shape_pose;
            if (pose == nullptr)
            {
                pose = actor.model->primary_shape_pose;
            }
            Vec3 point{read_f32(cpu, node + 0x10), read_f32(cpu, node + 0x20), read_f32(cpu, node + 0x30)};
            const i16 bone = std::bit_cast<i16>(read_u16(cpu, node + 2));
            if (bone >= 0)
            {
                const float* matrix;
                if (pose->packed_flag() != 0)
                {
                    matrix = pose->matrix_data() + i32(bone) * 12;
                }
                else
                {
                    const float* source = pose->matrix_data() + i32(bone) * 16;
                    for (u32 row = 0; row < 3; ++row)
                    {
                        for (u32 column = 0; column < 4; ++column)
                        {
                            state.matrix_scratch[row * 4 + column] = source[column * 4 + row];
                        }
                    }
                    matrix = state.matrix_scratch.data();
                }
                point = transform_point(matrix, point);
            }
            point = transform_point(actor.xform, point);
            if (kind == 8)
            {
                const i32 index = state.count;
                ++state.count;
                state.emitters[index] = point;
            }
            else if (kind == 9)
            {
                state.vary_point = point;
                random = random * 0x19660Du + 0x3C6EF35Fu;
                const float unit = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
                const float scale = unit * 0.3f + 0.7f; // DAT_0032B268 / DAT_0032B264.
                state.vary_enabled = 1;
                state.vary_scale = scale;
                state.light_color[3] = 1.f;
                state.light_color[2] = scale;
                state.light_color[1] = scale;
                state.light_color[0] = scale;
            }
            else
            {
                state.phase_point = point;
                state.phase_enabled = 1;
            }
        }
    }
}
}
