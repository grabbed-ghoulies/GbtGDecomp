#pragma once
#include "../runtime/random_state.h"
#include "dialogue_resource_owner.h"
#include "executable_resources.h"
#include "scene_marker_handlers.h"
#include "scene_actor_services.h"
#include "scene_controls.h"
#include <map>

namespace gbtg
{
// Host ownership and pointer-width adapter. Retail class implementations are
// installed separately; absent getters always fail rather than fabricate objects.
class SceneFactoryRuntime
{
public:
    static constexpr size_t type_count = 0xAB; // DAT_0030D41C through type 0xAA.
    SceneFactoryRuntime(std::shared_ptr<SceneExecutableResources> executable, SceneDialogueResourceOwner& owner);
    ~SceneFactoryRuntime();
    SceneFactoryRuntime(const SceneFactoryRuntime&) = delete;
    SceneFactoryRuntime& operator=(const SceneFactoryRuntime&) = delete;
    std::shared_ptr<SceneExecutableResources> executable;
    SceneDialogueResourceOwner& resources; // The boot-owned resource registry and memory (Game::native_resources).
    std::array<ObjectFactoryType, type_count> types;
    ObjectFactoryRegistry factory{types};
    std::array<u32, type_count> retail_getters{};
    std::array<u32, type_count> retail_names{};
    u32& random = g_shared_random; // DAT_0054DD7C.
    // Scene controls (types 3 and 0x91): the script can create them while the scene loads.
    std::unique_ptr<NativeSceneControlOwner> controls;
    std::vector<FactoryObject> objects;
    // Type 1 maps each instance's address on construction and releases it on destruction. Owners that
    // install their own base registration must call this again afterwards.
    void install_base_class();
    std::function<void(const u8*)> released; // After an object's type 1 destructor, before its storage is freed.
    FactoryObject* keep_object(FactoryObject object);
    void release_objects();
    u32 bind_address(std::span<u8> bytes);
    void unbind_address(const void* bytes);
    u32 address_of(const void* pointer) const;
    u32 find_address(const void* pointer) const; // 0 when the pointer is not in a scene object.
    std::span<u8> resolve_address(u32 address, size_t size) const;
    std::span<u8> resolve_tail(u32 address) const; // From the address to the end of its instance.
    size_t address_count() const;
    void load_markers(const char* scene_record);
    std::span<const u8> markers() const;
    // Builds the actor service graph and installs its classes; marker start then creates the scene's actors.
    SceneActorServices& start_actors(Game& game);
    // Boot 0x00033030: the services exist before the first scene (its request already starts a transition).
    void start_services(Game& game);
    // The runtime lives for the run like retail's factory and actor lists. A scene ends with 0x0011C5A0's
    // teardown (its objects, then the services' part); the next scene begins when its actors start.
    void end_scene();
    // Native scene start through the actor services: 0 not run, 1 complete, -1 stopped where a service
    // is not bound yet (the scene then keeps the legacy script path).
    i32 actor_start = 0;
    std::unique_ptr<SceneActorServices> actor_services; // Declared last: objects are released before it.

private:
    struct MarkerDeleter
    {
        void operator()(u8* pointer) const;
    };
    std::unique_ptr<u8, MarkerDeleter> markers_;
    std::map<u32, std::span<u8>> addresses_;
    u32 next_address_ = 0x10000000; // Native token arena, not an emulated heap implementation.
};
}
