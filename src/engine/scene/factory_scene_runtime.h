#pragma once
#include "dialogue_resource_owner.h"
#include "executable_resources.h"
#include "scene_marker_handlers.h"
#include "scene_actor_services.h"
#include <map>

namespace gbtg
{
// Host ownership and pointer-width adapter. Retail class implementations are
// installed separately; absent getters always fail rather than fabricate objects.
class SceneFactoryRuntime
{
public:
    static constexpr size_t type_count = 0xAB; // DAT_0030D41C through type 0xAA.
    SceneFactoryRuntime(AidStore& aids, std::shared_ptr<SceneExecutableResources> executable);
    ~SceneFactoryRuntime();
    SceneFactoryRuntime(const SceneFactoryRuntime&) = delete;
    SceneFactoryRuntime& operator=(const SceneFactoryRuntime&) = delete;
    std::shared_ptr<SceneExecutableResources> executable;
    SceneDialogueResourceOwner resources; // Actual bundle/demand resource provider, including markers.
    std::array<ObjectFactoryType, type_count> types;
    ObjectFactoryRegistry factory{types};
    std::array<u32, type_count> retail_getters{};
    std::array<u32, type_count> retail_names{};
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
    size_t address_count() const;
    void load_markers(const char* scene_record);
    std::span<const u8> markers() const;
    // Builds the actor service graph and installs its classes; marker start then creates the scene's actors.
    SceneActorServices& start_actors(Game& game);
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
