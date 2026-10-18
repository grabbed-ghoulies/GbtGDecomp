#pragma once
#include "actor_matrix_setup.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
float runtime_actor_view_radius(const Actor& actor, std::span<const u8> actor_bytes); // Xbox 0x0003D860, EDI actor
float runtime_actor_matrix_reach(const Actor& actor, ActorMatrixSetup& matrices); // Xbox 0x00040550, EAX actor
bool runtime_actor_sound_playing(const Actor& actor,
    const RuntimeActorSoundQueues& queues); // Xbox 0x00041CB0, EAX actor
// Actor +0x204 node table: the first entry of the kind names its data block, or null.
const u8* runtime_actor_model_block(const Actor& actor, u32 kind);
const u8* find_runtime_actor_shape(const Actor& actor, u32 mask, u8 kind,
    i16 id); // Xbox 0x00042560, EAX actor, BL kind, DI id
const u8* next_runtime_actor_shape(const Actor& actor, const u8* record, u32 mask, u8 kind,
    i16 id); // Xbox 0x000425D0, ECX actor, EAX record, BL kind, DI id
bool runtime_cylinders_apart(const EntityCylinderBound& first,
    const EntityCylinderBound& second); // Xbox 0x001205F0, ECX first, EAX second
}
