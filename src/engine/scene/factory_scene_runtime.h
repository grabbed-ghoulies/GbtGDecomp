#pragma once
#include "dialogue_resource_owner.h"
#include "executable_resources.h"
#include "scene_marker_handlers.h"
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
    FactoryObject* keep_object(FactoryObject object);
    void release_objects();
    u32 bind_address(std::span<u8> bytes);
    void unbind_address(const void* bytes);
    u32 address_of(const void* pointer) const;
    std::span<u8> resolve_address(u32 address, size_t size) const;
    size_t address_count() const;
    void load_markers(const char* scene_record);
    std::span<const u8> markers() const;

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
