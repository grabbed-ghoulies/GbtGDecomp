#pragma once
#include "gbtg/game.h"
#include "../scripts/script_dialogue_actions.h"
#include "../frontend/dialogue_draw_outer.h"

namespace gbtg
{
// Borrow the complete scene-owned graph. Its latches must be the live script
// globals, and its lifetime must include every script, update and draw call.
void bind_scene_dialogue(Game& game, RuntimeScriptDialogueContext& context,
    RuntimeDialogueOuterDrawContext& draw);
void unbind_scene_dialogue(Game& game);
RuntimeScriptDialogueContext& scene_dialogue_context(Game& game);
void tick_scene_dialogue(Game& game);
void draw_scene_dialogue(Game& game);
}
