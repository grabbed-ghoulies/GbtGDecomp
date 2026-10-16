#pragma once

#include "gbtg/game.h"
#include "gbtg/sound_runtime.h"

#include <vector>

namespace gbtg
{

// DAT_00512494 default, and the scene-order aidlist walked at boot.
inline constexpr const char* kNoScript = "There is no script for this scene!";
inline constexpr const char* kSceneOrder = "aid_aidlist_ghoulies_sceneorder_game";

// File owners for parallel reconstruction. frame_tick in game.cpp is the caller.
void load_scene_contents(Game& g, std::string_view name);
void scene_order_go(Game& g, size_t index);
void script_request(Game& g, std::string_view name); // 0x0011EB30: queue an aid_script. The pump loads it.
void script_pump(Game& g);                          // 0x0011E4C0: frame_tick loads any pending script
void script_vm_reset();                             // Clears the condition list and the fired-id list.
void script_vm_bind(Game& g);                       // 0x0004B440: walk the loaded script from its first node.
void script_vm_tick(Game& g, float dt);             // 0x00117100: update, eval, then drain fired ids.
void pad_tick(Game& g, const InputState& in); // Xbox 0x000FA3C0 / 0x000FAD80
// Cue-list name, the same key build_catalog stores. loop keeps the voice up.
void pcm_play_cue(const Game& g, const char* cue, bool loop);
u64 pcm_play_owned_cue(const Game& g, const char* cue);
// PrepareEx host boundary: submits the buffer without starting the source voice.
// On success, voice is a live IXAudio2SourceVoice and handle owns its PCM buffer.
i32 pcm_prepare_owned_cue(const Game& g, const char* cue, u64& handle, void*& voice);
void pcm_stop_owned_cue(u64 handle);
bool pcm_owned_cue_playing(u64 handle);
void pcm_stop_cue(const char* cue);
// S16 interleaved PCM. The buffer is copied. loop keeps the voice on the bed list.
void pcm_play_pcm(const char* cue, const std::vector<u8>& pcm, int rate, int channels, bool loop, float volume);
// PC mix scale, 0..1. Applied on the XAudio2 mastering voice. Cue and movie levels stay as reconstructed.
void pcm_set_master_volume(float volume);
// Cue handles at this host boundary are live IXAudio2SourceVoice pointers.
RuntimeSoundBoundaries native_sound_boundaries();

} // namespace gbtg
