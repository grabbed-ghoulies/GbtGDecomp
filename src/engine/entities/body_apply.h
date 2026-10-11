#pragma once
#include "body_channels.h"
#include "body_timers.h"
#include "body_wash.h"
#include "actor_shape_bounds.h"

namespace gbtg
{
struct RuntimeBodyApplyContext
{
    RuntimeBodyChannelContext& channels;
    std::span<u8>& actor_bytes;
    entity_detail::ListHead& anchors; // Actor +0x1F8.
    EntityBitBlock& flags;
    std::span<u8>& bound; // Actor +0x8F8.
    GhoulySpawnPool*& shapes; // Actor +0x8FC.
    entity_detail::LatchNode*& first; // DAT_00511AA4.
    RuntimeBodyTimerRecipient*& first_recipient; // Body +0x760.
    RuntimeBodyTimerRecipient*& second_recipient; // Body +0x7F0.
    const u8*& actor_bound; // Actor +0x964.
    RuntimeModelInstance*& secondary_model; // Actor +0x310.
    std::span<u32> shared_model_states;
    RuntimeBodyChannelChild*& action; // Body +0x744, vtable +0x14.
};
void apply_runtime_body(std::span<u8> body, RuntimeBodyApplyContext& context); // Xbox 0x00025AE0
void apply_runtime_player_body(std::span<u8> body, RuntimeBodyApplyContext& context); // Xbox 0x00074490
}
