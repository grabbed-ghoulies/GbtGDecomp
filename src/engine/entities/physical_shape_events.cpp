#include "physical_shape_events.h"

namespace gbtg
{
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
