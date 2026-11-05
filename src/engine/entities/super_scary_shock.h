#pragma once
#include <array>
#include "actor_fields.h"
#include "actor_factory_owner.h"
#include "gbtg/retail_field.h"
#include <functional>

namespace gbtg
{
struct AudioBankStreams;
struct RuntimeAreaDialoguePool;
// entityAvatarMiscSuperScaryShock, factory type 0x22 (getter 0x00072270, vtable 0x00504F40) over the actor (type 2):
// a Super Scary shock waiting at its marker. It starts the "uglygirlturnaround" ditty on its first update, and once
// the player comes within +0x938 (with no dialogue, prompt lock or challenge running) it spawns its ghouly from the
// template +0x9C0 and starts the Super Scary challenge (type 0x98) with its two clips. Message 0x8E plays the ghouly's
// clips (1 and 2), lets the ditty go (4) or ends the shock (7 and 8).
namespace super_scary_shock_parameter
{
constexpr RetailField<float, 0x340> reach{};
constexpr RetailField<float, 0x344> chance{}; // the construction roll must not exceed it
constexpr RetailRecord<0x348, 0x80> ghouly_name{};
constexpr RetailRecord<0x3C8, 0x80> ghouly_template{};
constexpr RetailField<float, 0x448> clip_time{};
constexpr RetailField<float, 0x44C> clip_time_range{};
constexpr RetailRecord<0x450, 0x80> first_clip{};
constexpr RetailRecord<0x4D0, 0x80> attack_animation{};
constexpr RetailRecord<0x550, 0x80> attack_events{};
constexpr RetailRecord<0x5D0, 0x80> second_clip{};
constexpr RetailRecord<0x650, 0x80> end_animation{};
constexpr RetailRecord<0x6D0, 0x80> end_events{};
constexpr RetailField<u32, 0x750> challenge_word{};
constexpr u32 kSize = 0x754;
static_assert(retail_fits(kSize, challenge_word));
} // namespace super_scary_shock_parameter
struct SuperScaryShockState
{
    ActorBase base{}; // +0
    u32 ghouly = 0; // +0x934, the spawned ghouly's address, zero until the player comes near
    float reach = 0.f; // +0x938, parameters +0x340
    float clip_time = 0.f; // +0x93C, drawn for each challenge; -1 when +0xA40 is -1
    std::array<char, 0x80> ghouly_name{}; // +0x940, written to the template's +0x28 and +0x2B4
    std::array<char, 0x80> ghouly_template{}; // +0x9C0
    float clip_time_base = 0.f; // +0xA40
    float clip_time_range = 0.f; // +0xA44
    std::array<char, 0x80> first_clip{}; // +0xA48
    std::array<char, 0x80> attack_animation{}; // +0xAC8, message 0x8E 0
    std::array<char, 0x80> attack_events{}; // +0xB48
    std::array<char, 0x80> second_clip{}; // +0xBC8
    std::array<char, 0x80> end_animation{}; // +0xC48, message 0x8E 2
    std::array<char, 0x80> end_events{}; // +0xCC8
    u32 ditty_started = 0; // +0xD48
    u32 ditty_held = 0; // +0xD4C, the destructor lets the ditty go while set
    u32 challenge_word = 0; // +0xD50, 0x000873A0's +0x14C
};
static_assert(sizeof(SuperScaryShockState) == 0xD54);
struct RuntimeSuperScaryShock
{
    std::span<u8> bytes;
    NativeActorFactoryInstance& view;
    u32& random; // DAT_0054DD7C.
    AudioBankStreams& streams;
    const i32& prompt_lock; // DAT_004E1C34.
    const RuntimeAreaDialoguePool*& dialogue; // 0x0005B830's pool.
    u32& runtime_flags; // DAT_0050EA9C.
    std::function<void(std::span<u32>)> base_message; // 0x0003EF70 over retail message words.
    std::function<const Actor*()> player; // DAT_00511B40 minus 0x93C; null for no player.
    // 0x00107720 clones the named template, the patch is applied, 0x00107520 creates it (the scene keeps it) and
    // 0x000663E0 frees the clone: the new object's address, or 0 with no template.
    std::function<u32(const char*, const std::function<void(std::span<u8>)>&)> create_from_template;
    std::function<void(std::span<const u8>)> create_object; // 0x00107520; the scene keeps the object.
    std::function<void(u32, std::span<u32>)> message; // Vtable +0xC of the object at an address.
    std::function<NativeActorFactoryInstance&(u32)> actor_at; // The actor at an address.
    std::function<void(std::span<const u8>)> queue_event; // 0x00116810.
};
void super_scary_shock_object_sizes(ObjectSizes& sizes); // Xbox 0x00072250
bool initialize_runtime_super_scary_shock(RuntimeSuperScaryShock& context,
    std::span<const u8> parameters); // Xbox 0x000722B0
void destroy_runtime_super_scary_shock(RuntimeSuperScaryShock& context); // Xbox 0x00072420
// `update_actor` is the actor update 0x000420D0, run between the ditty and the watch.
void update_runtime_super_scary_shock(RuntimeSuperScaryShock& context,
    const std::function<void()>& update_actor); // Xbox 0x00072480
void dispatch_runtime_super_scary_shock_message(RuntimeSuperScaryShock& context,
    std::span<u32> message); // Xbox 0x000724C0
// Spawns the ghouly and starts the challenge once the player is near enough.
void watch_runtime_super_scary_shock(RuntimeSuperScaryShock& context); // Xbox 0x00072750, EBX actor
u32 spawn_runtime_super_scary_ghouly(RuntimeSuperScaryShock& context); // Xbox 0x000727F0, EAX actor
void start_runtime_super_scary_challenge(RuntimeSuperScaryShock& context); // Xbox 0x000728C0, EDX actor
// Slot 0 of the ghouly takes `animation` with `events` (blend 0x23); a `time` other than -1 sets slot 0's position
// to 1 - (time + 1/15) / its duration.
void play_runtime_super_scary_ghouly_clip(RuntimeSuperScaryShock& context, u32 ghouly, float time,
    const char* animation, u16 mode, const char* events); // Xbox 0x00072980, EAX events
}
