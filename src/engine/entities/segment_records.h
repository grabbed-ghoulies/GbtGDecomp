#pragma once
#include "entity_types.h"
#include "ghouly_spawn_record.h"

namespace gbtg
{
void separate_runtime_segment_contacts(const entity_detail::SegmentPool& pool,
    const Vec3& position, float radius, Vec3& output,
    const Vec3& stationary); // Xbox 0x000FC3F0
void normalized_segment_contact_direction(const entity_detail::SegmentPool& pool,
    Vec3& output); // Xbox 0x000FCA60
// Drops the triangle rows (kind 0) that face away from the point, each replaced by the pool's last row.
void cull_back_facing_segment_contacts(entity_detail::SegmentPool& pool, const Vec3& point); // Xbox 0x000FC840, EDI pool
// Heap copy of a slot's 0x54-byte segment records (GhoulySpawnPool header plus rows).
GhoulySpawnPool* copy_segment_records(const entity_detail::SegmentPool& pool); // Xbox 0x000FCC90, EAX pool
// Records whose byte +0x28 is 1 hold a sphere (+0x2C centre, +0x38 radius); output is the
// sphere surface point nearest the reference, or the reference itself when none is closer.
void nearest_segment_contact(const GhoulySpawnPool& records, Vec3& output,
    const Vec3& reference); // Xbox 0x000FC6C0, ECX records, EBX output, EDI reference
}
