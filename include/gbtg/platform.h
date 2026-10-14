#pragma once

#include "gbtg/game.h"

namespace gbtg
{

struct PlatformConfig
{
    std::filesystem::path game_dir;
    bool headless = false;
    bool debug_ui = false; // Open Debug D3D11 diagnostics at startup, including captures.
    int replay_after = 0; // Profiling-only: freeze updates after this frame and replay retained native commands.
    int frames = 0; // 0 = until quit; headless defaults to 1
    std::filesystem::path screenshot; // PC readback of the last presented frame
    std::string scene;                // scene-order index or substring; empty boots the first scene
    int width = 960;                  // client width from gbtg.ini
    int height = 540;
    int vsync = 1;
};

int run_platform(Game& game, const PlatformConfig& cfg);

} // namespace gbtg
