#include "gbtg/engine_internal.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "runtime/rare_replay.h"
#endif
#include "scripts/script_actions.h"
#include "scene/ambient.h"
#include "scene/dialogue_scene_runtime.h"
#include "scene/executable_resources.h"
#include "scene/factory_scene_runtime.h"
#include "scene/scene_actor_services.h"
#include "scene/scene.h"
#include "scripts/script_chapter.h"
#include "frontend/app_error_screen.h"
#include "frontend/frontend_sounds.h"
#include "frontend/frontend_state.h"
#include "transitions/scene_transitions.h"
#include "audio/native_audio.h"
#include "audio/pcm_backend.h"
#include "video/native_movies.h"
#include "video/movie_session.h"
#include "saves/save_games.h"
#include "frontend/input_stack.h"
#include "camera/camera.h"
#include "camera/actor_camera_effects.h"
#include "gbtg/gfx.h"
#include "gbtg/log.h"
#include "camera/display_view.h"
#include "gbtg/movie.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "plugins/plugins.h"
#include "camera/wide_aspect.h"
#endif

#include <algorithm>
#include <string>

namespace gbtg
{
void Game::init(const std::filesystem::path& game_dir, std::string_view scene_select)
{
    // Xbox WinMain 0x00032390: gfx_init_32470, fs_init_33980, game_init_32630.
    clock_init(clock);
    aids.open(game_dir);
    executable_resources = std::make_shared<SceneExecutableResources>(game_dir);
    // Boot 0x00033030 -> 0x00035A90 and the provider and hook registrations.
    native_resources = std::make_shared<SceneDialogueResourceOwner>(aids, executable_resources->pools);
    native_movies = boot_native_movies(*this); // fs_init 0x00033980 -> 0x000E9CE0, before 0x00033030.
    native_audio = boot_native_audio(*this); // Boot 0x00033030 -> 0x0003A280.
    load_audio_general_settings(native_resources->registry, native_resources->error); // Boot 0x00033030
    boot_actor_camera_shakes(runtime_pad_rumble_commands()); // Boot 0x00033030 -> 0x000FADE0
    // Boot 0x00033030 after 0x000F8740: the aspect row is 16:9 on a widescreen display.
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // The widescreen row, or the wide_aspect plugin's (camera/wide_aspect.h).
    entity_detail::g_aspect_preset = entity_detail::g_video_widescreen != 0 ? entity_detail::widescreen_aspect_row() : 0;
#else
    entity_detail::g_aspect_preset = entity_detail::g_video_widescreen != 0 ? 1 : 0; // DAT_005077F8 = DAT_00509703 != 0
#endif
    native_input = boot_native_input_stack(); // Boot 0x00033030 -> 0x00033248, 0x00033274; 0x0011C3F0's push.
    reset_runtime_frontend(g_frontend, g_pause_ditty); // Boot 0x00033030 -> 0x000E5A80.
    // The object factory, actor lists and scene services, boot-owned like retail's globals.
    factory_scene = std::make_shared<SceneFactoryRuntime>(executable_resources, *native_resources);
    factory_scene->controls->context.chapter = [this]() -> i32
    {
        return script_chapter_index(scene_script.name); // 0x0004B4D0
    };
    factory_scene->start_services(*this);
    // Boot 0x00033030 -> 0x0011EA50 -> 0x0011C3F0. Its input push and handler inits run in the boot above and the
    // scene services' start; its zeroed globals are already zero. 0x001233C0, 0x0011B630 and 0x00123B30 clear lists
    // that are empty at boot, 0x00057D40 fills a host table nothing native reads (0x00119160's depth ramp is
    // made with the scene services' D3D resources), and the sums it
    // stores at DAT_0042A424 / DAT_0042A42C have no reader in the dumps.
    scene_initialize_handle_slots(); // 0x0011C3F0 -> 0x00129F30: the named sound rows' ids become 0xFFFF.
    lighting_initialize_volume(); // 0x0011C3F0 -> 0x000FD620
    ambient_initialize_beds(); // 0x0011C3F0 -> 0x0011C480 -> 0x00118470
    factory_scene->actor_services->hold_boot_scene_order(); // 0x0011C3F0 -> 0x0011C480 -> 0x001234B0
    save_initialize_name_scan(); // 0x0011C3F0 -> 0x0011C480 -> 0x0010D3B0: the saved games on drive U.
    g_scene_active_count = g_scene_active_count + 1; // 0x0011C3F0's last store (DAT_0050E7A0); the first scene's state 1 takes it back.
    movie_set_root(game_dir);
    movie_boot_probe();
    log::verbose("fs_init: %s common.bnl entries %zu", aids.game_dir.string().c_str(), aids.common.entries.size());
    log::verbose("clock %d Hz dt=%g", clock_fps(clock), clock.dt);

    script_set_default_name(*this);
    if (const BundleEntry* list = find_entry(aids.common, kSceneOrder))
    {
        scene_order = parse_aidlist(list->cpu);
        log::verbose("%s (%zu)", kSceneOrder, scene_order.size());
        for (size_t i = 0; i < scene_order.size() && i < 12; ++i)
        {
            log::verbose("  [%zu] %s", i, scene_order[i].c_str());
        }
        if (scene_order.size() > 12)
        {
            log::verbose("  ...");
        }
        if (!scene_order.empty())
        {
            size_t index = 0;
            if (!scene_select.empty())
            {
                bool digits = true;
                for (char c : scene_select)
                {
                    if (c < '0' || c > '9')
                    {
                        digits = false;
                        break;
                    }
                }
                if (digits)
                {
                    index = std::min(scene_order.size() - 1, size_t(std::stoul(std::string(scene_select))));
                }
                else
                {
                    for (size_t i = 0; i < scene_order.size(); ++i)
                    {
                        if (scene_order[i].find(scene_select) != std::string::npos)
                        {
                            index = i;
                            break;
                        }
                    }
                }
                log::verbose("scene select [%zu] %s", index, scene_order[index].c_str());
            }
            scene_index = index;
            // 0x0011EB30 queues the aid. 0x0011E4C0 loads it once the stream enters state 4.
#if !defined(GBTG_RETAIL_GAME_ONLY)
            // A full aid_script_ name outside the scene order, such as a plugin's scene (docs/PLUGINS.md).
            if (scene_select.starts_with("aid_script_")
                && std::find(scene_order.begin(), scene_order.end(), scene_select) == scene_order.end())
            {
                log::verbose("scene select %.*s", int(scene_select.size()), scene_select.data());
                script_request(*this, scene_select);
            }
            else
            {
                script_request(*this, scene_order[index]);
            }
#else
            script_request(*this, scene_order[index]);
#endif
        }
    }

    game_init_pump();
    log::verbose("game_init: io idle");
    // 0x0011E4C0. The first pass accepts the queued aid into state 3.
    // The next pass enters state 4 and loads that script.
    // No simulation frame has elapsed during initialization. In particular,
    // do not consume the transition's 1/60-second timer before the first tick.
    clock.dt = 0.f;
    script_pump(*this);
}

void frame_tick(Game& g, float dt, const InputState& in)
{
    // Xbox 0x00033B50.
    clock_pad(g.clock, dt);    // 0x000FC030
#if !defined(GBTG_RETAIL_GAME_ONLY)
    advance_rare_replay(g.clock.dt_raw); // Rare Replay: which ticks are retail's 1/30 s ticks.
#endif
    clock_frame_time(g.clock); // 0x00033C80
    g.time = g.clock.time;
    g.frame = g.clock.frame;
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).select_frame_display(); // DAT_005077A8 = &DAT_00507018.
    }
    if (in.quit)
    {
        g.running = false;
        return;
    }
    pad_tick(g, in); // 0x000FA3C0 / 0x000FAD80
    // 0x00033C47. 0x0011E4C0. A queued aid_script loads when the stream enters state 4; run state 1
    // (or 2) ticks the scene through 0x0011E7F0: the dialogue (0x00057E60), the fired script and
    // its condition timers (0x00117100).
    script_pump(g);
    // 0x00033B50 runs 0x0011E4C0 and 0x0011E640 while DAT_0050EA9C has bits 0x80 and 0x40000 clear.
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1 && (g_flags_ea9c & 0x40080u) == 0)
    {
        g.factory_scene->start_actors(g).run_scene_pass(); // 0x0011E640 after 0x0011E4C0.
        g.factory_scene->start_actors(g).update_scoreboard_hud(); // 0x00132210
        g.factory_scene->start_actors(g).tick_screen_effects(); // 0x00060D50
        tick_actor_camera_effects(g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f); // 0x00048F90
    }
    if ((g_flags_ea9c & 0x80u) == 0)
    {
        tick_app_error_screen(); // 0x00032AB0
    }
    tick_native_audio(g.clock.dt, g_stream_slot_gate); // 0x0015B780, 0x0003A040, 0x000396F0, 0x00038640, 0x00038C20
    // 0x00033B50: the file rows (0x000E9E60), the movie list (0x00107420 over DAT_0050E0B8) and the finished
    // movie event slots (0x00106C70).
    auto& movies = *g.native_movies;
    pump_runtime_movie_files(movies.session->files());
    // 0x00033B50: the first queued file copy steps while no file transfer or utility format is pending.
    auto& copies = movies.session->copies();
    if (copies.files.pending == 0 && copies.state.utility_pending == 0 && copies.state.copy_head.next != nullptr)
    {
        pump_runtime_file_copy(*copies.state.copy_head.next, copies); // 0x000E9280
    }
    // 0x000FAF70: the camera shakes into the pads' rumble commands, measured from the frame's view.
    tick_actor_camera_shakes(g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f,
        native_audio_runtime().streams.voices.listener, g.native_input->port_masks[1], // DAT_005077A8, DAT_0050E0F8
        runtime_pad_rumble_commands(), g.native_resources->registry);
    tick_native_movie_list(movies);
    release_finished_movie_events(movies.session->slots, movies.session->startup());
    // 0x00033BF2: the save timer and the queued saves (0x0010F440).
    SaveGameContext saves{movies.session->files(), g_runtime_profile, {}};
    tick_save_games(g_sim_dt, saves);
    // The Bink decoder's clock (BinkWait's timer).
    movie_advance_time(g.clock.dt);
    // 0x00033A90 collects the deferred model releases (0x000F9780) and flushes the deferred resource unloads
    // (0x00036180) before it presents.
    g.factory_scene->actor_services->collect_model_releases();
    flush_animation_resource_unloads(g.native_resources->registry);
    // Present 0x00033A90: the movie list, the movie frames into their hooked textures (0x00106CD0), each active
    // camera slot's view (0x00033A20), then the movie list again. The views run every frame, with or without a
    // started scene, as the transition camera can hold slot 0 between scenes.
    tick_native_movie_list(movies);
    copy_movie_event_frames(movies.session->slots, g.factory_scene->resources.special_textures,
        movies.session->startup());
    g.factory_scene->actor_services->present_views();
    tick_native_movie_list(movies);
}

void Game::tick(float dt, const InputState& in)
{
    frame_tick(*this, dt, in);
#if !defined(GBTG_RETAIL_GAME_ONLY)
    plugins_frame(*this);
#endif
}

} // namespace gbtg
