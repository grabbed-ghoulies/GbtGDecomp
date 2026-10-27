#include "gbtg/engine_internal.h"
#include "gbtg/game.h"
#include "gbtg/gfx.h"
#include "gbtg/host_config.h"
#include "gbtg/language.h"
#include "gbtg/log.h"
#include "gbtg/platform.h"

#include "graphics/render_profile.h"
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
        // Renewed every boot, beside the executable like gbtg.ini.
        const auto log_file = gbtg::host_exe_dir(argc > 0 ? argv[0] : "") / "gbtg.log";
        if (!gbtg::log::open(log_file))
        {
            gbtg::log::warn("log: could not open %s", log_file.string().c_str());
        }
        const gbtg::HostConfig host = gbtg::load_host_config(argc > 0 ? argv[0] : "");

        gbtg::PlatformConfig cfg;
        cfg.game_dir = arg(argc, argv, "--game-dir", "OriginalFiles");
        cfg.headless = has_flag(argc, argv, "--headless");
        cfg.debug_ui = has_flag(argc, argv, "--debug-ui");
        cfg.frames = int_arg(argc, argv, "--frames", 0);
        cfg.replay_after = int_arg(argc, argv, "--render-replay-after", 0);
        cfg.screenshot = arg(argc, argv, "--screenshot");
        cfg.scene = arg(argc, argv, "--scene").string();
        cfg.width = int_arg(argc, argv, "--width", host.width);
        cfg.height = int_arg(argc, argv, "--height", host.height);
        cfg.vsync = int_arg(argc, argv, "--vsync", host.vsync);

        const auto profile = arg(argc, argv, "--render-profile");
        if (!profile.empty())
        {
            gbtg::render_profile::start(profile.string());
#if defined(_DEBUG)
            gbtg::log::info("profile build=Debug");
#else
            gbtg::log::info("profile build=Release");
#endif
            gbtg::log::info("profile optimize=%s", GBTG_OPTIMIZE ? "on" : "off");
        }
        if (cfg.replay_after > 0 && !gbtg::render_profile::enabled)
        {
            throw gbtg::Error("command replay requires --render-profile");
        }
        // Headless runs are tests, which play at 10% master volume (AGENTS.md).
        const int master_volume = cfg.headless ? 10 : host.master_volume;
        gbtg::pcm_set_master_volume(static_cast<float>(master_volume) / 100.f);
        gbtg::g_language = host.language;
        {
            std::error_code ec;
            const auto loctext = cfg.game_dir / "loctext" / gbtg::language_folder(gbtg::g_language);
            if (gbtg::g_language != 0 && !std::filesystem::is_directory(loctext, ec))
            {
                gbtg::log::warn("config: %s missing, using English", loctext.string().c_str());
                gbtg::g_language = 0;
            }
        }
        gbtg::log::info("GbtGDecomp  game-dir=%s  %dx%d  volume=%d  language=%s", cfg.game_dir.string().c_str(),
                        cfg.width, cfg.height, master_volume, gbtg::language_folder(gbtg::g_language));
        gbtg::render_profile::reference = has_flag(argc, argv, "--render-reference");
        gbtg::render_profile::cpu_vertex = has_flag(argc, argv, "--cpu-vertex");
        gbtg::render_profile::validate_vertex = has_flag(argc, argv, "--validate-gpu-vertices");
        if (has_flag(argc, argv, "--validate-render-boundary"))
        {
            gbtg::gfx_validate_native_boundary();
            return 0;
        }
        gbtg::Game game;
        const auto startup = gbtg::render_profile::Clock::now();
        cfg.boot_started = std::chrono::steady_clock::now();
        game.init(cfg.game_dir, cfg.scene);
        if (gbtg::render_profile::enabled)
        {
            gbtg::log::info("profile game_init_ms=%g actor_update_hold=unchanged", std::chrono::duration<double, std::milli>(gbtg::render_profile::Clock::now() - startup).count());
        }
        return gbtg::run_platform(game, cfg);
    }
    catch (const std::exception& ex)
    {
        gbtg::log::error("error: %s", ex.what());
        gbtg::log::error("usage: gbtg --game-dir <dir> [--headless] [--frames N] [--screenshot file.bmp] [--scene name-or-index] [--width W --height H --vsync N] [--render-profile file.csv] [--render-reference] [--cpu-vertex] [--validate-gpu-vertices] [--validate-render-boundary]");
        return 1;
    }
}
