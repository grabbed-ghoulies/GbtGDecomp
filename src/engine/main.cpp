#include "gbtg/engine_internal.h"
#include "gbtg/game.h"
#include "gbtg/gfx.h"
#include "gbtg/host_config.h"
#include "gbtg/language.h"
#include "gbtg/log.h"
#include "camera/display_view.h"
#include "gbtg/platform.h"

#include "graphics/render_profile.h"
#include "runtime/throw_trace.h"
#include "saves/save_root.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "plugins/plugins.h"
#endif
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
    gbtg::install_throw_trace(); // A failed .at() lookup or a gbtg::Error prints where it was thrown.
    try
    {
        gbtg::log::set_verbose(has_flag(argc, argv, "--verbose"));
        // Renewed every boot, in the host data directory like gbtg.ini (beside the executable on Windows).
        const auto data_dir = gbtg::host_data_dir(argc > 0 ? argv[0] : "");
        const auto log_file = data_dir / "gbtg.log";
        if (gbtg::log::open(log_file))
        {
            gbtg::log::verbose("log %s", log_file.string().c_str());
        }
        else
        {
            gbtg::log::warn("log: could not open %s", log_file.string().c_str());
        }
        const gbtg::HostConfig host = gbtg::load_host_config(argc > 0 ? argv[0] : "");

        gbtg::PlatformConfig cfg;
        cfg.game_dir = arg(argc, argv, "--game-dir", host.game_dir.empty() ? "OriginalFiles" : host.game_dir.string());
        cfg.headless = has_flag(argc, argv, "--headless");
        cfg.frames = int_arg(argc, argv, "--frames", 0);
        cfg.replay_after = int_arg(argc, argv, "--render-replay-after", 0);
        cfg.screenshot = arg(argc, argv, "--screenshot");
        cfg.scene = arg(argc, argv, "--scene").string();
        cfg.width = int_arg(argc, argv, "--width", host.width);
        cfg.height = int_arg(argc, argv, "--height", host.height);
        cfg.vsync = int_arg(argc, argv, "--vsync", host.vsync);
        cfg.widescreen = host.widescreen || has_flag(argc, argv, "--widescreen");
        cfg.render_thread = int_arg(argc, argv, "--render-thread", host.render_thread ? 1 : 0) != 0;
        cfg.fullscreen = (host.fullscreen || has_flag(argc, argv, "--fullscreen")) && !has_flag(argc, argv, "--windowed");
#if !defined(_WIN32)
        // The data directory holds saves/ and, as the working directory, cache/; the game folder stays where the
        // launch named it.
        {
            std::error_code ec;
            cfg.game_dir = std::filesystem::absolute(cfg.game_dir, ec);
            gbtg::set_save_root(data_dir / "saves");
            std::filesystem::current_path(data_dir, ec);
            if (ec)
            {
                gbtg::log::warn("data directory %s: %s", data_dir.string().c_str(), ec.message().c_str());
            }
        }
#endif
        // 0x000FC310's XGetVideoFlags: the widescreen bit sets DAT_00509703.
        gbtg::entity_detail::g_video_widescreen = cfg.widescreen ? 1 : 0;
        gbtg::gfx_select_backend(arg(argc, argv, "--gfx", host.gfx_backend).string().c_str());

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
        gbtg::log::info("GbtGDecomp  game-dir=%s  %dx%d%s  volume=%d  language=%s", cfg.game_dir.string().c_str(),
                        cfg.width, cfg.height, cfg.widescreen ? " widescreen" : "", master_volume,
                        gbtg::language_folder(gbtg::g_language));
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
#if !defined(GBTG_RETAIL_GAME_ONLY)
        // Plugins load before the first disc file opens. Headless runs are tests and skip the user's plugins
        // unless --plugins-dir names a folder.
        {
            // Without --plugins-dir: the plugins built with gbtg beside the executable, then the user's in the data
            // directory (the same folder on Windows).
            const auto plugins_dir = arg(argc, argv, "--plugins-dir");
            const auto exe_dir = gbtg::host_exe_dir(argc > 0 ? argv[0] : "");
            if (!has_flag(argc, argv, "--no-plugins") && (!plugins_dir.empty() || !cfg.headless))
            {
                const std::vector<std::filesystem::path> dirs = plugins_dir.empty()
                    ? std::vector<std::filesystem::path>{exe_dir / "plugins", data_dir / "plugins"}
                    : std::vector<std::filesystem::path>{plugins_dir};
                gbtg::load_plugins(dirs, cfg.game_dir, exe_dir);
            }
        }
#endif
        game.init(cfg.game_dir, cfg.scene);
#if !defined(GBTG_RETAIL_GAME_ONLY)
        gbtg::plugins_boot(game);
#endif
        if (gbtg::render_profile::enabled)
        {
            gbtg::log::info("profile game_init_ms=%g actor_update_hold=unchanged", std::chrono::duration<double, std::milli>(gbtg::render_profile::Clock::now() - startup).count());
        }
        return gbtg::run_platform(game, cfg);
    }
    catch (const std::exception& ex)
    {
        gbtg::log::error("error: %s", ex.what());
        if (dynamic_cast<const gbtg::Error*>(&ex) != nullptr)
        {
            gbtg::print_last_error_stack(); // Where this error was thrown.
        }
        gbtg::log::error("usage: gbtg --game-dir <dir> [--headless] [--frames N] [--screenshot file.bmp] [--scene name-or-index] [--width W --height H --vsync N] [--fullscreen|--windowed] [--gfx d3d11|vulkan|null] [--render-thread 0|1] [--render-profile file.csv] [--render-reference] [--cpu-vertex] [--validate-gpu-vertices] [--validate-render-boundary] [--verbose]");
        return 1;
    }
}
