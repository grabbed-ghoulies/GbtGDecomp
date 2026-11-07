#include "actor_model_pieces.h"
#include "gbtg/model_nodes.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
// DAT_0054DD7C's step and its [1, 2) mantissa float.
float next_unit(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u);
}

// Model-internal pointers in the actor's CPU copy are offsets into it.
std::span<const u8> model_range(std::span<const u8> cpu, u32 offset, size_t size)
{
    if (offset > cpu.size() || size > cpu.size() - offset)
    {
        throw Error("actor model piece list lies outside the model");
    }
    return cpu.subspan(offset, size);
}
}

// Xbox 0x0003F170
void build_actor_model_pieces(const Actor& actor, std::vector<u8>& pieces, u32& random)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return;
    }
    const auto cpu = actor.model->cpu;
    const u32 block = runtime_model_piece_header(cpu);
    if (block == 0)
    {
        return;
    }
    const u8 count = cpu[block + 1];
    pieces.assign(size_t(count) * sizeof(ActorModelPiece), 0); // 0x000F72D0.
    constexpr float one = 1.f; // DAT_0032B0F8.
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    constexpr float quarter_turn = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.
    constexpr float two_turns = std::bit_cast<float>(0x41490FDBu); // DAT_0032B21C.
    for (u32 index = 0; index < count; ++index)
    {
        ActorModelPiece& row = actor_model_piece(pieces, index);
        row.euler.z = 0.f;
        row.euler.y = 0.f;
        row.euler.x = 0.f;
        row.turn.z = 0.f;
        row.turn.y = 0.f;
        row.turn.x = 0.f;
        row.age = 0.f;
        row.active = 0u;
        const u32 lists = read_u32(cpu, block + 8);
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
        float radius = 4.f; // DAT_0032B1B0.
        u32 points = 0;
        if (lists != 0)
        {
            points = read_u32(cpu, lists + index * 8);
        }
        if (points == 0)
        {
            row.extent = 0.f;
        }
        else
        {
            const auto list = model_range(cpu, read_u32(cpu, lists + index * 8 + 4), size_t(points) * 0xC);
            for (u32 point = 0; point < points; ++point)
            {
                x += read_f32(list, point * 0xC);
                y += read_f32(list, point * 0xC + 4);
                z += read_f32(list, point * 0xC + 8);
            }
            long double scale = static_cast<long double>(i32(points));
            if (i32(points) < 0)
            {
                scale += 4294967296.0L; // DAT_0032B144.
            }
            scale = static_cast<long double>(one) / scale;
            x = float(static_cast<long double>(x) * scale);
            y = float(static_cast<long double>(y) * scale);
            z = float(static_cast<long double>(z) * scale);
            long double distance = 0.0L;
            for (u32 point = 0; point < points; ++point)
            {
                const long double dx = static_cast<long double>(x) - read_f32(list, point * 0xC);
                const long double dy = static_cast<long double>(y) - read_f32(list, point * 0xC + 4);
                const long double dz = static_cast<long double>(z) - read_f32(list, point * 0xC + 8);
                distance = std::sqrt((dz * dz + dy * dy) + dx * dx) + distance;
            }
            long double divisor = static_cast<long double>(i32(points));
            if (i32(points) < 0)
            {
                divisor += 4294967296.0L;
            }
            radius = float(distance / divisor);
            row.turn.z = 0.f;
            row.turn.y = 0.f;
            row.turn.x = 0.f;
            float max_x = read_f32(list, 0);
            float max_y = read_f32(list, 4);
            float max_z = read_f32(list, 8);
            float min_x = max_x;
            float min_y = max_y;
            float min_z = max_z;
            for (u32 point = 1; point < points; ++point)
            {
                const float px = read_f32(list, point * 0xC);
                const float py = read_f32(list, point * 0xC + 4);
                const float pz = read_f32(list, point * 0xC + 8);
                if (px > max_x)
                {
                    max_x = px;
                }
                if (py > max_y)
                {
                    max_y = py;
                }
                if (pz > max_z)
                {
                    max_z = pz;
                }
                if (min_x > px)
                {
                    min_x = px;
                }
                if (min_y > py)
                {
                    min_y = py;
                }
                if (min_z > pz)
                {
                    min_z = pz;
                }
            }
            const float extent_y = max_y - min_y;
            const float extent_x = max_x - min_x;
            const float extent_z = max_z - min_z;
            if (extent_y > extent_x || extent_y > extent_z)
            {
                if (extent_x > extent_z)
                {
                    row.turn.x = quarter_turn;
                    row.extent = extent_z * 0.5f; // DAT_0032B0F4.
                }
                else
                {
                    row.turn.z = quarter_turn;
                    row.extent = extent_x * 0.5f;
                }
            }
            else
            {
                row.extent = extent_y * 0.5f;
            }
            row.turn.y = (next_unit(random) - one) * turn;
        }
        radius *= std::bit_cast<float>(0x3F4CCCCDu); // DAT_0032B244.
        row.center.z = z;
        row.center.x = x;
        row.center.y = y;
        constexpr float largest = 5.f; // DAT_0032B184.
        if (radius > largest)
        {
            radius = largest;
        }
        row.radius = radius;
        const float first = next_unit(random);
        const float second = next_unit(random);
        row.spin.x = (second - one) * two_turns - turn;
        row.spin.z = 0.f;
        row.spin.y = (first - one) * two_turns - turn;
    }
}

// Xbox 0x0003F820, EAX group
void launch_actor_model_piece_group(Actor& actor, i32 group, std::vector<u8>& pieces, const ActorMatrixSetup& matrices,
    RuntimeActorMatrixSampling& sampling, const std::function<std::span<const u8>(u32)>& resolve, u32& random)
{
    if (pieces.empty()) // +0x2A4.
    {
        return;
    }
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr || actor.model->runtime == nullptr)
    {
        throw Error("0x0003F820 launches pieces of an actor without a model instance");
    }
    const auto cpu = actor.model->cpu;
    const u32 header = runtime_model_piece_header(cpu);
    const u8 count = runtime_model_piece_group_size(cpu, header, u32(group));
    const u32 list = runtime_model_piece_group_list(cpu, header, u32(group));
    // The world matrix through the root bone, inverted: piece offsets are measured in the model's root frame.
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, 48);
    const ModelPoseBuffer* pose = actor.model->runtime->alternate;
    if (pose == nullptr)
    {
        pose = actor.model->runtime->primary;
    }
    if (pose == nullptr)
    {
        throw Error("0x0003F820 reads the root bone of a model instance without a pose");
    }
    multiply_affine_matrices(world.data(), world.data(), model_pose_matrix(*pose, 0).data()); // 0x00031530, 0x001041B0
    invert_model_matrix(world.data(), world.data()); // 0x00104400
    const auto& w = world;
    for (u32 index = 0; index < count; ++index)
    {
        const u8 piece = cpu[list + index];
        ActorModelPiece& row = actor_model_piece(pieces, piece);
        const u32 one = 1;
        row.active = one;
        const auto matrix = resolve(runtime_matrix_array_matrix_offset(matrices.matrices.node, piece)); // 0x00155FF0
        std::memcpy(row.matrix.data(), matrix.data(), sizeof(row.matrix));
        Vec3 position;
        Vec3 euler;
        sample_actor_model_matrix(actor, piece, position, euler, 1, sampling); // 0x00040640
        row.position = position;
        row.euler = euler;
        const float px = row.position.x;
        const float py = row.position.y;
        const float pz = row.position.z;
        const float dx = ((py * w[1] + pz * w[2]) + px * w[0]) + w[3] - row.center.x;
        const float dy = ((px * w[4] + py * w[5]) + pz * w[6]) + w[7] - row.center.y;
        const float dz = ((px * w[8] + py * w[9]) + pz * w[10]) + w[11] - row.center.z;
        const long double length = std::sqrt(std::sqrt((static_cast<long double>(dz) * dz +
            static_cast<long double>(dy) * dy) + static_cast<long double>(dx) * dx));
        float extent = float(static_cast<long double>(row.extent) - length);
        if (0.f > extent)
        {
            extent = 0.f;
        }
        row.extent = extent;
        const float first = next_unit(random);
        const float second = next_unit(random);
        const float third = next_unit(random);
        constexpr float spread = std::bit_cast<float>(0x3FB33333u); // DAT_0032B3D0.
        constexpr float offset = std::bit_cast<float>(0x3F333333u); // DAT_0032B264.
        row.velocity.x = (first - 1.f) * spread - offset;
        row.velocity.y = (second - 1.f) * offset;
        row.velocity.z = (third - 1.f) * spread - offset;
    }
}
}
