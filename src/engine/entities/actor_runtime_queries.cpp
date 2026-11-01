#include "actor_runtime_queries.h"
#include "actor_transform.h"
#include "entity.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float float_at(const u8* bytes, size_t offset)
{
    float value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

}

// Actor +0x204 node table: the first entry of the kind names its data block, or null.
const u8* runtime_actor_model_block(const Actor& actor, u32 kind)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return nullptr;
    }
    const auto cpu = actor.model->cpu;
    const u32 count = read_u32(cpu, 4);
    const u32 table = read_u32(cpu, 0);
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(cpu, table + index * 8) == kind)
        {
            const u32 offset = read_u32(cpu, table + index * 8 + 4);
            return offset != 0 ? cpu.data() + offset : nullptr;
        }
    }
    return nullptr;
}

namespace
{
const u8* model_block(const Actor& actor, u32 kind)
{
    return runtime_actor_model_block(actor, kind);
}

bool shape_matches(const u8* record, u32 mask, u8 kind, i16 id)
{
    u32 record_mask;
    std::memcpy(&record_mask, record + 4, 4);
    if ((record_mask & mask) == 0 || record[0] != kind)
    {
        return false;
    }
    if (id == -1)
    {
        return true;
    }
    i16 record_id;
    std::memcpy(&record_id, record + 2, 2);
    return record_id == id;
}

// Walks the records from record by their +1 byte step; a zero step ends the walk.
const u8* scan_shapes(const u8* record, u32 mask, u8 kind, i16 id)
{
    while (record != nullptr)
    {
        if (shape_matches(record, mask, kind, id))
        {
            return record;
        }
        if (record[1] == 0)
        {
            return nullptr;
        }
        record += record[1];
    }
    return nullptr;
}
}

// Xbox 0x0003D860
float runtime_actor_view_radius(const Actor& actor, std::span<const u8> actor_bytes)
{
    const u8* block = model_block(actor, 5);
    if (block == nullptr)
    {
        return 0.f;
    }
    const float scale = actor.scale_200 * actor.radius_xz;
    const float extent = float(static_cast<long double>(engine_absolute(float_at(block, 0) * scale))
        * actor.model_factor);
    const float bound = actor.model_extent * scale; // +0x1D0
    if (bound > extent)
    {
        return bound;
    }
    return extent;
}

// Xbox 0x00040550
float runtime_actor_matrix_reach(const Actor& actor, ActorMatrixSetup& matrices)
{
    if (matrices.flags == 0)
    {
        const Vec3 position = actor_world_position(actor);
        matrices.progress = 0.f;
        for (i32 index = 0; index < matrices.matrices.count; ++index)
        {
            const u8* record = matrices.matrices.records.data() + size_t(index) * 0x48;
            u32 active;
            std::memcpy(&active, record + 0x44, 4);
            if (active == 0)
            {
                continue;
            }
            const float dx = position.x - float_at(record, 4);
            const float dy = position.y - float_at(record, 8);
            const float dz = position.z - float_at(record, 0xC);
            const float distance = (dz * dz + dx * dx) + dy * dy;
            if (distance > matrices.progress)
            {
                matrices.progress = distance;
            }
        }
        if (matrices.progress > 0.f)
        {
            matrices.progress = float(retail_square_root(matrices.progress));
        }
        matrices.flags = 1;
    }
    return matrices.progress;
}

// Xbox 0x00041CB0
bool runtime_actor_sound_playing(const Actor& actor, const RuntimeActorSoundQueues& queues)
{
    for (const ActorSoundSlot& slot : actor.sound_slots)
    {
        if (actor_sound_is_active(slot, queues) && i16(slot.previous_bone) != -1)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00042560
const u8* find_runtime_actor_shape(const Actor& actor, u32 mask, u8 kind, i16 id)
{
    const u8* block = model_block(actor, 4);
    if (block == nullptr)
    {
        return nullptr;
    }
    return scan_shapes(block + 8, mask, kind, id);
}

// Xbox 0x000425D0
const u8* next_runtime_actor_shape(const Actor& actor, const u8* record, u32 mask, u8 kind, i16 id)
{
    // Retail also locates the actor's kind-4 entry here but never uses it.
    (void)actor;
    if (record[1] == 0)
    {
        return nullptr;
    }
    return scan_shapes(record + record[1], mask, kind, id);
}

// Xbox 0x001205F0
bool runtime_cylinders_apart(const EntityCylinderBound& first, const EntityCylinderBound& second)
{
    if (second.y > first.height + first.y)
    {
        return true;
    }
    if (first.y > second.height + second.y)
    {
        return true;
    }
    const float reach = first.radius + second.radius;
    const float dz = second.z - first.z;
    const float dx = second.x - first.x;
    const long double root = retail_square_root(dz * dz + dx * dx);
    float distance = float(root);
    if (static_cast<long double>(std::bit_cast<float>(0x3727C5ACu)) > root) // DAT_0032B230.
    {
        distance = reach - 0.01f; // DAT_0032B124.
    }
    return distance >= reach;
}
}
