#pragma once
#include "actor_factory_owner.h"
#include "trigger_actor.h"
#include "actor_word_messages.h"

namespace gbtg
{
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
    void release_area(FactoryObject& object);
    void release_trigger(FactoryObject& object);
    void message(FactoryObject& object, std::span<u32> message);
    void update_trigger(FactoryObject& object);

private:
    struct TriggerInstance;
    NativeSceneActorClassBindings bindings_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeAreaActorContext>> areas_;
    std::unordered_map<const u8*, std::unique_ptr<TriggerInstance>> triggers_;
};
}
