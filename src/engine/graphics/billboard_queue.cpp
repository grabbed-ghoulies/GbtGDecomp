#include "billboard_queue.h"

namespace gbtg
{
BillboardQueue g_billboard_queue;

// Xbox 0x0005C1A0
void queue_billboard(BillboardQueue& queue, const u8* sprite, u32 color, u32 frame, const Vec3& position,
    const float* size, float rotation)
{
    if (queue.count < 0x800)
    {
        auto& entry = queue.entries[size_t(queue.count)];
        entry.sprite = sprite;
        entry.position = position;
        entry.size = {size[0], size[1]};
        entry.rotation = rotation;
        entry.color = color;
        entry.frame = frame;
        ++queue.count;
    }
}
}
