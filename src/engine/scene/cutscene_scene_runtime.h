#pragma once
#include "../cutscenes/event_runtime.h"
#include "gbtg/game.h"

namespace gbtg
{
struct SceneCutsceneBindings
{
    CutsceneRuntimeContext& runtime;
    u32& scene_flags; // DAT_0050EA9C.
    u32& pause_count; // DAT_00510CEC.
    u32& transition_pause_count; // DAT_004DEA24.
    i32& active_count; // DAT_0050E7A0.
    const float& freeze; // DAT_004312DC.
    const float& dt; // DAT_005096C0.
    const float& scene_scale; // DAT_004312C4.
};
struct SceneCutsceneRuntime
{
    SceneCutsceneBindings bindings;
    std::array<char, 128> pending_name{}; // DAT_004E1B40.
    i32 pending_interpolation = 0; // DAT_004E1BC0.
    EntityCamNode camera{}; // DAT_004E1BC4.
    u32 linked = 0; // DAT_004E1C24.
    float prefix_time = 0.f; // DAT_004E1C28.
    float time = 0.f; // DAT_004E1C2C.
    float previous_time = 0.f; // DAT_004E1C30.
    i32 index = -1; // DAT_004E1C34.
    std::array<RuntimeCutsceneInstance*, 4> instances{}; // DAT_004E1C38.
    u32 stop = 0; // DAT_004E1C48.
    u32 paused = 0; // DAT_004E1C54.
};
// Scene ownership adapters. A real dependency graph must be installed before
// script startup; lookup never silently falls back to the approximate player.
void install_scene_cutscene_runtime(Game& game, SceneCutsceneBindings bindings);
SceneCutsceneRuntime& scene_cutscene_runtime(Game& game);
void remove_scene_cutscene_runtime(Game& game);
void release_scene_cutscene_instance(RuntimeCutsceneInstance& instance,
    SceneCutsceneRuntime& state); // Xbox 0x0004F050
void pop_scene_cutscene(SceneCutsceneRuntime& state); // Xbox 0x0004EAD0
void push_scene_cutscene(const char* name, i32 interpolation,
    SceneCutsceneRuntime& state); // Xbox 0x0004EB20
RuntimeCutsceneInstance* start_scene_cutscene(const char* name, i32 view_flags,
    i32 interpolation, SceneCutsceneRuntime& state); // Xbox 0x0004EDA0
void release_scene_cutscenes(SceneCutsceneRuntime& state); // Xbox 0x0004ECE0
void update_scene_cutscene_camera_event(float dt,
    SceneCutsceneRuntime& state); // Xbox 0x0004F110
void tick_scene_cutscenes(SceneCutsceneRuntime& state); // Xbox 0x0004EBD0
}
