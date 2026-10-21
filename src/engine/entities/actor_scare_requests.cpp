#include "actor_scare_requests.h"
#include <array>
#include <bit>
#include "entity.h"
#include "actor_model_anchors.h"
#include "actor_transform.h"
#include "gbtg/model_runtime.h"

#include <cstring>

namespace gbtg
{
namespace
{
template<class T>
void store(u8* bytes, size_t offset, const T& value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

Vec3 transform_scare_point(const float* matrix, const Vec3& point)
{
    return Vec3{matrix[2] * point.z + matrix[1] * point.y + matrix[0] * point.x + matrix[3],
        matrix[4] * point.x + matrix[6] * point.z + matrix[5] * point.y + matrix[7],
        matrix[8] * point.x + matrix[10] * point.z + matrix[9] * point.y + matrix[11]};
}
}

// Xbox 0x00122CE0
std::function<void(std::span<const u32>)> g_marked_actor_message;

void broadcast_actor_world_message(std::span<const u32> message)
{
    if (!g_marked_actor_message)
    {
        throw Error("0x00122CE0 walks DAT_00511B38 outside a native scene");
    }
    g_marked_actor_message(message);
}

// Xbox 0x0003DB90
long double actor_model_height(const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            do
            {
                if (read_u32(cpu, table + index * 8) == 5)
                {
                    const u32 offset = read_u32(cpu, table + index * 8 + 4);
                    if (offset != 0)
                    {
                        const float height = float(static_cast<long double>(actor.scale_200) *
                            read_f32(cpu, offset + 0x20) * actor.radius_xz);
                        return animation_absolute(height);
                    }
                    break;
                }
                ++index;
            }
            while (index < count);
        }
    }
    return 0.L;
}

// Xbox 0x0013B270
void request_actor_scare_at(const Vec3& position, u32 actor_address, float intensity,
    float scale, i32 broadcast, u32 value, ActorScareContext& context)
{
    if ((context.flags & 8) == 0)
    {
        auto parameters = clone_executable_resource(context.resources, "aid_objparams_ghoulies_misc_scare");
        if (parameters.bytes != nullptr)
        {
            if (context.player_address != 0 && context.player_address != 0x93C)
            {
                if (intensity == 0.f)
                {
                    intensity = 1.f;
                }
                store(parameters.bytes.get(), 0x334, intensity);
                store(parameters.bytes.get(), 0x33C, value);
                store(parameters.bytes.get(), 8, position);
                store(parameters.bytes.get(), 0x14, context.default_euler);
                if (scale == 0.f)
                {
                    scale = 1.f;
                }
                store(parameters.bytes.get(), 0x20, scale);
                context.objects.push_back(create_factory_object(context.factory,
                    {parameters.bytes.get(), parameters.size}));
                parameters.bytes.reset();
                if (broadcast != 0)
                {
                    const std::array<u32, 4> message{0x37, std::bit_cast<u32>(position.x),
                        std::bit_cast<u32>(position.y), std::bit_cast<u32>(position.z)};
                    broadcast_actor_world_message(message);
                }
                context.objects.push_back(create_resource_object_with_data(context.factory,
                    context.resources, "aid_objparams_ghoulies_fx_solidredflash", actor_address));
            }
            else
            {
                // Retail leaves this clone allocated when the player is absent.
                parameters.bytes.release();
            }
        }
    }
}

// Xbox 0x0013B3A0
void request_actor_scare(Actor& actor, u32 actor_address, float intensity, float scale,
    i32 broadcast, u32 value, ActorScareContext& context)
{
    const u8* binding = find_actor_model_binding(actor, 6);
    Vec3 position;
    if (binding != nullptr)
    {
        const ModelPoseBuffer* pose;
        if (actor.model != nullptr)
        {
            pose = actor.model->runtime->alternate;
            if (pose == nullptr)
            {
                pose = actor.model->runtime->primary;
            }
        }
        else
        {
            pose = nullptr;
        }
        position = Vec3{read_f32({binding, 0x34}, 0x10), read_f32({binding, 0x34}, 0x20),
            read_f32({binding, 0x34}, 0x30)};
        i16 joint;
        std::memcpy(&joint, binding + 2, 2);
        if (joint >= 0)
        {
            position = transform_scare_point(model_pose_matrix(*pose, joint).data(), position);
        }
        position = transform_scare_point(actor.xform, position);
    }
    else
    {
        position = actor_world_position(actor);
        position.y = float(actor_model_height(actor) * 0.5L + position.y);
    }
    request_actor_scare_at(position, actor_address, intensity, scale, broadcast, value, context);
}
}
