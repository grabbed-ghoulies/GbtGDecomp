#include "gbtg/gfx.hpp"
#include "gbtg/platform.hpp"
#include "gbtg/texture.hpp"

#include <chrono>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(GBTG_WITH_SDL)
#include <SDL3/SDL.h>
#endif

namespace gbtg
{

int run_platform(Game& game, const PlatformConfig& cfg)
{
    int remaining = cfg.frames;
    if ((cfg.headless || !cfg.screenshot.empty()) && remaining <= 0)
    {
        remaining = 1;
    }

    GfxConfig gfx_cfg;
    gfx_cfg.visible = !cfg.headless && cfg.screenshot.empty();
    const bool gfx_ok = gfx_init(gfx_cfg);
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
        win = SDL_CreateWindow("GbtGDecomp", 640, 360, 0);
    }
#endif

    auto last = std::chrono::steady_clock::now();
    while (game.running)
    {
        InputState in;
        bool gfx_quit = false;
        gfx_pump(gfx_quit);
        if (gfx_quit)
        {
            in.quit = true;
        }
#if defined(_WIN32)
        // Xbox pad is read in frame_tick. On PC the host copies the keyboard into that same pad state.
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
        }
#endif
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (cfg.headless || !cfg.screenshot.empty())
        {
            dt = 1.f / 60.f;
        }
        gfx_begin_frame();
        game.tick(dt, in);
        gfx_end_frame();
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
        RgbaImage image;
        if (gfx_readback(image))
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
    }

    std::cout << "frames " << game.frame << " t=" << game.time << "s scene " << game.scene_name;
    if (game.cutscenes.playing && !game.cutscenes.items.empty())
    {
        const auto& c = game.cutscenes.items[game.cutscenes.index];
        std::cout << " cutscene[" << game.cutscenes.index << "] " << c.name << " t=" << game.cutscenes.time << "/"
                  << c.duration << "s";
    }
    if (game.camera && game.camera->actor)
    {
        const Vec3& p = game.camera->actor->position;
        const Quat& q = game.camera->actor->quat;
        std::cout << " cam=" << p.x << "," << p.y << "," << p.z << " quat=" << q.x << "," << q.y << "," << q.z << ","
                  << q.w;
    }
    std::cout << "\n";

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
