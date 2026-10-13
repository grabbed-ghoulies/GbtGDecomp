#include "physical_shape_events.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0012C930
u32 set_physical_shape_event_bits(std::span<u8> event, u32 mask)
{
    const u32 flags = read_u32(event, 0xC) | mask;
    std::memcpy(event.data() + 0xC, &flags, 4);
    return 1;
}

// Xbox 0x0012C950
u32 clear_physical_shape_event_bits(std::span<u8> event, u32 mask)
{
    const u32 flags = read_u32(event, 0xC) & ~mask;
    std::memcpy(event.data() + 0xC, &flags, 4);
    return 1;
}

// Xbox 0x0012C8A0
void visit_scene_script_shape_events(u32 first, u8 category,
    const std::function<std::span<const u8>(u32)>& resolve,
    PhysicalShapeEventContext& context)
{
    u32 address = first; // DAT_00511AF4.
    while (true)
    {
        if (address == 0)
        {
            return;
        }
        const auto object = resolve(address);
        const u32 reference = read_u32(object, 0x24);
        if (reference != 0)
        {
            const u32 cpu = read_u32(resolve(reference), 0);
            if (cpu != 0)
            {
                const u32 runtime = read_u32(resolve(cpu), 0x10);
                const u32 shape_address = read_u32(resolve(runtime), 4);
                if (shape_address != 0)
                {
                    const auto shapes = resolve(shape_address);
                    i32 index = 0;
                    auto shape = shapes.subspan(0x10);
                    bool found = false;
                    if (i32(read_u32(shapes, 0)) > 0)
                    {
                        do
                        {
                            if (read_u32(shape, 0xC) == 0x1235 && shape[0] < 3)
                            {
                                found = true;
                                break;
                            }
                            ++index;
                            shape = shape.subspan(read_u32(shape, 8));
                        }
                        while (index < i32(read_u32(shapes, 0)));
                    }
                    if (found)
                    {
                        context.category = category;
                        context.subtype = 0;
                        context.mask = 0;
                        visit_model_physical_shape_events(shapes, context);
                    }
                }
            }
        }
        address = read_u32(resolve(address), 4);
    }
}

// Xbox 0x000555C0
u32 visit_physical_shape_event_list(const PhysicalShapeEventList& list,
    PhysicalShapeEventContext& context)
{
    const u8* current = list.records;
    const u8* end = current + list.count * 0x14u;
    while (true)
    {
        if (end <= current)
        {
            return 1;
        }
        if ((context.mask == 0 || (read_u32({current, 0x14}, 0xC) & context.mask) != 0) &&
            (context.category == 0 || current[0x10] == context.category) &&
            (context.subtype == 0 || current[0x11] == context.subtype) &&
            context.callback(current, context.user) == 0)
        {
            break;
        }
        current += 0x14;
    }
    return 0;
}

// Xbox 0x00054110
u32 visit_compound_shape_events(std::span<const u8> shape,
    PhysicalShapeEventContext& context)
{
    i32 index = 0;
    if (read_u16(shape, 0x20) != 0)
    {
        do
        {
            if (visit_physical_shape_event_list(
                context.lists.at(shape.data() + 0x2C + index * 8), context) == 0)
            {
                return 0;
            }
            ++index;
        }
        while (index < i32(read_u16(shape, 0x20)));
    }
    return 1;
}

// Xbox 0x00054DB0
u32 visit_single_shape_events(std::span<const u8> shape,
    PhysicalShapeEventContext& context)
{
    return visit_physical_shape_event_list(context.lists.at(shape.data() + 0x10), context);
}

// Xbox 0x00055A30
void visit_model_physical_shape_events(std::span<const u8> shapes,
    PhysicalShapeEventContext& context)
{
    using Visitor = u32 (*)(std::span<const u8>, PhysicalShapeEventContext&);
    // DAT_0042381C, stride 0x14; the type-0 entry has no visitor.
    constexpr Visitor visitors[]{nullptr, visit_compound_shape_events, visit_single_shape_events};
    i32 index = 0;
    auto shape = shapes.subspan(0x10);
    if (0 < i32(read_u32(shapes, 0)))
    {
        do
        {
            if ((context.mask == 0 || (read_u32(shape, 4) & context.mask) != 0) &&
                visitors[shape[0]] != nullptr && visitors[shape[0]](shape, context) == 0)
            {
                return;
            }
            ++index;
            shape = shape.subspan(read_u32(shape, 8));
        }
        while (index < i32(read_u32(shapes, 0)));
    }
}
}
