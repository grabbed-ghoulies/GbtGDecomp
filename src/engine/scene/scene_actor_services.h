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
    // 0x00140020 over the scene's loaded marker records (DAT_0051108C).
    void start_markers();
    // 0x00123070: activates the actors created since the last pass (message 0x23, then 0x7B notices).
    void activate_actors();
    u32 player_link() const; // DAT_00511B40.
    const std::unordered_map<u32, Actor*>& actor_addresses() const; // Retail actor address -> native actor.
    size_t player_total() const;
    struct State;

private:
    std::unique_ptr<State> state_;
};
}
