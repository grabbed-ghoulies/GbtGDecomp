#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"
#include "inspector_ui.h"
#include "asset_viewer.h"
#include "inspector_graphics.h"
#include "loctext_tool.h"
#include "scene_build_tool.h"
#include "scene_builder.h"
#include "../scene/factory_scene_runtime.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#endif
#include "gbtg/gfx.h"
#include "gbtg/engine_internal.h"
#include "gbtg/host_config.h"
#include "gbtg/language.h"
#include "gbtg/log.h"
#include <chrono>
#include <cmath>
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
#if !defined(GBTG_RETAIL_GAME_ONLY)
// --edit-check: the viewport editor end to end. Edit this scene, move and turn a marker and copy it through the
// editor interface the gizmo uses, Apply, then compare the native marker block the rebuilt scene loaded.
struct EditCheck
{
    int stage = 0; // 0 off, 1 start editing, 2 edit, 3 check, 4 done
    bool passed = false;
    gbtg::u32 kind = 0;
    gbtg::u32 id = 0;
    gbtg::u32 copy = 0;
    gbtg::Vec3 moved{};
    gbtg::Vec3 copied{};
    float yaw = 0.f;

    void step(gbtg::InspectorSession& session, gbtg::SceneBuilder& builder, gbtg::InspectorUI& ui)
    {
        using namespace gbtg;
        if (stage == 1)
        {
            builder.start_editing(session);
            log::info("edit check: %s", builder.status().c_str());
            stage = builder.busy() ? 2 : 4;
            return;
        }
        if (!builder.editing(session))
        {
            log::error("edit check: the loaded scene is not the recipe's (%s)", builder.status().c_str());
            stage = 4;
            return;
        }
        if (stage == 2)
        {
            for (size_t index = 0; index < session.items.size(); ++index)
            {
                InspectionItem& item = session.items[index];
                if (item.marker && item.position && item.euler && item.failure.empty())
                {
                    kind = item.type;
                    id = item.address;
                    moved = {item.position->x + 2.f, item.position->y, item.position->z - 1.f};
                    yaw = item.euler->y + 0.5f;
                    item.position = moved;
                    item.euler->y = yaw;
                    builder.set_marker_pose(item);
                    copied = {moved.x, moved.y, moved.z + 3.f};
                    const InspectionItem source = item;
                    copy = builder.duplicate_marker(session, source, copied);
                    session.selection = int(index);
                    ui.reselect = std::pair{kind, id};
                    log::info("edit check: marker kind %u id %u to %.2f %.2f %.2f yaw %.3f; copy id %u", kind, id,
                        moved.x, moved.y, moved.z, yaw, copy);
                    builder.apply(session);
                    stage = 3;
                    return;
                }
            }
            log::error("edit check: the scene has no marker with a position and rotation");
            stage = 4;
            return;
        }
        if (stage == 3)
        {
            const auto native = inspect_marker_records(session.game->factory_scene->markers());
            bool found_moved = false;
            bool found_copy = false;
            const auto near = [](const Vec3& a, const Vec3& b)
            {
                return std::abs(a.x - b.x) < 1e-3f && std::abs(a.y - b.y) < 1e-3f && std::abs(a.z - b.z) < 1e-3f;
            };
            for (const auto& item : native)
            {
                if (item.type == kind && item.address == id && item.position && item.euler)
                {
                    found_moved = near(*item.position, moved) && std::abs(item.euler->y - yaw) < 1e-4f;
                }
                if (item.type == kind && item.address == copy && item.position)
                {
                    found_copy = near(*item.position, copied);
                }
            }
            passed = found_moved && found_copy && !builder.pending();
            log::info("edit check: native moved marker %s, copy %s, pending %d -> %s", found_moved ? "matches" : "differs",
                found_copy ? "matches" : "missing", int(builder.pending()), passed ? "PASS" : "FAIL");
            stage = 4;
        }
    }
};
#endif
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
        const auto preview_capture = argument(argc, argv, "--preview-screenshot");
        config.visible = !flag(argc, argv, "--headless") && screenshot.empty() && viewport_capture.empty()
            && preview_capture.empty();
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
            AssetViewer assets(ui.directory);
            ui.assets = &assets;
            // --asset <aid>: open the asset viewer on a model or texture once the scene is ready, then
            // --asset-animation <aid> on the model; --asset-seek <0..1> pauses it there.
            std::string asset_request = argument(argc, argv, "--asset");
            std::string animation_request = argument(argc, argv, "--asset-animation");
            const std::string seek_request = argument(argc, argv, "--asset-seek");
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
            ui.editor = &builder;
            EditCheck edit_check;
            edit_check.stage = flag(argc, argv, "--edit-check") ? 1 : 0;
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
                if (session.state == InspectionState::Ready && !asset_request.empty())
                {
                    ui.show_assets = true;
                    assets.select(session, asset_request);
                    if (const std::string view = argument(argc, argv, "--asset-view"); !view.empty())
                    {
                        float yaw = 0.f;
                        float pitch = 0.f;
                        if (std::sscanf(view.c_str(), "%f,%f", &yaw, &pitch) != 2)
                        {
                            throw Error("--asset-view expects yaw,pitch in degrees");
                        }
                        assets.set_view(yaw * .01745329252f, pitch * .01745329252f);
                    }
                    asset_request.clear();
                    if (!animation_request.empty())
                    {
                        session.preview_animation(animation_request);
                        animation_request.clear();
                        if (!seek_request.empty())
                        {
                            session.seek_preview(std::stof(seek_request));
                        }
                    }
                    if (!session.preview_error.empty())
                    {
                        log::error("asset viewer: %s", session.preview_error.c_str());
                    }
                }
#if !defined(GBTG_RETAIL_GAME_ONLY)
                // --edit-check: Edit this scene, move/turn a marker and copy it, Apply, then check that the native
                // markers the rebuilt scene loaded have the edits.
                if (edit_check.stage > 0 && session.state == InspectionState::Ready && !builder.busy())
                {
                    edit_check.step(session, builder, ui);
                }
#endif
                gfx_begin_frame();
                session.present();
                void* viewport = inspector_capture_viewport(ui.wants_depth() && session.state == InspectionState::Ready);
                ui.preview_viewport = nullptr;
                if (ui.show_assets && assets.wants_preview(session))
                {
                    // The asset viewer's model, drawn alone by the native model draw on a cleared target.
                    inspector_begin_preview(assets.preview_background);
                    session.present_preview(dt);
                    if (session.preview_ready())
                    {
                        ui.preview_viewport = inspector_capture_viewport(false, 1);
                    }
                }
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
                if (builder.busy() || session.state == InspectionState::Loading
                    || (edit_check.stage > 0 && edit_check.stage < 4))
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
            const auto save = [](const std::string& path, int viewport)
            {
                if (!path.empty())
                {
                    RgbaImage image;
                    if (!(viewport >= 0 ? inspector_viewport_readback(image, viewport) : gfx_readback(image)))
                    {
                        throw Error("inspector screenshot readback failed");
                    }
                    write_bmp(path, image);
                    log::info("screenshot %s", path.c_str());
                }
            };
            save(screenshot, -1);
            save(viewport_capture, 0);
            save(preview_capture, 1);
            log::info("inspector state=%d scene=%s native_frame=%u objects_and_markers=%zu commands=%zu "
                      "omitted_markers=%zu log=%s", int(session.state), session.requested.c_str(), session.frozen_frame,
                      session.items.size(), session.frame.draws.size(), session.inspection.omitted.size(),
                      session.log_path.string().c_str());
            if (session.state != InspectionState::Ready)
            {
                log::error("%s", session.error.empty() ? "scene did not reach Ready" : session.error.c_str());
                result = 1;
            }
#if !defined(GBTG_RETAIL_GAME_ONLY)
            if (edit_check.stage > 0 && !edit_check.passed)
            {
                log::error("edit check failed");
                result = 1;
            }
#endif
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
            "[--camera-offset X,Y,Z --camera-angles yaw,pitch] [--log errors.log] "
            "[--asset aid [--asset-animation aid [--asset-seek 0..1]] --preview-screenshot model.bmp]");
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
