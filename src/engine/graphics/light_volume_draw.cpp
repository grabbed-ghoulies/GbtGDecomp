#include "light_volume_draw.h"
#include "light_volume_records.h"
#include "../runtime/crt_sort.h"

namespace gbtg
{
// Xbox 0x000FE430
int compare_runtime_light_volumes(const void* left, const void* right)
{
    const auto& first = g_light_records[*static_cast<const u8*>(left)];
    const auto& second = g_light_records[*static_cast<const u8*>(right)];
    if (first.flags == second.flags)
    {
        if (second.distance > first.distance)
        {
            return -1;
        }
        return 1;
    }
    if (first.flags == 0 || second.flags != 0)
    {
        return 1;
    }
    return -1;
}

// Xbox 0x000FE480
void order_runtime_light_volumes(RuntimeLightVolumeOrder& context)
{
    for (i32 index = 0; index < context.count; ++index)
    {
        context.order[size_t(index)] = u8(index);
    }
    if (context.limit < context.count)
    {
        runtime_quick_sort(context.order.data(), size_t(context.count), 1, compare_runtime_light_volumes); // 0x00166780
        context.count = context.limit;
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
