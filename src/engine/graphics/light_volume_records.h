#pragma once
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
#include <array>

namespace gbtg
{
// One model that casts into a light volume: the record's own caster at +0 and each extra caster at +0x4C,
// stride 0x4C.
struct LightVolumeCaster
{
    RuntimeModelInstance* model = nullptr; // +0
    ModelPoseBuffer* pose = nullptr; // +4
    Vec3 position{}; // +8
    float radius = 0.f; // +0x14
    float height = 0.f; // +0x18
    std::array<float, 12> matrix{}; // +0x1C
};
// DAT_00509C20, stride 0x25C. The frame's shadow volumes, filled up to DAT_0050D71C.
struct LightVolumeRecord
{
    LightVolumeCaster caster; // +0. 0x000FE1D0 clears +0.
    std::array<LightVolumeCaster, 5> casters{}; // +0x4C
    i32 caster_count = 0; // +0x1C8
    float radius = 0.f; // +0x1CC
    u32 flags = 0; // +0x1D0, the actor's +0x331 byte.
    float distance = 0.f; // +0x1D4, squared distance from the view.
    u32 player = 0; // +0x1D8
    Vec3 light{}; // +0x1DC
    u32 tail = 0; // +0x1E8. 0x000FE1D0 clears it.
    u16 kind = 0; // +0x1F0, the group word 0x000FE1D0 writes.
};
constexpr i32 kLightVolumeRecords = 0x19;
extern std::array<LightVolumeRecord, kLightVolumeRecords> g_light_records; // DAT_00509C20.

// Registers a volume at `position` toward `light` (ECX, EDX) for `model` with `pose` and `matrix`; XMM0 is the
// radius and XMM1 the height. Returns the record index, or -1 once 0x19 are filled.
i32 register_runtime_light_volume(const Vec3& position, const Vec3& light, RuntimeModelInstance* model,
    ModelPoseBuffer* pose, const float* matrix, u32 flags, u32 player, float radius, float height,
    const Vec3& view, i32& count); // Xbox 0x000FE320
// Adds a further caster to record `index` (EAX), up to five; XMM0 is the radius.
void add_runtime_light_volume_caster(i32 index, const Vec3& position, RuntimeModelInstance* model,
    ModelPoseBuffer* pose, const float* matrix, float height, float radius); // Xbox 0x000FE260
}
