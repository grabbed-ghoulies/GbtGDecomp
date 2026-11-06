#pragma once

#include "gbtg/retail_field.h"
#include "entity_types.h"

namespace gbtg
{
// A segment contact record (0x54 bytes).
namespace segment_contact_field
{
constexpr RetailField<Vec3, 0x00> point{};
constexpr RetailField<Vec3, 0x0C> normal{};
constexpr RetailField<Vec3, 0x2C> center{}; // of the sphere swept
constexpr RetailField<float, 0x38> radius{};
} // namespace segment_contact_field
inline Vec3 segment_contact_normal(std::span<const u8> contact)
{
    return get(contact, segment_contact_field::normal);
}
// The capsule behind a body message's `bound` and DAT_0050E398: its start, end and radius (0x00114910). Inferred from the
// cloth's use of the same words.
namespace actor_capsule_field
{
constexpr RetailField<Vec3, 0x20> start{};
} // namespace actor_capsule_field
namespace entity_detail
{
// DAT_00511188 indexes DAT_005111AC. Message 99 writes the contact and does not bump the index.
// 0x00120480 stores the hitting object at DAT_0051118C[index], then bumps the index.
// Eight pointers fit before DAT_005111AC; further stores spill into that array on Xbox.
extern i32 g_contact_slot;

extern Vec3 g_contact_points[64];

extern SegmentSlot g_segment_slots[kSegmentSlots];
extern i32 g_segment_count; // DAT_00511A18.
void release_segment_slots(); // Xbox 0x001222A0, the scene teardown's

extern // DAT_004274B8

// DAT_004274C8. This function is the only reader. The retail image holds 1,
// which skips the probes and leaves the gate set. Zero runs three probes.
i32 g_slide_skip;

// DAT_004274B8. Opaque param pushed into 0x001226C0. The retail image holds 3.
extern float g_slide_param;

} // namespace entity_detail
// namespace

// Xbox 0x001226C0
i32 entity_record_movement_probe(Vec3& kept, Vec3& candidate, u32 param, i32 kind, u32 mask); // Xbox 0x001226C0
i32 entity_record_sweep_probe(Vec3& kept, Vec3& candidate, i32 kind, u32 mask); // Xbox 0x00122480, EBX kept
i32 entity_record_point_probe(Vec3& kept, Vec3& candidate, float radius, u32 mask); // Xbox 0x001229F0, EBX kept
// The segment from kept to candidate against each probe's model shapes (vtable +0x28); the last probe hit is slot 0.
i32 entity_record_model_segment_probe(Vec3& kept, Vec3& candidate, u32 mask); // Xbox 0x001228F0, EBX kept
// A sphere of the radius moved from start to target: the target is held back by the radius at a hit.
i32 entity_record_sphere_probe(Vec3& target, Vec3& start, float radius, i32 kind,
    u32 mask); // Xbox 0x001225A0, ESI target, EDI start
i32 entity_collect_contact_probes(Vec3& origin, float radius, u32 mask); // Xbox 0x00122B00
i32 entity_collect_sphere_contacts(Vec3& origin, float radius, i32 kind,
    u32 mask); // Xbox 0x001227E0
entity_detail::SegmentPool entity_merge_contact_records(); // Xbox 0x00122310

} // namespace gbtg
