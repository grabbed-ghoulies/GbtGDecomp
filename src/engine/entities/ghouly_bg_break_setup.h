#pragma once
#include <array>
#include "actor_fields.h"
#include "trigger_actor.h"
#include "camera_director.h"
#include "ghouly_spawn_scheduler.h"
#include "../scene/background_parts.h"

namespace gbtg
{
// entityAvatarGhoulySetupBgBreak, factory type 0x24 (getter 0x0006A4F0, vtable 0x00504ACC) over the actor (type 2):
// a ghoulybox held behind a background part (scene 4's false fireplace). Message 0x7D finds the background part
// nearest the actor and, when asked, holds it so hits don't break it. With the player inside its radius the update
// breaks the part itself (message 0x13); the part's removal broadcast (0xAB) then loads the ghoulybox, whose spawn
// requests (0xA7, 0xA9, 0xAA) place the ghoulies at the actor. It keeps the actor's draw and visibility slots.
namespace ghouly_bg_break_parameter
{
constexpr RetailField<u32, 0x334> controller_key{}; // set: the setup's ghouly controller is registered under it
constexpr RetailField<std::array<char, 0x80>, 0x338> box{}; // the ghoulybox
constexpr RetailField<std::array<char, 0x80>, 0x3B8> attributes{}; // the spawns' attributes
constexpr RetailField<u16, 0x438> spawn_tag{};
constexpr RetailField<float, 0x43C> box_chance{}; // without a group: the chance the setup keeps its box
constexpr RetailField<float, 0x440> radius{};
constexpr RetailField<float, 0x444> chance{};
constexpr RetailField<u32, 0x448> grouped{}; // set: the box is shared out over a group (0x0006A170)
constexpr RetailField<u32, 0x44C> group{};
constexpr RetailField<u32, 0x450> hold_part{}; // set: message 0x7D holds the found part
constexpr RetailField<u32, 0x454> unk_454{};
constexpr RetailField<u32, 0x458> unk_458{};
constexpr RetailField<u32, 0x45C> spawn_slots{};
constexpr u32 kSize = 0x460;
static_assert(retail_fits(kSize, spawn_slots));
} // namespace ghouly_bg_break_parameter
struct GhoulyBgBreakState
{
    ActorBase base{}; // +0
    std::array<char, 0x80> box{}; // +0x934, empty unless 0x0006A350 / 0x0006A120 keep it
    std::array<char, 0x80> attributes{}; // +0x9B4
    u16 spawn_tag = 0; // +0xA34
    std::array<u8, 2> unk_A36{}; // +0xA36, padding
    float radius = 0.f; // +0xA38, the player radius, then the scare radius (0x0013B660)
    float chance = 0.f; // +0xA3C
    u32 grouped = 0; // +0xA40
    u32 group = 0; // +0xA44, the script records' +0x140 group word
    u32 anchor = 0; // +0xA48, the anchor actor message 0x7D looks up by actor +0x1FE
    u32 load = 0; // +0xA4C, the ghoulybox load (0x000F39A0); its native view is the context's
    u32 broken = 0; // +0xA50, the part is gone or none was found
    u32 background = 0; // +0xA54, the background whose part this setup holds
    i32 part = 0; // +0xA58
    u32 hold_part = 0; // +0xA5C
    u32 unk_A60 = 0; // +0xA60, set: message 0xA8 waits for the part (0xA50)
    u32 unk_A64 = 0; // +0xA64
    u32 spawn_slots = 0; // +0xA68, 0: one unchecked slot
    std::array<u32, 4> spawned{}; // +0xA6C, the spawned actors (message 0x7C clears them)
};
static_assert(sizeof(GhoulyBgBreakState) == 0xA7C);
struct RuntimeGhoulyBgBreakSetup
{
    std::span<u8> bytes;
    Actor& actor;
    GhoulyController& controller;
    u32& random;
    float occupancy_initial;
    GhoulySpawnRecord*& load; // +0xA4C, native pointer view.
    std::function<u32(const void*)> address_of;
    std::function<void(u32, std::span<u32>)> message; // An actor's actual vtable +0xC by address.
    std::function<void(std::span<u32>)> self_message; // This actor's actual vtable +0xC.
    std::function<GhoulySpawnContext&()> spawns; // 0x0013E790's queue state.
    const u8*& records; // DAT_0051108C; 0x0006AF20 and 0x0006A170 write its rows.
    // The +0x934 list (DAT_00511B20 order) for 0x0013B980.
    std::function<std::vector<BackgroundListEntry>()> backgrounds;
    // 0x00045D40 on a background (EDI) and its part (ESI), with the source and value it passes on.
    std::function<void(u32, i32, const Vec3&, u32)> break_part;
    std::function<void(u32, i32)> hold_part; // 0x00046440 on a background (EDI) and its part (EAX).
    std::function<std::vector<RuntimeDirectorActor>()> marks; // DAT_00511B38 nodes, for 0x0013B660.
};
struct RuntimeGhoulyBgBreakSetupMessages
{
    RuntimeGhoulyBgBreakSetup& setup;
    ActorBaseContext& base;
    RuntimeMarkerObjects& objects;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<Actor&(FactoryObject&)> actor_of;
    std::function<FactoryObject*(FactoryObject)> keep; // Native scene ownership, not a creation callee.
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message; // 0x0003EF70.
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // The object a DAT_00511AA4 node sits in.
};
void ghouly_bg_break_setup_object_sizes(ObjectSizes& sizes); // Xbox 0x0006A4D0
bool initialize_runtime_ghouly_bg_break_setup(RuntimeGhoulyBgBreakSetup& context,
    std::span<const u8> parameters); // Xbox 0x0006A530
void destroy_runtime_ghouly_bg_break_setup(RuntimeGhoulyBgBreakSetup& context); // Xbox 0x0006A600
void dispatch_runtime_ghouly_bg_break_setup_message(RuntimeGhoulyBgBreakSetupMessages& context,
    std::span<u32> message); // Xbox 0x0006A7B0
void update_runtime_ghouly_bg_break_setup(RuntimeGhoulyBgBreakSetup& context); // Xbox 0x0006ADC0 after 0x000420D0
}
