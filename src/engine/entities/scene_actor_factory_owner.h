#pragma once
#include "actor_factory_owner.h"
#include "trigger_actor.h"
#include "actor_word_messages.h"
#include "fx_anim_emitter.h"

namespace gbtg
{
// Scene services for the FX anim emitter (type 0xD) beyond the shared actor bindings.
struct NativeSceneEmitterBindings
{
    ActorFrameContext& frame; // 0x000420D0.
    std::span<u8*> timed_attachments; // DAT_00506930.
    const RuntimeActorClock& clock;
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    std::span<const u8> challenge_bits; // *(DAT_00506C54+8).
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    const std::vector<RuntimeFxEmitterPlayer*>& players; // DAT_00511B40 order.
    const RuntimePlayerStrategyResume& resume;
    u8& story_byte; // **(DAT_00506C6C+8).
    RuntimeIntroNotices& intro;
    entity_detail::ListHead& emitters; // DAT_00511B2C.
    std::function<void(u32, RuntimeFxEmitterPlayer&, u32, float, u32)> award; // 0x00123E70.
};
struct NativeSceneActorClassBindings
{
    NativeActorFactoryOwner& actors;
    RuntimeMarkerObjects& objects;
    ActorEffectStreamContext& effects;
    const i32& prompt_lock;
    const RuntimeAreaDialoguePool*& dialogue;
    u32& random;
    float occupancy_initial;
    std::function<ActorBaseContext&(FactoryObject&)> base_of;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<u32(const void*)> address_of;
    std::function<void(u32, std::span<u32>)> message;
    std::function<FactoryObject*(FactoryObject)> keep;
    const RuntimeActorWordBindings& word_messages;
    NativeSceneEmitterBindings& emitter;
};
// Native contexts for the retail area/trigger classes, over the existing
// actor lifecycle owner. All referenced scene services must outlive this owner.
class NativeSceneActorClassOwner
{
public:
    explicit NativeSceneActorClassOwner(NativeSceneActorClassBindings bindings);
    ~NativeSceneActorClassOwner();
    NativeSceneActorClassOwner(const NativeSceneActorClassOwner&) = delete;
    NativeSceneActorClassOwner& operator=(const NativeSceneActorClassOwner&) = delete;
    void install(std::span<ObjectFactoryType> types);
    bool construct_area(FactoryObject& object, std::span<const u8> parameters);
    bool construct_trigger(FactoryObject& object, std::span<const u8> parameters);
    bool construct_emitter(FactoryObject& object, std::span<const u8> parameters);
    bool defer_release = false; // Keep message views through the actor-base destructor.
    void release_view(const u8* bytes); // Native ownership cleanup after all retail destructors.
    void release_area(FactoryObject& object);
    void release_trigger(FactoryObject& object);
    void release_emitter(FactoryObject& object);
    void message(FactoryObject& object, std::span<u32> message);
    void update_trigger(FactoryObject& object);
    void update_emitter(FactoryObject& object); // Vtable +0x10, Xbox 0x0010A2E0.
    u32 draw_emitter(FactoryObject& object, i32 kind, RuntimeFxEmitterDraw& draw); // Vtable +0x14, Xbox 0x00109EE0.
    bool emitter_visible(FactoryObject& object, u32 pass, u32 slot); // Vtable +0x18, Xbox 0x00109E30.

private:
    struct TriggerInstance;
    struct EmitterInstance;
    NativeSceneActorClassBindings bindings_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeAreaActorContext>> areas_;
    std::unordered_map<const u8*, std::unique_ptr<TriggerInstance>> triggers_;
    std::unordered_map<const u8*, std::unique_ptr<EmitterInstance>> emitters_;
};
}
