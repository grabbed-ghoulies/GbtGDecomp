#include "gbtg/scene.hpp"
#include "gbtg/texture.hpp"
#include "gbtg/wavebank.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

#if defined(GBTG_WITH_SDL)
#include <SDL3/SDL.h>
#endif

namespace {

std::filesystem::path arg(int argc, char** argv, const std::string& key, const std::string& def = {}) {
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] == key) {
            return argv[i + 1];
        }
    }
    return def;
}

bool has_flag(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == key) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto game = arg(argc, argv, "--game-dir", "OriginalFiles");
        const bool headless = has_flag(argc, argv, "--headless");
        std::cout << "loading vertical slice from " << game.string() << "\n";
        auto scene = gbtg::load_vertical_slice(game);
        std::cout << "common entries " << scene.common.entries.size() << " playcam " << scene.playcam.entries.size()
                  << " player " << scene.player.entries.size() << "\n";
        std::cout << "meshes " << scene.meshes.size() << " hud textures " << scene.hud_textures.size() << "\n";
        for (const auto& m : scene.meshes) {
            std::cout << "  " << m.name << " verts=" << m.positions.size() << " idx=" << m.indices.size()
                      << " tex=" << m.textures.size() << "\n";
        }

        gbtg::Camera cam;
        gbtg::init_fly_camera(scene, cam);
        auto fb = gbtg::rasterize_scene(scene, cam, 960, 540);
        gbtg::RgbaImage img{fb.width, fb.height, fb.bgra};
        gbtg::write_bmp("dump/preview.bmp", img);
        std::cout << "wrote dump/preview.bmp\n";

        if (headless) {
            std::cout << "headless: slice loaded, preview written\n";
            return 0;
        }

#if defined(GBTG_WITH_SDL)
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
            std::cerr << "SDL_Init: " << SDL_GetError() << "\n";
            return 1;
        }

        SDL_AudioStream* audio = nullptr;
        const auto wbdir = game / "xwavebank";
        if (std::filesystem::exists(wbdir)) {
            for (auto& p : std::filesystem::directory_iterator(wbdir)) {
                if (!p.is_regular_file() || p.file_size() > 16 * 1024 * 1024) {
                    continue;
                }
                try {
                    auto bank = gbtg::parse_wavebank(p.path(), 1);
                    for (const auto& e : bank.entries) {
                        if (e.pcm.empty() || e.codec != 0) {
                            continue;
                        }
                        SDL_AudioSpec spec{};
                        spec.freq = int(e.samples_per_sec ? e.samples_per_sec : 22050);
                        spec.format = e.bits == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE;
                        spec.channels = int(e.channels ? e.channels : 1);
                        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
                        if (audio) {
                            SDL_PutAudioStreamData(audio, e.pcm.data(), int(e.pcm.size()));
                            SDL_ResumeAudioStreamDevice(audio);
                            std::cout << "audio cue " << e.name << " from " << p.path().filename().string() << "\n";
                        }
                        break;
                    }
                } catch (const std::exception& ex) {
                    std::cerr << "wavebank " << p.path().filename().string() << ": " << ex.what() << "\n";
                }
                if (audio) {
                    break;
                }
            }
        }

        SDL_Window* win = SDL_CreateWindow("GbtGDecomp — chapter 1 slice", 960, 540, SDL_WINDOW_RESIZABLE);
        SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
        SDL_Texture* tex =
            SDL_CreateTexture(ren, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, fb.width, fb.height);
        SDL_SetWindowRelativeMouseMode(win, true);

        SDL_Gamepad* pad = nullptr;
        int n_pads = 0;
        SDL_JoystickID* pads = SDL_GetGamepads(&n_pads);
        if (pads && n_pads > 0) {
            pad = SDL_OpenGamepad(pads[0]);
        }
        SDL_free(pads);

        bool running = true;
        bool dirty = true;
        auto last = std::chrono::steady_clock::now();
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                    cam.yaw -= e.motion.xrel * 0.0035f;
                    cam.pitch = std::clamp(cam.pitch - e.motion.yrel * 0.0035f, -1.35f, 1.35f);
                    dirty = true;
                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    SDL_SetWindowRelativeMouseMode(win, true);
                }
            }
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            auto moved = false;
            const float look = 1.4f * dt;
            if (keys[SDL_SCANCODE_LEFT]) {
                cam.yaw -= look;
                moved = true;
            }
            if (keys[SDL_SCANCODE_RIGHT]) {
                cam.yaw += look;
                moved = true;
            }
            if (keys[SDL_SCANCODE_UP]) {
                cam.pitch = std::min(1.35f, cam.pitch + look);
                moved = true;
            }
            if (keys[SDL_SCANCODE_DOWN]) {
                cam.pitch = std::max(-1.35f, cam.pitch - look);
                moved = true;
            }
            const float cp = std::cos(cam.pitch);
            const gbtg::Vec3 forward{std::sin(cam.yaw) * cp, std::sin(cam.pitch), std::cos(cam.yaw) * cp};
            const gbtg::Vec3 right{-std::cos(cam.yaw), 0.f, std::sin(cam.yaw)};
            float speed = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT] ? 90.f : 32.f;
            auto fly = [&](gbtg::Vec3 dir, float s) {
                cam.eye.x += dir.x * s;
                cam.eye.y += dir.y * s;
                cam.eye.z += dir.z * s;
                moved = true;
            };
            if (keys[SDL_SCANCODE_W]) {
                fly(forward, speed * dt);
            }
            if (keys[SDL_SCANCODE_S]) {
                fly(forward, -speed * dt);
            }
            if (keys[SDL_SCANCODE_D]) {
                fly(right, speed * dt);
            }
            if (keys[SDL_SCANCODE_A]) {
                fly(right, -speed * dt);
            }
            if (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_SPACE]) {
                cam.eye.y += speed * dt;
                moved = true;
            }
            if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LCTRL]) {
                cam.eye.y -= speed * dt;
                moved = true;
            }
            if (pad) {
                const float lx = float(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX)) / 32768.f;
                const float ly = float(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY)) / 32768.f;
                const float rx = float(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX)) / 32768.f;
                const float ry = float(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY)) / 32768.f;
                const float trig = float(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) / 32767.f;
                if (trig > 0.1f) {
                    speed *= 1.f + 2.5f * trig;
                }
                if (lx * lx + ly * ly > 0.04f) {
                    fly(right, lx * speed * dt);
                    fly(forward, -ly * speed * dt);
                }
                if (rx * rx + ry * ry > 0.04f) {
                    cam.yaw += rx * dt * 1.6f;
                    cam.pitch = std::clamp(cam.pitch - ry * dt * 1.6f, -1.35f, 1.35f);
                    moved = true;
                }
                if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
                    cam.eye.y += speed * dt;
                    moved = true;
                }
                if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
                    cam.eye.y -= speed * dt;
                    moved = true;
                }
            }
            if (keys[SDL_SCANCODE_ESCAPE]) {
                running = false;
            }
            if (moved) {
                dirty = true;
            }
            if (dirty) {
                fb = gbtg::rasterize_scene(scene, cam, fb.width, fb.height);
                SDL_UpdateTexture(tex, nullptr, fb.bgra.data(), fb.width * 4);
                dirty = false;
            }
            SDL_RenderClear(ren);
            SDL_RenderTexture(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
        }
        if (pad) {
            SDL_CloseGamepad(pad);
        }
        if (audio) {
            SDL_DestroyAudioStream(audio);
        }
        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
#else
        std::cout << "built without SDL; open dump/preview.bmp (arrow keys need GBTG_WITH_SDL=ON)\n";
#endif
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        std::cerr << "usage: gbtg --game-dir <dir> [--headless]\n";
        return 1;
    }
}
