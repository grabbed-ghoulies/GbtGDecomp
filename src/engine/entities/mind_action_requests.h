#pragma once
#include "body_attachment_requests.h"
#include "player_mind_feedback.h"
#include "player_messages.h"

namespace gbtg
{
struct RuntimeMindBoost
{
    std::span<u8> boost; // Player mind +0x158; cinema mind +0x94.
    std::span<const u8> actor_bytes;
    Actor& actor;
    Actor& body_actor;
    EntityBitBlock& flags;
    RuntimePlayerAttachment& attachment;
    const RuntimePlayerStrategyResume& resume;
    std::span<u8> story_bits;
    const u32& story_word; // DAT_0050EA98.
    u32 actor_address;
    std::function<u16()> range_action_key; // Actual body vtable +0x48.
    std::function<u16()> contact_action_key; // Actual body vtable +0x44.
};
bool request_runtime_mind_boost(RuntimeMindBoost& context); // Xbox 0x00137A80
bool request_runtime_mind_turn(RuntimeMindBoost& context,
    const Vec3& point); // Xbox 0x00137C30
}
