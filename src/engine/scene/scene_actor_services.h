#pragma once
#include "gbtg/game.h"
#include <memory>
#include <unordered_map>

namespace gbtg
{
class SceneFactoryRuntime;
struct RuntimeEffectPassScratch;
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
    void update_actor(u32 address, RuntimeEffectPassScratch& scratch); // Native vtable +0x10 adapter; caller's incoming effect stack state.
    // The scene tick 0x0011E7F0's recurring actor updates: the bind tree under DAT_00511B10 (0x00123020), then the
    // +0x934 list from DAT_00511B20 (0x00122FE0).
    void update_scene_actors();
    void update_sounds(); // Native context binding for Xbox 0x0012A150.
    void select_frame_display(); // 0x00033B50 sets DAT_005077A8 to DAT_00507018 before the frame's work.
    // The scene pass after the scene tick: controls and actors created since the last frame are activated and
    // given their first update, then scare timers, effects and scene times (run states 1 and 2), actor sounds,
    // control disposal, retired actors and the shadow volumes.
    void run_scene_pass(); // Xbox 0x0011E640
    void update_effects(RuntimeEffectPassScratch& scratch); // Native context binding for Xbox 0x000ED480.
    bool activation_pending() const; // Pending actor +0x900 list is nonempty.
    std::span<const u32> active_actor_addresses() const; // Activated actor +0x910 order.
    u32 player_link() const; // DAT_00511B40.
    const std::unordered_map<u32, Actor*>& actor_addresses() const; // Retail actor address -> native actor.
    size_t player_total() const;
    // Script type 0x6F (0x00127BC0) over the scene's player link and actor graph: 0 holds, 2 continues.
    int wait_script_player() const;
    // The frame's views (0x00033A20). Each active camera slot sets its display, then records its actor bins
    // (0x0011C240's 0x00121730..0x00121D40) and the scene dialogue (0x00058150 and its sprite flush), and the
    // presenter draws what the recording boundary captured. A frame with no active slot draws no scene.
    void present_views();
    struct State;

private:
    std::unique_ptr<State> state_;
};
}
