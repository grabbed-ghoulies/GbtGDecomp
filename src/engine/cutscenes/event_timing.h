#pragma once
#include "gbtg/types.h"

namespace gbtg
{
bool select_cutscene_event_phase(const u8* event, i32 ending, float current,
    float previous, i32 restricted, u32& phase); // Xbox 0x0004C6C0
}
