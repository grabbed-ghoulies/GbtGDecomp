#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
namespace scene_detail
{
void sync_cutscene_props(Game& g);

float actor_curve(const CutsceneActor& actor, size_t index, float time, float duration, float fallback);

} // namespace scene_detail
void release_props(Game& g);

void draw_cutscene_props(Game& g);

} // namespace gbtg
