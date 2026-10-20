#include "scene_actor_factory_owner.h"
#include "../cutscenes/event_runtime.h"
#include "gbtg/actor_spawn_factory.h"
#include "entity.h"
#include "ghouly_spawn_scheduler.h"

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
bool construct_cutscene_setup_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_cutscene_setup(object, parameters);
}
void release_cutscene_setup_view(FactoryObject& object)
{
    active_owner->release_cutscene_setup(object);
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
bool construct_afterimage_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_afterimage(object, parameters);
}
void release_afterimage_view(FactoryObject& object)
{
    active_owner->release_afterimage(object);
}
bool construct_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_weapon(object, parameters);
}
bool construct_chair_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_chair_weapon(object, parameters);
}
bool construct_large_projectile_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_large_projectile(object, parameters);
}
void release_large_projectile_view(FactoryObject& object)
{
    active_owner->release_large_projectile(object);
}
bool construct_bob_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_bob_weapon(object, parameters);
}
void release_bob_weapon_view(FactoryObject& object)
{
    active_owner->release_bob_weapon(object);
}
void release_weapon_view(FactoryObject& object)
{
    active_owner->release_weapon(object);
}
// Retail 0x001628B0 is the chair's empty destructor.
void release_chair_weapon_view(FactoryObject&)
{
}
const ObjectFactoryVtable* weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x005461A8, weapon_actor_object_sizes,
        construct_weapon_view, release_weapon_view};
    return &table;
}
const ObjectFactoryVtable* chair_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506438, chair_weapon_object_sizes,
        construct_chair_weapon_view, release_chair_weapon_view};
    return &table;
}
const ObjectFactoryVtable* large_projectile_lifecycle()
{
    static const ObjectFactoryVtable table{0x005063E8, large_projectile_weapon_object_sizes,
        construct_large_projectile_view, release_large_projectile_view};
    return &table;
}
const ObjectFactoryVtable* bob_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x005060B8, bob_weapon_object_sizes, construct_bob_weapon_view,
        release_bob_weapon_view};
    return &table;
}
const ObjectFactoryVtable* cutscene_setup_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504B08, ghouly_cutscene_setup_object_sizes,
        construct_cutscene_setup_view, release_cutscene_setup_view};
    return &table;
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
bool construct_bone_debris_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_bone_debris(object, parameters);
}
void release_bone_debris_view(FactoryObject& object)
{
    active_owner->release_bone_debris(object);
}
const ObjectFactoryVtable* bone_debris_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504A18, bone_debris_object_sizes, construct_bone_debris_view,
        release_bone_debris_view};
    return &table;
}
const ObjectFactoryVtable* afterimage_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504A54, avatar_afterimage_object_sizes,
        construct_afterimage_view, release_afterimage_view};
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
            }, bindings.latch_owner}
    {
        message = [](GhoulyController& controller, void* data)
        {
            auto& instance = static_cast<TriggerInstance&>(controller);
            // 0x0013E8D0 sends the full 0x108-byte A8 request; the spawn scheduler's 0xA7 request
            // (0x000F3E90) and its 0xA9 / 0xAA variant requests are their own sizes.
            auto* words = static_cast<u32*>(data);
            size_t count = 0x42;
            if (words[0] == 0xA7)
            {
                count = sizeof(GhoulySpawnMessage) / 4;
            }
            else if (words[0] == 0xA9 || words[0] == 0xAA)
            {
                count = sizeof(GhoulyVariantMessage) / 4;
            }
            dispatch_runtime_trigger_message(instance.messages, {words, count});
        };
    }
};

struct NativeSceneActorClassOwner::CutsceneSetupInstance : GhoulyController
{
    GhoulySpawnRecord* load = nullptr; // +0x944.
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x948.
    EntityCamNode camera{}; // +0x94C.
    RuntimeGhoulyCutsceneSetup context;
    RuntimeGhoulyCutsceneSetupMessages messages;
    CutsceneSetupInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor, *this, bindings.random,
            bindings.occupancy_initial, load, cutscene, camera, bindings.weapon_services.cutscenes,
            bindings.weapon_services.voices,
            [&bindings, bytes = object.bytes.get()]()
            {
                return bindings.address_of(bytes);
            },
            bindings.message, bindings.emitter.clock},
          messages{context, bindings.base_of(object), bindings.objects, bindings.bytes_at,
            [&bindings](FactoryObject& actor) -> Actor&
            {
                return bindings.actors.view(actor).bat.actor;
            }, bindings.keep, [&bindings](ActorBaseContext& base, std::span<u32> message)
            {
                dispatch_base_actor_word_message(base, message, bindings.word_messages);
            }, bindings.latch_owner}
    {
        message = [](GhoulyController& controller, void* data)
        {
            auto& instance = static_cast<CutsceneSetupInstance&>(controller);
            // As the trigger's: the 0x108-byte A8 request, and the scheduler's 0xA7 / 0xA9 / 0xAA requests.
            auto* words = static_cast<u32*>(data);
            size_t count = 0x42;
            if (words[0] == 0xA7)
            {
                count = sizeof(GhoulySpawnMessage) / 4;
            }
            else if (words[0] == 0xA9 || words[0] == 0xAA)
            {
                count = sizeof(GhoulyVariantMessage) / 4;
            }
            dispatch_runtime_ghouly_cutscene_setup_message(instance.messages, {words, count});
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

// One weapon: its native views and the ghouly controller its DAT_00512B74 entry names.
struct NativeSceneActorClassOwner::WeaponInstance : GhoulyController
{
    RuntimeWeaponActor context;
    WeaponInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor,
            bindings.actors.view(object), object.type, *this, bindings.random, bindings.executable,
            bindings.actors.bindings.startup.registry, bindings.actors.bindings.startup.error, bindings.model_types,
            bindings.matrix_bindings, bindings.matrix_boundaries, bindings.global_attachments,
            [&bindings, bytes = object.bytes.get()](const char* name,
                const std::function<void(std::span<u8>)>& patch) -> u32
            {
                // 0x00107720 clones the template, +8 takes this actor's address, 0x00107520 creates it and the
                // scene keeps it (0x0003DF40 when there is no further patch).
                FactoryObject created;
                if (!patch)
                {
                    created = create_resource_object_with_data(bindings.actors.bindings.factory, bindings.pools,
                        name, bindings.address_of(bytes));
                }
                else
                {
                    auto parameters = clone_executable_resource(bindings.pools, name);
                    if (parameters.bytes == nullptr)
                    {
                        return 0;
                    }
                    const u32 owner = bindings.address_of(bytes);
                    std::memcpy(parameters.bytes.get() + 8, &owner, 4);
                    patch({parameters.bytes.get(), parameters.size});
                    created = create_factory_object(bindings.actors.bindings.factory,
                        {parameters.bytes.get(), parameters.size});
                }
                if (created.bytes == nullptr)
                {
                    return 0;
                }
                return bindings.address_of(bindings.keep(std::move(created))->bytes.get());
            },
            [&bindings, bytes = object.bytes.get()](std::span<u32> message)
            {
                auto* self = bindings.object_at(bindings.address_of(bytes));
                if (self == nullptr)
                {
                    throw Error("weapon base message for an object outside the scene");
                }
                dispatch_base_actor_word_message(bindings.base_of(*self), message, bindings.word_messages);
            },
            [&bindings, bytes = object.bytes.get()](std::span<u32> message)
            {
                bindings.message(bindings.address_of(bytes), message);
            },
            bindings.latch_owner, bindings.bytes_at, bindings.message,
            [&bindings, bytes = object.bytes.get()]()
            {
                return bindings.address_of(bytes);
            },
            bindings.emitter.frame, bindings.prompt_lock, bindings.marks, bindings.case_actors, bindings.query,
            bindings.emitter.runtime_flags, bindings.contact_actor, entity_detail::g_world.body_a,
            bindings.emitter.clock,
            [&bindings](u32 address) -> Actor*
            {
                auto* object = bindings.object_at(address);
                if (object == nullptr)
                {
                    throw Error("weapon message names an object outside the scene");
                }
                return &bindings.actors.view(*object).bat.actor;
            },
            [&bindings, bytes = object.bytes.get()]() -> ActorBaseContext&
            {
                auto* self = bindings.object_at(bindings.address_of(bytes));
                if (self == nullptr)
                {
                    throw Error("weapon base views for an object outside the scene");
                }
                return bindings.base_of(*self);
            },
            bindings.default_euler, bindings.navigation_nodes, bindings.weapon_services}
    {
        context.animation_tables = &bindings.actors.bindings.startup.tables;
        context.model_source = [this](u32 address)
        {
            return context.services.resolve(context.actor, address);
        };
        context.object_address = [&bindings](const FactoryObject& created)
        {
            return bindings.address_of(created.bytes.get());
        };
        context.contact_actor_at = [&bindings](u32 address) -> RuntimeSegmentContactActor&
        {
            auto* object = bindings.object_at(address);
            if (object == nullptr)
            {
                throw Error("weapon contact override for an object outside the scene");
            }
            return bindings.contact_actor(bindings.actors.view(*object).segment);
        };
        message = [](GhoulyController&, void*)
        {
            throw Error("untranslated Xbox 0x000CF0D0 (the chair's message) reached from a ghoulybox request");
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
    types[0x25] = {2, cutscene_setup_lifecycle}; // Getter 0x0006B110.
    types[0xD] = {2, emitter_lifecycle}; // Getter 0x00109DD0.
    types[0xF] = {2, motion_blur_lifecycle}; // Getter 0x00066900.
    types[0x10] = {2, afterimage_lifecycle}; // Getter 0x000690A0.
    types[0x12] = {2, bone_debris_lifecycle}; // Getter 0x00067E60.
    types[0xB] = {2, weapon_lifecycle}; // Getter 0x00145900.
    types[0xAA] = {0xB, chair_weapon_lifecycle}; // Getter 0x000CEF70.
    types[0xA9] = {0xB, large_projectile_lifecycle}; // Getter 0x000CD640.
    types[0xA0] = {0xB, bob_weapon_lifecycle}; // Getter 0x000C5BB0.
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

bool NativeSceneActorClassOwner::construct_afterimage(FactoryObject& object, std::span<const u8> parameters)
{
    auto views = [this](FactoryObject& actor)
    {
        auto& view = bindings_.actors.view(actor);
        return RuntimeAvatarActorViews{{actor.bytes.get(), actor.size}, view.bat.actor, view.startup};
    };
    auto& view = bindings_.actors.view(object);
    auto context = std::make_unique<RuntimeAvatarAfterimage>(RuntimeAvatarAfterimage{views(object),
        [this, views](u32 address)
        {
            auto* actor = bindings_.object_at(address);
            if (actor == nullptr)
            {
                throw Error("after-image source actor is outside the scene");
            }
            return views(*actor);
        },
        bindings_.actors.bindings.startup, bindings_.actors.bindings.empty_name, bindings_.emitter.clock, view.frame,
        bindings_.emitter.frame, view.render, view.shape, view.anchors,
        [this](u32 address) -> std::span<const u8>
        {
            return bindings_.bytes_at(address);
        }});
    auto* afterimage = context.get();
    afterimages_.emplace(object.bytes.get(), std::move(context));
    return initialize_runtime_avatar_afterimage(parameters, *afterimage);
}

void NativeSceneActorClassOwner::release_afterimage(FactoryObject& object)
{
    destroy_runtime_avatar_afterimage(*afterimages_.at(object.bytes.get()));
    if (!defer_release)
    {
        afterimages_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_afterimage(FactoryObject& object)
{
    tick_runtime_avatar_afterimage(*afterimages_.at(object.bytes.get()));
}

void NativeSceneActorClassOwner::message_afterimage(FactoryObject& object, u32 id)
{
    message_runtime_avatar_afterimage(id, *afterimages_.at(object.bytes.get()));
}

bool NativeSceneActorClassOwner::construct_bone_debris(FactoryObject& object, std::span<const u8> parameters)
{
    auto views = [this](FactoryObject& actor)
    {
        auto& view = bindings_.actors.view(actor);
        return RuntimeAvatarActorViews{{actor.bytes.get(), actor.size}, view.bat.actor, view.startup};
    };
    auto& view = bindings_.actors.view(object);
    auto& services = bindings_.weapon_services;
    Actor& actor = view.bat.actor;
    auto context = std::make_unique<RuntimeBoneDebris>(RuntimeBoneDebris{views(object),
        bindings_.address_of(object.bytes.get()),
        [this, views](u32 address)
        {
            auto* source = bindings_.object_at(address);
            if (source == nullptr)
            {
                throw Error("bone debris source actor is outside the scene");
            }
            return views(*source);
        },
        bindings_.actors.bindings.startup, bindings_.model_types, bindings_.matrix_bindings,
        bindings_.matrix_boundaries, {}, view.matrices, bindings_.effects, nullptr,
        [this](const ActorEffectNode& effect)
        {
            return bindings_.heap_address(effect.bytes.data(), effect.bytes.size());
        },
        bindings_.actors.bindings.resource_address, services.sampling_scratch, services.sampling_active,
        actor_transform_origin,
        [&services, &actor](u32 address)
        {
            return services.resolve(actor, address);
        },
        bindings_.random, bindings_.pal, bindings_.pal_override, bindings_.emitter.clock, view.frame,
        bindings_.emitter.frame, bindings_.tick_owned_effect, bindings_.effect_collision.normal,
        bindings_.base_of(object).callouts, bindings_.actors.bindings.factory, bindings_.keep,
        bindings_.actors.bindings.empty_name, bindings_.effect_release, view.render, view.shape, view.anchors,
        bindings_.actors.bindings.sounds});
    auto* debris = context.get();
    bone_debris_.emplace(object.bytes.get(), std::move(context));
    return initialize_runtime_bone_debris(parameters, *debris);
}

void NativeSceneActorClassOwner::release_bone_debris(FactoryObject& object)
{
    destroy_runtime_bone_debris(*bone_debris_.at(object.bytes.get()));
    if (!defer_release)
    {
        bone_debris_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_bone_debris(FactoryObject& object)
{
    update_runtime_bone_debris(*bone_debris_.at(object.bytes.get()));
}

void NativeSceneActorClassOwner::message_bone_debris(FactoryObject& object, std::span<const u32> message)
{
    message_runtime_bone_debris(message, *bone_debris_.at(object.bytes.get()));
}

u32 NativeSceneActorClassOwner::draw_bone_debris(FactoryObject& object, i32 kind, RuntimeBoneDebrisDraw& draw)
{
    return draw_runtime_bone_debris(kind, draw, *bone_debris_.at(object.bytes.get()));
}

ActorMatrixSetup& NativeSceneActorClassOwner::bone_debris_matrices(FactoryObject& object)
{
    return bone_debris_.at(object.bytes.get())->matrices;
}

bool NativeSceneActorClassOwner::construct_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<WeaponInstance>(bindings_, object);
    auto* weapon = instance.get();
    bindings_.refresh_shape_poses(object);
    weapons_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_weapon_actor(weapon->context, parameters);
}

bool NativeSceneActorClassOwner::construct_chair_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_chair_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

bool NativeSceneActorClassOwner::construct_large_projectile(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_large_projectile(weapons_.at(object.bytes.get())->context, parameters);
}

void NativeSceneActorClassOwner::release_large_projectile(FactoryObject& object)
{
    destroy_runtime_large_projectile(weapons_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_bob_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_bob_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

void NativeSceneActorClassOwner::release_bob_weapon(FactoryObject& object)
{
    destroy_runtime_bob_weapon(weapons_.at(object.bytes.get())->context);
}

void NativeSceneActorClassOwner::release_weapon(FactoryObject& object)
{
    destroy_runtime_weapon_actor(weapons_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        weapons_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_weapon(FactoryObject& object)
{
    update_runtime_weapon_actor(weapons_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::weapon_draw_hidden(FactoryObject& object)
{
    return runtime_weapon_draw_hidden(weapons_.at(object.bytes.get())->context);
}

void NativeSceneActorClassOwner::draw_weapon_prompt(FactoryObject& object, i32 pass)
{
    draw_runtime_weapon_prompt(weapons_.at(object.bytes.get())->context, pass);
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

bool NativeSceneActorClassOwner::construct_cutscene_setup(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<CutsceneSetupInstance>(bindings_, object);
    auto* setup = instance.get();
    cutscene_setups_.emplace(object.bytes.get(), std::move(instance));
    // Cutscene event 0x13 calls instance +0x150 (0x0006BA60) with +0x154, this setup's address.
    bindings_.weapon_services.cutscenes().event_callbacks[0x0006BA60] =
        [this](RuntimeCutsceneInstance&, u32 owner, u32)
        {
            auto* object = bindings_.object_at(owner);
            if (object == nullptr)
            {
                throw Error("cutscene spawn event for an object outside the scene");
            }
            release_runtime_ghouly_cutscene_spawn({object->bytes.get(), object->size}); // 0x0006BA60
        };
    return initialize_runtime_ghouly_cutscene_setup(setup->context, parameters);
}

void NativeSceneActorClassOwner::release_cutscene_setup(FactoryObject& object)
{
    destroy_runtime_ghouly_cutscene_setup(cutscene_setups_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        cutscene_setups_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_cutscene_setup(FactoryObject& object)
{
    update_runtime_ghouly_cutscene_setup(cutscene_setups_.at(object.bytes.get())->context);
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
    cutscene_setups_.erase(bytes);
    emitters_.erase(bytes);
    motion_blurs_.erase(bytes);
    afterimages_.erase(bytes);
    bone_debris_.erase(bytes);
    weapons_.erase(bytes);
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
    else if (object.type == 0x25)
    {
        dispatch_runtime_ghouly_cutscene_setup_message(cutscene_setups_.at(object.bytes.get())->messages, message);
    }
    else if (object.type == 0xAA)
    {
        dispatch_runtime_chair_weapon_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA9)
    {
        dispatch_runtime_large_projectile_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA0)
    {
        dispatch_runtime_bob_weapon_message(weapons_.at(object.bytes.get())->context, message);
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

void NativeSceneActorClassOwner::controller_actors(std::unordered_map<const GhoulyController*, Actor*>& output)
{
    output.clear();
    for (auto& [bytes, trigger] : triggers_)
    {
        output[trigger.get()] = &trigger->context.actor;
    }
    for (auto& [bytes, setup] : cutscene_setups_)
    {
        output[setup.get()] = &setup->context.actor;
    }
    for (auto& [bytes, weapon] : weapons_)
    {
        output[weapon.get()] = &weapon->context.actor;
    }
}

RuntimeBodyCollisionState*& NativeSceneActorClassOwner::weapon_markers(const u8* bytes)
{
    return weapons_.at(bytes)->context.markers;
}

bool NativeSceneActorClassOwner::owns_weapon(const u8* bytes) const
{
    return weapons_.find(bytes) != weapons_.end();
}
}
