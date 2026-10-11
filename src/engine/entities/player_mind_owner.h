#pragma once
#include "player_mind_messages.h"
#include "player_mind_request_update.h"
#include "player_mind_setup.h"
#include "player_parent_lifecycle.h"
#include "cinema_mind_slots.h"
#include "gbtg/object_factory.h"
#include <memory>

namespace gbtg
{
// Per-mind contexts for parent mind type 7 (vtable 0x004DD6F4) and player mind type 0x34
// (vtable 0x00505498). The player owner builds them from the player's native objects before the
// constructors run, so every reference outlives the factory object.
struct NativePlayerMindSlots
{
    // Lifecycle: 0x00029F20/0x00029FF0 (type 7) and 0x0007EF10/0x0007F0D0 (type 0x34).
    std::span<u8> mind;
    u16 actor_class;
    const RuntimeMindClassSettings& settings;
    u32& random; // DAT_0054DD7C.
    std::span<ExecutableResourcePool> resources;
    std::span<const u8>& script_table; // Mind +0x60.
    RuntimePlayerMindSetup& setup;
    RuntimePlayerMindRequests& requests; // Mind +0x58.
    LiveScript*& events; // Mind +0x28.
    RuntimeMindStorage*& storage; // Mind +0x1C.
    std::span<const u8> actor_bytes; // Mind +8.
    const u32& runtime_flags; // DAT_0050EA9C.
    RuntimeInputActivationContext& input_activation;
    // Virtual slots +0xC through +0x18; the player owner attaches them once its method contexts exist.
    RuntimePlayerMindMessages* messages;
    RuntimePlayerMindTick* tick;
    RuntimePlayerMindRequestUpdate* request_update;
    Actor& actor; // Mind +8.
    std::function<const Vec3*()> target_position; // Mind +0x10 object +0x20, or null.
    std::function<std::span<u8>(u32)> input_at; // Retail input device address -> device bytes.
    std::function<std::span<const u8>(u32)> object_at; // Message +4 object for 0x0002A2E0.
    std::unique_ptr<NativeCinemaMindSlots> cinema; // Owned native slot view, required for shared type 0x35.
};
struct NativePlayerMindBindings
{
    std::function<std::unique_ptr<NativePlayerMindSlots>(FactoryObject&, std::span<const u8>)> create_slots;
    std::function<void(const FactoryObject&)> released; // Optional; after 0x00029FF0 releases the view.
};
class NativePlayerMindOwner
{
public:
    explicit NativePlayerMindOwner(NativePlayerMindBindings bindings);
    ~NativePlayerMindOwner();
    NativePlayerMindOwner(const NativePlayerMindOwner&) = delete;
    NativePlayerMindOwner& operator=(const NativePlayerMindOwner&) = delete;
    // Installs types 7 and 0x34 (getters 0x00029D30 and 0x0007E100).
    void install(std::span<ObjectFactoryType> types);
    void install_cinema(std::span<ObjectFactoryType> types); // Native getter view of Xbox 0x0007C520.
    NativePlayerMindSlots& construct_view(FactoryObject& object, std::span<const u8> parameters);
    NativePlayerMindSlots& view(const FactoryObject& object);
    void release_view(const FactoryObject& object);
    // Player mind vtable 0x00505498 virtual slots.
    void message(FactoryObject& mind, const void* message); // +0xC, 0x0007F1E0.
    void tick(FactoryObject& mind, u32 input); // +0x10, 0x0007FB10.
    i32 update_request(FactoryObject& mind, RuntimePlayerMindRequest& request); // +0x14, 0x0007D360.
    u32 initialize_request(FactoryObject& mind, RuntimePlayerMindRequest& request); // +0x18, 0x0007EBC0.
    void release_request(FactoryObject& mind, RuntimePlayerMindRequest& request); // +0x1C, 0x001628B0.
    NativePlayerMindBindings bindings;

private:
    std::unordered_map<const u8*, std::unique_ptr<NativePlayerMindSlots>> instances_;
};
}
