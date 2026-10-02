#pragma once

#include "gbtg/game.hpp"

namespace gbtg
{

struct PlatformConfig
{
    std::filesystem::path game_dir{"OriginalFiles"};
    bool headless = false;
    int frames = 0; // 0 = until quit; headless defaults to 1
    std::filesystem::path screenshot; // PC readback of the last presented frame
    std::string scene;                // scene-order index or substring; empty boots the first scene
};

int run_platform(Game& game, const PlatformConfig& cfg);

} // namespace gbtg
