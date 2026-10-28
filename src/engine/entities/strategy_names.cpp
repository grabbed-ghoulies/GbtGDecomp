#include "strategy_names.h"
#include "../runtime/name_lookup.h"

namespace gbtg
{
namespace
{
// PTR_s_actorStrategyId_ActionEnabledDel_00339924, sorted for 0x000FBFB0.
constexpr const char* kActorStrategyIds[0x30] = {
    "actorStrategyId_ActionEnabledDelay",
    "actorStrategyId_Aloof",
    "actorStrategyId_Angry",
    "actorStrategyId_Approach",
    "actorStrategyId_Attack_KnockBack",
    "actorStrategyId_Attack_KnockDown",
    "actorStrategyId_Attack_Special",
    "actorStrategyId_Attack_Special_End",
    "actorStrategyId_ButlersBrew",
    "actorStrategyId_Celebrate",
    "actorStrategyId_CelebrateLoop",
    "actorStrategyId_Coward",
    "actorStrategyId_Crowd",
    "actorStrategyId_Director_FollowAvatar",
    "actorStrategyId_Director_FollowPlayer",
    "actorStrategyId_Dizzy",
    "actorStrategyId_Dodge",
    "actorStrategyId_Drink",
    "actorStrategyId_DroneAction",
    "actorStrategyId_Dying",
    "actorStrategyId_Emerge",
    "actorStrategyId_Evade",
    "actorStrategyId_ExitExclusionRegion",
    "actorStrategyId_Explode",
    "actorStrategyId_Far",
    "actorStrategyId_FetchWeapon",
    "actorStrategyId_ForceAngry",
    "actorStrategyId_GetHelp",
    "actorStrategyId_GetWeapon",
    "actorStrategyId_HitWall",
    "actorStrategyId_Idle",
    "actorStrategyId_Interactive",
    "actorStrategyId_KnockedDown",
    "actorStrategyId_KnockedDown_Wait",
    "actorStrategyId_Mutated",
    "actorStrategyId_Near",
    "actorStrategyId_Observe",
    "actorStrategyId_PostKnockDownCelebrate",
    "actorStrategyId_PreIntro",
    "actorStrategyId_ReturnToSetupPosition",
    "actorStrategyId_Scared",
    "actorStrategyId_Sit",
    "actorStrategyId_Sleep",
    "actorStrategyId_Spin",
    "actorStrategyId_Stall",
    "actorStrategyId_Wait",
    "actorStrategyId_Wake",
    "actorStrategyId_Worried",
};
}

// Xbox 0x0002B710
u16 find_actor_strategy_id(const char* name)
{
    const int index = binary_search_names(0x30, kActorStrategyIds, name); // 0x000FBFB0
    if (index == -1)
    {
        return 1000;
    }
    return u16(index + 0x3E9);
}
}
