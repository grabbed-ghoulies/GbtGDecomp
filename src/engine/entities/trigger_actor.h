#pragma once
#include "actor_fields.h"
#include "area_actor.h"
#include "ghouly_requests.h"
#include "ghouly_spawn_record.h"
#include "../scene/scene_marker_handlers.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// The ghouly trigger (factory type 0x23 over the actor; 0x00069600's sizes): it spawns actors near itself on
// messages 0xA7 / 0xA9 / 0xAA, into up to four slots, and can arm a ghoulybox spawn trigger at random.
namespace trigger_parameter
{
constexpr RetailField<u32, 0x334> controller_key{}; // 0 for none
constexpr RetailField<float, 0x338> min_radius{};
constexpr RetailField<float, 0x33C> max_radius{};
constexpr RetailField<u16, 0x340> goal{};
constexpr RetailField<std::array<u8, 0x200>, 0x342> goal_names{};
constexpr RetailField<u32, 0x544> facing{};
constexpr RetailField<u32, 0x548> unk_548{};
constexpr RetailField<std::array<char, 0x80>, 0x54C> box_name{};
constexpr RetailField<std::array<char, 0x80>, 0x5CC> box_attributes{};
constexpr RetailField<float, 0x64C> box_chance{}; // the box is armed when a random fraction is no more than this
constexpr RetailField<float, 0x650> trigger_radius{};
constexpr RetailField<float, 0x654> trigger_chance{};
constexpr RetailField<u32, 0x658> slot_count{};
constexpr u32 kSize = 0x65C;
static_assert(retail_fits(kSize, slot_count));
} // namespace trigger_parameter
struct TriggerState
{
    ActorBytes base{}; // +0
    // +0x934: the spawn distance from the trigger: an annulus (0x00106670) or the navigation spawn's band (0x00111420).
    float min_radius = 0.f;
    float max_radius = 0.f; // +0x938
    u16 goal = 0; // +0x93C, the spawned marker actor's goal
    std::array<u8, 0x200> goal_names{}; // +0x93E, 0x000280D0's copy
    std::array<u8, 2> unk_B3E{}; // +0xB3E, padding
    u32 anchor = 0; // +0xB40, the actor the next marker names (message 0x7D), which places spawns
    u32 box_record = 0; // +0xB44, the loaded ghoulybox's spawn record
    u32 facing = 0; // +0xB48, 1 a spawn faces the trigger, 2 away from it, else the trigger's own
    u32 unk_B4C = 0; // +0xB4C, clear with goal 0: spawns pick a random annulus point
    std::array<char, 0x80> box_name{}; // +0xB50
    std::array<char, 0x80> box_attributes{}; // +0xBD0
    // +0xC50: the ghoulybox spawn trigger (0x0013E980): its radius and chance, cleared once it fires.
    float trigger_radius = 0.f;
    float trigger_chance = 0.f; // +0xC54
    u32 slot_count = 0; // +0xC58
    std::array<u32, 4> slots{}; // +0xC5C, the spawned objects, by address
};
static_assert(sizeof(TriggerState) == 0xC6C);
struct RuntimeTriggerActorContext
{
    std::span<u8> bytes;
    Actor& actor;
    GhoulyController& controller;
    u32& random;
    float occupancy_initial;
    std::function<void(u32, std::span<u32>)> message; // Native actor vtable +0xC mapping.
    GhoulySpawnRecord*& load; // Actor +0xB44, native pointer view.
    std::function<void(std::span<u32>)> self_message; // This actor's actual vtable +0xC.
};
void copy_runtime_trigger_attributes(u8* destination, const u8* source); // Xbox 0x000280D0
void runtime_trigger_storage_sizes(ObjectSizes& sizes); // Xbox 0x00069600
bool initialize_runtime_trigger_actor(RuntimeTriggerActorContext& context,
    std::span<const u8> parameters); // Xbox 0x00069660
bool find_runtime_trigger_spawn_slot(std::span<u8> bytes, i32& slot); // Xbox 0x00069910
i32 query_runtime_trigger_pose(RuntimeTriggerActorContext& context, i32 select,
    Vec3& position, Vec3& euler); // Xbox 0x00069E00
bool query_runtime_trigger_spawn_pose(RuntimeTriggerActorContext& context,
    Vec3& source_position, Vec3& source_euler, Vec3& position,
    Vec3& euler); // Xbox 0x000697D0
void destroy_runtime_trigger_actor(RuntimeTriggerActorContext& context); // Xbox 0x000697A0
void update_runtime_trigger_actor(RuntimeTriggerActorContext& context); // Xbox 0x00069D20
struct RuntimeTriggerMessages
{
    RuntimeTriggerActorContext& trigger;
    ActorBaseContext& base;
    RuntimeMarkerObjects& objects;
    ActorEffectStreamContext& effects;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<u32(const void*)> address_of;
    std::function<Actor&(FactoryObject&)> actor_of;
    std::function<FactoryObject*(FactoryObject)> keep; // Native scene ownership, not a creation callee.
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message;
    // Required pointer-width adapter into the translated 0x0003EF70.
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // The object a DAT_00511AA4 node sits in.
};
void dispatch_runtime_trigger_message(RuntimeTriggerMessages& context,
    std::span<u32> message); // Xbox 0x00069960
}
