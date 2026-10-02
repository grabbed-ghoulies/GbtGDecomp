#pragma once

#include "gbtg/game.hpp"

namespace gbtg
{

// DAT_00512494 default, and the scene-order aidlist walked at boot.
inline constexpr const char* kNoScript = "There is no script for this scene!";
inline constexpr const char* kSceneOrder = "aid_aidlist_ghoulies_sceneorder_game";

// File owners for parallel reconstruction. frame_tick in game.cpp is the caller.
void load_ui(Game& g);
void begin_frontend(Game& g);
void load_scene_contents(Game& g, std::string_view name);
void gameselect_tick(Game& g, const InputState& in);
void draw_ui(const Game& g);
void draw_cutscene_props(Game& g);
void pad_tick(Game& g, const InputState& in); // Xbox 0x000FA3C0 / 0x000FAD80
void audio_tick(Game& g, float dt);           // Xbox 0x0015B780 and the XACT calls

} // namespace gbtg
