#include "light_volume_records.h"
#include <algorithm>

namespace gbtg
{
std::array<LightVolumeRecord, kLightVolumeRecords> g_light_records{};

// Xbox 0x000FE320
i32 register_runtime_light_volume(const Vec3& position, const Vec3& light, RuntimeModelInstance* model,
    ModelPoseBuffer* pose, const float* matrix, u32 flags, u32 player, float radius, float height,
    const Vec3& view, i32& count)
{
    if (count < 0x19)
    {
        auto& record = g_light_records[size_t(count)];
        record.flags = flags;
        record.player = player;
        record.caster.model = model;
        record.caster.pose = pose;
        record.caster.position = position;
        record.light = light;
        record.caster.height = height;
        record.caster.radius = radius;
        record.radius = radius;
        record.caster_count = 0;
        record.distance = (view.z - position.z) * (view.z - position.z) +
            (view.y - position.y) * (view.y - position.y) + (view.x - position.x) * (view.x - position.x);
        std::copy_n(matrix, 12, record.caster.matrix.begin());
        const i32 index = count;
        count = count + 1;
        return index;
    }
    return -1;
}

// Xbox 0x000FE260
void add_runtime_light_volume_caster(i32 index, const Vec3& position, RuntimeModelInstance* model,
    ModelPoseBuffer* pose, const float* matrix, float height, float radius)
{
    auto& record = g_light_records[size_t(index)];
    if (record.caster_count < 5)
    {
        auto& caster = record.casters[size_t(record.caster_count)];
        caster.model = model;
        caster.pose = pose;
        std::copy_n(matrix, 12, caster.matrix.begin());
        caster.position = position;
        caster.radius = radius;
        caster.height = height;
        record.caster_count = record.caster_count + 1;
    }
}
}
