#include "actor_collision_queries.h"
#include "actor_collision_culling.h"
#include "entity.h"
#include <array>

namespace gbtg
{
namespace
{
std::span<const u8> physical_list(const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 header = read_u32(cpu, 0x10);
        const u32 address = read_u32(cpu, header + 4);
        if (address != 0)
        {
            const auto list = cpu.subspan(address);
            if (runtime_collision_shape_list_has_physical_shape(list))
            {
                return list;
            }
        }
    }
    return {};
}
std::span<const u8> model_list(const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        const u32 table = read_u32(cpu, 0);
        for (u32 index = 0; index < count; ++index)
        {
            if (read_u32(cpu, table + index * 8) == 4)
            {
                const u32 address = read_u32(cpu, table + index * 8 + 4);
                return address != 0 ? cpu.subspan(address) : std::span<const u8>{};
            }
        }
    }
    return {};
}
void bind_actor_pose(RuntimeActorCollision& context)
{
    auto* model = context.actor.model;
    const EntityBonePose* pose = nullptr;
    if (model != nullptr)
    {
        pose = model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = model->primary_shape_pose;
        }
    }
    context.shapes.call.pose = pose;
    context.shapes.call.world = context.actor.xform;
    context.shapes.call.inverse = context.actor.xform_inv;
}
}

// Xbox 0x0003DF70
u32 query_runtime_actor_physical_segment(RuntimeActorCollision& context)
{
    const auto list = physical_list(context.actor);
    if (list.data() != nullptr)
    {
        auto& call = context.shapes.call;
        call.mask &= context.actor.model_flags;
        if (call.mask != 0)
        {
            if (!runtime_actor_culls_segment(context.actor, context.bytes, call.candidate, call.kept))
            {
                bind_actor_pose(context);
                std::array<u8, 0x54> record;
                const auto output = call.record;
                call.record = record;
                const u32 result = dispatch_runtime_physical_segment_shapes(list, context.shapes);
                call.record = output;
                if (result != 0)
                {
                    entity_copy_match_block(output.data(), record.data());
                    return 1;
                }
            }
            return 0;
        }
    }
    return 0;
}

// Xbox 0x0003E110
u32 query_runtime_actor_physical_sweep(RuntimeActorCollision& context)
{
    const auto list = physical_list(context.actor);
    if (list.data() != nullptr)
    {
        auto& call = context.shapes.call;
        call.mask &= context.actor.model_flags;
        if (call.mask != 0)
        {
            if (!runtime_actor_culls_swept_sphere(context.actor, context.bytes, call.candidate, call.kept, call.radius))
            {
                bind_actor_pose(context);
                std::array<u8, 0x54> record;
                const auto output = call.record;
                call.record = record;
                const u32 result = dispatch_runtime_physical_swept_shapes(list, context.shapes);
                call.record = output;
                if (result != 0)
                {
                    entity_copy_match_block(output.data(), record.data());
                    return 1;
                }
            }
            return 0;
        }
    }
    return 0;
}

// Xbox 0x0003E050
u32 collect_runtime_actor_physical_contacts(RuntimeActorCollision& context, u32 filter)
{
    const auto list = physical_list(context.actor);
    if (list.data() != nullptr && (filter & context.actor.model_flags) != 0)
    {
        auto& call = context.shapes.call;
        if (!runtime_actor_culls_sphere(context.actor, context.bytes, call.candidate, call.radius))
        {
            bind_actor_pose(context);
            return dispatch_runtime_physical_contact_shapes(list, context.shapes);
        }
        return 0;
    }
    return 0;
}

// Xbox 0x0003E200
u32 query_runtime_actor_model_sweep(RuntimeActorCollision& context)
{
    const auto list = model_list(context.actor);
    if (list.data() != nullptr)
    {
        auto& call = context.shapes.call;
        call.mask &= context.actor.model_flags;
        if (call.mask != 0)
        {
            if (!runtime_actor_culls_swept_sphere(context.actor, context.bytes, call.candidate, call.kept, call.radius))
            {
                bind_actor_pose(context);
                return dispatch_runtime_model_swept_shapes(list, context.shapes);
            }
        }
    }
    return 0;
}

// Xbox 0x0003E2E0
u32 query_runtime_actor_model_segment(RuntimeActorCollision& context)
{
    const auto list = model_list(context.actor);
    if (list.data() != nullptr)
    {
        auto& call = context.shapes.call;
        call.mask &= context.actor.model_flags;
        if (call.mask != 0)
        {
            if (!runtime_actor_culls_segment(context.actor, context.bytes, call.candidate, call.kept)) // 0x0003E4A0
            {
                bind_actor_pose(context);
                return dispatch_runtime_model_segment_shapes(list, context.shapes); // 0x000568E0
            }
        }
    }
    return 0;
}

// Xbox 0x0003E3C0
u32 collect_runtime_actor_model_contacts(RuntimeActorCollision& context)
{
    const auto list = model_list(context.actor);
    if (list.data() != nullptr)
    {
        auto& call = context.shapes.call;
        call.mask &= context.actor.model_flags;
        if (call.mask != 0)
        {
            if (!runtime_actor_culls_sphere(context.actor, context.bytes, call.candidate, call.radius))
            {
                bind_actor_pose(context);
                return dispatch_runtime_model_contact_shapes(list, context.shapes);
            }
        }
    }
    return 0;
}
}
