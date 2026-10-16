#pragma once
#if defined(GBTG_INSPECTOR_APP)

#include "../audio/native_audio.h"
#include <set>

namespace gbtg
{
struct Game;
struct InspectionSound
{
    std::string bank;
    std::string name;
    AudioCoreBank* native_bank = nullptr;
    u32 cue_index = 0;
};

// Tool-owned native cue/source wrappers. No scene audio or gameplay tick runs here.
class InspectorAudio
{
public:
    InspectorAudio(Game& game, NativeAudioBanks& banks);
    ~InspectorAudio();
    void play(size_t index);
    void pause();
    void stop();
    void update();
    void set_volume(float value);
    std::vector<InspectionSound> sounds;
    int selected = -1;
    std::string status = "Stopped";
    std::string error;
    std::string current;
    float volume = .5f;
    float seconds = 0.f;
    bool playing = false;
    bool paused = false;
private:
    std::set<void*> allocations_;
    AudioCoreSources sources_{};
    AudioCoreCues cues_{sources_};
    AudioCoreSource* source_ = nullptr;
    AudioCoreCue* cue_ = nullptr;
};
}
#endif
