#include "collision_shape_dispatch.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr u32 physical_methods[3][3]{{0, 0, 0}, {0x53720, 0x53AA0, 0x53F20},
    {0x54520, 0x54850, 0x54C40}};
constexpr u32 model_methods[4][3]{{0x55CB0, 0x55F80, 0x56580}, {0x55A90, 0x55A90, 0x55A90},
    {0, 0, 0}, {0x55CB0, 0x55F80, 0x56580}};

bool coincident(RuntimeCollisionShapeCall& call)
{
    const float dz = call.candidate.z - call.kept.z;
    const float dx = call.candidate.x - call.kept.x;
    const float dy = call.candidate.y - call.kept.y;
    if ((dz * dz + dx * dx) + dy * dy <= std::bit_cast<float>(0x3456BF95u))
    {
        call.kept = call.candidate;
        return true;
    }
    return false;
}
void offset_contact(RuntimeCollisionShapeCall& call)
{
    const float scale = std::bit_cast<float>(0x3A83126Fu);
    for (size_t index = 0; index < 3; ++index)
    {
        const float value = read_f32(call.record, 0xC + index * 4) * scale + read_f32(call.record, index * 4);
        std::memcpy(call.record.data() + index * 4, &value, 4);
    }
    call.kept.x = read_f32(call.record, 0xC) * scale + call.kept.x;
    call.kept.y = read_f32(call.record, 0x10) * scale + call.kept.y;
    call.kept.z = read_f32(call.record, 0x14) * scale + call.kept.z;
}
u32 physical(std::span<const u8> list, RuntimeCollisionShapeDispatch& context, u32 slot)
{
    const i32 count = std::bit_cast<i32>(read_u32(list, 0));
    const u8* shape = list.data() + 0x10;
    u32 result = 0;
    for (i32 index = 0; index < count; ++index)
    {
        const std::span<const u8> bytes{shape, 0x10};
        if ((read_u32(bytes, 4) & context.call.mask) != 0)
        {
            const u32 method = physical_methods[shape[0]][slot];
            if (method != 0)
            {
                context.call.shape = shape;
                context.call.shape_address = context.address_of(shape);
                context.call.data_address = read_u32(list, 0xC);
                if (context.physical_methods.at(method)(context.call) != 0)
                {
                    result = 1;
                }
            }
        }
        shape += read_u32(bytes, 8);
    }
    return result;
}
u32 model(std::span<const u8> list, RuntimeCollisionShapeDispatch& context, u32 slot)
{
    const i32 count = std::bit_cast<i32>(read_u32(list, 0));
    const u8* shape = list.data() + 8;
    u32 result = 0;
    for (i32 index = 0; index < count; ++index)
    {
        const std::span<const u8> bytes{shape, 0x10};
        const i16 bone = std::bit_cast<i16>(read_u16(bytes, 2));
        if ((context.call.mask & context.bone_masks[bone] & read_u32(bytes, 4)) != 0)
        {
            const u32 method = model_methods[shape[0]][slot];
            if (method != 0)
            {
                auto& call = context.call;
                call.shape = shape;
                call.shape_address = context.address_of(shape);
                const auto& node = *reinterpret_cast<const EntityShapeNode*>(shape);
                u32 hit;
                switch (method)
                {
                case 0x55CB0:
                    hit = collide_runtime_model_sphere_segment(node, call.shape_address, call.world,
                        call.inverse, call.pose, call.candidate, call.kept, call.record, call.stationary);
                    break;
                case 0x55F80:
                    hit = collide_runtime_model_sphere_sweep(node, call.shape_address, call.world,
                        call.inverse, call.pose, call.candidate, call.kept, call.radius, call.record, call.stationary);
                    break;
                case 0x56580:
                    hit = collect_runtime_model_sphere_contact(node, call.shape_address, call.world,
                        call.inverse, call.pose, call.candidate, call.radius, call.pool);
                    break;
                default:
                    hit = reject_runtime_model_shape_collision();
                    break;
                }
                if (hit != 0)
                {
                    result = 1;
                }
            }
        }
        shape = shape[1] == 0 ? context.address_zero.data() : shape + shape[1];
    }
    return result;
}
}

// Xbox 0x00055670
u32 dispatch_runtime_physical_segment_shapes(std::span<const u8> list, RuntimeCollisionShapeDispatch& context)
{
    if (coincident(context.call))
    {
        return 0;
    }
    const u32 result = physical(list, context, 0);
    if (result != 0 && context.call.record.data() != nullptr)
    {
        offset_contact(context.call);
    }
    return result;
}
// Xbox 0x000557E0
u32 dispatch_runtime_physical_swept_shapes(std::span<const u8> list, RuntimeCollisionShapeDispatch& context)
{
    if (coincident(context.call))
    {
        return 0;
    }
    const u32 result = physical(list, context, 1);
    if (result != 0 && context.call.record.data() != nullptr)
    {
        offset_contact(context.call);
    }
    return result;
}
// Xbox 0x00055960
u32 dispatch_runtime_physical_contact_shapes(std::span<const u8> list, RuntimeCollisionShapeDispatch& context)
{
    return physical(list, context, 2);
}
// Xbox 0x000568E0
u32 dispatch_runtime_model_segment_shapes(std::span<const u8> list, RuntimeCollisionShapeDispatch& context)
{
    if (coincident(context.call))
    {
        return 0;
    }
    const u32 result = model(list, context, 0);
    if (result != 0 && context.call.record.data() != nullptr)
    {
        offset_contact(context.call);
    }
    return result;
}
// Xbox 0x00056A60
u32 dispatch_runtime_model_swept_shapes(std::span<const u8> list, RuntimeCollisionShapeDispatch& context)
{
    if (coincident(context.call))
    {
        return 0;
    }
    const u32 result = model(list, context, 1);
    if (result != 0 && context.call.record.data() != nullptr)
    {
        offset_contact(context.call);
    }
    return result;
}
// Xbox 0x00056BE0
u32 dispatch_runtime_model_contact_shapes(std::span<const u8> list, RuntimeCollisionShapeDispatch& context)
{
    return model(list, context, 2);
}
}
