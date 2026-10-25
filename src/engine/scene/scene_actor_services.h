#pragma once
#include "gbtg/game.h"
#include <memory>
#include <span>
#include <unordered_map>

namespace gbtg
{
class SceneFactoryRuntime;
struct RuntimeDirectorActor;
struct RuntimeEffectPassScratch;
#if defined(GBTG_INSPECTOR_APP)
struct InspectionView;
struct InspectionFrame;
#endif
// The scene's actor service graph: the native storage behind the retail globals that actor, player,
// mind, strategy and scene-class constructors read, the class owners installed over the scene's
// factory, and the marker handler table (DAT_0042A720). Every service is bound to real scene state;
// a method whose context is not built yet raises when reached rather than returning a value.
class SceneActorServices
{
public:
    SceneActorServices(SceneFactoryRuntime& scene, Game& game);
    ~SceneActorServices();
    SceneActorServices(const SceneActorServices&) = delete;
    SceneActorServices& operator=(const SceneActorServices&) = delete;
    // The services live for the run; each scene begins (its markers and scene data) and ends (0x0011C5A0's
    // teardown of its actors, cutscenes, doors and effects) inside them. start_actors begins each scene.
    void begin_scene();
    void begin_teardown(); // 0x0011C5A0's 0x000584D0, 0x0004ECE0 and 0x0005B1E0
    void retire_actors(); // 0x0011C5A0's 0x00122C70 (message 0x80) and 0x001232D0
    void end_scene();
    bool in_scene() const;
    // 0x0011CBD0 ahead of the markers: door pool, requested background (0x0011C8B0), its activation
    // (0x00123070) and the navigation build (0x00111820) that fills DAT_0050E24C.
    void start_background();
    // 0x00140020 over the scene's loaded marker records (DAT_0051108C).
    void start_markers();
    // 0x00123070: activates the actors created since the last pass (message 0x23, then 0x7B notices).
    void activate_actors();
    void send_scene_start_notice(); // 0x001231F0: message 0x88 to the activated +0x910 list.
    void send_scene_start(); // 0x001231A0: message 0x7D to the activated +0x910 list.
    // The translated part of 0x0011CBD0 in its order: background and navigation, markers (0x00140020),
    // activation (0x00123070), the script's 0x7D event (0x00116810, through script_event), then the
    // 0x88 notice (0x001231F0) and the actor 0x7D sweep (0x001231A0).
    void start_scene(const std::function<void()>& script_event);
    void send_actor_message(u32 address, std::span<const u32> message); // Native vtable +0xC address adapter.
    // Vtable +0xC with the message to each DAT_00511AA4 actor whose +6 tag is the tag.
    void message_tagged_actors(u16 tag, std::span<const u32> message); // Xbox 0x00122CA0, EDI tag, EBX message
    // An actor cloned from the named template at the position and euler with the scale (+0x20); it joins the
    // scene's objects like any creation.
    void create_template_actor(const char* name, const Vec3& position, const Vec3& euler,
        float scale); // Xbox 0x00112C50
    // Script type 0x9D's tail: DAT_00506C84 counts up and 0x0012D7E0 shows the count in slot 1.
    void count_script_challenge();
    // The scene tick's per-frame passes over scene state.
    void age_actor_snapshots(); // Xbox 0x001162F0
    void animate_light_records(); // Xbox 0x000FDDA0
    void tick_post_models(); // Xbox 0x0012AA70
    void tick_frontend_state(); // Xbox 0x00049DE0
    // The scene tick's tail: with a DAT_0050E0E8 device selected (DAT_0050E0EC) and its row nonzero, 0x001083F0 (EAX
    // the device, 0x10): Start newly pressed.
    bool start_pressed();
    void update_actor(u32 address, RuntimeEffectPassScratch& scratch); // Native vtable +0x10 adapter; caller's incoming effect stack state.
    // The scene tick 0x0011E7F0's recurring actor updates: the bind tree under DAT_00511B10 (0x00123020), then the
    // +0x934 list from DAT_00511B20 (0x00122FE0).
    void update_scene_actors();
    void update_sounds(); // Native context binding for Xbox 0x0012A150.
    void reset_sound_queues(); // Xbox 0x00129F70 in the scene teardown.
    void select_frame_display(); // 0x00033B50 sets DAT_005077A8 to DAT_00507018 before the frame's work.
    // The scene pass after the scene tick: controls and actors created since the last frame are activated and
    // given their first update, then scare timers, effects and scene times (run states 1 and 2), actor sounds,
    // control disposal, retired actors and the shadow volumes.
    void run_scene_pass(); // Xbox 0x0011E640
    void update_scoreboard_hud(); // Xbox 0x00132210, after the scene pass
    void tick_live_spawns(); // Xbox 0x0011B670, in the scene tick after the actors
    // The scene tick's fog: the renderer takes the scene fog record (DAT_00509AFC..) each frame, so a cutscene's
    // fog events (0x1A / 0x1B), which change the record after configuring the renderer, apply on the next frame.
    void configure_scene_fog(); // Xbox 0x000FD5C0
    // The scene tick's background cloths and curtain pairs at the paced step.
    void tick_background_cloths(float step); // Xbox 0x00115BB0
    // The scene tick's ghoul intro: 0x000F4A00 while DAT_00506E28 is 1, before the control list, then 0x000F4560
    // after the script timers.
    void start_pending_ghoul_intro();
    void update_ghoul_intro();
    void update_effects(RuntimeEffectPassScratch& scratch); // Native context binding for Xbox 0x000ED480.
    bool activation_pending() const; // Pending actor +0x900 list is nonempty.
    std::span<const u32> active_actor_addresses() const; // Activated actor +0x910 order.
    u32 player_link() const; // DAT_00511B40.
    std::vector<RuntimeDirectorActor> marked_actors(); // DAT_00511B38 (actor +0x934 nodes) in list order.
    const u8* marker_records() const; // DAT_0051108C.
    void restart_marker_player(); // Xbox 0x00127FF0: the next player marker (kind 2) runs its handler again.
    void place_scene_camera(const Vec3& position, const Vec3& euler); // Xbox 0x0013D250, ESI position, stack euler
    u32 find_actor_by_id(u16 key) const; // Xbox 0x00129ED0 over DAT_00511AA4: the actor address, or 0.
    void activate_actor(u32 address); // Xbox 0x00112CC0, ESI actor.
    void queue_aid_spawns(const char* name); // Xbox 0x000F4E50 with the scene's resource registry, EAX name.
    // Script type 0x9F's walk of DAT_00511B38: 0x000F96F0 (AL value, stack slot) on the +0x204 and +0x310 models
    // of every actor whose +6 tag matches.
    void set_tagged_actor_model_nodes(u16 tag, u8 slot, u8 value);
    // Script type 0x67 with a live 0x7D: 0x0012C8A0 over DAT_00511AF4 for the category byte, setting (0x0012C930) or
    // clearing (0x0012C950) the player bit 4 in each matching collision event's +0xC flags.
    void change_script_shape_events(u8 category, bool set);
    // Script type 0x46 with a live 0x7D: the 0x001292D0 player drops its weapon (0x0013A7C0), then 0x0012ECC0.
    void release_script_player_weapon();
    // Script type 0x31 with a live 0x7D: the 0x001292D0 player is given the named weapon (0x0013A720, replacing the
    // held one), which the scene keeps.
    void give_script_player_weapon(const char* name);
    Actor* first_latched_actor(u16 tag) const; // Xbox 0x0013D610 for tag 6: the first DAT_00511AA4 actor with the tag.
    // Script type 0x96's 0x0002B430 / 0x0002B560 on the DAT_00511B40 player.
    void place_script_player(const Vec3& position, const Vec3& euler);
    const std::unordered_map<u32, Actor*>& actor_addresses() const; // Retail actor address -> native actor.
    size_t player_total() const;
    // Host inspection: the factory types of a player's body actions (body +0x314 and +0x744), by its actor, and
    // the second action's bytes.
    std::pair<u16, u16> player_action_types(const Actor& actor, std::span<const u8>* second = nullptr) const;
    // Script type 0x6F (0x00127BC0) over the scene's player link and actor graph: 0 holds, 2 continues.
    int wait_script_player() const;
    // The frame's views (0x00033A20). Each active camera slot sets its display, then records its actor bins
    // (0x0011C240's 0x00121730..0x00121D40) and the scene dialogue (0x00058150 and its sprite flush), and the
    // presenter draws what the recording boundary captured. A frame with no active slot draws no scene.
    void present_views();
    // Boot 0x0011C3F0 -> 0x0011C480: 0x001234B0 over *DAT_00430CF8 holds the game's scene order (DAT_00511B64).
    void hold_boot_scene_order();
    std::span<const u8> held_scene_order() const; // DAT_00511B64, which 0x00117140 picks the next scene from.
#if defined(GBTG_INSPECTOR_APP)
    // Host inspector: native geometry without simulation.
    void inspect_view(const InspectionView& camera, InspectionFrame& frame);
    InspectionView inspection_camera(std::string* source = nullptr) const;
#endif
    struct State;

private:
    std::unique_ptr<State> state_;
};
}
