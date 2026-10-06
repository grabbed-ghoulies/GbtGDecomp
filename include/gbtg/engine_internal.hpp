#pragma once

#include "gbtg/game.hpp"

#include <vector>

namespace gbtg
{

// DAT_00512494 default, and the scene-order aidlist walked at boot.
inline constexpr const char* kNoScript = "There is no script for this scene!";
inline constexpr const char* kSceneOrder = "aid_aidlist_ghoulies_sceneorder_game";

// File owners for parallel reconstruction. frame_tick in game.cpp is the caller.
void load_ui(Game& g);
void begin_frontend(Game& g);
void load_scene_contents(Game& g, std::string_view name);
void scene_order_go(Game& g, size_t index);
void script_request(Game& g, std::string_view name); // 0x0011EB30: queue an aid_script. The pump loads it.
void script_pump(Game& g);                          // 0x0011E4C0: frame_tick loads any pending script
void script_vm_reset();                             // Clears the condition list and the fired-id list.
void script_vm_bind(Game& g);                       // 0x0004B440: walk the loaded script from its first node.
void script_vm_tick(Game& g, float dt);             // 0x00117100: update, eval, then drain fired ids.
void gameselect_tick(Game& g, const InputState& in);
bool attract_confirm_pressed(); // 0x001431B0: new A or Start on the merged pad, not both
void draw_ui(const Game& g);
// 0x000117400 pauses the attract clock while a type-0x15 line is up.
float attract_caption_dt(const Game& g, float dt);
void draw_cutscene_props(Game& g);
void draw_scene_transition(Game& g);
void draw_map_label(const Game& g, std::string_view key, float x, float y, float spot_height);
void prepare_dialogue_texture(const Game& g);
void pad_tick(Game& g, const InputState& in); // Xbox 0x000FA3C0 / 0x000FAD80
void audio_tick(Game& g, float dt);           // Xbox 0x0015B780 and the XACT calls
void pcm_cutscene_sync(const Game& g);
// 0x0004E640 / 0x00039AA0. A dialog hold stops playing voices. Releasing it
// resumes them, then 0x0004C4F0 starts the cue from the type 0x15 record at +0xD0.
void pcm_dialog_hold(bool hold);
void pcm_dialog_advance(const Game& g, const char* cue);
// Cue-list name, the same key build_catalog stores. loop keeps the voice up.
void pcm_play_cue(const Game& g, const char* cue, bool loop);
void pcm_stop_cue(const char* cue);
// S16 interleaved PCM. The buffer is copied. loop keeps the voice on the bed list.
void pcm_play_pcm(const char* cue, const std::vector<u8>& pcm, int rate, int channels, bool loop, float volume);
// PC mix scale, 0..1. Applied on the XAudio2 mastering voice. Cue and movie levels stay as reconstructed.
void pcm_set_master_volume(float volume);

} // namespace gbtg
