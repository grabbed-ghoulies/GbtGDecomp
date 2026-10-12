#include "gbtg/entity.h"
#include "../navigation/navigation.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>


#include "entity_types.h"
#include "actor_transform.h"
#include "entity.h"
#include "../camera/entity_camera.h"
#include "entity_collision.h"
#include "entity_heap.h"

namespace gbtg
{
using namespace entity_detail;

u32 EntityBonePose::packed_flag() const
{
    return buffer != nullptr ? u32(buffer->dirty) : packed;
}

const float* EntityBonePose::matrix_data() const
{
    if (buffer == nullptr)
    {
        return mats.data();
    }
    if (buffer->dirty)
    {
        return buffer->matrices.empty() ? nullptr : buffer->matrices.front().data();
    }
    return buffer->packed_matrices.empty() ? nullptr : buffer->packed_matrices.front().data();
}

size_t EntityBonePose::matrix_floats() const
{
    if (buffer == nullptr)
    {
        return mats.size();
    }
    return buffer->dirty ? buffer->matrices.size() * 12 : buffer->packed_matrices.size() * 16;
}

namespace entity_detail
{


// DAT_00511188 indexes DAT_005111AC. Message 99 writes the contact and does not bump the index.
// 0x00120480 stores the hitting object at DAT_0051118C[index], then bumps the index.
// Eight pointers fit before DAT_005111AC; further stores spill into that array on Xbox.
i32 g_contact_slot = 0;

Vec3 g_contact_points[64]{};


ShapeProbe* g_contact_owners[64]{};

} // namespace entity_detail


static_assert(offsetof(EntityShapeNode, bone) == 8);

static_assert(offsetof(EntityShapeNode, along_x) == 0x10);

static_assert(offsetof(EntityShapeNode, along_y) == 0x14);

static_assert(offsetof(EntityShapeNode, point) == 0x18);

static_assert(offsetof(EntityCylinderBound, x) == 4);

static_assert(offsetof(EntityCylinderBound, height) == 0x14);

static_assert(offsetof(EntityCylinderBound, radius) == 0x1C);

static_assert(sizeof(EntityCylinderBound) == 0x20);

namespace entity_detail
{


Vec3 mul_row(const float m[12], const Vec3& p)
{
    return Vec3{
        m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3],
        m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
        m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11],
    };
}


float length3(const Vec3& d)
{
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}


// 0x000315F0. A packed table returns the 3x4 at +0x10 + index * 0x30.
// A clear +4 transposes the 4x4 at +0x10 + index * 0x40 into that layout.
bool bone_matrix(const EntityBonePose& pose, i16 bone, float out[12])
{
    const i32 index = bone;
    if (index < 0)
    {
        return false;
    }
    if (pose.packed_flag() != 0)
    {
        const size_t base = static_cast<size_t>(index) * 12u;
        if (base + 12u > pose.matrix_floats())
        {
            return false;
        }
        std::memcpy(out, pose.matrix_data() + base, sizeof(float) * 12u);
        return true;
    }
    const size_t base = static_cast<size_t>(index) * 16u;
    if (base + 16u > pose.matrix_floats())
    {
        return false;
    }
    const float* s = pose.matrix_data() + base;
    out[0] = s[0];
    out[1] = s[4];
    out[2] = s[8];
    out[3] = s[12];
    out[4] = s[1];
    out[5] = s[5];
    out[6] = s[9];
    out[7] = s[13];
    out[8] = s[2];
    out[9] = s[6];
    out[10] = s[10];
    out[11] = s[14];
    return true;
}


Vec3 xform_point(const EntityShapeNode& node, const float* world, const EntityBonePose* pose, Vec3 local)
{
    // 0x00056790. Bone 0xFFFF copies the local point. Otherwise the pose
    // matrix is applied first. A null world matrix skips the actor multiply.
    Vec3 p = local;
    if (node.bone != 0xFFFF && pose != nullptr)
    {
        float bone_m[12];
        if (bone_matrix(*pose, static_cast<i16>(node.bone), bone_m))
        {
            p = mul_row(bone_m, p);
        }
    }
    if (world != nullptr)
    {
        p = mul_row(world, p);
    }
    return p;
}


// 0x00056330. World position of the three floats at node+0x14. The radius is
// the world length of that point after the float at +0x10 is added on local x.
// Both samples use the node's bone, the same pose, and the same actor matrix.
float sphere_metrics(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
                    Vec3& center)
{
    Vec3 local;
    std::memcpy(&local, reinterpret_cast<const char*>(&node) + 0x14, sizeof(local));
    center = xform_point(node, world, pose, local);
    local.x += node.along_x;
    const Vec3 arm = xform_point(node, world, pose, local);
    return length3(Vec3{center.x - arm.x, center.y - arm.y, center.z - arm.z});
}

} // namespace entity_detail
 // namespace

// Xbox 0x00055AA0
void entity_transform_cylinder(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
              EntityCylinderBound& out)
{
    // Xbox 0x00055AA0
    // ESI is the 0x20-byte record. 0x00055B30 transforms the point at node+0x18,
    // that point plus the float at +0x10 on local x, and that point plus the
    // float at +0x14 on local y. The radius is the world length of the x
    // offset. The y offset's length is stored positive and negated; the record
    // y is the world point plus the negated length, and the height is the
    // positive length minus that negated length. Bytes are 4, 0x20, 1. The
    // floats at +0x10 and +0x18 are cleared. 0x00103BE0 transforms node+0x24
    // into a scratch this record does not read.
    const Vec3 base = xform_point(node, world, pose, node.point);
    Vec3 along_x = node.point;
    along_x.x += node.along_x;
    Vec3 along_y = node.point;
    along_y.y += node.along_y;
    const Vec3 world_x = xform_point(node, world, pose, along_x);
    const Vec3 world_y = xform_point(node, world, pose, along_y);
    const float reach = length3(Vec3{base.x - world_x.x, base.y - world_x.y, base.z - world_x.z});
    const float hy = length3(Vec3{base.x - world_y.x, base.y - world_y.y, base.z - world_y.z});
    const float down = -hy;
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = base.x;
    out.y = base.y + down;
    out.z = base.z;
    out.pad0 = 0.f;
    out.height = hy - down;
    out.pad1 = 0.f;
    out.radius = reach;
}


// Xbox 0x00056720
void entity_sphere_to_cylinder(const EntityShapeNode& node, const float* world, const EntityBonePose* pose,
              EntityCylinderBound& out)
{
    // Xbox 0x00056720
    // EDX is the shape node, ECX the pose, the stack argument the actor matrix,
    // and ESI the 0x20-byte record. 0x00056330 writes the sphere center and
    // returns its radius. The record stores that sphere as a cylinder: y is
    // the center minus the radius, the height is twice the radius, and the
    // radius is stored unchanged. Bytes are 4, 0x20, 1. The floats at +0x10
    // and +0x18 are cleared.
    Vec3 center;
    const float radius = sphere_metrics(node, world, pose, center);
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = center.x;
    out.y = center.y - radius;
    out.z = center.z;
    out.pad0 = 0.f;
    out.height = radius + radius;
    out.pad1 = 0.f;
    out.radius = radius;
}

namespace entity_detail
{


// Xbox 0x001202C0. ECX is the cylinder array, EAX the count, EBX the source
// point, EDI the merged record. XZ stays on the source. Y runs from the lower
// of the source and every cylinder bottom to the higher of the source and every
// top. The radius is the largest horizontal reach (distance to a center plus
// that cylinder's radius). An empty array is a zero-height, zero-radius record.
EntityCylinderBound merge_cylinders(const Vec3& src, const EntityCylinderBound* shapes, i32 count)
{
    float min_y = src.y;
    float max_top = src.y;
    float reach = 0.f;
    if (shapes != nullptr && count > 0)
    {
        for (i32 i = 0; i < count; ++i)
        {
            const EntityCylinderBound& cyl = shapes[i];
            const float dx = cyl.x - src.x;
            const float dz = cyl.z - src.z;
            const float dist = std::sqrt(dx * dx + dz * dz) + cyl.radius;
            if (dist > reach)
            {
                reach = dist;
            }
            if (cyl.y <= min_y)
            {
                min_y = cyl.y;
            }
            const float top = cyl.y + cyl.height;
            if (max_top < top)
            {
                max_top = top;
            }
        }
    }
    EntityCylinderBound out{};
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = src.x;
    out.y = min_y;
    out.z = src.z;
    out.pad0 = 0.f;
    out.height = max_top - min_y;
    out.pad1 = 0.f;
    out.radius = reach;
    return out;
}


// Xbox 0x00120D10. ECX is the push, EAX the count, EDI the cylinder array, and
// the stack argument is the merged record. Each cylinder's position, and the
// merged record's position, gain the push. Radius and height stay put.
void shift_cylinders(EntityCylinderBound* shapes, i32 count, EntityCylinderBound& merged, const Vec3& push)
{
    if (shapes != nullptr && count > 0)
    {
        for (i32 i = 0; i < count; ++i)
        {
            shapes[i].x += push.x;
            shapes[i].y += push.y;
            shapes[i].z += push.z;
        }
    }
    merged.x += push.x;
    merged.y += push.y;
    merged.z += push.z;
}

} // namespace entity_detail
 // namespace

// Xbox 0x00120480
i32 entity_sum_collision_push(const Vec3& src, const Actor* actor, EntityCylinderBound* shapes, i32 count, Vec3& push_out)
{
    // Xbox 0x00120480
    // EAX is the source. Stack args are the actor, the cylinder array, the count,
    // and the summed push. DAT_00511188 is cleared, then 0x001202C0 merges the
    // array into one cylinder around the source. A null actor leaves the query
    // null; otherwise the query is source minus actor_world_position, and that
    // origin is still in the push slot when vtable +0x34 is entered. Each node
    // on DAT_00511B04 (object+0x110, next at +4) is called with the actor, the
    // query, the merged cylinder, the array, the count, and the push slot.
    // A hit stores the object at DAT_0051118C[index], bumps the index, adds the
    // push through 0x00120D10, and accumulates it. The return is 1 after any hit.
    g_contact_slot = 0;
    EntityCylinderBound merged = merge_cylinders(src, shapes, count);

    Vec3 origin{};
    Vec3 query_storage{};
    const Vec3* query = nullptr;
    if (actor != nullptr)
    {
        origin = actor_world_position(*actor);
        query_storage = Vec3{src.x - origin.x, src.y - origin.y, src.z - origin.z};
        query = &query_storage;
    }

    push_out.x = 0.f;
    push_out.y = 0.f;
    push_out.z = 0.f;
    i32 hit = 0;
    for (ShapeProbe* probe = g_world.shape; probe != nullptr; probe = probe->next)
    {
        if (probe->push == nullptr)
        {
            continue;
        }
        Vec3 delta = origin;
        const i32 ok = probe->push(*probe, actor, query, merged, shapes, count, delta);
        if (ok == 0)
        {
            continue;
        }
        if (g_contact_slot >= 0 && g_contact_slot < 64)
        {
            g_contact_owners[g_contact_slot] = probe;
        }
        g_contact_slot += 1;
        shift_cylinders(shapes, count, merged, delta);
        push_out.x += delta.x;
        push_out.y += delta.y;
        push_out.z += delta.z;
        hit = 1;
    }
    return hit;
}


// Xbox 0x001203E0
void entity_push_point_from_actor(const Vec3& src, Vec3& dest, const Actor* actor, float height, float radius)
{
    // Xbox 0x001203E0
    // ESI is the source point and EDI is the destination. EDX is the actor
    // passed to 0x00120480. XMM1 is the cylinder height; the stack float is
    // the radius. The record is kind 4, size 0x20, flag 1, centered on ESI,
    // with the floats at +0x10 and +0x18 cleared. 0x00120480 sums one push per
    // hit; the destination is the source plus that sum.
    EntityCylinderBound bound{};
    bound.kind = 4;
    bound.size = 0x20;
    bound.flag = 1;
    bound.x = src.x;
    bound.y = src.y;
    bound.z = src.z;
    bound.pad0 = 0.f;
    bound.height = height;
    bound.pad1 = 0.f;
    bound.radius = radius;

    Vec3 push{};
    entity_sum_collision_push(src, actor, &bound, 1, push);
    dest.x = src.x + push.x;
    dest.y = src.y + push.y;
    dest.z = src.z + push.z;
}


// Xbox 0x00120760
i32 entity_cylinder_collision_push(const EntityCylinderBound& query, const EntityCylinderBound& obstacle, const Vec3& offset,
              Vec3& push)
{
    // Xbox 0x00120760
    // EBX is the query cylinder, EDI the obstacle, and the two stack arguments are
    // the offset and the push. The query point is the cylinder position plus the
    // offset. A miss on the vertical slabs, or an XZ distance at or beyond the
    // summed radius, returns 0. 0x00152B30 is the horizontal length. Under 1e-5
    // the X separation becomes 1 and the length becomes the summed radius minus
    // 0.01. 0x00120250 then scales that (dx, 0, dz) vector out to the summed
    // radius when its length is not zero. The push is the scaled point minus the
    // query point, with Y cleared. The contact midpoint is stored at
    // DAT_005111AC[DAT_00511188] and the index is left alone.
    const float point_x = offset.x + query.x;
    const float point_y = query.y + offset.y;
    const float point_z = query.z + offset.z;
    if (point_y > obstacle.y + obstacle.height || obstacle.y > point_y + query.height)
    {
        return 0;
    }

    const float combined = obstacle.radius + query.radius;
    float dx = point_x - obstacle.x;
    float dz = point_z - obstacle.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < kHitEps)
    {
        dx = 1.f;
        dist = combined - kHitPad;
    }
    if (combined <= dist)
    {
        return 0;
    }

    const float len2 = dx * dx + dz * dz;
    if (len2 != 0.f)
    {
        const float scale = combined / std::sqrt(len2);
        dx *= scale;
        dz *= scale;
    }
    push.x = (obstacle.x + dx) - point_x;
    push.y = 0.f;
    push.z = (obstacle.z + dz) - point_z;

    if (g_contact_slot >= 0 && g_contact_slot < 64)
    {
        const float mid_y = obstacle.height * 0.5f + obstacle.y;
        const float sample_y = offset.y + query.height * 0.5f + query.y;
        g_contact_points[g_contact_slot].x = (point_x - obstacle.x) * 0.5f + obstacle.x;
        g_contact_points[g_contact_slot].y = (sample_y - mid_y) * 0.5f + mid_y;
        g_contact_points[g_contact_slot].z = (point_z - obstacle.z) * 0.5f + obstacle.z;
    }
    return 1;
}

namespace entity_detail
{

constexpr u16 kSegmentStride = 0x54;

constexpr u16 kSegmentGrow = 10;

constexpr i32 kSegmentPoolBytes = 0x358;


i32 g_segment_count = 0;

SegmentSlot g_segment_slots[kSegmentSlots]{};


void bind_segment_pool(SegmentPool& pool, u8* block, i32 block_size, i32 used)
{
    pool.block = block;
    pool.block_size = block_size;
    pool.stride = kSegmentStride;
    pool.grow = kSegmentGrow;
    std::memcpy(block, &pool.stride, sizeof(pool.stride));
    std::memcpy(block + 2, &pool.grow, sizeof(pool.grow));
    pool.begin = block + 0x10;
    const i32 stride = static_cast<i32>(pool.stride);
    pool.cursor = pool.begin + static_cast<ptrdiff_t>(stride) * used;
    const i32 capacity = (block_size - 0x10) / stride;
    pool.limit = pool.begin + static_cast<ptrdiff_t>(stride) * capacity;
}


// Xbox 0x000662A0. EAX is the pool. used = (cursor - begin) / stride.
// The new count is used plus the u16 at +2. 0x000F73A0 reallocates, begin
// becomes the new header+0x10, and the cursor stays on the same record.
void grow_segment_pool(SegmentPool& pool)
{
    const i32 stride = static_cast<i32>(pool.stride);
    const i32 used = static_cast<i32>(pool.cursor - pool.begin) / stride;
    const i32 new_count = used + static_cast<i32>(pool.grow);
    const i32 new_bytes = stride * new_count + 0x10;
    u8* fresh = static_cast<u8*>(engine_heap_reallocate(pool.block, new_bytes));
    pool.block = fresh;
    pool.block_size = new_bytes;
    pool.begin = fresh + 0x10;
    pool.cursor = pool.begin + stride * used;
    pool.limit = pool.begin + stride * new_count;
}


// Xbox 0x00122C10. The index is DAT_00511A18. A missing pool is one 0x358-byte
// block: stride 0x54 and ten records. An existing pool rewinds its cursor.
SegmentPool* prepare_segment_pool()
{
    const i32 index = g_segment_count;
    SegmentPool& pool = g_segment_slots[index].pool;
    if (!pool.live)
    {
        u8* block = static_cast<u8*>(engine_heap_allocate(kSegmentPoolBytes));
        bind_segment_pool(pool, block, kSegmentPoolBytes, 0);
        pool.live = true;
    }
    else
    {
        pool.cursor = pool.begin;
    }
    return &pool;
}


u8* bump_segment_record(SegmentPool& pool)
{
    if (pool.cursor == pool.limit)
    {
        grow_segment_pool(pool);
    }
    u8* record = pool.cursor;
    pool.cursor = record + pool.stride;
    record[0x26] = 0;
    return record;
}

} // namespace entity_detail
 // namespace

// Xbox 0x001227E0
i32 entity_collect_sphere_contacts(Vec3& origin, float radius, i32 kind, u32 mask)
{
    g_segment_count = 0;
    for (SegmentProbe* probe = g_world.segment; probe != nullptr; probe = probe->next)
    {
        SegmentSlot& slot = g_segment_slots[g_segment_count];
        SegmentPool& pool = slot.pool;
        if (!pool.live)
        {
            u8* block = static_cast<u8*>(engine_heap_allocate(0x35C));
            bind_segment_pool(pool, block, 0x35C, 0);
            pool.live = true;
        }
        else
        {
            pool.cursor = pool.begin;
        }
        if (probe->sphere_contacts(*probe, origin, radius, kind, mask, pool) != 0)
        {
            g_segment_slots[g_segment_count].object = probe;
            ++g_segment_count;
        }
    }
    return g_segment_count;
}

// Xbox 0x00122310
SegmentPool entity_merge_contact_records()
{
    u32 count = 0;
    for (i32 index = 0; index < g_segment_count; ++index)
    {
        const SegmentPool& pool = g_segment_slots[index].pool;
        count += i32(pool.cursor - pool.begin) / pool.stride;
    }
    const u16 capacity = u16(count);
    const i32 bytes = i32(capacity) * 0x54 + 0x14;
    u8* block = static_cast<u8*>(engine_heap_allocate(bytes));
    SegmentPool output;
    bind_segment_pool(output, block, bytes, 0);
    output.grow = capacity != 0 ? capacity : 10;
    std::memcpy(block + 2, &output.grow, 2);
    output.live = true;
    for (i32 index = 0; index < g_segment_count; ++index)
    {
        const SegmentPool& pool = g_segment_slots[index].pool;
        u8* end = pool.cursor;
        for (u8* record = pool.begin; record < end; record += 0x54)
        {
            if (output.cursor == output.limit)
            {
                grow_segment_pool(output);
            }
            u8* target = output.cursor;
            output.cursor += output.stride;
            entity_copy_match_block(target, record);
        }
    }
    return output;
}

// Xbox 0x001226C0
i32 entity_record_movement_probe(Vec3& kept, Vec3& candidate, u32 param, i32 kind, u32 mask)
{
    // Xbox 0x001226C0
    // EBX is the point that a move under 2e-7 (DAT_0032B27C) overwrites with the
    // candidate. The four stack dwords are the candidate, an opaque param, a kind
    // (4 or 0xC), and a mask. DAT_00511A18 is cleared first. A longer move asks
    // 0x00122C10 for the record pool, bumps one 0x54-byte record, and clears the
    // byte at +0x26. Each node on DAT_00511AF4 (object+0x1e0, next at +4) is
    // called through vtable +0x20 with the candidate, EBX, param, kind, mask, and
    // that record. The last node that returns non-zero is stored at
    // DAT_00511A1C[count] and the count is incremented. The return is 1 when any
    // node hit. The first record stays at pool begin for 0x000FCD70.
    g_segment_count = 0;
    const float dx = candidate.x - kept.x;
    const float dy = candidate.y - kept.y;
    const float dz = candidate.z - kept.z;
    const float dist2 = dz * dz + dy * dy + dx * dx;
    // comiss / jb: the long path runs when the epsilon is below dist2, including NaN.
    if (!(dist2 <= kLookEps))
    {
        SegmentPool* pool = prepare_segment_pool();
        u8* record = bump_segment_record(*pool);

        SegmentProbe* found = nullptr;
        i32 hit = 0;
        for (SegmentProbe* probe = g_world.segment; probe != nullptr; probe = probe->next)
        {
            const i32 ok = probe->query(*probe, candidate, kept, param, kind, mask, record);
            if (ok != 0)
            {
                found = probe;
                hit = 1;
            }
        }
        if (found != nullptr)
        {
            const i32 slot = g_segment_count;
            g_segment_slots[slot].object = found;
            g_segment_count = slot + 1;
        }
        return hit;
    }

    kept.x = candidate.x;
    kept.y = candidate.y;
    kept.z = candidate.z;
    return 0;
}

// Xbox 0x00122480
i32 entity_record_sweep_probe(Vec3& kept, Vec3& candidate, i32 kind, u32 mask)
{
    g_segment_count = 0;
    const float dz = candidate.z - kept.z;
    const float dy = candidate.y - kept.y;
    const float dx = candidate.x - kept.x;
    const float dist2 = dz * dz + dy * dy + dx * dx;
    if (kLookEps < dist2 || dist2 != dist2)
    {
        SegmentPool& pool = *prepare_segment_pool();
        if (pool.cursor == pool.limit)
        {
            grow_segment_pool(pool);
        }
        u8* record = pool.cursor;
        pool.cursor = record + pool.stride;
        record[0x26] = 0;
        SegmentProbe* found = nullptr;
        i32 hit = 0;
        for (SegmentProbe* probe = g_world.segment; probe != nullptr; probe = probe->next)
        {
            if (probe->sweep(*probe, candidate, kept, kind, mask, record) != 0)
            {
                found = probe;
                hit = 1;
            }
        }
        if (found != nullptr)
        {
            g_segment_slots[g_segment_count].object = found;
            ++g_segment_count;
        }
        return hit;
    }
    kept.x = candidate.x;
    kept.y = candidate.y;
    kept.z = candidate.z;
    return 0;
}

// Xbox 0x001229F0
i32 entity_record_point_probe(Vec3& kept, Vec3& candidate, float radius, u32 mask)
{
    g_segment_count = 0;
    const float dz = candidate.z - kept.z;
    const float dy = candidate.y - kept.y;
    const float dx = candidate.x - kept.x;
    const float dist2 = dz * dz + dy * dy + dx * dx;
    if (kLookEps < dist2 || dist2 != dist2)
    {
        SegmentPool& pool = *prepare_segment_pool();
        if (pool.cursor == pool.limit)
        {
            grow_segment_pool(pool);
        }
        u8* record = pool.cursor;
        pool.cursor = record + pool.stride;
        record[0x26] = 0;
        SegmentProbe* found = nullptr;
        for (SegmentProbe* probe = g_world.segment; probe != nullptr; probe = probe->next)
        {
            if (probe->point(*probe, candidate, kept, radius, mask, record) != 0)
            {
                found = probe;
            }
        }
        if (found != nullptr)
        {
            g_segment_slots[g_segment_count].object = found;
            ++g_segment_count;
            return 1;
        }
        return 0;
    }
    kept.x = candidate.x;
    kept.y = candidate.y;
    kept.z = candidate.z;
    return 0;
}

// Xbox 0x00122B00
i32 entity_collect_contact_probes(Vec3& origin, float radius, u32 mask)
{
    g_segment_count = 0;
    SegmentProbe* probe = g_world.segment;
    if (probe == nullptr)
    {
        return 0;
    }
    do
    {
        // Inline 0x00122C10 body: an empty slot gets a fresh 0x358-byte pool, otherwise it rewinds.
        SegmentPool& pool = *prepare_segment_pool();
        if (probe->contacts(*probe, origin, radius, mask, pool) != 0)
        {
            g_segment_slots[g_segment_count].object = probe;
            ++g_segment_count;
        }
        probe = probe->next;
    } while (probe != nullptr);
    return g_segment_count;
}

namespace entity_detail
{


static_assert(sizeof(SlideRecord) == 0x54);

static_assert(offsetof(SlideRecord, x) == 0x0c);

static_assert(offsetof(SlideRecord, match) == 0x28);

static_assert(std::bit_cast<u32>(0.05f) == 0x3D4CCCCDu);
 // 0x0032B250
static_assert(std::bit_cast<u32>(3.f) == 0x40400000u);
   // DAT_004274B8

// DAT_004274C8. This function is the only reader. The retail image holds 1,
// which skips the probes and leaves the gate set. Zero runs three probes.
i32 g_slide_skip = 1;


// DAT_004274B8. Opaque param pushed into 0x001226C0. The retail image holds 3.
float g_slide_param = 3.f;


// DAT_0050e240 holds four floor grids. The count is DAT_0050e250. The Y bands
// are two floats at DAT_0050e254 and two at DAT_0050e25c. Origins are the
// dwords at DAT_0050e264 and DAT_0050e26c.
constexpr float kNavBandPad = 2.f;
 // DAT_0032B110

NavGrid* g_nav_grids[kNavSlots]{};

i32 g_nav_count = 0;

float g_nav_min_y[kNavBands]{};

float g_nav_max_y[kNavBands]{};

i32 g_nav_origin_x = 0;

i32 g_nav_origin_z = 0;


// DAT_00314EB4, stride 8. The low float is the radius bump. Index is
// class + state * 3, and only state 1..4 with class 1..3 are read.
constexpr float kRadiusBump[16] = {
    0.f, 0.f, 0.f, 0.f, 5.f, 5.f, 5.f, 0.f, 5.f, 0.f, 0.f, 5.f, 0.f, 0.f, 0.f, 0.f,
};


constexpr float kCellLift = 5.f;
    // DAT_0032B184
constexpr float kCellHeight = 10.f;
 // DAT_0032B10C
constexpr float kCellRadius = 4.f;
  // DAT_00434018

bool nav_cell(i32 floor, i32 index, NavCell& out)
{
    if (floor < 0 || floor >= kNavSlots)
    {
        return false;
    }
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr || index < 0 || static_cast<size_t>(index) >= grid->cells.size())
    {
        return false;
    }
    out = grid->cells[static_cast<size_t>(index)];
    return true;
}


// 0x00145FF0. EDI is the case link, BX is the actor word at +6.
i32 class_from_link(const EntityCaseLink& link, u16 key)
{
    if (link.classes.empty())
    {
        return 0;
    }
    for (const EntityClassSlot& slot : link.classes)
    {
        if (slot.key == key)
        {
            return slot.id;
        }
    }
    return link.classes[0].id;
}

} // namespace entity_detail
 // namespace

// Xbox 0x0013D2C0
void entity_build_navigation_cylinder(const Vec3& point, const Actor* actor, EntityCylinderBound& out)
{
    // Xbox 0x0013D2C0
    // EAX is the point. The actor is the first stack argument; null leaves the
    // radius at 4. The second argument is the 0x20-byte cylinder. 0x00110BF0
    // picks the floor and 0x00110EF0 the cell, twice. The short at cell+2 is
    // the bottom. The short at cell+0xe minus that bottom is the height when
    // the second lookup hits and the difference is >= 0. A miss, or a negative
    // difference, uses point Y + 5 and height 10. The floats at +0x10 and +0x18
    // stay 0. Radius is DAT_00434018 plus the DAT_00314EB4 bump for case state
    // 1..4 and class 1..3, plus another 5 when bit 0 of bits byte +5 is set.
    const i32 floor = navigation_floor_index(point, -1, 0);
    float bottom = point.y + kCellLift;
    float height = kCellHeight;
    const i32 cell = navigation_cell_index(floor, point);
    NavCell sample{};
    if (cell >= 0 && nav_cell(floor, cell, sample))
    {
        const float y0 = static_cast<float>(sample.bottom);
        float span = point.y - y0;
        const i32 cell_b = navigation_cell_index(floor, point);
        NavCell sample_b{};
        if (cell_b >= 0 && nav_cell(floor, cell_b, sample_b))
        {
            span = static_cast<float>(sample_b.top) - y0;
        }
        if (span >= 0.f)
        {
            bottom = y0;
            height = span;
        }
    }

    float bump = 0.f;
    if (actor != nullptr && actor->euler_out != nullptr)
    {
        const ActorEulerOut& host = *actor->euler_out;
        u8 state = 0;
        i32 cls = 0;
        if (host.case_link != nullptr)
        {
            state = host.case_link->state;
            if (host.actor != nullptr)
            {
                cls = class_from_link(*host.case_link, host.actor->class_key);
            }
        }
        if (state > 0 && state < 5 && cls > 0 && cls < 4)
        {
            bump = kRadiusBump[cls + static_cast<i32>(state) * 3];
        }
        if (host.bits != nullptr && host.bits->bytes != nullptr && (host.bits->bytes[5] & 1u) != 0)
        {
            bump += kCellLift;
        }
    }

    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = point.x;
    out.y = bottom;
    out.z = point.z;
    out.pad0 = 0.f;
    out.height = height;
    out.pad1 = 0.f;
    out.radius = kCellRadius + bump;
}

} // namespace gbtg
