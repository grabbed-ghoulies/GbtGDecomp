#pragma once
#include "actor_clock.h"
#include "actor_base_messages.h"
#include "player_state.h"
#include "../scene/background_parts.h"

namespace gbtg
{
struct AudioEventVoices;
namespace entity_detail
{
struct DisplayView;
struct LatchNode;
}

// entityAvatarMiscBgBreakKey, factory type 0x1E (getter 0x0006CC70, vtable 0x00504BF8) over the actor (type 2):
// a key hidden in a background part. Message 0x7D finds the part nearest the actor and 0x81 (an event's start, with
// a kind 1 step) arms it. When the armed part breaks (0xAB), every actor pauses and the key
// (aid_model_ghoulies_misc_eventkey) jumps out of the class 0x47 actor, bounces three times under the "breakkeycam"
// view and spins; when its 2 s are up the event 0xAE is queued and the actors resume.
namespace bg_break_key_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x334> unk_334{};
constexpr u32 kSize = 0x3B4;
static_assert(retail_fits(kSize, unk_334));
} // namespace bg_break_key_parameter
namespace bg_break_key_field
{
constexpr RetailField<std::array<char, 0x80>, 0x934> unk_934{};
constexpr RetailField<u32, 0x9B4> background{}; // the background whose part hides the key
constexpr RetailField<i32, 0x9B8> part{};
constexpr RetailField<u32, 0x9BC> armed{}; // an event's start (0x81) had a kind 1 step
constexpr RetailField<u32, 0x9C0> active{}; // the key is out
constexpr RetailField<Vec3, 0x9C4> launch_euler{}; // the class 0x47 actor's euler
constexpr RetailField<Vec3, 0x9D0> position{}; // the key's position
constexpr RetailField<Vec3, 0x9DC> eye{}; // the display view's position when the key came out
constexpr RetailField<Vec3, 0x9E8> velocity{};
constexpr RetailField<float, 0x9F4> timer{};
constexpr RetailField<u32, 0x9F8> key{}; // the key actor; its native view is the context's
// +0x9FC..+0xA5B is the "breakkeycam" view (EntityCamNode); its native view is the context's.
constexpr RetailField<float, 0xA5C> spin{};
constexpr RetailField<i32, 0xA60> bounces{};
constexpr RetailField<float, 0xA64> music_delay{};
constexpr RetailField<u32, 0xA68> music_started{};
constexpr RetailField<u32, 0xA6C> camera_linked{};
constexpr u32 kSize = 0xA70;
static_assert(retail_fits(kSize, camera_linked));
} // namespace bg_break_key_field
struct RuntimeBgBreakKey
{
    std::span<u8> bytes;
    Actor& actor;
    EntityCamNode& camera; // +0x9FC, native view.
    FactoryObject*& key; // +0x9F8, native pointer view.
    ActorBaseContext& base;
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message; // 0x0003EF70.
    const RuntimeActorClock& clock;
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    AudioEventVoices& voices; // DAT_004E0CE8, for 0x000395E0.
    const char* empty_name; // DAT_00546208, 0x000411A0's default names.
    std::function<u32(const void*)> address_of;
    std::function<std::span<const u8>(u32)> memory_at; // Message 0x81's step rows by address.
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // The object a DAT_00511AA4 node sits in.
    std::function<Actor&(u32)> actor_at; // An actor by retail address.
    std::function<Actor&(FactoryObject&)> actor_of;
    std::function<FactoryObject*(std::span<const u8>)> create; // 0x00107520, kept by the scene.
    std::function<void(FactoryObject&, i32)> callout; // 0x0003C040 on an actor's own resource.
    std::function<std::vector<BackgroundListEntry>()> backgrounds; // DAT_00511B20 order, for 0x0013B980.
    std::function<RuntimeActorPauseGroups&()> pause; // 0x00117400.
    std::function<void(std::span<const u8>)> queue_event; // 0x00116810.
    // A scene object by retail address: the update finds the key again from +0x9F8, since the scene's object list
    // may have moved it since 0x00107520 made it.
    std::function<FactoryObject*(u32)> object_at{};
};
void bg_break_key_object_sizes(ObjectSizes& sizes); // Xbox 0x0006CC50
bool initialize_runtime_bg_break_key(RuntimeBgBreakKey& context, std::span<const u8> parameters); // Xbox 0x0006CCB0
void destroy_runtime_bg_break_key(RuntimeBgBreakKey& context); // Xbox 0x0006CD10
void dispatch_runtime_bg_break_key_message(RuntimeBgBreakKey& context, std::span<u32> message); // Xbox 0x0006CD40
void update_runtime_bg_break_key(RuntimeBgBreakKey& context); // Xbox 0x0006CFD0 after 0x000420D0
}
