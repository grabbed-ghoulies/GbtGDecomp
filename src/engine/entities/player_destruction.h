#pragma once
#include "player_construction.h"
#include "player_goal_cleanup.h"
#include "../navigation/cell_registration.h"

namespace gbtg
{
struct RuntimePlayerDestruction
{
    RuntimePlayerConstruction& owner;
    RuntimePlayerGoalCleanup& goals;
    std::span<entity_detail::ListHead, 4> links; // Native aliases of actor +0x934/+0x93C/+0x944/+0x94C.
    entity_detail::NavigationCellRegistration*& navigation; // Actor +0xCE8.
};
void destroy_runtime_player(std::span<u8> actor,
    RuntimePlayerDestruction& context); // Xbox 0x00028A30
}
