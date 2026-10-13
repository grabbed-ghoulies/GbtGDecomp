#include "dialogue_scene_runtime.h"
#include "../scripts/script_actions.h"
#include "../camera/camera.h"

namespace gbtg
{
extern i32 g_draw_e1ec;

void bind_scene_dialogue(Game& game, RuntimeScriptDialogueContext& context,
    RuntimeDialogueOuterDrawContext& draw)
{
    if (&context.active != &g_flag_2474 || &context.scene_flags != &g_flags_ea9c ||
        &context.dialogue.scene_flags != &g_flags_ea9c)
    {
        throw Error("scene dialogue must share the live script latch and scene flags");
    }
    if (&draw.draw.dialogue != &context.dialogue)
    {
        throw Error("scene dialogue drawing must share the script activation context");
    }
    if (game.script_dialogue != nullptr || game.dialogue_draw != nullptr)
    {
        throw Error("scene dialogue runtime already has a binding");
    }
    game.script_dialogue = &context;
    game.dialogue_draw = &draw;
    g_draw_e1ec = context.dialogue.state.mode;
}

RuntimeScriptDialogueContext& scene_dialogue_context(Game& game)
{
    if (game.script_dialogue == nullptr)
    {
        throw Error("scene dialogue runtime is not installed: type 0x14 requires the owned 0x000584D0 context");
    }
    return *game.script_dialogue;
}

void tick_scene_dialogue(Game& game)
{
    if (game.script_dialogue != nullptr)
    {
        auto& context = game.script_dialogue->dialogue;
        context.frame = game.frame;
        context.dt = game.clock.dt;
        context.freeze = scene_detail::g_sim_freeze;
        context.scene_scale = scene_detail::g_sim_scale;
        context.widget_update.dt = game.clock.dt;
        context.widget_update.freeze = context.freeze;
        context.widget_update.scene_scale = context.scene_scale;
        update_runtime_dialogue(context);
        g_draw_e1ec = context.state.mode;
    }
}

void draw_scene_dialogue(Game& game)
{
    if (game.script_dialogue != nullptr)
    {
        if (game.dialogue_draw == nullptr)
        {
            throw Error("scene dialogue has no native draw context");
        }
        auto& draw = *game.dialogue_draw;
        draw.visibility_mask = game.script_dialogue->scene_flags;
        draw.draw.animation_time = game.time;
        render_runtime_dialogue(draw);
        // 0x0011C240 draws the dialogue (0x00058150) and then flushes the sprites it queued (0x00122230).
        draw_queued_runtime_dialogue_sprites(draw.sprites, draw.draw);
    }
}

void unbind_scene_dialogue(Game& game)
{
    auto& context = scene_dialogue_context(game);
    close_runtime_dialogue(context.dialogue);
    game.script_dialogue = nullptr;
    game.dialogue_draw = nullptr;
    g_draw_e1ec = 0;
}
}
