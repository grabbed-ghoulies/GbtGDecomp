#pragma once
#include "gbtg/game.h"
#include <memory>
#include <unordered_map>

namespace gbtg
{
class SceneFactoryRuntime;
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
    // 0x0011CBD0 ahead of the markers: door pool, requested background (0x0011C8B0), its activation
    // (0x00123070) and the navigation build (0x00111820) that fills DAT_0050E24C.
    void start_background();
    // 0x00140020 over the scene's loaded marker records (DAT_0051108C).
    void start_markers();
    // 0x00123070: activates the actors created since the last pass (message 0x23, then 0x7B notices).
    void activate_actors();
    void send_scene_start_notice(); // 0x001231F0: message 0x88 to the activated +0x910 list.
    void send_scene_start(); // 0x001231A0: message 0x7D to the activated +0x910 list.
    void send_actor_message(u32 address, std::span<const u32> message); // Native vtable +0xC address adapter.
    bool activation_pending() const; // Pending actor +0x900 list is nonempty.
    std::span<const u32> active_actor_addresses() const; // Activated actor +0x910 order.
    u32 player_link() const; // DAT_00511B40.
    const std::unordered_map<u32, Actor*>& actor_addresses() const; // Retail actor address -> native actor.
    size_t player_total() const;
    struct State;

private:
    std::unique_ptr<State> state_;
};
}
