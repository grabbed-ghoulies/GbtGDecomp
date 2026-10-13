#pragma once

#include "background_construction.h"
#include "background_lifetime.h"
#include "../entities/actor_factory_owner.h"

namespace gbtg
{
// Native state of one background (factory type 4, vtable 0x004E1644) over its actor base.
struct NativeBackgroundInstance
{
    NativeBackgroundInstance(std::span<u8> object, Actor& actor);
    std::span<u8> bytes; // The factory object; its FactoryObject moves after construction.
    ActorMatrixSetup matrices{};
    RuntimeBackgroundLifetime lifetime{};
    RuntimeBackgroundAnimation animation{};
    RuntimeBackgroundConstruction state;
    std::unique_ptr<RuntimeBackgroundConstructionServices> services;
};
struct NativeBackgroundClassBindings
{
    NativeActorFactoryOwner& actors;
    entity_detail::ListHead& backgrounds; // DAT_00511B1C; DAT_00511B20 is the first node.
    // The constructor's scene services for one background; first_animation is the owner's view of the
    // first background (0x00044490 reads it).
    std::function<std::unique_ptr<RuntimeBackgroundConstructionServices>(NativeBackgroundInstance&,
        NativeActorFactoryInstance&, RuntimeBackgroundAnimation*& first_animation)> services;
    i32& disable_framebuffer_clear; // DAT_004DEA50.
    RuntimeModelSoundRelease& model_sound;
    const std::function<void(u8*)>& heap_free;
    // Message slot 0x00043E50 with the scene's contexts for this background.
    std::function<void(NativeBackgroundInstance&, std::span<u32>)> message;
    std::function<void(NativeBackgroundInstance&)> update; // Vtable +0x10, 0x000464D0.
};
class NativeBackgroundClassOwner
{
public:
    explicit NativeBackgroundClassOwner(NativeBackgroundClassBindings bindings);
    ~NativeBackgroundClassOwner();
    NativeBackgroundClassOwner(const NativeBackgroundClassOwner&) = delete;
    NativeBackgroundClassOwner& operator=(const NativeBackgroundClassOwner&) = delete;
    // Background type 4 inherits actor type 2 (getter 0x00042BC0).
    void install(std::span<ObjectFactoryType> types);
    bool construct(FactoryObject& object, std::span<const u8> parameters); // Vtable +4, 0x00042DA0.
    void release(FactoryObject& object); // Vtable +8, 0x000430C0.
    void message(FactoryObject& object, std::span<u32> message); // Vtable +0xC, 0x00043E50.
    void update(FactoryObject& object); // Native adapter for vtable +0x10, 0x000464D0.
    NativeBackgroundInstance& instance(const FactoryObject& object);
    NativeBackgroundInstance* find(const u8* bytes);
    NativeBackgroundClassBindings bindings;
    RuntimeBackgroundAnimation* first_animation = nullptr;
    NativeBackgroundInstance* first_background(); // Owner of the first +0x934 list node (DAT_00511B20).

private:
    std::unordered_map<const u8*, std::unique_ptr<NativeBackgroundInstance>> instances_;
};
}
