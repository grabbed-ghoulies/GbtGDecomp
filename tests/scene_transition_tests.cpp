#include "gbtg/engine_internal.hpp"
#include "gbtg/gfx.hpp"

#include <iostream>

namespace gbtg
{
extern i32 g_546328;
extern i32 g_54632c;
}

int main(int argc, char** argv)
{
    const bool title_test = argc == 3 && std::string_view(argv[2]) == "--title";
    const bool goop_test = argc == 3 && std::string_view(argv[2]) == "--goop";
    if (argc != 2 && argc != 4 && !title_test && !goop_test)
    {
        std::cerr << "Usage: gbtg-transition-tests GAME_DIRECTORY [WIDTH HEIGHT | --title | --goop]\n";
        return 2;
    }
    try
    {
        gbtg::GfxConfig config;
        config.visible = false;
        config.width = argc == 4 ? std::stoi(argv[2]) : 640;
        config.height = argc == 4 ? std::stoi(argv[3]) : 480;
        if (!gbtg::gfx_init(config))
        {
            throw gbtg::Error("test graphics initialization failed");
        }
        gbtg::Game game;
        game.init(argv[1], title_test ? "" : "scene0_1book");
        if (title_test)
        {
            bool animated = false;
            bool accepted = false;
            for (int frame = 0; frame < 600; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                gbtg::gfx_begin_frame();
                // Exercise the ordinary wall-clock path, including the first
                // frame after loading, rather than a headless crash check.
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (frame == 119)
                {
                    animated = game.cutscenes.playing && game.cutscenes.time > 1.f;
                }
                if (game.scene_script.name.find("gameselect") != std::string::npos &&
                    game.frontend_mode == 1 && gbtg::g_54632c == 0)
                {
                    const int previous_slot = game.slot;
                    gbtg::InputState down;
                    down.down = true;
                    gbtg::gfx_begin_frame();
                    game.tick(1.f / 59.f, down);
                    gbtg::gfx_end_frame();
                    if (game.slot != (previous_slot + 1) % 3)
                    {
                        throw gbtg::Error("slot selection ignored Down input");
                    }
                    accepted = true;
                    break;
                }
            }
            if (!animated || !accepted)
            {
                throw gbtg::Error("title animation or Start input stalled");
            }
            std::cout << "PASS: title animates and Start opens slot selection\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        bool saw_intro = false;
        bool saw_map = false;
        bool saw_load_release = false;
        bool saw_close = false;
        bool saw_next = false;
        int goop_enter_frames = 0;
        int goop_exit_frames = 0;
        int map_frames = 0;
        for (int frame = 0; frame < 18000; ++frame)
        {
            gbtg::InputState input;
            // Advance the retail dialogue pages with distinct A presses.
            input.accept = frame % 30 == 0;
            gbtg::gfx_begin_frame();
            game.tick(1.f / 60.f, input);
            gbtg::gfx_end_frame();
            saw_intro = saw_intro || game.scene_script.name.find("scene0_1book") != std::string::npos;
            if (gbtg::g_546328 == 15 && (gbtg::g_54632c == 1 || gbtg::g_54632c == 3))
            {
                int& frames = gbtg::g_54632c == 1 ? goop_enter_frames : goop_exit_frames;
                ++frames;
                if (frames == 30)
                {
                    gbtg::RgbaImage image;
                    if (!gbtg::gfx_readback(image))
                    {
                        throw gbtg::Error("goop transition readback failed");
                    }
                    gbtg::write_bmp(gbtg::g_54632c == 1 ? "cache/goop_enter_check.bmp" :
                                                                         "cache/goop_exit_check.bmp", image);
                }
            }
            if (gbtg::g_546328 == 8 && gbtg::g_54632c != 0)
            {
                if (!saw_intro)
                {
                    throw gbtg::Error("map started before the intro");
                }
                saw_map = true;
                ++map_frames;
                saw_load_release = saw_load_release || gbtg::g_54632c == 2;
                saw_close = saw_close || gbtg::g_54632c == 3;
                if (map_frames == 210)
                {
                    gbtg::RgbaImage screenshot;
                    if (gbtg::gfx_readback(screenshot))
                    {
                        gbtg::write_bmp("cache/transition_map_check.bmp", screenshot);
                    }
                }
            }
            if (game.scene_script.name.find("scene1_1book") != std::string::npos && game.cutscenes.playing)
            {
                if (!saw_map || !saw_load_release || !saw_close)
                {
                    throw gbtg::Error("next story started before the map load barrier");
                }
                saw_next = true;
                if (gbtg::g_54632c == 0 && game.cutscenes.time > 1.f)
                {
                    break;
                }
            }
        }
        if (!saw_intro || !saw_map || !saw_load_release || !saw_close || !saw_next || gbtg::g_54632c != 0)
        {
            throw gbtg::Error("intro -> map -> next story did not complete");
        }
        if (map_frames < 300)
        {
            throw gbtg::Error("map route was skipped");
        }
        if (goop_enter_frames < 80 || goop_exit_frames < 80)
        {
            throw gbtg::Error("goop entry or exit was skipped");
        }
        if (goop_test)
        {
            gbtg::script_request(game, "aid_script_ghoulies_chapter1_scene0_1book");
            int entering = 0;
            int exiting = 0;
            for (int frame = 0; frame < 600; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, {});
                gbtg::gfx_end_frame();
                if (gbtg::g_546328 == 15 && gbtg::g_54632c == 1)
                {
                    ++entering;
                    if (entering == 30)
                    {
                        gbtg::RgbaImage image;
                        if (!gbtg::gfx_readback(image))
                        {
                            throw gbtg::Error("second goop readback failed");
                        }
                        gbtg::write_bmp("cache/goop_second_check.bmp", image);
                    }
                }
                if (gbtg::g_546328 == 15 && gbtg::g_54632c == 3)
                {
                    ++exiting;
                }
                if (entering > 80 && exiting > 80 && gbtg::g_54632c == 0 && game.cutscenes.playing)
                {
                    break;
                }
            }
            if (entering < 80 || exiting < 80 || gbtg::g_54632c != 0)
            {
                throw gbtg::Error("second goop transition did not complete");
            }
            std::cout << "PASS: repeated goop scene transition completes\n";
        }
        std::cout << "PASS: scene0_1book -> front-garden map -> scene1_1book; map frames=" << map_frames << "\n";
        gbtg::gfx_shutdown();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << "\n";
        gbtg::gfx_shutdown();
        return 1;
    }
}
