#pragma once
#include "actor_tracks.h"
#include "../audio/event_voices.h"
#include <functional>

namespace gbtg
{
using CutsceneResumeCallback = std::function<void(RuntimeCutsceneInstance&, u32, i32)>;
struct CutsceneEventPauseContext
{
    CutsceneActorRowView* actors;
    const std::unordered_map<Actor*, ActorAnimationState*>& animations;
    AudioEventVoices& voices;
    const std::unordered_map<u32, CutsceneResumeCallback>& callbacks;
};
void pause_cutscene_events(RuntimeCutsceneInstance& instance, i32 paused,
    CutsceneEventPauseContext& context); // Xbox 0x0004E640
void stop_cutscene_event_voices(std::span<u8> event_state,
    AudioEventVoices& voices); // Xbox 0x0004C420
}
