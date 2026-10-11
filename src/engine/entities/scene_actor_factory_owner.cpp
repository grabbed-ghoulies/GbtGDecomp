#include "scene_actor_factory_owner.h"

namespace gbtg
{
namespace
{
NativeSceneActorClassOwner* active_owner = nullptr;
bool construct_area_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_area(object, parameters);
}
bool construct_trigger_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_trigger(object, parameters);
}
void release_area_view(FactoryObject& object)
{
    active_owner->release_area(object);
}
void release_trigger_view(FactoryObject& object)
{
    active_owner->release_trigger(object);
}
const ObjectFactoryVtable* area_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504CAC, runtime_area_storage_sizes,
        construct_area_view, release_area_view};
    return &table;
}
const ObjectFactoryVtable* trigger_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504A90, runtime_trigger_storage_sizes,
        construct_trigger_view, release_trigger_view};
    return &table;
}
}

struct NativeSceneActorClassOwner::TriggerInstance : GhoulyController
{
    GhoulySpawnRecord* load = nullptr;
    RuntimeTriggerActorContext context;
    RuntimeTriggerMessages messages;
    TriggerInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor,
            *this, bindings.random, bindings.occupancy_initial, bindings.message, load,
            bindings.prompt_lock, [this](std::span<u32> message)
            {
                dispatch_runtime_trigger_message(messages, message);
            }},
          messages{context, bindings.base_of(object), bindings.objects, bindings.effects,
            bindings.bytes_at, bindings.address_of,
            [&bindings](FactoryObject& actor) -> Actor&
            {
                return bindings.actors.view(actor).bat.actor;
            }, bindings.keep, [&bindings](ActorBaseContext& base, std::span<u32> message)
            {
                dispatch_base_actor_word_message(base, message, bindings.word_messages);
            }}
    {
        message = [](GhoulyController& controller, void* data)
        {
            auto& instance = static_cast<TriggerInstance&>(controller);
            // 0x0013E8D0 sends the full 0x108-byte A8 request.
            dispatch_runtime_trigger_message(instance.messages, {static_cast<u32*>(data), 0x42});
        };
    }
};

NativeSceneActorClassOwner::NativeSceneActorClassOwner(NativeSceneActorClassBindings bindings)
    : bindings_(std::move(bindings))
{
}

NativeSceneActorClassOwner::~NativeSceneActorClassOwner()
{
    if (active_owner == this)
    {
        active_owner = nullptr;
    }
}

void NativeSceneActorClassOwner::install(std::span<ObjectFactoryType> types)
{
    if (active_owner != nullptr && active_owner != this)
    {
        throw Error("native scene actor classes already have an owner");
    }
    types[0x1F] = {2, area_lifecycle};
    types[0x23] = {2, trigger_lifecycle};
    active_owner = this;
}

bool NativeSceneActorClassOwner::construct_area(FactoryObject& object, std::span<const u8> parameters)
{
    auto& actor = bindings_.actors.view(object).bat.actor;
    auto context = std::make_unique<RuntimeAreaActorContext>(RuntimeAreaActorContext{
        {object.bytes.get(), object.size}, actor, bindings_.base_of(object), bindings_.prompt_lock,
        bindings_.dialogue, bindings_.random, bindings_.occupancy_initial,
        [this](ActorBaseContext& base, std::span<u32> message)
        {
            dispatch_base_actor_word_message(base, message, bindings_.word_messages);
        }});
    areas_.emplace(object.bytes.get(), std::move(context));
    return initialize_runtime_area_actor(object, parameters);
}

bool NativeSceneActorClassOwner::construct_trigger(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<TriggerInstance>(bindings_, object);
    auto* context = instance.get();
    triggers_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_trigger_actor(context->context, parameters);
}

void NativeSceneActorClassOwner::release_area(FactoryObject& object)
{
    areas_.erase(object.bytes.get()); // Retail derived destructor 0x001628B0 is empty.
}

void NativeSceneActorClassOwner::release_trigger(FactoryObject& object)
{
    destroy_runtime_trigger_actor(triggers_.at(object.bytes.get())->context);
    triggers_.erase(object.bytes.get());
}

void NativeSceneActorClassOwner::message(FactoryObject& object, std::span<u32> message)
{
    if (object.type == 0x1F)
    {
        dispatch_runtime_area_message(*areas_.at(object.bytes.get()), message);
    }
    else if (object.type == 0x23)
    {
        dispatch_runtime_trigger_message(triggers_.at(object.bytes.get())->messages, message);
    }
    else
    {
        throw Error("scene actor class owner has no message view for this class");
    }
}

void NativeSceneActorClassOwner::update_trigger(FactoryObject& object)
{
    update_runtime_trigger_actor(triggers_.at(object.bytes.get())->context);
}
}
