#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"
#include "inspector_ui.h"
#include "inspector_graphics.h"
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
            session.load(ui.directory, argument(argc, argv, "--scene", "chapter1_scene1_2playcam"));
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
        return 1;
    }
}
#endif
