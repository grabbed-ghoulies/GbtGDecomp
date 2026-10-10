#pragma once
#include "entity_types.h"
#include "ghouly_spawn_record.h"

namespace gbtg
{
// Heap copy of a slot's 0x54-byte segment records (GhoulySpawnPool header plus rows).
GhoulySpawnPool* copy_segment_records(const entity_detail::SegmentPool& pool); // Xbox 0x000FCC90, EAX pool
// Records whose byte +0x28 is 1 hold a sphere (+0x2C centre, +0x38 radius); output is the
// sphere surface point nearest the reference, or the reference itself when none is closer.
void nearest_segment_contact(const GhoulySpawnPool& records, Vec3& output,
    const Vec3& reference); // Xbox 0x000FC6C0, ECX records, EBX output, EDI reference
}
