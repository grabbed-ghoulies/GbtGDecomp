#pragma once

#include "body_pickup_state.h"
#include "body_hit_state.h"
#include "body_contacts.h"
#include "camera_director.h"
#include "actor_trail_requests.h"
#include "../cutscenes/door_registration.h"
#include "../graphics/actor_effect_stream.h"
#include "../scene/cutscene_scene_runtime.h"

namespace gbtg
{
// Factory type 0x78 (getter 0x00097FB0, vtable 0x005059E0): the body takes the huge weapon its mind chose (mind
// +0x18, the snooker room's bob) and swings it round. It steps to the weapon's nearest pick-up point, attaches it
// (0x00098E00), lifts it to the side the nearby enemies are on (0x00098980), spins once with the weapon's contact
// shapes live, and puts it back where it stood. The +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body.
namespace swing_state_parameter
{
// The seven animations' names (XBE addresses): the grab, the lift and the swing to each side, and the two drops.
constexpr RetailField<u32, 0x0C> grab{};
constexpr RetailField<u32, 0x10> lift_positive{};
constexpr RetailField<u32, 0x14> lift_negative{};
constexpr RetailField<u32, 0x18> swing_positive{};
constexpr RetailField<u32, 0x1C> swing_negative{};
constexpr RetailField<u32, 0x20> drop_positive{};
constexpr RetailField<u32, 0x24> drop_negative{};
constexpr RetailField<std::array<char, 0x80>, 0x28> hit_sound{}; // the sound each hit the weapon reports plays
constexpr RetailField<std::array<char, 0x80>, 0xA8> trail_effect{}; // the effect on each of the weapon's 0x12 shapes
constexpr u32 kSize = 0x128;
static_assert(retail_fits(kSize, trail_effect));
} // namespace swing_state_parameter
// The swing state's own fields at their retail offsets (0x1AC bytes), after the body-state base (body_state_field).
namespace swing_state_field
{
// 1 the grab, 2 / 3 the lift, 4 / 5 the swing, 6 / 7 the drop (the positive side first), 8 done.
constexpr RetailField<u32, 0x18> phase{};
constexpr RetailField<Vec3, 0x1C> weapon_position{}; // the weapon's world position when it was taken
constexpr RetailField<Vec3, 0x28> weapon_euler{}; // and its world euler
constexpr RetailField<float, 0x34> trail_timer{}; // the weapon's trail clock (0x0013B030)
constexpr RetailField<float, 0x38> spin_timer{}; // 1.5 s (DAT_0032B210) counted down by the swing
constexpr RetailField<float, 0x3C> direction{}; // 1 or -1, the spin's sense
constexpr RetailField<Vec3, 0x40> spin_euler{}; // the actor's euler when the swing started; +0x44 is its yaw
constexpr RetailField<Vec3, 0x4C> start_position{}; // the actor's world position at the grab
constexpr RetailField<Vec3, 0x58> start_euler{}; // and its euler
// Four {DAT_00506930 slot, shape} pairs (0x00098F40): the trail effects on the weapon's shapes, -1 when unused.
constexpr RetailField<i32, 0x64> effect_0{};
constexpr RetailField<u32, 0x68> effect_shape_0{};
constexpr RetailField<std::array<char, 0x80>, 0x84> trail_effect{};
// Seven animation records (0x0003AAA0), in the parameters' order; set when the record was found.
constexpr RetailField<u32, 0x104> grab{};
constexpr RetailField<u32, 0x108> lift_positive{};
constexpr RetailField<u32, 0x10C> lift_negative{};
constexpr RetailField<u32, 0x110> swing_positive{};
constexpr RetailField<u32, 0x114> swing_negative{};
constexpr RetailField<u32, 0x118> drop_positive{};
constexpr RetailField<u32, 0x11C> drop_negative{};
constexpr RetailField<u8, 0x120> shape_mode{}; // body +0x244 at entry, restored by 0x0001FEB0 at the end
constexpr RetailField<u32, 0x124> counted{}; // holds a body +0x258 count
constexpr RetailField<std::array<char, 0x80>, 0x128> hit_sound{};
constexpr RetailField<u32, 0x1A8> cutscene{}; // the bob cutscene 0x000993A0 started (its heap address), or 0
constexpr u32 kSize = 0x1AC;
static_assert(retail_fits(kSize, cutscene));
} // namespace swing_state_field
struct RuntimeSwingStateContext
{
    // Body, actor, bits, animation, clock, the attachment views (0x0001B190, 0x0001B4C0), the mind, the weapon views
    // and callouts, the actor and weapon messages, the latches and the defaults.
    RuntimePickupStateContext& pickup;
    RuntimeBodyStateContext& shape; // 0x0001FEB0's views.
    RuntimeBodyNextAction& next; // 0x00134180's views.
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
    std::function<RuntimeBodyCollisionState*&(u32 weapon)> weapon_collision; // Weapon +0x998, or null.
    std::function<std::vector<RuntimeDirectorActor>()> marks; // DAT_00511B38 nodes in list order.
    std::function<ActorTrailContext(u32 actor)> trails; // 0x0013B030's views for the actor at an address.
    ActorEffectStreamContext& effects;
    RuntimeActorSoundQueues& sounds;
    std::function<SceneCutsceneRuntime&()> cutscenes; // DAT_004E1B40..
    AnimationResourceRegistry& resources;
    const char* room; // DAT_00545D40.
    std::span<const DoorRoomAlias, 2> aliases; // PTR_DAT_00434034.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses; null for 0.
    std::function<std::span<const u8>(u32 address, size_t size)> record_at; // Message 0x5A's +0x1C contact pool.
    std::function<Actor*(u32 address)> actor_at; // Message 0x5A's +0x18 actor.
    std::function<u32(const void*)> heap_address; // A cutscene instance's word, as message 0x7F carries it.
    u32& random; // DAT_0054DD7C.
};
void swing_state_object_sizes(ObjectSizes& output); // Xbox 0x00097F90
bool initialize_runtime_swing_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSwingStateContext& context); // Xbox 0x00098010
void destroy_runtime_swing_state(std::span<u8> state, RuntimeSwingStateContext& context); // Xbox 0x00098370
void tick_runtime_swing_state(std::span<u8> state, RuntimeSwingStateContext& context); // Xbox 0x00098460
void message_runtime_swing_state(std::span<u8> state, std::span<const u32> message,
    RuntimeSwingStateContext& context); // Xbox 0x00098620
// Which side of the body's actor `other` stands on: 1 for a turn toward it above zero, otherwise -1; `ahead`, when
// given, says whether the turn is under DAT_0032B4A8.
i32 runtime_actor_side(const Actor& other, const Actor& actor, u32* ahead); // Xbox 0x00026720, EAX body, ECX other
// Each of `from - to`'s angles wrapped into [-pi, pi).
void wrap_runtime_euler_difference(const Vec3& from, const Vec3& to,
    Vec3& output); // Xbox 0x001057F0, ESI from, EDI to, EBX output
// Type 0x78 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_swing_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSwingStateContext&(u32 body_address)> context_of);
}
