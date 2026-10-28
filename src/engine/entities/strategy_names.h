#pragma once

#include "gbtg/types.h"

namespace gbtg
{
// The name's index in the actorStrategyId names (0x00339924, 0x30 names) plus 1001, or 1000 when absent.
u16 find_actor_strategy_id(const char* name); // Xbox 0x0002B710
}
