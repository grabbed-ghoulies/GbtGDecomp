#include "scene_actor_factory_owner.h"
#include "../scripts/script_actions.h"
#include "../scene/scene.h"
#include "../cutscenes/event_runtime.h"
#include "gbtg/actor_spawn_factory.h"
#include "entity.h"
#include "ghouly_spawn_scheduler.h"
#include "actor_lookup.h"

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
bool construct_bg_break_setup_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_bg_break_setup(object, parameters);
}
void release_bg_break_setup_view(FactoryObject& object)
{
    active_owner->release_bg_break_setup(object);
}
bool construct_swap_setup_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_swap_setup(object, parameters);
}
void release_swap_setup_view(FactoryObject& object)
{
    active_owner->release_swap_setup(object);
}
bool construct_scare_figure_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_scare_figure(object, parameters);
}
void release_scare_figure_view(FactoryObject& object)
{
    active_owner->release_scare_figure(object);
}
bool construct_super_scary_shock_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_super_scary_shock(object, parameters);
}
void release_super_scary_shock_view(FactoryObject& object)
{
    active_owner->release_super_scary_shock(object);
}
bool construct_sss_dropper_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_sss_dropper(object, parameters);
}
void release_sss_dropper_view(FactoryObject&)
{
    // 0x001628B0 returns at once.
}
bool construct_bg_break_key_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_bg_break_key(object, parameters);
}
void release_bg_break_key_view(FactoryObject& object)
{
    active_owner->release_bg_break_key(object);
}
bool construct_ghouly_key_view(FactoryObject& object, std::span<const u8>)
{
    return active_owner->construct_ghouly_key(object);
}
void release_ghouly_key_view(FactoryObject& object)
{
    active_owner->release_ghouly_key(object);
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
bool construct_candle_weapon_view(FactoryObject& object, std::span<const u8>)
{
    return active_owner->construct_candle_weapon(object);
}
void release_candle_weapon_view(FactoryObject& object)
{
    active_owner->release_candle_weapon(object);
}
bool construct_gun_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_gun_weapon(object, parameters);
}
void release_gun_weapon_view(FactoryObject&)
{
    // The gun's vtable +8 is 0x001628B0, which returns at once.
}
bool construct_extinguisher_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_extinguisher_weapon(object, parameters);
}
void release_extinguisher_weapon_view(FactoryObject& object)
{
    active_owner->release_extinguisher_weapon(object);
}
bool construct_water_pistol_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_water_pistol_weapon(object, parameters);
}
void release_water_pistol_weapon_view(FactoryObject& object)
{
    active_owner->release_water_pistol_weapon(object);
}
bool construct_container_weapon_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_container_weapon(object, parameters);
}
void release_container_weapon_view(FactoryObject&)
{
    // 0x001628B0 returns at once.
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
bool construct_thrown_projectile_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_thrown_projectile(object, parameters);
}
void release_thrown_projectile_view(FactoryObject& object)
{
    active_owner->release_thrown_projectile(object);
}
const ObjectFactoryVtable* thrown_projectile_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506348, thrown_projectile_object_sizes,
        construct_thrown_projectile_view, release_thrown_projectile_view};
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
const ObjectFactoryVtable* gun_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x005062E0, gun_weapon_object_sizes, construct_gun_weapon_view,
        release_gun_weapon_view};
    return &table;
}
// Getter 0x000C9210 (vtable 0x00506278) over the gun's 0x000CA990.
const ObjectFactoryVtable* water_pistol_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506278, water_pistol_weapon_object_sizes,
        construct_water_pistol_weapon_view, release_water_pistol_weapon_view};
    return &table;
}

const ObjectFactoryVtable* extinguisher_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x005061A8, extinguisher_weapon_object_sizes,
        construct_extinguisher_weapon_view, release_extinguisher_weapon_view};
    return &table;
}
const ObjectFactoryVtable* container_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506158, container_weapon_object_sizes, construct_container_weapon_view,
        release_container_weapon_view};
    return &table;
}
const ObjectFactoryVtable* candle_weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506108, candle_weapon_object_sizes, construct_candle_weapon_view,
        release_candle_weapon_view};
    return &table;
}
const ObjectFactoryVtable* cutscene_setup_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504B08, ghouly_cutscene_setup_object_sizes,
        construct_cutscene_setup_view, release_cutscene_setup_view};
    return &table;
}
const ObjectFactoryVtable* bg_break_setup_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504ACC, ghouly_bg_break_setup_object_sizes,
        construct_bg_break_setup_view, release_bg_break_setup_view};
    return &table;
}
const ObjectFactoryVtable* swap_setup_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504B44, ghouly_swap_setup_object_sizes, construct_swap_setup_view,
        release_swap_setup_view};
    return &table;
}
const ObjectFactoryVtable* super_scary_shock_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504F40, super_scary_shock_object_sizes,
        construct_super_scary_shock_view, release_super_scary_shock_view};
    return &table;
}
const ObjectFactoryVtable* sss_dropper_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504F04, sss_dropper_object_sizes, construct_sss_dropper_view,
        release_sss_dropper_view};
    return &table;
}
const ObjectFactoryVtable* scare_figure_lifecycle()
{
    // Vtable +8 is the empty 0x001628B0.
    static const ObjectFactoryVtable table{0x00504E14, scare_figure_object_sizes, construct_scare_figure_view,
        release_scare_figure_view};
    return &table;
}
// Getter 0x0006E950 (vtable 0x00504D24) over the actor's (0x0003C080); +8 is the empty 0x001628B0.
const ObjectFactoryVtable* ghouly_key_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504D24, ghouly_key_object_sizes, construct_ghouly_key_view,
        release_ghouly_key_view};
    return &table;
}

const ObjectFactoryVtable* bg_break_key_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504BF8, bg_break_key_object_sizes, construct_bg_break_key_view,
        release_bg_break_key_view};
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
bool construct_animated_light_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct_animated_light(object, parameters);
}
void release_animated_light_view(FactoryObject& object)
{
    active_owner->release_animated_light(object);
}
const ObjectFactoryVtable* animated_light_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504B80, animated_light_object_sizes, construct_animated_light_view,
        release_animated_light_view};
    return &table;
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
            [this](std::span<u32> message)
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

struct NativeSceneActorClassOwner::BgBreakSetupInstance : GhoulyController
{
    GhoulySpawnRecord* load = nullptr; // +0xA4C.
    RuntimeGhoulyBgBreakSetup context;
    RuntimeGhoulyBgBreakSetupMessages messages;
    BgBreakSetupInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor, *this, bindings.random,
            bindings.occupancy_initial, load,
            [this, &bindings](const void* pointer) -> u32
            {
                // 0x000F39A0's record is a heap block the setup keeps at +0xA4C.
                if (pointer != nullptr && pointer == load)
                {
                    return bindings.heap_address(load, sizeof(GhoulySpawnRecord));
                }
                return bindings.address_of(pointer);
            },
            bindings.message,
            [this](std::span<u32> message)
            {
                dispatch_runtime_ghouly_bg_break_setup_message(messages, message);
            },
            bindings.spawns, bindings.objects.records, bindings.backgrounds,
            bindings.break_background_part, bindings.hold_background_part, bindings.marks},
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
            auto& instance = static_cast<BgBreakSetupInstance&>(controller);
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
            dispatch_runtime_ghouly_bg_break_setup_message(instance.messages, {words, count});
        };
    }
};

struct NativeSceneActorClassOwner::SwapSetupInstance : GhoulyController
{
    GhoulySpawnRecord* load = nullptr; // +0x938.
    RuntimeGhoulySwapSetup context;
    RuntimeGhoulySwapSetupMessages messages;
    SwapSetupInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor, *this, bindings.random,
            load,
            [this, &bindings](const void* pointer) -> u32
            {
                // 0x000F39A0's record is a heap block the setup keeps at +0x938.
                if (pointer != nullptr && pointer == load)
                {
                    return bindings.heap_address(load, sizeof(GhoulySpawnRecord));
                }
                return bindings.address_of(pointer);
            },
            bindings.message,
            [this](std::span<u32> message)
            {
                dispatch_runtime_ghouly_swap_setup_message(messages, message);
            },
            [&bindings](u32 address) -> Actor*
            {
                FactoryObject* object = address == 0 ? nullptr : bindings.object_at(address);
                return object == nullptr ? nullptr : &bindings.actors.view(*object).bat.actor;
            }},
          messages{context, bindings.base_of(object), bindings.objects, bindings.bytes_at, bindings.keep,
            [&bindings](ActorBaseContext& base, std::span<u32> message)
            {
                dispatch_base_actor_word_message(base, message, bindings.word_messages);
            }, bindings.latch_owner,
            [&bindings](const Actor& actor) -> u32
            {
                return bindings.weapon_services.actor_address(actor);
            }, g_flags_ea9c, // DAT_0050EA9C.
            [&bindings](u32 address)
            {
                FactoryObject* object = bindings.object_at(address);
                if (object == nullptr)
                {
                    throw Error("0x00112CC0 retires an address that is not a scene actor");
                }
                activate_actor_bind_tree(bindings.actors.view(*object).bat.actor); // 0x00112CC0
            }}
    {
        message = [](GhoulyController& controller, void* data)
        {
            auto& instance = static_cast<SwapSetupInstance&>(controller);
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
            dispatch_runtime_ghouly_swap_setup_message(instance.messages, {words, count});
        };
    }
};

struct NativeSceneActorClassOwner::SuperScaryShockInstance
{
    RuntimeSuperScaryShock context;
    SuperScaryShockInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object), bindings.random, bindings.streams,
            bindings.prompt_lock, bindings.dialogue, g_flags_ea9c,
            [&bindings, bytes = object.bytes.get()](std::span<u32> message)
            {
                auto* self = bindings.object_at(bindings.address_of(bytes));
                if (self == nullptr)
                {
                    throw Error("Super Scary shock base message for an object outside the scene");
                }
                dispatch_base_actor_word_message(bindings.base_of(*self), message, bindings.word_messages);
            },
            [&bindings]() -> const Actor*
            {
                const auto players = bindings.case_actors(); // DAT_00511B40.
                return players.empty() ? nullptr : &players.front().actor;
            },
            [&bindings](const char* name, const std::function<void(std::span<u8>)>& patch) -> u32
            {
                auto parameters = clone_executable_resource(bindings.pools, name); // 0x00107720
                if (parameters.bytes == nullptr)
                {
                    return 0;
                }
                patch({parameters.bytes.get(), parameters.size});
                FactoryObject* created = bindings.keep(create_factory_object(bindings.actors.bindings.factory,
                    {parameters.bytes.get(), parameters.size})); // 0x00107520
                parameters.bytes.reset(); // 0x000663E0
                if (created == nullptr || created->bytes == nullptr)
                {
                    return 0;
                }
                return bindings.address_of(created->bytes.get());
            },
            [&bindings](std::span<const u8> parameters)
            {
                bindings.keep(create_factory_object(bindings.actors.bindings.factory, parameters)); // 0x00107520
            },
            bindings.message,
            [&bindings](u32 address) -> NativeActorFactoryInstance&
            {
                FactoryObject* found = bindings.object_at(address);
                if (found == nullptr)
                {
                    throw Error("Super Scary shock's ghouly is outside the scene");
                }
                return bindings.actors.view(*found);
            },
            bindings.queue_event}
    {
    }
};

struct NativeSceneActorClassOwner::SssDropperInstance
{
    RuntimeSssDropper context;
    SssDropperInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object),
            bindings.actors.bindings.startup.registry, bindings.actors.bindings.startup.error, bindings.random,
            bindings.emitter.clock, g_script_30d0c, g_flags_ea9c, bindings.default_name,
            [&bindings, bytes = object.bytes.get()](std::span<u32> message)
            {
                auto* self = bindings.object_at(bindings.address_of(bytes));
                if (self == nullptr)
                {
                    throw Error("Super Scary dropper base message for an object outside the scene");
                }
                dispatch_base_actor_word_message(bindings.base_of(*self), message, bindings.word_messages);
            },
            [&bindings]() -> const Actor*
            {
                const auto players = bindings.case_actors(); // DAT_00511B40.
                return players.empty() ? nullptr : &players.front().actor;
            },
            bindings.player_body,
            [&bindings](const Actor& actor) -> u32
            {
                for (const auto& [bytes, native] : bindings.actors.actors)
                {
                    if (native == &actor)
                    {
                        return bindings.address_of(bytes);
                    }
                }
                throw Error("Super Scary dropper addresses an actor outside the scene");
            },
            bindings.message,
            [&bindings](std::span<const u8> parameters)
            {
                bindings.keep(create_factory_object(bindings.actors.bindings.factory, parameters)); // 0x00107520
            },
            bindings.pause_groups,
            [&bindings, &actor = bindings.actors.view(object).bat.actor](float& timer)
            {
                const auto& clock = bindings.emitter.clock;
                return actor_tick_timer(actor, timer, clock.freeze, clock.dt, clock.scale); // 0x00042360
            }}
    {
    }
};

struct NativeSceneActorClassOwner::ScareFigureInstance
{
    RuntimeScareFigure context;
    ScareFigureInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor,
            bindings.actors.view(object).animation, bindings.base_of(object),
            [&bindings](ActorBaseContext& base, std::span<u32> message)
            {
                dispatch_base_actor_word_message(base, message, bindings.word_messages);
            },
            bindings.actors.bindings.startup.registry,
            bindings.actors.bindings.startup.error, bindings.random, bindings.emitter.clock,
            bindings.emitter.runtime_flags,
            [&bindings]() -> const Actor*
            {
                const auto players = bindings.case_actors(); // DAT_00511B40.
                return players.empty() ? nullptr : &players.front().actor;
            },
            [&bindings](std::span<const u32> message)
            {
                // 0x00122D10: each DAT_00511B40 actor's vtable +0xC, the next node read before the call.
                for (const auto& player : bindings.case_actors())
                {
                    std::array<u32, 0x42> words{};
                    std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
                    bindings.message(bindings.weapon_services.actor_address(player.actor), words);
                }
            },
            bindings.queue_event,
            [&bindings](u16 tag) -> Actor*
            {
                const u32 address = find_runtime_actor_by_class(tag, bindings.latch_owner, bindings.bytes_at);
                if (address == 0)
                {
                    return nullptr;
                }
                FactoryObject* found = bindings.object_at(address);
                if (found == nullptr)
                {
                    throw Error("scare figure finds a tagged actor outside the scene");
                }
                return &bindings.actors.view(*found).bat.actor;
            },
            [&bindings](Actor& actor, std::span<const u32> message)
            {
                std::array<u32, 0x42> words{};
                std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
                bindings.message(bindings.weapon_services.actor_address(actor), words);
            }}
    {
    }
};

struct NativeSceneActorClassOwner::BgBreakKeyInstance
{
    EntityCamNode camera{}; // +0x9FC.
    FactoryObject* key = nullptr; // +0x9F8.
    RuntimeBgBreakKey context;
    BgBreakKeyInstance(NativeSceneActorClassBindings& bindings, FactoryObject& object)
        : context{{object.bytes.get(), object.size}, bindings.actors.view(object).bat.actor, camera, key,
            bindings.base_of(object), [&bindings](ActorBaseContext& base, std::span<u32> message)
            {
                dispatch_base_actor_word_message(base, message, bindings.word_messages);
            },
            bindings.emitter.clock, bindings.emitter.view, bindings.weapon_services.voices,
            bindings.actors.bindings.empty_name, bindings.address_of, bindings.word_messages.memory_at,
            bindings.bytes_at, bindings.latch_owner,
            [&bindings](u32 address) -> Actor&
            {
                FactoryObject* source = bindings.object_at(address);
                if (source == nullptr)
                {
                    throw Error("bg-break key reads an actor outside the scene");
                }
                return bindings.actors.view(*source).bat.actor;
            },
            [&bindings](FactoryObject& actor) -> Actor&
            {
                return bindings.actors.view(actor).bat.actor;
            },
            [&bindings](std::span<const u8> parameters)
            {
                return bindings.keep(create_factory_object(bindings.actors.bindings.factory, parameters));
            },
            [&bindings](FactoryObject& actor, i32 id)
            {
                dispatch_actor_resource_callout(bindings.actors.view(actor).startup.resource, id, nullptr,
                    bindings.base_of(actor).callouts);
            },
            bindings.backgrounds, bindings.pause_groups, bindings.queue_event}
    {
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
    std::unique_ptr<RuntimeBodyAttachmentLists> lists; // Actor +0x1B8 / +0x1B0 and DAT_00506930.
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
        auto& view = bindings.actors.view(object);
        lists = std::make_unique<RuntimeBodyAttachmentLists>(RuntimeBodyAttachmentLists{view.pending.next,
            view.startup.attachments.sentinel.next, bindings.emitter.timed_attachments});
        context.attachment_lists = lists.get();
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

std::vector<const u8*> NativeSceneActorClassOwner::weapons_held_by(const Actor& holder) const
{
    std::vector<const u8*> instances;
    for (const auto& [bytes, weapon] : weapons_)
    {
        if (weapon->context.actor.spawn_owner == &holder) // +0xD8C
        {
            instances.push_back(bytes);
        }
    }
    return instances;
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
    types[0x24] = {2, bg_break_setup_lifecycle}; // Getter 0x0006A4F0.
    types[0x26] = {2, swap_setup_lifecycle}; // Getter 0x0006BB30.
    types[0x1D] = {2, scare_figure_lifecycle}; // Getter 0x0006EFB0.
    types[0x22] = {2, super_scary_shock_lifecycle}; // Getter 0x00072270.
    types[0x21] = {2, sss_dropper_lifecycle}; // Getter 0x00071820.
    types[0x1E] = {2, bg_break_key_lifecycle}; // Getter 0x0006CC70.
    types[0x18] = {2, ghouly_key_lifecycle}; // Getter 0x0006E950.
    types[0xD] = {2, emitter_lifecycle}; // Getter 0x00109DD0.
    types[0xF] = {2, motion_blur_lifecycle}; // Getter 0x00066900.
    types[0x10] = {2, afterimage_lifecycle}; // Getter 0x000690A0.
    types[0x12] = {2, bone_debris_lifecycle}; // Getter 0x00067E60.
    types[0x13] = {2, animated_light_lifecycle}; // Getter 0x0006C260.
    types[0xB] = {2, weapon_lifecycle}; // Getter 0x00145900.
    types[0xAA] = {0xB, chair_weapon_lifecycle}; // Getter 0x000CEF70.
    types[0xA7] = {0xB, thrown_projectile_lifecycle}; // Getter 0x000CB570.
    types[0xA9] = {0xB, large_projectile_lifecycle}; // Getter 0x000CD640.
    types[0xA0] = {0xB, bob_weapon_lifecycle}; // Getter 0x000C5BB0.
    types[0xA1] = {0xB, candle_weapon_lifecycle}; // Getter 0x000C64F0.
    types[0xA2] = {0xB, container_weapon_lifecycle}; // Getter 0x000C75D0.
    types[0xA4] = {0xB, gun_weapon_lifecycle}; // Getter 0x000CA980.
    types[0xA3] = {0xA4, extinguisher_weapon_lifecycle}; // Getter 0x000C7B70.
    types[0xA6] = {0xA4, water_pistol_weapon_lifecycle}; // Getter 0x000C9210.
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
        bindings_.emitter.frame, bindings_.point_lights, bindings_.script_lights});
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

bool NativeSceneActorClassOwner::construct_animated_light(FactoryObject& object, std::span<const u8> parameters)
{
    auto& view = bindings_.actors.view(object);
    auto& startup = bindings_.actors.bindings.startup;
    auto context = std::make_unique<RuntimeAnimatedLight>(RuntimeAnimatedLight{{object.bytes.get(), object.size},
        view.bat.actor, view.animation, bindings_.point_lights, startup.registry, startup.error, bindings_.random,
        g_billboard_queue});
    auto* light = context.get();
    animated_lights_.emplace(object.bytes.get(), std::move(context));
    return initialize_runtime_animated_light(*light, parameters);
}

void NativeSceneActorClassOwner::release_animated_light(FactoryObject& object)
{
    destroy_runtime_animated_light(*animated_lights_.at(object.bytes.get()));
    if (!defer_release)
    {
        animated_lights_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_animated_light(FactoryObject& object)
{
    update_runtime_animated_light(*animated_lights_.at(object.bytes.get()));
}

void NativeSceneActorClassOwner::queue_animated_light_glow(FactoryObject& object)
{
    queue_runtime_animated_light_glow(*animated_lights_.at(object.bytes.get()));
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

bool NativeSceneActorClassOwner::construct_thrown_projectile(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_thrown_projectile(weapons_.at(object.bytes.get())->context, parameters);
}

void NativeSceneActorClassOwner::release_thrown_projectile(FactoryObject& object)
{
    destroy_runtime_thrown_projectile(weapons_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_bob_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_bob_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

void NativeSceneActorClassOwner::release_bob_weapon(FactoryObject& object)
{
    destroy_runtime_bob_weapon(weapons_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_gun_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_gun_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

bool NativeSceneActorClassOwner::construct_extinguisher_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_extinguisher_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

void NativeSceneActorClassOwner::release_extinguisher_weapon(FactoryObject& object)
{
    destroy_runtime_extinguisher_weapon(weapons_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_water_pistol_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_water_pistol_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

void NativeSceneActorClassOwner::release_water_pistol_weapon(FactoryObject& object)
{
    destroy_runtime_water_pistol_weapon(weapons_.at(object.bytes.get())->context);
}

void NativeSceneActorClassOwner::draw_gun_effects(FactoryObject& object, u32 pass)
{
    draw_runtime_gun_weapon_effects(weapons_.at(object.bytes.get())->context, pass); // 0x000CAD90
}

bool NativeSceneActorClassOwner::construct_container_weapon(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_container_weapon(weapons_.at(object.bytes.get())->context, parameters);
}

bool NativeSceneActorClassOwner::construct_candle_weapon(FactoryObject& object)
{
    return initialize_runtime_candle_weapon(weapons_.at(object.bytes.get())->context);
}

void NativeSceneActorClassOwner::release_candle_weapon(FactoryObject& object)
{
    destroy_runtime_candle_weapon(weapons_.at(object.bytes.get())->context);
}

void NativeSceneActorClassOwner::queue_candle_flame(FactoryObject& object)
{
    RuntimeCandleFlame flame = runtime_candle_weapon_flame(weapons_.at(object.bytes.get())->context);
    queue_runtime_candle_flame(flame); // 0x000C6C80, EDI +0xE5C
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

bool NativeSceneActorClassOwner::construct_bg_break_setup(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<BgBreakSetupInstance>(bindings_, object);
    auto* setup = instance.get();
    bg_break_setups_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_ghouly_bg_break_setup(setup->context, parameters);
}

void NativeSceneActorClassOwner::release_bg_break_setup(FactoryObject& object)
{
    destroy_runtime_ghouly_bg_break_setup(bg_break_setups_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        bg_break_setups_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_bg_break_setup(FactoryObject& object)
{
    update_runtime_ghouly_bg_break_setup(bg_break_setups_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_swap_setup(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<SwapSetupInstance>(bindings_, object);
    auto* setup = instance.get();
    swap_setups_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_ghouly_swap_setup(setup->context, parameters);
}

void NativeSceneActorClassOwner::release_swap_setup(FactoryObject& object)
{
    destroy_runtime_ghouly_swap_setup(swap_setups_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        swap_setups_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_swap_setup(FactoryObject& object)
{
    update_runtime_ghouly_swap_setup(swap_setups_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_scare_figure(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<ScareFigureInstance>(bindings_, object);
    auto* figure = instance.get();
    scare_figures_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_scare_figure(figure->context, parameters);
}

void NativeSceneActorClassOwner::release_scare_figure(FactoryObject& object)
{
    if (!defer_release)
    {
        scare_figures_.erase(object.bytes.get());
    }
}

bool NativeSceneActorClassOwner::construct_super_scary_shock(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<SuperScaryShockInstance>(bindings_, object);
    auto* shock = instance.get();
    super_scary_shocks_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_super_scary_shock(shock->context, parameters);
}

void NativeSceneActorClassOwner::release_super_scary_shock(FactoryObject& object)
{
    const auto found = super_scary_shocks_.find(object.bytes.get());
    if (found == super_scary_shocks_.end())
    {
        return;
    }
    destroy_runtime_super_scary_shock(found->second->context);
    if (!defer_release)
    {
        super_scary_shocks_.erase(found);
    }
}

bool NativeSceneActorClassOwner::construct_sss_dropper(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<SssDropperInstance>(bindings_, object);
    auto* dropper = instance.get();
    sss_droppers_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_sss_dropper(dropper->context, parameters);
}

void NativeSceneActorClassOwner::update_sss_dropper(FactoryObject& object, const std::function<void()>& update_actor)
{
    update_runtime_sss_dropper(sss_droppers_.at(object.bytes.get())->context, update_actor);
}

u32 NativeSceneActorClassOwner::draw_sss_dropper(ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& draw, RuntimeGlowPass& glow)
{
    return draw_runtime_sss_dropper(state, kind, attachments, draw, glow);
}

void NativeSceneActorClassOwner::update_super_scary_shock(FactoryObject& object,
    const std::function<void()>& update_actor)
{
    update_runtime_super_scary_shock(super_scary_shocks_.at(object.bytes.get())->context, update_actor);
}

void NativeSceneActorClassOwner::update_scare_figure(FactoryObject& object)
{
    update_runtime_scare_figure(scare_figures_.at(object.bytes.get())->context);
}

u32 NativeSceneActorClassOwner::draw_scare_figure(FactoryObject& object, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& draw, RuntimeGlowPass& glow)
{
    return draw_runtime_scare_figure(scare_figures_.at(object.bytes.get())->context, state, kind, attachments, draw,
        glow);
}

bool NativeSceneActorClassOwner::construct_bg_break_key(FactoryObject& object, std::span<const u8> parameters)
{
    auto instance = std::make_unique<BgBreakKeyInstance>(bindings_, object);
    auto* key = instance.get();
    bg_break_keys_.emplace(object.bytes.get(), std::move(instance));
    return initialize_runtime_bg_break_key(key->context, parameters);
}

void NativeSceneActorClassOwner::release_bg_break_key(FactoryObject& object)
{
    destroy_runtime_bg_break_key(bg_break_keys_.at(object.bytes.get())->context);
    if (!defer_release)
    {
        bg_break_keys_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_bg_break_key(FactoryObject& object)
{
    update_runtime_bg_break_key(bg_break_keys_.at(object.bytes.get())->context);
}

bool NativeSceneActorClassOwner::construct_ghouly_key(FactoryObject& object)
{
    auto& view = bindings_.actors.view(object);
    auto context = std::make_unique<RuntimeGhoulyKey>(RuntimeGhoulyKey{{object.bytes.get(), object.size},
        view.bat.actor, bindings_.emitter.clock, bindings_.weapon_services.voices,
        [this, &object](i32 id)
        {
            dispatch_actor_attachment_records(bindings_.actors.view(object).startup.resource.resource.data(), id,
                nullptr, nullptr, bindings_.base_of(object).callouts, 0); // 0x0003BE20
        }});
    auto* key = context.get();
    ghouly_keys_.emplace(object.bytes.get(), std::move(context));
    return initialize_runtime_ghouly_key(*key);
}

void NativeSceneActorClassOwner::release_ghouly_key(FactoryObject& object)
{
    if (!defer_release)
    {
        ghouly_keys_.erase(object.bytes.get());
    }
}

void NativeSceneActorClassOwner::update_ghouly_key(FactoryObject& object)
{
    update_runtime_ghouly_key(*ghouly_keys_.at(object.bytes.get()));
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
    bg_break_setups_.erase(bytes);
    swap_setups_.erase(bytes);
    bg_break_keys_.erase(bytes);
    scare_figures_.erase(bytes);
    super_scary_shocks_.erase(bytes);
    sss_droppers_.erase(bytes);
    emitters_.erase(bytes);
    motion_blurs_.erase(bytes);
    afterimages_.erase(bytes);
    bone_debris_.erase(bytes);
    animated_lights_.erase(bytes);
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
    else if (object.type == 0x24)
    {
        dispatch_runtime_ghouly_bg_break_setup_message(bg_break_setups_.at(object.bytes.get())->messages, message);
    }
    else if (object.type == 0x26)
    {
        dispatch_runtime_ghouly_swap_setup_message(swap_setups_.at(object.bytes.get())->messages, message);
    }
    else if (object.type == 0x1E)
    {
        dispatch_runtime_bg_break_key_message(bg_break_keys_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0x1D)
    {
        dispatch_runtime_scare_figure_message(scare_figures_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0x22)
    {
        dispatch_runtime_super_scary_shock_message(super_scary_shocks_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0x21)
    {
        dispatch_runtime_sss_dropper_message(sss_droppers_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xAA)
    {
        dispatch_runtime_chair_weapon_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA9)
    {
        dispatch_runtime_large_projectile_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA7)
    {
        dispatch_runtime_thrown_projectile_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA0)
    {
        dispatch_runtime_bob_weapon_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA1)
    {
        dispatch_runtime_candle_weapon_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA2)
    {
        dispatch_runtime_container_weapon_message(weapons_.at(object.bytes.get())->context, message);
    }
    else if (object.type == 0xA3 || object.type == 0xA6)
    {
        dispatch_runtime_gun_weapon_message(weapons_.at(object.bytes.get())->context, message); // 0x000CAB40
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
    for (auto& [bytes, setup] : bg_break_setups_)
    {
        output[setup.get()] = &setup->context.actor;
    }
    for (auto& [bytes, setup] : swap_setups_)
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
