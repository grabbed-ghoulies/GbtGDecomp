#include "blob_shadows.h"
#include "scene_point_light_pass.h"

namespace gbtg
{
// Xbox 0x00119AD0
void draw_runtime_blob_shadows(const RuntimeBlobShadows& shadows, u32 suppress, RuntimeScenePointLightPass& pass)
{
    if (shadows.enabled == 0 || shadows.count == 0 || suppress != 0) // DAT_0050EF18, DAT_0050EF10, DAT_0050EF1C
    {
        return;
    }
    constexpr u32 kBlack = 0xFF000000u; // DAT_00314F4C
    constexpr u32 kWhite = 0xFFFFFFFFu; // DAT_00314F48
    pass.targets.copy_mode = 1; // DAT_0050486C
    pass.targets.mode = 0; // DAT_00504870
    begin_runtime_scene_point_lights(shadows.enabled, kBlack, pass); // 0x00062B00, EAX the grid
    i32 drawn = 0;
    for (const RuntimeBlobShadow& shadow : shadows.entries) // DAT_0050EC10, stride 0x18
    {
        if (shadows.count <= drawn)
        {
            break;
        }
        if (shadow.active != 0)
        {
            add_runtime_scene_point_light(shadows.enabled, shadow.position, kWhite, kWhite, 1.f, 1.f, shadow.radius,
                shadow.strength, 1, pass); // 0x00063770, EAX the grid
            ++drawn;
        }
    }
    finish_runtime_scene_point_lights(pass); // 0x00063320
}

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

// Xbox 0x001199F0
void reset_runtime_blob_shadows(std::array<RuntimeBlobShadow, 0x20>& entries, i32& count, i32& next_free,
    u32& enabled, RuntimeScenePointLightTargets& targets, const std::function<void(std::uintptr_t)>& release)
{
    for (auto& entry : entries)
    {
        entry.active = 0; // DAT_0050EC24, stride 0x18
    }
    count = 0; // DAT_0050EF10
    next_free = 0; // DAT_0050EF14
    enabled = 0; // DAT_0050EF18
    targets.light_texture = 0;
    targets.light_surface = 0;
    if (targets.depth_texture != 0)
    {
        release(targets.depth_texture); // D3DResource_Release
        targets.depth_texture = 0;
    }
    if (targets.depth_surface != 0)
    {
        release(targets.depth_surface);
        targets.depth_surface = 0;
    }
}
}
