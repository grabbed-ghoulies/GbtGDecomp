#pragma once
#include "body_hit_messages.h"
#include "player_mind_events.h"

namespace gbtg
{
struct RuntimeBodySourceContact
{
    RuntimeBodyContactHitViews contacts;
    std::span<u8> source_body;
    std::span<const u8> hit; // Source body +0x84.
    std::span<const u8> bound; // Target actor +0x8F8.
    RuntimePlayerGoalRing* goals; // Target actor +0x980.
    std::span<const u8> address_zero;
    RuntimeBodyAttachedActor& actor; // Target body +8.
    RuntimeBodyDamageEntry& target;
    RuntimeBodyDamageEntry& source;
};
void receive_runtime_body_source_contact(std::span<u8> body,
    std::span<u8> message,
    RuntimeBodySourceContact& context); // Xbox 0x00020020
void receive_runtime_body_owned_contact(std::span<u8> body,
    std::span<u8> message,
    RuntimeBodySourceContact& context); // Xbox 0x000240C0
}
