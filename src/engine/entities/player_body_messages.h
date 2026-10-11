#pragma once
#include "body_message_dispatch.h"
#include "body_source_contacts.h"
#include "body_collision.h"
#include "player_body_motion.h"
#include "actor_render_registration.h"

namespace gbtg
{
struct RuntimePlayerBodyMessageViews
{
    RuntimeBodyMessageViews parent;
    RuntimeBodySourceContact* source_contact;
    RuntimeBodyAttachedActor* source;
    std::span<const u8> source_body;
    std::span<const u8> bound;
};
struct RuntimePlayerBodyMessages
{
    RuntimeBodyMessageDispatch& parent;
    RuntimeBodyDamageEntry& damage;
    RuntimePlayerGoalRing*& goals;
    std::span<const u8> address_zero;
    const EntityMaskHost& mask;
    const i32& floor; // Actor +0xCEC.
    ActorRenderRegistration& registration;
    const RuntimeActorClock& clock;
    const u8* tag_rows;
    const char* const* tag_names;
    std::span<const char* const, 4> first_health_names;
    std::span<const char* const, 4> second_health_names;
    std::span<const u8>& settings; // Body +0x268.
    std::span<const u8>& hit; // Body +0x84.
    u32& scene_flags; // DAT_0050EA9C.
    u32& restart_requested; // DAT_00430D0C.
};
void dispatch_runtime_player_body_message(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views,
    RuntimePlayerBodyMessages& context); // Xbox 0x00074080
}
