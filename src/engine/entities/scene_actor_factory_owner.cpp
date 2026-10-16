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
bool construct_emitter_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_emitter(object, parameters);
}
void release_emitter_view(FactoryObject& object)
{
    active_owner->release_emitter(object);
}
bool construct_motion_blur_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_motion_blur(object, parameters);
}
void release_motion_blur_view(FactoryObject& object)
{
    active_owner->release_motion_blur(object);
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
const ObjectFactoryVtable* emitter_lifecycle()
{
    static const ObjectFactoryVtable table{0x0050E108, fx_anim_emitter_object_sizes,
        construct_emitter_view, release_emitter_view};
    return &table;
}
const ObjectFactoryVtable* motion_blur_lifecycle()
{
    static const ObjectFactoryVtable table{0x005049A0, avatar_motion_blur_object_sizes,
        construct_motion_blur_view, release_motion_blur_view};
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
            bindings.bytes_at,
            [this, &bindings](const void* pointer) -> u32
            {
                // 0x000F39A0's record is a heap block the trigger keeps at +0xB44.
                if (pointer != nullptr && pointer == load)
                {
                    return bindings.heap_address(load, sizeof(GhoulySpawnRecord));
                }
                return bindings.address_of(pointer);
            },
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

// Native views of one emitter. The actor base does not build model matrix records natively, so
// the matrix view starts empty with the base's cached reach words.
struct NativeSceneActorClassOwner::EmitterInstance
{
    std::span<const u8> sprite; // +0x994.
    entity_detail::ListHead link{}; // +0x934.
    ActorMatrixSetup matrices{};
    RuntimeBodyAttachmentLists lists;
    RuntimeFxAnimEmitter context;
    EmitterInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object, NativeActorFactoryInstance& view)
        : lists{view.pending.next, view.startup.attachments.sentinel.next, bindings.emitter.timed_attachments},
          context{{object.bytes.get(), object.size}, view.bat.actor, bindings.address_of(object.bytes.get()),
            bindings.base_of(object),
            [&bindings](ActorBaseContext& base, std::span<u32> message)
            {
                dispatch_base_actor_word_message(base, message, bindings.word_messages);
            },
            bindings.word_messages.memory_at, view.animation, view.frame, bindings.emitter.frame, matrices,
            view.startup.attachments, view.pending, view.anchors, view.startup.resource, lists,
            bindings.actors.bindings.sounds, bindings.actors.bindings.startup.registry,
            bindings.actors.bindings.startup.error, bindings.random, sprite, link, bindings.emitter.emitters,
            bindings.emitter.clock, bindings.emitter.view, bindings.emitter.challenge_bits, bindings.emitter.scene_flags,
            bindings.emitter.runtime_flags, bindings.emitter.players, bindings.emitter.resume,
            bindings.emitter.story_byte, bindings.emitter.intro, bindings.actors.bindings.factory, bindings.keep,
            bindings.actors.bindings.empty_name, bindings.emitter.award}
    {
        matrices.progress = view.startup.value_29c;
        matrices.flags = view.startup.word_2a0;
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
    types[0xD] = {2, emitter_lifecycle}; // Getter 0x00109DD0.
    types[0xF] = {2, motion_blur_lifecycle}; // Getter 0x00066900.
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

bool NativeSceneActorClassOwner::construct_emitter(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<EmitterInstance>(bindings_, object, bindings_.actors.view(object));
    auto* emitter = instance.get();
    emitters_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_fx_anim_emitter(parameters, emitter->context);
}

bool NativeSceneActorClassOwner::construct_motion_blur(FactoryObject& object, std::span<const u8> parameters)
{
    auto views = [this](FactoryObject& actor)
    {
        auto& view = bindings_.actors.view(actor);
        return RuntimeAvatarActorViews{{actor.bytes.get(), actor.size}, view.bat.actor, view.startup};
    };
    auto context = std::make_unique<RuntimeAvatarMotionBlur>(RuntimeAvatarMotionBlur{views(object),
        [this, views](u32 address)
        {
            auto* actor = bindings_.object_at(address);
            if (actor == nullptr)
            {
                throw Error("avatar motion blur source actor is outside the scene");
            }
            return views(*actor);
        },
        bindings_.actors.bindings.startup, bindings_.effects, bindings_.snapshots(), bindings_.actors.bindings.empty_name,
        bindings_.address_of(object.bytes.get()), bindings_.actors.bindings.resource_address, {},
        bindings_.emitter.clock, bindings_.actors.bindings.sounds, bindings_.actors.view(object).frame,
        bindings_.emitter.frame, bindings_.lights_enabled, bindings_.script_lights});
    auto* blur = context.get();
    motion_blurs_.emplace(object.bytes.get(), std::move(context));
    return initialize_runtime_avatar_motion_blur(parameters, *blur);
}

void NativeSceneActorClassOwner::release_motion_blur(FactoryObject& object)
{
    destroy_runtime_avatar_motion_blur(*motion_blurs_.at(object.bytes.get()));
    if (!defer_release)
    {
        motion_blurs_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::release_area(FactoryObject& object)
{
    // Retail derived destructor 0x001628B0 is empty.
    if (!defer_release)
    {
        areas_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::release_trigger(FactoryObject& object)
{
    destroy_runtime_trigger_actor(triggers_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        triggers_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::release_emitter(FactoryObject& object)
{
    destroy_runtime_fx_anim_emitter(emitters_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        emitters_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::release_view(const u8* bytes)
{
    areas_.erase(bytes);
    triggers_.erase(bytes);
    emitters_.erase(bytes);
    motion_blurs_.erase(bytes);
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
    else if (object.type == 0xD)
    {
        dispatch_runtime_fx_anim_emitter_message(message, emitters_.at(object.bytes.get())->context);
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

void NativeSceneActorClassOwner::update_emitter(FactoryObject& object)
{
    update_runtime_fx_anim_emitter(emitters_.at(object.bytes.get())->context);
}

u32 NativeSceneActorClassOwner::draw_emitter(FactoryObject& object, i32 kind, RuntimeFxEmitterDraw& draw)
{
    return draw_runtime_fx_anim_emitter(kind, draw, emitters_.at(object.bytes.get())->context);
}

void NativeSceneActorClassOwner::update_motion_blur(FactoryObject& object)
{
    tick_runtime_avatar_motion_blur(*motion_blurs_.at(object.bytes.get()));
}

bool NativeSceneActorClassOwner::emitter_visible(FactoryObject& object, u32 pass, u32 slot)
{
    return runtime_fx_anim_emitter_visible(pass, slot, emitters_.at(object.bytes.get())->context);
}
}
