#include "gbtg/game.hpp"
#include "gbtg/platform.hpp"

#include <iostream>
#include <string>

namespace
{

std::filesystem::path arg(int argc, char** argv, const std::string& key, const std::string& def = {})
{
    for (int i = 1; i < argc - 1; ++i)
    {
        if (argv[i] == key)
        {
            return argv[i + 1];
        }
    }
    return def;
}

bool has_flag(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] == key)
        {
            return true;
        }
    }
    return false;
}

int int_arg(int argc, char** argv, const std::string& key, int def)
{
    for (int i = 1; i < argc - 1; ++i)
    {
        if (argv[i] == key)
        {
            return std::stoi(argv[i + 1]);
        }
    }
    return def;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        gbtg::PlatformConfig cfg;
        cfg.game_dir = arg(argc, argv, "--game-dir", "OriginalFiles");
        cfg.headless = has_flag(argc, argv, "--headless");
        cfg.frames = int_arg(argc, argv, "--frames", 0);
        cfg.screenshot = arg(argc, argv, "--screenshot");
        cfg.scene = arg(argc, argv, "--scene").string();
        std::cout << "GbtGDecomp  game-dir=" << cfg.game_dir.string() << "\n";
        gbtg::Game game;
        game.init(cfg.game_dir, cfg.scene);
        return gbtg::run_platform(game, cfg);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        std::cerr << "usage: gbtg --game-dir <dir> [--headless] [--frames N] [--screenshot file.bmp] [--scene name-or-index]\n";
        return 1;
    }
}
