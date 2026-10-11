#pragma once
#include "player_control_update.h"
#include "player_mind_requests.h"
#include "../navigation/player_navigation.h"

namespace gbtg
{
// Player mind request update (mind vtable slot +0x14).
struct RuntimePlayerMindRequestUpdate
{
    RuntimeParentMindRequests& parent; // 0x000295E0.
    RuntimePlayerControlUpdate& control; // 0x0007D130.
    RuntimePlayerNavigationControl& navigation; // 0x00136620.
    std::span<u8> body; // Actor +0x95C.
    EntityBitBlock& flags; // Body +0x15C -> +8.
    RuntimeBodyActionSelection& actions; // 0x00027650.
    std::span<const char* const, 0x41> flag_names; // 0x0001EF70.
    std::function<u16()> effect_action_key; // Actual body vtable +0x38.
    std::function<u16()> contact_action_key; // Actual body vtable +0x3C.
};
i32 update_runtime_player_mind_request(RuntimePlayerMindRequestUpdate& context,
    RuntimePlayerMindRequest& request); // Xbox 0x0007D360
}
