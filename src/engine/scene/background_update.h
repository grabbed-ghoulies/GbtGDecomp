#pragma once

#include "background_construction.h"
#include "../graphics/effect_tick.h"

namespace gbtg
{
struct RuntimeBackgroundUpdate
{
    RuntimeBackgroundConstruction* first; // Owner of DAT_00511B20, used by the animation path.
    const RuntimeActorClock& clock;
    const std::function<std::span<const u8>(u32, size_t)>& resolve;
    RuntimeEffectTickContext* effects; // Required when current +0x940 is nonnull.
    const RuntimeShaderMatrixBoundaries& matrices;
};
void update_background_animation(RuntimeBackgroundConstruction& background,
    RuntimeBackgroundUpdate& context); // Xbox 0x00045A90 (tail path 0x000446A0)
void update_runtime_background(RuntimeBackgroundConstruction& background,
    RuntimeBackgroundUpdate& context); // Xbox 0x000464D0
}
