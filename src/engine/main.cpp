#include "gbtg/engine_internal.h"
#include "gbtg/game.h"
#include "gbtg/host_config.h"
#include "gbtg/language.h"
#include "gbtg/platform.h"

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

gbtg::PlatformConfig create_platform_config(int argc, char** argv)
{
    gbtg::PlatformConfig cfg;
    
    cfg.game_dir = arg(argc, argv, "--game-dir", "OriginalFiles");
    cfg.headless = has_flag(argc, argv, "--headless");
    cfg.frames = int_arg(argc, argv, "--frames", 0);
    cfg.screenshot = arg(argc, argv, "--screenshot");
    cfg.scene = arg(argc, argv, "--scene").string();
    
    return cfg;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        gbtg::PlatformConfig cfg = create_platform_config(argc, argv);
        const gbtg::HostConfig host = gbtg::load_host_config(argc > 0 ? argv[0] : "");
        cfg.width = host.width;
        cfg.height = host.height;
        cfg.vsync = host.vsync;
        gbtg::pcm_set_master_volume(static_cast<float>(host.master_volume) / 100.f);
        gbtg::g_language = host.language;
        {
            std::error_code ec;
            const auto loctext = cfg.game_dir / "loctext" / gbtg::language_folder(gbtg::g_language);
            if (gbtg::g_language != 0 && !std::filesystem::is_directory(loctext, ec))
            {
                std::cerr << "config: " << loctext.string() << " missing, using English\n";
                gbtg::g_language = 0;
            }
        }
        std::cout << "GbtGDecomp  game-dir=" << cfg.game_dir.string() << "  " << cfg.width << "x" << cfg.height
                  << "  volume=" << host.master_volume << "  language=" << gbtg::language_folder(gbtg::g_language)
                  << "\n";
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
