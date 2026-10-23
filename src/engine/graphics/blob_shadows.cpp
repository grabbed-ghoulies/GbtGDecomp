#include "blob_shadows.h"

namespace gbtg
{
// Xbox 0x00119A60
i32 register_runtime_blob_shadow(const Vec3& position, float radius, float strength, RuntimeBlobShadows& shadows)
{
    const i32 slot = shadows.next_free;
    if (shadows.enabled == 0 || shadows.count >= 0x20 || slot < 0)
    {
        return -1;
    }
    RuntimeBlobShadow& shadow = shadows.entries[size_t(slot)];
    shadow.position = position;
    shadow.radius = radius;
    shadow.strength = strength;
    shadow.active = 1;
    advance_runtime_blob_shadow_free(shadows); // 0x00119950
    ++shadows.count;
    return slot;
}

// Xbox 0x00119950
void advance_runtime_blob_shadow_free(RuntimeBlobShadows& shadows)
{
    while (true)
    {
        if (shadows.next_free > 0x1F)
        {
            shadows.next_free = -1;
            return;
        }
        if (shadows.entries[size_t(shadows.next_free)].active == 0)
        {
            return;
        }
        ++shadows.next_free;
    }
}
}
