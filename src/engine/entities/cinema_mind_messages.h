#pragma once
#include "cinema_mind_control.h"
#include "mind_target_policy.h"
#include "mind_action_requests.h"
#include "player_mind_requests.h"

namespace gbtg
{
struct RuntimeCinemaMindMessages
{
    RuntimeParentMindRequests& parent;
    RuntimeMindBoost& boost; // Actual mind +0x94.
    std::span<const u8> actor_bytes;
    u32 actor_address;
    i32& allowed_count; // DAT_0050E148.
    std::span<const u8> allowed_rows; // DAT_0050E14C, stride 4.
    std::function<RuntimeMindTarget&(u32)> target_at;
    i32 floor;
    std::span<entity_detail::NavGrid* const> grids;
    u8* nodes;
    RuntimeMindStorage*& storage;
    std::function<u32(const void*)> address_of;
};
void dispatch_runtime_cinema_mind_message(RuntimeCinemaMindMessages& context,
    std::span<const u32> message); // Xbox 0x0007C8B0
}
