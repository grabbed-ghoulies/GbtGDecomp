#pragma once

#include "gbtg/game.h"

#include <chrono>

namespace gbtg
{

struct PlatformConfig
{
    std::filesystem::path game_dir;
    bool headless = false;
    int replay_after = 0; // Profiling-only: freeze updates after this frame and replay retained native commands.
    int frames = 0; // 0 = until quit; headless defaults to 1
    std::filesystem::path screenshot; // PC readback of the last presented frame
    std::string scene;                // scene-order index or substring; empty boots the first scene
    int width = 960;                  // client width from gbtg.ini
    int height = 540;
    int vsync = 1;
    bool widescreen = false; // gbtg.ini [Video] Widescreen, or --widescreen
    bool fullscreen = false; // gbtg.ini [Video] Fullscreen (the SDL window), or --fullscreen / --windowed
    bool render_thread = true; // gbtg.ini [Video] RenderThread, or --render-thread 0|1 (GBTG_OPTIMIZE builds)
    // When the boot frame (Game::init, which loads the first script) began. Paced runs count fields from it.
    std::chrono::steady_clock::time_point boot_started = std::chrono::steady_clock::now();
};

int run_platform(Game& game, const PlatformConfig& cfg);

} // namespace gbtg
