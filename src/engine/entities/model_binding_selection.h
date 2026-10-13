#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
bool advance_runtime_model_binding(const u8* block, const u8*& current,
    i16 kind); // Xbox 0x00051C60
bool advance_actor_model_binding(const Actor& actor, const u8*& current,
    i16 kind); // Xbox 0x00040930
const u8* actor_physical_shape_block(const Actor& actor); // Xbox 0x000424C0
const u8* select_actor_effect_shape(const Actor& actor, const u8* previous,
    u32 mask); // Xbox 0x000EC750
}
