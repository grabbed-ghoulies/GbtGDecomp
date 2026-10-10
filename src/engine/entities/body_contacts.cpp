#include "body_contacts.h"
#include "actor_transform.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "segment_records.h"
#include "shape_transform.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
void engine_copy_bytes(void* destination, const void* source, u32 count); // Xbox 0x000F71F0

namespace
{
using OverrideRow = RuntimeBodyCollisionOverride;

OverrideRow* override_rows(const GhoulySpawnPool& pool)
{
    return reinterpret_cast<OverrideRow*>(pool.begin);
}

OverrideRow* override_end(const GhoulySpawnPool& pool)
{
    return reinterpret_cast<OverrideRow*>(pool.cursor);
}

void age_row(OverrideRow& row, float step)
{
    row.retained = std::bit_cast<u32>(std::bit_cast<float>(row.retained) + step);
}

Vec3 record_vec3(const RuntimeBodyCollisionRecord& record, size_t offset)
{
    Vec3 value;
    std::memcpy(&value, record.bytes.data() + offset, sizeof(value));
    return value;
}
}

// Xbox 0x0013ED20
void age_runtime_body_contact_overrides(GhoulySpawnPool& overrides, const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    OverrideRow* row = override_rows(overrides);
    OverrideRow* end = override_end(overrides);
    // Retail rounds the 0x10-byte row count up and walks four rows while four remain.
    if (end - row >= 4)
    {
        do
        {
            age_row(row[0], step);
            age_row(row[1], step);
            age_row(row[2], step);
            age_row(row[3], step);
            row += 4;
        } while (row < end - 3);
    }
    for (; row < end; ++row)
    {
        age_row(*row, step);
    }
}

// Xbox 0x0013EE10
void suspend_runtime_body_contact_overrides(GhoulySpawnPool& overrides)
{
    OverrideRow* end = override_end(overrides);
    OverrideRow* row = override_rows(overrides);
    if (row < end)
    {
        do
        {
            if (row->active == 0)
            {
                const u32 filter = row->actor->model_flags;
                if (filter == 0)
                {
                    u8* last = overrides.cursor - overrides.stride;
                    if (reinterpret_cast<u8*>(row) < last)
                    {
                        engine_copy_bytes(row, last, overrides.stride);
                    }
                    --end;
                    overrides.cursor -= overrides.stride;
                    --row;
                }
                else
                {
                    row->actor->model_flags = 0;
                    row->saved = filter;
                    row->active = 1;
                }
            }
            ++row;
        } while (row < end);
    }
}

// Xbox 0x0013EBE0
u32 insert_runtime_body_contact_override(GhoulySpawnPool*& overrides, RuntimeSegmentContactActor& object,
    u32 suspend, const entity_detail::ListHead& players)
{
    for (auto* node = static_cast<const entity_detail::ListHead*>(players.next); node != nullptr;
        node = static_cast<const entity_detail::ListHead*>(node->next))
    {
        if (node == object.body_node)
        {
            return 1;
        }
    }
    if (object.child_link != 0)
    {
        return 1;
    }
    if (overrides == nullptr)
    {
        auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(
            i32(sizeof(GhoulySpawnPool) + 4 * sizeof(OverrideRow))));
        u8* rows = reinterpret_cast<u8*>(pool + 1);
        pool->begin = rows;
        pool->cursor = rows;
        pool->stride = u16(sizeof(OverrideRow));
        pool->grow = 4;
        pool->limit = rows + 4 * sizeof(OverrideRow);
        overrides = pool;
    }
    else
    {
        for (OverrideRow* row = override_rows(*overrides); row < override_end(*overrides); ++row)
        {
            if (row->actor == &object.actor)
            {
                return 0;
            }
        }
    }
    if (suspend != 0 && object.actor.model_flags == 0)
    {
        return 0;
    }
    GhoulySpawnPool* pool = overrides;
    if (pool->cursor == pool->limit)
    {
        const i32 kept = i32(pool->cursor - pool->begin) / i32(pool->stride);
        const i32 capacity = i32(pool->grow) + kept;
        pool = static_cast<GhoulySpawnPool*>(engine_heap_reallocate(pool,
            i32(sizeof(GhoulySpawnPool)) + i32(pool->stride) * capacity));
        u8* rows = reinterpret_cast<u8*>(pool + 1);
        pool->begin = rows;
        pool->limit = rows + size_t(pool->stride) * capacity;
        pool->cursor = rows + size_t(pool->stride) * kept;
        overrides = pool;
    }
    auto* row = reinterpret_cast<OverrideRow*>(pool->cursor);
    pool->cursor += pool->stride;
    row->actor = &object.actor;
    row->saved = object.actor.model_flags;
    row->retained = 0;
    row->active = 0;
    if (suspend != 0)
    {
        object.actor.model_flags = 0;
        row->active = 1;
    }
    return 1;
}

// Xbox 0x0013FB50
u32 notify_runtime_body_contact(RuntimeBodyContactWalk& walk, const RuntimeBodyCollisionRecord& shape,
    float radius, const Vec3& point, GhoulySpawnPool* contacts, entity_detail::SegmentProbe& object,
    const Vec3& delta)
{
    RuntimeSegmentContactActor& hit = walk.contact_actor(object);
    actor_world_position(hit.actor); // The position is overwritten unused; a stale bind is released.
    u32 result = 0;
    if (walk.callback)
    {
        RuntimeSegmentContactRecord record;
        record.point = point;
        record.radius = radius;
        constexpr float kVelocityScale = 60.0f; // DAT_0032B28C.
        const Vec3 velocity{delta.x * kVelocityScale, delta.y * kVelocityScale, delta.z * kVelocityScale};
        record.kind = 1;
        record.flag = 1;
        record.size = 0x28;
        u16 word;
        std::memcpy(&word, shape.shape + 2, 2);
        result = walk.callback(walk.actor, hit, point, contacts, record, velocity, word);
        if (walk.state.active == nullptr)
        {
            return 1;
        }
    }
    insert_runtime_body_contact_override(walk.state.overrides, hit, 1, walk.players);
    return result;
}

// Xbox 0x0013F7B0
void walk_runtime_body_contacts(RuntimeBodyContactWalk& walk)
{
    Actor& actor = walk.actor;
    const EntityBonePose* pose;
    if (actor.model == nullptr)
    {
        pose = nullptr;
    }
    else
    {
        pose = actor.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
    }
    RuntimeBodyCollisionRecord* shape = walk.state.active;
    if (shape == nullptr)
    {
        return;
    }
    do
    {
        RuntimeBodyCollisionRecord* following = shape->next;
        Vec3 centre;
        const float metric = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape->shape),
            actor.xform, pose, centre);
        if (metric != 0.0f)
        {
            float scale;
            std::memcpy(&scale, shape->bytes.data() + 0x18, 4);
            const float radius = scale * metric;
            Vec3 previous = record_vec3(*shape, 8);
            std::memcpy(shape->bytes.data() + 8, &centre, sizeof(centre));
            const u8 flags = shape->bytes[0x17];
            u32 mask = 0;
            if ((flags & 1) != 0)
            {
                mask = 0x220002;
            }
            if ((flags & 2) != 0)
            {
                mask |= 0x40;
            }
            if ((flags & 4) != 0)
            {
                mask |= 0x100;
            }
            const Vec3 delta{centre.x - previous.x, centre.y - previous.y, centre.z - previous.z};
            const i32 hits = entity_collect_contact_probes(previous, radius, mask);
            Vec3 point;
            if (hits > 0)
            {
                // Retail keeps these pairs in a 16-entry frame; the host sizes it for every slot.
                std::array<entity_detail::SegmentProbe*, entity_detail::kSegmentSlots> objects;
                std::array<GhoulySpawnPool*, entity_detail::kSegmentSlots> copies;
                for (i32 index = 0; index < hits; ++index)
                {
                    objects[index] = entity_detail::g_segment_slots[index].object;
                    copies[index] = copy_segment_records(entity_detail::g_segment_slots[index].pool);
                }
                for (i32 index = 0; index < hits; ++index)
                {
                    GhoulySpawnPool* copy = copies[index];
                    nearest_segment_contact(*copy, point, previous);
                    const u32 stop = notify_runtime_body_contact(walk, *shape, radius, point, copy,
                        *objects[index], delta);
                    engine_heap_free(copy);
                    if (stop != 0 || walk.state.active == nullptr)
                    {
                        while (++index < hits)
                        {
                            engine_heap_free(copies[index]);
                        }
                        return;
                    }
                }
            }
            point = centre;
            if (entity_record_point_probe(point, previous, radius, mask) != 0)
            {
                GhoulySpawnPool* copy = copy_segment_records(entity_detail::g_segment_slots[0].pool);
                const u32 stop = notify_runtime_body_contact(walk, *shape, radius, point, copy,
                    *entity_detail::g_segment_slots[0].object, delta);
                engine_heap_free(copy);
                if (stop != 0)
                {
                    return;
                }
                if (walk.state.active == nullptr)
                {
                    return;
                }
            }
        }
        shape = following;
    } while (shape != nullptr);
}

// Xbox 0x0013F2C0
void update_runtime_body_contacts(RuntimeBodyContactWalk& walk)
{
    RuntimeBodyCollisionState& state = walk.state;
    if (state.overrides != nullptr)
    {
        age_runtime_body_contact_overrides(*state.overrides, walk.clock);
    }
    if (state.active == nullptr)
    {
        return;
    }
    const float step = runtime_frame_step(walk.clock);
    const float rate = runtime_actor_rate(walk.actor);
    if (!(rate * step > 0.0f) && state.forced == 0)
    {
        return;
    }
    const u32 filter = walk.actor.model_flags;
    walk.actor.model_flags = 0;
    if (state.overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*state.overrides);
    }
    state.walking = 1;
    walk_runtime_body_contacts(walk);
    state.walking = 0;
    if (state.overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*state.overrides);
    }
    GhoulySpawnPool* removed = state.removed;
    if (removed != nullptr)
    {
        auto** end = reinterpret_cast<Actor**>(removed->cursor);
        for (auto** entry = reinterpret_cast<Actor**>(removed->begin); entry < end; ++entry)
        {
            GhoulySpawnPool* overrides = state.overrides;
            Actor* removed_actor = *entry;
            if (overrides != nullptr)
            {
                OverrideRow* rows_end = override_end(*overrides);
                for (OverrideRow* row = override_rows(*overrides); row < rows_end; ++row)
                {
                    if (row->actor == removed_actor)
                    {
                        u8* last = overrides->cursor - overrides->stride;
                        if (reinterpret_cast<u8*>(row) < last)
                        {
                            engine_copy_bytes(row, last, overrides->stride);
                        }
                        overrides->cursor -= overrides->stride;
                        break;
                    }
                }
            }
        }
        engine_heap_free(state.removed);
        state.removed = nullptr;
    }
    walk.actor.model_flags = filter;
}

// Xbox 0x0001F8C0
u32 report_runtime_player_contact(Actor& actor, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word, RuntimePlayerContactReport& context)
{
    std::span<u8> body = context.body;
    RuntimeContactNotice notice{0x15, 0, &actor, velocity, contacts, point};
    object.message(&notice);
    RuntimeContactReport report{0x41, i32(i16(word)), velocity, &object, contacts, point, notice.reply};
    context.message(&report);
    if (notice.reply != 0)
    {
        u32 count = read_u32(body, 0x2CC);
        if (count == 0)
        {
            const size_t index = size_t(read_u16(context.actor, 6)) * 0x3D;
            ++context.totals[index];
            ++context.totals[0];
            ++context.session[index];
            ++context.session[0];
        }
        count = read_u32(body, 0x2CC) + 1;
        std::memcpy(body.data() + 0x2CC, &count, 4);
    }
    return 0;
}
}
