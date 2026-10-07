#pragma once

#include "gbtg/game.h"

namespace gbtg
{

// The dialogue owns a detached cutscene; pausing the story does not pause this player.
struct DialogueScreen
{
    Cutscene page;
    std::vector<Mesh> meshes;
    Mesh button_bat;
    std::vector<u8> stand;
    std::vector<u8> hop;
    float time = 0.f;
    float bat_time = 0.f;
    float bat_timer = 4.5f;
    int bat_state = 1;
    bool active = false;

    void open(const Game& g);
    void close();
    void tick(float dt);
    void draw(bool background, bool button, float widget_scale);
    void draw_button(float widget_scale);
};

// Xbox 0x000FC190 / 0x000584D0: 480 safe area, inset 12.5, widget inset 16.
constexpr float dialogue_button_y = 480.f - (480.f - 480.f * 0.85f) * 0.5f - 12.5f - 16.f;

} // namespace gbtg
