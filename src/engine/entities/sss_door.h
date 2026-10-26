#pragma once
#include "actor_clock.h"
#include "actor_factory_owner.h"
#include "gbtg/retail_field.h"
#include <functional>

namespace gbtg
{
struct AnimationResourceRegistry;
struct ResourceErrorState;
struct RuntimeDoorContext;
// entityAvatarMiscSSSDoor, factory type 0x20 (getter 0x000711A0, vtable 0x00504EC8) over the actor (type 2): a Super
// Scary figure behind a door. The construction roll must not exceed the parameters' chance (+0x33C when the scene
// restarts, else +0x338); a reusable one (+0x440) that misses waits out of sight (phase 4), any other miss is not
// made. Waiting (phase 0) at the start of "out", held, it springs once the player is free, within its reach and, with
// a door (+0x444), that door is at rest: the door closes its clips, the actors hear {0x37, its position}, and "out"
// plays (phase 1), turning to face the player with +0x448. When it ends the Super Scary challenge (type 0x98) starts 35
// ahead of it with its two clips and "scare" plays (phase 2) until message 0x8E 7 or 8 sends it back "in" (phase 3);
// then a reusable figure waits out of sight again and any other releases itself. The +0x14 / +0x18 slots are the
// actor base's.
namespace sss_door_parameter
{
constexpr RetailField<float, 0x334> reach{}; // 0 or less: any distance
constexpr RetailField<float, 0x338> chance{};
constexpr RetailField<float, 0x33C> restart_chance{}; // used while DAT_00430D0C is 1
constexpr RetailRecord<0x340, 0x80> first_clip{};
constexpr RetailRecord<0x3C0, 0x80> second_clip{};
constexpr RetailField<u32, 0x440> reusable{};
constexpr RetailField<u32, 0x444> door{};
constexpr RetailField<u32, 0x448> face_player{};
constexpr RetailField<u32, 0x44C> move_player{}; // the challenge's +0x134
constexpr u32 kSize = 0x450;
static_assert(retail_fits(kSize, move_player));
} // namespace sss_door_parameter
namespace sss_door_field
{
constexpr RetailField<float, 0x934> reach{};
constexpr RetailField<u32, 0x93C> phase{}; // 0 waiting, 1 coming out, 2 scaring, 3 going in, 4 out of sight
constexpr RetailField<float, 0x940> timer{};
constexpr RetailRecord<0x944, 0x80> first_clip{};
constexpr RetailRecord<0x9C4, 0x80> second_clip{};
constexpr RetailField<u32, 0xA44> reusable{};
constexpr RetailField<u32, 0xA48> door{};
constexpr RetailField<u32, 0xA4C> face_player{};
constexpr RetailField<u32, 0xA50> move_player{};
constexpr u32 kSize = 0xA54;
static_assert(retail_fits(kSize, move_player));
} // namespace sss_door_field
struct RuntimeSssDoor
{
    std::span<u8> bytes;
    NativeActorFactoryInstance& view;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    const RuntimeActorClock& clock;
    const i32& restarting; // DAT_00430D0C.
    u32& runtime_flags; // DAT_0050EA9C.
    std::function<void(std::span<u32>)> base_message; // 0x0003EF70 over retail message words.
    std::function<const Actor*()> player; // DAT_00511B40 minus 0x93C; null for no player.
    std::function<std::span<const u8>(const Actor&)> player_body; // That player's +0x95C body.
    std::function<u32(const Actor&)> address_of; // An actor's retail address.
    std::function<void(std::span<const u8>)> create_object; // 0x00107520; the scene keeps the object.
    std::function<RuntimeDoorContext&()> doors; // DAT_004EE2B8.
};
void sss_door_object_sizes(ObjectSizes& sizes); // Xbox 0x00071180
bool initialize_runtime_sss_door(RuntimeSssDoor& context, std::span<const u8> parameters); // Xbox 0x000711E0
// `update_actor` is the actor update 0x000420D0 that ends every phase.
void update_runtime_sss_door(RuntimeSssDoor& context, const std::function<void()>& update_actor); // Xbox 0x00071380
void dispatch_runtime_sss_door_message(RuntimeSssDoor& context, std::span<u32> message); // Xbox 0x000715B0
// The player is free (its body still, not invisible, no cutscene hold, decay on), within reach and, with a door, the
// door nearest the figure is at rest.
bool runtime_sss_door_triggered(RuntimeSssDoor& context); // Xbox 0x00071640, EBX actor
void start_runtime_sss_door_challenge(RuntimeSssDoor& context); // Xbox 0x00071750, EAX actor
}
