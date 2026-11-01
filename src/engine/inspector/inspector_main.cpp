#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"
#include "inspector_ui.h"
#include "inspector_graphics.h"
#include "loctext_tool.h"
#include "scene_build_tool.h"
#include "scene_builder.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#endif
#include "gbtg/gfx.h"
#include "gbtg/engine_internal.h"
#include "gbtg/host_config.h"
#include "gbtg/language.h"
#include "gbtg/log.h"
#include <chrono>
#include <thread>
#include <cstdio>

namespace
{
std::string argument(int argc, char** argv, std::string_view key, const std::string& fallback = {})
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (argv[i] == key)
        {
            return argv[i + 1];
        }
    }
    return fallback;
}
bool flag(int argc, char** argv, std::string_view key)
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
}

int main(int argc, char** argv)
{
    using namespace gbtg;
    bool graphics = false;
    try
    {
        // Renewed every boot, beside the executable; gbtg.log belongs to the game.
        const auto log_file = host_exe_dir(argc > 0 ? argv[0] : "") / "gbtg-inspector.log";
        if (!log::open(log_file))
        {
            log::warn("log: could not open %s", log_file.string().c_str());
        }
#if !defined(GBTG_RETAIL_GAME_ONLY)
        // Plugin authoring tools (docs/PLUGINS.md); they run without a window.
        if (const auto source = argument(argc, argv, "--write-loctext"); !source.empty())
        {
            return write_loctext_sources(source, argument(argc, argv, "--out", "."));
        }
        if (const auto loctext = argument(argc, argv, "--check-loctext"); !loctext.empty())
        {
            return check_loctext_writer(loctext);
        }
        if (const auto bundles = argument(argc, argv, "--check-bundle-builder"); !bundles.empty())
        {
            return check_bundle_builder(bundles);
        }
        if (const auto recipe = argument(argc, argv, "--build-recipe"); !recipe.empty())
        {
            return build_recipe_file(recipe, argument(argc, argv, "--game-dir", "OriginalFiles"),
                argument(argc, argv, "--out", "scene.bnl"));
        }
        if (const auto scene = argument(argc, argv, "--recipe-from-scene"); !scene.empty())
        {
            return write_recipe_from_scene(scene, argument(argc, argv, "--as"),
                argument(argc, argv, "--game-dir", "OriginalFiles"), argument(argc, argv, "--out", "scene.scene"));
        }
#endif
        GfxConfig config;
        config.width = std::stoi(argument(argc, argv, "--width", "1440"));
        config.height = std::stoi(argument(argc, argv, "--height", "900"));
        config.vsync = std::stoi(argument(argc, argv, "--vsync", "1"));
        if (config.width < 800 || config.height < 600)
        {
            throw Error("inspector workspace requires at least 800x600");
        }
        const auto screenshot = argument(argc, argv, "--screenshot");
        const auto viewport_capture = argument(argc, argv, "--viewport-screenshot");
        config.visible = !flag(argc, argv, "--headless") && screenshot.empty() && viewport_capture.empty();
        config.title = "GbtG Runtime Scene Inspector";
        int remaining = std::stoi(argument(argc, argv, "--frames", config.visible ? "0" : "180"));
        if (remaining < 0)
        {
            throw Error("--frames must be nonnegative");
        }
        if (!config.visible && remaining == 0)
        {
            remaining = 180;
        }
        if (!gfx_init(config))
        {
            throw Error("inspector requires the D3D11 renderer");
        }
        graphics = true;
        pcm_set_master_volume(0.f); // Loading may instantiate native voices; the static workspace is silent.
        g_language = 0;
        int result = 0;
        {
            InspectorSession session;
            session.log_path = argument(argc, argv, "--log",
                (std::filesystem::path(argv[0]).parent_path() / "inspector-errors.log").string());
            InspectorUI ui;
            ui.directory = argument(argc, argv, "--game-dir", "OriginalFiles");
            ui.depth_test = flag(argc, argv, "--depth-overlays");
#if !defined(GBTG_RETAIL_GAME_ONLY)
            // Plugins as the game loads them (docs/PLUGINS.md), so plugin scenes and files load here too, and the
            // scene builder, which edits the recipes of the plugin in --scene-plugin.
            const std::filesystem::path exe_dir = host_exe_dir(argv[0]);
            const std::string plugins_dir = argument(argc, argv, "--plugins-dir");
            if (!flag(argc, argv, "--no-plugins"))
            {
                load_plugins({plugins_dir.empty() ? exe_dir / "plugins" : std::filesystem::path(plugins_dir)},
                    ui.directory, exe_dir);
            }
            else
            {
                load_plugins({}, ui.directory, exe_dir);
            }
            SceneBuilder builder(ui.directory, argument(argc, argv, "--scene-plugin",
                (exe_dir / "plugins" / "my_scenes").string()));
            if (const auto recipe = argument(argc, argv, "--recipe"); !recipe.empty())
            {
                builder.open_recipe(recipe);
                plugin_register_recipe(recipe);
                ui.show_scene_builder = true;
                if (flag(argc, argv, "--resolve"))
                {
                    builder.resolve();
                }
            }
            ui.extra_windows = [&builder, &ui](InspectorSession& inspected)
            {
                if (ui.show_scene_builder)
                {
                    builder.draw(inspected, ui.show_scene_builder);
                }
            };
            // A --resolve run boots straight into its build; any other --recipe run opens on the recipe's scene.
            if (!builder.busy())
            {
                session.load(ui.directory, argument(argc, argv, "--scene",
                    builder.scene_name().empty() ? "chapter1_scene1_2playcam" : builder.scene_name()));
            }
#else
            session.load(ui.directory, argument(argc, argv, "--scene", "chapter1_scene1_2playcam"));
#endif
            bool camera_arguments_applied = false;
            auto last = std::chrono::steady_clock::now();
            bool quit = false;
            while (!quit)
            {
                gfx_pump(quit);
                if (quit)
                {
                    break;
                }
                if (inspector_window_minimized())
                {
                    ui.release_capture();
                    last = std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                    continue;
                }
                const auto now = std::chrono::steady_clock::now();
                const float dt = config.visible ? std::chrono::duration<float>(now - last).count() : 1.f / 60.f;
                last = now;
#if !defined(GBTG_RETAIL_GAME_ONLY)
                if (!builder.restart_scene.empty())
                {
                    // The scene builder rebuilt its recipe: boot a new runtime so the bundle is assembled again.
                    ui.release_capture();
                    const std::string scene = builder.restart_scene;
                    builder.restart_scene.clear();
                    session.restart(ui.directory, scene);
                    ui.sync_angles = true;
                    camera_arguments_applied = false;
                }
#endif
                if (!ui.load_request.empty() || ui.retry)
                {
                    ui.release_capture();
                    const std::string scene = ui.load_request.empty() ? session.requested : ui.load_request;
                    ui.load_request.clear();
                    ui.retry = false;
                    session.load(ui.directory, scene);
                    ui.sync_angles = true;
                    camera_arguments_applied = false;
                }
                // Loading is static inspection preparation: batch native pumps so rebuilding
                // around several unsupported markers does not wait through each transition in real time.
                for (int pump = 0; pump < 8 && session.state == InspectionState::Loading; ++pump)
                {
                    session.advance_loading();
                }
                if (session.state == InspectionState::Ready && !camera_arguments_applied)
                {
                    const std::string offset = argument(argc, argv, "--camera-offset");
                    Vec3 movement;
                    if (!offset.empty())
                    {
                        if (std::sscanf(offset.c_str(), "%f,%f,%f", &movement.x, &movement.y, &movement.z) != 3)
                        {
                            throw Error("--camera-offset expects X,Y,Z");
                        }
                        session.camera.position.x += movement.x;
                        session.camera.position.y += movement.y;
                        session.camera.position.z += movement.z;
                    }
                    const std::string angles = argument(argc, argv, "--camera-angles");
                    float yaw, pitch;
                    if (!angles.empty())
                    {
                        if (std::sscanf(angles.c_str(), "%f,%f", &yaw, &pitch) != 2)
                        {
                            throw Error("--camera-angles expects yaw,pitch in degrees");
                        }
                        session.camera.rotation = inspection_rotation(yaw * .01745329252f, pitch * .01745329252f);
                    }
                    camera_arguments_applied = true;
                    ui.sync_angles = true;
                }
                gfx_begin_frame();
                session.present();
                void* viewport = inspector_capture_viewport(ui.depth_test && session.state == InspectionState::Ready);
                ui.build(session, viewport, dt);
                if (ui.capture)
                {
                    RgbaImage image;
                    try
                    {
                        if (session.state != InspectionState::Ready || !inspector_viewport_readback(image))
                        {
                            throw Error("no ready scene viewport to capture");
                        }
                        write_bmp(ui.screenshot_path, image);
                        ui.capture_status = "Saved " + std::string(ui.screenshot_path);
                    }
                    catch (const std::exception& error)
                    {
                        ui.capture_status = error.what();
                    }
                    ui.capture = false;
                }
                inspector_finish_frame();
#if !defined(GBTG_RETAIL_GAME_ONLY)
                // A scene builder build counts no frames until it has loaded.
                if (builder.busy() || session.state == InspectionState::Loading)
                {
                    continue;
                }
#endif
                if (remaining > 0 && --remaining == 0)
                {
                    break;
                }
                if (config.visible)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            ui.release_capture();
            const auto save = [](const std::string& path, bool viewport)
            {
                if (!path.empty())
                {
                    RgbaImage image;
                    if (!(viewport ? inspector_viewport_readback(image) : gfx_readback(image)))
                    {
                        throw Error("inspector screenshot readback failed");
                    }
                    write_bmp(path, image);
                    log::info("screenshot %s", path.c_str());
                }
            };
            save(screenshot, false);
            save(viewport_capture, true);
            log::info("inspector state=%d scene=%s native_frame=%u objects_and_markers=%zu commands=%zu "
                      "omitted_markers=%zu log=%s", int(session.state), session.requested.c_str(), session.frozen_frame,
                      session.items.size(), session.frame.draws.size(), session.inspection.omitted.size(),
                      session.log_path.string().c_str());
            if (session.state != InspectionState::Ready)
            {
                log::error("%s", session.error.empty() ? "scene did not reach Ready" : session.error.c_str());
                result = 1;
            }
        }
        inspector_release_graphics();
        gfx_shutdown();
        return result;
    }
    catch (const std::exception& error)
    {
        log::error("inspector: %s", error.what());
        if (graphics)
        {
            inspector_release_graphics();
            gfx_shutdown();
        }
        log::error("usage: gbtg-inspector --game-dir DIR [--scene name-or-index] [--frames N] "
            "[--screenshot workspace.bmp] [--viewport-screenshot scene.bmp] [--headless] "
            "[--width W --height H --vsync N] [--depth-overlays] "
            "[--camera-offset X,Y,Z --camera-angles yaw,pitch] [--log errors.log]");
#if !defined(GBTG_RETAIL_GAME_ONLY)
        log::error("       gbtg-inspector --write-loctext <source dir> --out <plugin dir>");
        log::error("       gbtg-inspector --check-loctext <game loctext dir>");
        log::error("       gbtg-inspector --check-bundle-builder <game bundles dir>");
        log::error("       gbtg-inspector --build-recipe <recipe> --game-dir DIR --out <file.bnl>");
        log::error("       gbtg-inspector --recipe-from-scene <aid_script_...> --as <aid_script_...> --game-dir DIR "
                   "--out <file.scene>");
#endif
        return 1;
    }
}
#endif
