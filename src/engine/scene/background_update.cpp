#include "background_update.h"
#include "../graphics/effect_particles.h"
#include "../graphics/point_graph_solver.h"
#include "../graphics/d3dx_matrix.h"
#include "../entities/euler_wrapping.h"
#include "../entities/entity.h"
#include "gbtg/model_runtime.h"

#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
void store_real(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void store_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
float packed_component(std::span<const u8> record, size_t offset, float scale)
{
    return float(i16(read_u16(record, offset))) * scale;
}
}

// Xbox 0x00045A90
void update_background_animation(RuntimeBackgroundConstruction& background,
    RuntimeBackgroundUpdate& context)
{
    if (background.lifetime.animation == nullptr)
    {
        if (background.lifetime.effect != nullptr)
        {
            tick_actor_effect(*background.lifetime.effect, runtime_frame_step(context.clock), *context.effects);
            const auto header = background.animation.header;
            u16 particle_index = 0;
            const i32 count = i32(read_u32(background.fields, 0x944));
            for (i32 index = 0; index < count; ++index)
            {
                auto& volume = background.volumes[index];
                const float timer = read_f32(volume.fields, 8);
                if (timer > 0.f)
                {
                    const float remaining = timer - runtime_frame_step(context.clock);
                    store_real(volume.fields, 8, remaining);
                    if (0.f >= remaining)
                    {
                        store_real(volume.fields, 8, 0.f);
                    }
                }
                const u32 descriptor = read_u32(volume.fields, 0) * 0xC;
                const u16 table = read_u16(header, descriptor + 0x14);
                u32 item = 0;
                if (header[descriptor + 0x16] != 0)
                {
                    do
                    {
                        const i32 signed_index = i16(particle_index);
                        const EffectParticle& particle =
                            actor_effect_particle(*background.lifetime.effect, size_t(signed_index));
                        const u8 node = header[table + item];
                        ActorMatrixRecord& matrix = actor_matrix_records(background.matrices)[node];
                        matrix.translation = particle.position;
                        background.matrices.progress = 0.f;
                        background.matrices.flags = 0;
                        store_word(background.fields, 0x29C, 0);
                        store_word(background.fields, 0x2A0, 0);
                        matrix.active = 1;
                        matrix.euler = particle.euler;
                        Vec3 euler = matrix.euler;
                        wrap_runtime_euler_angles(euler);
                        matrix.euler = euler;
                        ++item;
                        ++particle_index;
                        matrix.active = 1;
                    }
                    while (item < header[descriptor + 0x16]);
                }
            }
        }
        return;
    }
    // 0x00045C99 tail-jumps to 0x000446A0, which always reads the first background.
    auto& first = *context.first;
    auto& animation = first.animation;
    const auto block = animation.animation_block;
    const u32 count = animation.header[1];
    const u32 recorded_address = read_u32(block, 0x2C);
    if (recorded_address == 0)
    {
        constexpr float longest_step = std::bit_cast<float>(0x3D088889u);
        float step = runtime_actor_rate(first.actor) * runtime_frame_step(context.clock);
        if (step > longest_step)
        {
            step = longest_step;
        }
        step *= 6.f;
        for (u32 index = 0; index < count; ++index)
        {
            auto& graph = animation.rows[index];
            if (graph.record().active != 0)
            {
                const auto grid = context.resolve(read_u32(block, 0x14), 0x48);
                step_runtime_point_graph_collision(graph, step, grid, context.resolve);
                recover_runtime_point_graph_transform(graph);
                const float age = runtime_actor_rate(first.actor) * runtime_frame_step(context.clock)
                    + graph.record().age;
                graph.record().age = age;
                float motion = 0.f;
                const u32 points = graph.header().points;
                for (u32 point = 0; point < points; ++point)
                {
                    const PointGraphPoint& row = graph.point(point);
                    const float z = row.position.z - row.previous.z;
                    const float x = row.position.x - row.previous.x;
                    const float y = row.position.y - row.previous.y;
                    motion = ((z * z + x * x) + y * y) + motion;
                }
                if (((points == 0 || std::bit_cast<float>(0x399D4952u) > motion) && age > 1.f)
                    || age > 4.f)
                {
                    graph.record().active = 0;
                }
            }
        }
        publish_background_graph_matrices(animation, first.matrices.matrices.instance);
    }
    else
    {
        const auto recorded = context.resolve(recorded_address, 0x20);
        u32 graph_index = 0;
        u32 group = 0;
        if (recorded[0] != 0)
        {
            do
            {
                const auto groups = context.resolve(read_u32(recorded, 8) + group * 8, 8);
                u32 item = 0;
                if (groups[0] != 0)
                {
                    do
                    {
                        auto& graph = animation.rows[graph_index];
                        if (graph.record().active != 0)
                        {
                            const float age = runtime_actor_rate(first.actor) * runtime_frame_step(context.clock)
                                + graph.record().age;
                            graph.record().age = age;
                            const auto range = context.resolve(read_u32(groups, 4) + item * 4, 4);
                            const i32 frame = _mm_cvttss_si32(_mm_set_ss(read_f32(recorded, 4) * age));
                            if (frame < i32(read_u16(range, 0)))
                            {
                                const u32 offset = (u32(read_u16(range, 2)) + u32(frame)) * 14;
                                const auto record = context.resolve(read_u32(recorded, 0x1C) + offset, 14);
                                constexpr float quaternion_scale = std::bit_cast<float>(0x3803126Fu);
                                const Quat rotation{packed_component(record, 0, quaternion_scale),
                                    packed_component(record, 2, quaternion_scale), packed_component(record, 4, quaternion_scale),
                                    packed_component(record, 6, quaternion_scale)};
                                const Vec3 position{packed_component(record, 8, .0625f),
                                    packed_component(record, 10, .0625f), packed_component(record, 12, .0625f)};
                                RuntimeShaderMatrix matrix;
                                compose_runtime_graphics_matrix(matrix.data(), {read_f32(recorded, 0x10),
                                    read_f32(recorded, 0x14), read_f32(recorded, 0x18)}, rotation, position);
                                const RuntimeShaderMatrix rest = graph.record().bind_matrix;
                                RuntimeShaderMatrix result;
                                multiply_runtime_graphics_matrices(result.data(), rest.data(), matrix.data());
                                graph.record().placed_matrix = result;
                                write_runtime_model_matrix(first.matrices.matrices.node, first.matrices.matrices.instance,
                                    i32(graph_index), result);
                            }
                            else
                            {
                                graph.record().active = 0;
                            }
                        }
                        ++graph_index;
                        ++item;
                    }
                    while (item < groups[0]);
                }
                ++group;
            }
            while (group < recorded[0]);
        }
    }
}

// Xbox 0x000464D0
void update_runtime_background(RuntimeBackgroundConstruction& background,
    RuntimeBackgroundUpdate& context)
{
    update_background_animation(background, context);
    if (background.actor.model != nullptr && background.actor.model->runtime->shader_animations != nullptr)
    {
        tick_runtime_model_shader_animations(*background.actor.model->runtime->shader_animations, 1.f, context.matrices);
    }
}
}
