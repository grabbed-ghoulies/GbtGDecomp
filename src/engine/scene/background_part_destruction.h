#pragma once

#include "background_parts.h"
#include "background_part_removal.h"
#include "background_animation.h"
#include "../entities/actor_matrix_sampling.h"

namespace gbtg
{
// A breakable background's part state. Rows are the removal volumes (actor +0x94C, stride 0x9C).
struct BackgroundPartDestruction
{
    ActorEffectNode*& effect; // Actor +0x940, the debris emitter.
    i32& row_count; // Actor +0x944, live rows.
    i32& particle_count; // Actor +0x948; its low half indexes the emitter particles.
    std::span<RuntimePointGraph> graphs; // Actor +0x950, stride 0x1E0; empty when null.
    i32& settings_part; // Actor +0x954.
    std::span<char> settings_name; // Actor +0x958.
    i32 sample_count; // Actor +0x28C.
    RuntimeModelInstance* secondary; // Actor +0x310.
    std::span<u32> shared_states;
    RuntimeActorMatrixSampling& sampling;
    BackgroundPartDispatch& dispatch;
    BackgroundPartRemoval& removal;
    RuntimeBackgroundGraphPush& push;
    const std::function<u32(const void*)>& address_of; // Retail addresses stored in particles.
    i32 pal; // DAT_005096E0.
    i32 pal_override; // DAT_005096E4.
};
// The part header: model reference +0 -> CPU +0x10 block -> +0x10. Empty when the reference, CPU or header
// address is null, so reads through it raise.
std::span<const u8> background_part_header(const Actor& actor);
// Breaks one part: without graphs it bursts debris particles from the part's sample matrices, with graphs it
// pushes each listed graph away from the nearest sphere record. Then removes the part volume and advances
// its stage.
void dispatch_background_part_destruction(Actor& actor, i32 part, const Vec3& source, u32 value,
    BackgroundPartDestruction& context); // Xbox 0x00045170
// 0x00045CB0, then 0x00045170 when the part can break; `zero` serves the part header reads through address zero.
void break_background_part_if_ready(Actor& actor, i32 part, const Vec3& source, u32 value, std::span<const u8> zero,
    BackgroundPartDestruction& context); // Xbox 0x00045D40, EDI actor, ESI part
}
