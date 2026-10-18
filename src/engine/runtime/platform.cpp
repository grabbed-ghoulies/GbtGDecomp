#include "gbtg/gfx.h"
#include "../graphics/render_profile.h"
#include "gbtg/platform.h"
#include "gbtg/texture.h"

#if defined(GBTG_DEBUG_UI)
#include "gbtg/debug_ui.h"
#endif

#include <chrono>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#endif

#if defined(GBTG_WITH_SDL)
#include <SDL3/SDL.h>
#endif

namespace gbtg
{
namespace
{
// The retail game runs its logic once per 60 Hz NTSC field; DAT_005096C0 holds that 1/60 s step.
constexpr std::chrono::duration<double> kLogicStep{1.0 / 60.0};

// Waits until `deadline`: a high-resolution timer sleep until about a millisecond before it, then a short spin.
void wait_until(std::chrono::steady_clock::time_point deadline)
{
#if defined(_WIN32)
    static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
#endif
    for (;;)
    {
        const auto left = deadline - std::chrono::steady_clock::now();
        if (left <= std::chrono::steady_clock::duration::zero())
        {
            return;
        }
        if (left > std::chrono::milliseconds(1))
        {
#if defined(_WIN32)
            if (timer != nullptr)
            {
                LARGE_INTEGER due{};
                // Relative 100 ns units, stopping a millisecond short.
                due.QuadPart = -std::chrono::duration_cast<std::chrono::duration<long long, std::ratio<1, 10000000>>>(
                    left - std::chrono::milliseconds(1)).count();
                if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                {
                    WaitForSingleObject(timer, INFINITE);
                    continue;
                }
            }
            Sleep(0);
#endif
            continue;
        }
#if defined(_WIN32)
        YieldProcessor();
#endif
    }
}
}

int run_platform(Game& game, const PlatformConfig& cfg)
{
    int remaining = cfg.frames;
    if ((cfg.headless || !cfg.screenshot.empty()) && remaining <= 0)
    {
        remaining = 1;
    }

    GfxConfig gfx_cfg;
    gfx_cfg.width = cfg.width;
    gfx_cfg.height = cfg.height;
    gfx_cfg.vsync = cfg.vsync;
    gfx_cfg.visible = !cfg.headless && cfg.screenshot.empty();
    const auto gfx_start = render_profile::Clock::now();
    const bool gfx_ok = gfx_init(gfx_cfg);
    if (render_profile::enabled)
    {
        std::cout << "profile gfx_init_ms=" << std::chrono::duration<double, std::milli>(render_profile::Clock::now() - gfx_start).count() << "\n";
    }
#if defined(GBTG_DEBUG_UI)
    if (cfg.debug_ui && !debug_ui_menu_open())
    {
        debug_ui_toggle_menu();
    }
#endif
    std::cout << "gfx: " << (gfx_ok ? gfx_backend_name() : "unavailable") << (gfx_has_window() ? " window" : " offscreen")
              << "\n";

#if !defined(GBTG_WITH_SDL)
    if (!gfx_has_window() && remaining <= 0)
    {
        remaining = 1;
    }
#endif

#if defined(GBTG_WITH_SDL)
    SDL_Window* win = nullptr;
    if (!gfx_has_window() && !cfg.headless)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::cerr << "SDL_Init: " << SDL_GetError() << "\n";
            gfx_shutdown();
            return 1;
        }
        win = SDL_CreateWindow("GbtGDecomp", cfg.width, cfg.height, 0);
    }
#endif

    // Interactive runs pace each pass to the next 60 Hz slot whatever the display refresh, so input is sampled and
    // the logic ticks at the retail rate. Headless and screenshot runs tick as fast as they can.
    const bool paced = !cfg.headless && cfg.screenshot.empty();
    const auto step = std::chrono::duration_cast<std::chrono::steady_clock::duration>(kLogicStep);
    auto next = std::chrono::steady_clock::now();
    while (game.running)
    {
        if (paced)
        {
            wait_until(next);
            next += step;
            const auto now = std::chrono::steady_clock::now();
            if (now - next > step * 4)
            {
                next = now + step; // Too far behind (a stall or a breakpoint): restart the schedule.
            }
        }
        InputState in;
        bool gfx_quit = false;
        gfx_pump(gfx_quit);
        if (gfx_quit)
        {
            in.quit = true;
        }
#if defined(_WIN32)
        // Xbox pad is read in frame_tick. On PC the host copies the keyboard into that same pad state.
#if defined(GBTG_DEBUG_UI)
        // F8 and F12 never enter InputState. While the menu bar is open, keyboard
        // mappings stay clear so the scene list can use those keys.
        static bool f8_down = false;
        static bool f12_down = false;
        const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        const bool f12 = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
        if (f8 && !f8_down)
        {
            debug_ui_toggle_menu();
        }
        if (f12 && !f12_down)
        {
            debug_ui_toggle_overlay();
        }
        f8_down = f8;
        f12_down = f12;
        if (!debug_ui_menu_open())
        {
#endif
        if (GetAsyncKeyState(VK_UP) & 0x8000)
        {
            in.up = true;
        }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        {
            in.down = true;
        }
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        {
            in.left = true;
        }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        {
            in.right = true;
        }
        if (GetAsyncKeyState(VK_RETURN) & 0x8000)
        {
            in.accept = true;
        }
        if (GetAsyncKeyState(VK_BACK) & 0x8000 || GetAsyncKeyState('B') & 0x8000)
        {
            in.back = true;
        }
        // Start is its own bit. Enter stays A so the two are never down together.
        if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        {
            in.start = true;
        }
        // 0x00014C10. X is bit 0x0400 (keyboard code 5). '[' and ']' stand in for
        // the trigger bits 0x4000 and 0x8000 (codes 9 and 10).
        if (GetAsyncKeyState('X') & 0x8000)
        {
            in.page = true;
        }
        if (GetAsyncKeyState(VK_OEM_4) & 0x8000)
        {
            in.caret_left = true;
        }
        if (GetAsyncKeyState(VK_OEM_6) & 0x8000)
        {
            in.caret_right = true;
        }
#if defined(GBTG_DEBUG_UI)
        }
#endif
        // Keyboard stand-ins for the sticks on port 0: WASD is the left stick, IJKL the right.
#if defined(GBTG_DEBUG_UI)
        if (!debug_ui_menu_open())
#endif
        {
            const auto axis = [](int negative, int positive) -> i16
            {
                const bool low = (GetAsyncKeyState(negative) & 0x8000) != 0;
                const bool high = (GetAsyncKeyState(positive) & 0x8000) != 0;
                return i16(high == low ? 0 : high ? 32767 : -32767);
            };
            in.pads[0].lx = axis('A', 'D');
            in.pads[0].ly = axis('S', 'W');
            in.pads[0].rx = axis('J', 'L');
            in.pads[0].ry = axis('K', 'I');
        }
        // 0x001555B0 polls XInput into the four ports: thumbsticks, trigger bytes and the button word. XInput's
        // shoulders stand in for Black (RB) and White (LB); 0x00154E50 sets 0x4000/0x8000 from trigger bytes >= 30.
        for (DWORD port = 0; port < 4; ++port)
        {
            XINPUT_STATE state{};
            if (XInputGetState(port, &state) != 0)
            {
                continue;
            }
            const XINPUT_GAMEPAD& pad = state.Gamepad;
            const WORD buttons = pad.wButtons;
            auto& packet = in.pads[port];
            packet.lx = pad.sThumbLX;
            packet.ly = pad.sThumbLY;
            packet.rx = pad.sThumbRX;
            packet.ry = pad.sThumbRY;
            packet.trigger_l = pad.bLeftTrigger;
            packet.trigger_r = pad.bRightTrigger;
            const std::pair<WORD, u16> map[]{{XINPUT_GAMEPAD_DPAD_UP, 0x0001}, {XINPUT_GAMEPAD_DPAD_DOWN, 0x0002},
                {XINPUT_GAMEPAD_DPAD_LEFT, 0x0004}, {XINPUT_GAMEPAD_DPAD_RIGHT, 0x0008},
                {XINPUT_GAMEPAD_START, 0x0010}, {XINPUT_GAMEPAD_BACK, 0x0020},
                {XINPUT_GAMEPAD_LEFT_THUMB, 0x0040}, {XINPUT_GAMEPAD_RIGHT_THUMB, 0x0080},
                {XINPUT_GAMEPAD_A, 0x0100}, {XINPUT_GAMEPAD_B, 0x0200}, {XINPUT_GAMEPAD_X, 0x0400},
                {XINPUT_GAMEPAD_Y, 0x0800}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, 0x1000},
                {XINPUT_GAMEPAD_LEFT_SHOULDER, 0x2000}};
            for (const auto& [from, to] : map)
            {
                if ((buttons & from) != 0)
                {
                    packet.buttons = u16(packet.buttons | to);
                }
            }
            if (pad.bLeftTrigger >= 30)
            {
                packet.buttons = u16(packet.buttons | 0x4000);
            }
            if (pad.bRightTrigger >= 30)
            {
                packet.buttons = u16(packet.buttons | 0x8000);
            }
            // The host frontend still reads these booleans directly.
            in.up = in.up || (packet.buttons & 0x0001) != 0;
            in.down = in.down || (packet.buttons & 0x0002) != 0;
            in.left = in.left || (packet.buttons & 0x0004) != 0;
            in.right = in.right || (packet.buttons & 0x0008) != 0;
            in.start = in.start || (packet.buttons & 0x0010) != 0;
            in.accept = in.accept || (packet.buttons & 0x0100) != 0;
            in.back = in.back || (packet.buttons & 0x0200) != 0;
            in.page = in.page || (packet.buttons & 0x0400) != 0;
            in.caret_left = in.caret_left || (packet.buttons & 0x4000) != 0;
            in.caret_right = in.caret_right || (packet.buttons & 0x8000) != 0;
        }
#endif
#if defined(GBTG_WITH_SDL)
        if (win)
        {
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_EVENT_QUIT)
                {
                    in.quit = true;
                }
            }
            const bool* keys = SDL_GetKeyboardState(nullptr);
            if (keys[SDL_SCANCODE_ESCAPE])
            {
                in.quit = true;
            }
            if (keys[SDL_SCANCODE_UP])
            {
                in.up = true;
            }
            if (keys[SDL_SCANCODE_DOWN])
            {
                in.down = true;
            }
            if (keys[SDL_SCANCODE_LEFT])
            {
                in.left = true;
            }
            if (keys[SDL_SCANCODE_RIGHT])
            {
                in.right = true;
            }
            if (keys[SDL_SCANCODE_RETURN])
            {
                in.accept = true;
            }
            if (keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_B])
            {
                in.back = true;
            }
            if (keys[SDL_SCANCODE_SPACE])
            {
                in.start = true;
            }
            if (keys[SDL_SCANCODE_X])
            {
                in.page = true;
            }
            if (keys[SDL_SCANCODE_LEFTBRACKET])
            {
                in.caret_left = true;
            }
            if (keys[SDL_SCANCODE_RIGHTBRACKET])
            {
                in.caret_right = true;
            }
        }
#endif
        if (cfg.headless && render_profile::enabled)
        {
            in = {}; // Reproducible neutral host input for automated measurements.
        }
        const float dt = float(kLogicStep.count());
        const auto frame_start = render_profile::Clock::now();
        render_profile::count_allocations = render_profile::enabled;
        gfx_begin_frame();
        if (cfg.replay_after > 0 && render_profile::number >= static_cast<unsigned long long>(cfg.replay_after))
        {
            if (!render_profile::replay)
            {
                throw Error("selected replay frame contains no native recorded commands");
            }
            render_profile::replay();
        }
        else
        {
            render_profile::capture_replay = cfg.replay_after > 0 && render_profile::number + 1 == static_cast<unsigned long long>(cfg.replay_after);
            render_profile::Scope measurement(render_profile::Tick);
            game.tick(dt, in);
            render_profile::capture_replay = false;
        }
#if defined(GBTG_DEBUG_UI)
        debug_ui_build(game, dt);
#endif
        gfx_end_frame();
        render_profile::count_allocations = false;
        render_profile::finish(std::chrono::duration<double, std::milli>(render_profile::Clock::now() - frame_start).count());
        if (remaining > 0 && --remaining == 0)
        {
            game.running = false;
        }
#if defined(GBTG_WITH_SDL)
        if (win)
        {
            SDL_Delay(1);
        }
#endif
    }

    if (!cfg.screenshot.empty())
    {
        const auto capture_start = render_profile::Clock::now();
        RgbaImage image;
        const bool captured = gfx_readback(image);
        const double readback_ms = std::chrono::duration<double, std::milli>(render_profile::Clock::now() - capture_start).count();
        if (captured)
        {
            int changed = 0;
            for (size_t i = 0; i + 3 < image.bgra.size(); i += 4)
            {
                const u8 b = image.bgra[i];
                const u8 g = image.bgra[i + 1];
                const u8 r = image.bgra[i + 2];
                const bool clear = b >= 20 && b <= 26 && g >= 12 && g <= 18 && r >= 10 && r <= 16;
                if (!clear)
                {
                    ++changed;
                }
            }
            write_bmp(cfg.screenshot, image);
            std::cout << "screenshot " << cfg.screenshot.string() << " " << image.width << "x" << image.height
                      << " drawn_px=" << changed << "\n";
        }
        else
        {
            std::cerr << "screenshot failed\n";
        }
        if (render_profile::enabled)
        {
            std::cout << "profile readback_ms=" << readback_ms << " capture_total_ms=" << std::chrono::duration<double, std::milli>(render_profile::Clock::now() - capture_start).count() << "\n";
        }
    }

    std::cout << "frames " << game.frame << " t=" << game.time << "s scene " << game.scene_name;
    if (game.camera && game.camera->actor)
    {
        const Vec3& p = game.camera->actor->position;
        const Quat& q = game.camera->actor->quat;
        std::cout << " cam=" << p.x << "," << p.y << "," << p.z << " quat=" << q.x << "," << q.y << "," << q.z << ","
                  << q.w;
    }
    std::cout << "\n";

    // The Game still owns every captured resource here; release replay commands before unloading it.
    render_profile::replay = {};
    gfx_shutdown();

#if defined(GBTG_WITH_SDL)
    if (win)
    {
        SDL_DestroyWindow(win);
        SDL_Quit();
    }
#endif
    return 0;
}

} // namespace gbtg
