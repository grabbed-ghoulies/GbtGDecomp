#include "light_volume_draw.h"

namespace gbtg
{
// Xbox 0x000FE480
void order_runtime_light_volumes(RuntimeLightVolumeOrder& context)
{
    for (i32 index = 0; index < context.count; ++index)
    {
        context.order[size_t(index)] = u8(index);
    }
    if (context.limit < context.count)
    {
        // CRT qsort with the distance comparator 0x000FE430.
        throw Error("light volume ordering comparator 0x000FE430 is not translated");
    }
}

// Xbox 0x000FE960
void draw_runtime_light_volumes(RuntimeLightVolumeDraw& context)
{
    const auto header = context.background_header;
    if (header.data() == nullptr || read_u32(header, read_u32(header, 0x10)) == 0)
    {
        return;
    }
    order_runtime_light_volumes(context.order);
    for (i32 index = 0; index < context.order.count; ++index)
    {
        context.draw_volume(context.order.order[size_t(index)]);
    }
    context.bind_projection();
}
}
