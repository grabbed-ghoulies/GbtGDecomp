#pragma once
#include "actor_matrix_setup.h"
#include "../camera/display_view.h"

namespace gbtg
{
struct RuntimeActorViewVisibility
{
    const Actor& actor;
    std::span<u8> actor_bytes; // +0x184 plane hints (stride 4 per pass, one byte per view), +0x28C matrix count.
    ActorMatrixSetup* matrices; // Actor +0x288..+0x2A0; read only when +0x28C is positive.
    const entity_detail::DisplayView& view; // DAT_005077A8.
};
// Base actor vtable +0x18: 1 when the actor's view sphere, or one of its 0x200 physical spheres, meets the
// display frustum.
u32 runtime_actor_in_view(RuntimeActorViewVisibility& context, i32 pass, i32 view); // Xbox 0x0003C830
}
