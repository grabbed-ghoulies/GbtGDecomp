#pragma once

#include "body_state_actions.h"
#include "actor_clock.h"
#include "actor_attachment_events.h"
#include "actor_trail_requests.h"
#include "body_contacts.h"
#include "body_damage_tables.h"
#include "body_state_fields.h"

namespace gbtg
{
// Factory type 0x5F, entityStateArcAttack (getter 0x000889B0, vtable 0x005056C0): the body crouches through a
// take-off animation, rises by a height while a lift animation plays, spins in the air for a hold time, then leaps on
// a ballistic arc at its mind's target (actor +0x958 -> +0x14) or 40 ahead of itself, with its contact shapes live. A
// contact ends the leap: it hurts the target, bounces off, or lands; message 0x41 makes it hop back toward where it
// started (the spiders of the Lower Corridors). The +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body.
namespace arc_attack_state_parameter
{
constexpr RetailField<float, 0x0C> speed{}; // the leap's launch speed (0x00140D80)
constexpr RetailField<float, 0x10> spin_degrees{}; // per second, while the body rises
constexpr RetailField<float, 0x14> gravity{}; // the leap's gravity, into body +0x324
constexpr RetailField<u32, 0x18> zero_gravity{}; // set: body +0x324 is cleared for the state
constexpr RetailField<float, 0x1C> take_off_time{}; // the take-off animation's position that starts the rise
constexpr RetailField<float, 0x20> lift_start{}; // the lift animation's position the rise starts from
constexpr RetailField<float, 0x24> lift_minimum{}; // the random rise without a target
constexpr RetailField<float, 0x28> lift_maximum{};
constexpr RetailField<std::array<char, 0x80>, 0x2C> take_off_sound{}; // actor sound slot 1 at the rise
constexpr RetailField<u32, 0xAC> flat{}; // set: the leap keeps no vertical speed and the rise scales the target's
constexpr RetailField<float, 0xB0> height_scale{}; // the target record's +0x30 by this: the rise and the aim height
constexpr RetailField<float, 0xB4> hold{}; // seconds spinning in the air before the leap
constexpr RetailField<float, 0xB8> settle{}; // seconds into the leap before body +0x324 takes the landing gravity
constexpr RetailField<float, 0xBC> landing_gravity{};
constexpr RetailField<std::array<char, 0x80>, 0xC0> launch_sound{}; // actor sound slot 1 at the leap
constexpr RetailField<std::array<char, 0x80>, 0x140> flight_sound{}; // actor sound slot 7 at the leap
constexpr RetailField<u32, 0x1C0> retreat{}; // set: message 0x41 hops the body back
constexpr RetailField<u32, 0x1C4> reflect{}; // set: message 0x6E from the target turns the leap round
constexpr RetailField<float, 0x1C8> retreat_speed{};
constexpr RetailField<float, 0x1CC> retreat_spin_degrees{};
constexpr RetailField<float, 0x1D0> retreat_gravity{};
constexpr RetailField<u32, 0x1D4> damage_end{}; // set: the end reads body slot +0x34 after the class's 0x11 value
constexpr RetailField<u16, 0x1D8> end_key{};
constexpr RetailField<u16, 0x1DA> shape_tag{}; // the contact shapes the leap activates; negative for none
constexpr RetailField<float, 0x1DC> shape_scale{};
constexpr RetailField<float, 0x1E0> trail_scale{};
// The five animations' names (XBE addresses): take-off, flight, lift, retreat and leap.
constexpr RetailField<u32, 0x1E4> take_off_animation{};
constexpr RetailField<u32, 0x1E8> flight_animation{};
constexpr RetailField<u32, 0x1EC> lift_animation{};
constexpr RetailField<u32, 0x1F0> leap_animation{};
constexpr RetailField<u32, 0x1F4> retreat_animation{};
constexpr RetailField<u32, 0x1F8> bounce{}; // set: a wall or a reported contact ends the leap (0x00089DB0)
constexpr RetailField<u32, 0x1FC> keep_pitch{}; // set: landing keeps the actor's pitch
constexpr RetailField<u32, 0x208> land_on_contact{}; // set: any contact bit ends the leap as a landing
constexpr RetailField<u16, 0x20C> bounce_key{}; // the key a bounce hands the body; 0 hurts the target instead
constexpr RetailField<u32, 0x210> latch{}; // set: the leap holds all six body latches (0x0001F3F0, mask 0x3F)
constexpr u32 kSize = 0x214;
static_assert(retail_fits(kSize, latch));
} // namespace arc_attack_state_parameter
// The arc attack state's own fields at their retail offsets (0x268 bytes), after the body-state base
// (body_state_field).
namespace arc_attack_state_field
{
constexpr RetailField<u32, 0x18> target{}; // the mind's target record (actor +0x958 -> +0x14) at entry, or 0
constexpr RetailField<u32, 0x1C> phase{}; // 1 take-off, 2 rise, 3 hold, 4 leap, 5 retreat, 0 done
constexpr RetailField<float, 0x20> spin_rate{}; // radians per second about the actor's x axis
constexpr RetailField<float, 0x24> launch_yaw{}; // the leap's direction (0x00105F90)
constexpr RetailField<float, 0x2C> trail_timer{};
constexpr RetailField<float, 0x30> trail_scale{};
constexpr RetailField<u32, 0x34> retreated{};
constexpr RetailField<u32, 0x38> bounce{};
constexpr RetailField<u16, 0x3C> bounce_key{};
constexpr RetailField<float, 0x40> speed{}; // the parameters', or the actor's reply to message 0x58
constexpr RetailField<float, 0x44> spin_degrees{};
constexpr RetailField<float, 0x48> gravity{};
constexpr RetailField<float, 0x4C> landing_gravity{};
constexpr RetailField<u32, 0x50> settled{}; // set once body +0x324 took the landing gravity
constexpr RetailField<float, 0x54> hold{}; // counted down by the hold phase
constexpr RetailField<float, 0x58> settle{};
constexpr RetailField<float, 0x5C> saved_gravity{}; // body +0x324 at entry, restored by the destructor
constexpr RetailField<u32, 0x60> shape_mode{}; // 0x0001FEB0's previous mode, restored by the destructor
constexpr RetailField<Vec3, 0x64> start{}; // the actor's world position at entry
constexpr RetailField<u32, 0x70> zero_gravity{};
constexpr RetailField<float, 0x74> take_off_time{};
constexpr RetailField<u16, 0x78> shape_tag{};
constexpr RetailField<float, 0x7C> shape_scale{};
constexpr RetailField<u32, 0x80> shapes_active{}; // set while the leap's contact shapes are live
constexpr RetailField<std::array<char, 0x80>, 0x84> launch_sound{};
constexpr RetailField<std::array<char, 0x80>, 0x104> flight_sound{};
constexpr RetailField<float, 0x184> settle_timer{};
constexpr RetailField<u32, 0x188> latch{};
constexpr RetailField<u32, 0x18C> latched{}; // set while the leap holds the body latches
constexpr RetailField<u32, 0x190> retreat{};
constexpr RetailField<u32, 0x194> reflect{};
constexpr RetailField<float, 0x198> retreat_speed{};
constexpr RetailField<float, 0x19C> retreat_spin_degrees{};
constexpr RetailField<float, 0x1A0> retreat_gravity{};
constexpr RetailField<u32, 0x1A4> damage_end{};
constexpr RetailField<u16, 0x1A8> end_key{};
constexpr RetailField<Vec3, 0x1AC> lift_origin{}; // the actor's world position when the rise starts
constexpr RetailField<float, 0x1B8> lift_height{};
constexpr RetailField<float, 0x1BC> lift_start{};
constexpr RetailField<std::array<char, 0x80>, 0x1C0> take_off_sound{};
constexpr RetailField<u32, 0x240> flat{};
constexpr RetailField<float, 0x244> height_scale{};
// Five animation records (0x0003AAA0); set when the record was found. 0x00089700 starts phase 1's +0x248, 2's +0x250,
// 3's +0x24C, 4's +0x254 and 5's +0x258.
constexpr RetailField<u32, 0x248> take_off_animation{};
constexpr RetailField<u32, 0x24C> flight_animation{};
constexpr RetailField<u32, 0x250> lift_animation{};
constexpr RetailField<u32, 0x254> leap_animation{};
constexpr RetailField<u32, 0x258> retreat_animation{};
constexpr RetailField<u32, 0x25C> keep_pitch{};
// Read by the hurt (0x00089DB0) to price the hit; no dump writes it, so it keeps the factory's zero fill.
constexpr RetailField<u32, 0x260> unk_260{};
constexpr RetailField<u32, 0x264> land_on_contact{};
constexpr u32 kSize = 0x268;
static_assert(retail_fits(kSize, land_on_contact));
} // namespace arc_attack_state_field
struct RuntimeArcAttackStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    EntityBitBlock& bits; // Body +0x15C.
    RuntimeBodyStateContext& shape; // 0x0001FEB0's views.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    std::span<const u8> mind; // Actor +0x958, or empty.
    std::function<std::span<const u8>(u32 address)> target_at; // A mind +0x14 target record's retail image.
    std::function<Actor*(u32 address)> actor_at; // The target record's +0x10 actor.
    std::function<std::span<const u8>(u32 address)> bytes_at; // That actor's bytes (+0xCE0).
    const RuntimeActorResourceReference& resource; // Actor +0x8C.
    ActorAttachmentEventContext& callouts;
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    RuntimeActorSoundQueues& sounds;
    ActorTrailContext trails;
    const RuntimeBodyDamageTables& damage_tables;
    const u32& player_damage; // DAT_00512200.
    std::span<const u32, 2> player_blocked; // DAT_0050E764.
    std::span<const u8> player_modes; // DAT_0030F144.
    u32& player_mode; // DAT_0051248C.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses; null for 0.
    std::function<void(std::span<u32>)> actor_message; // Actor vtable +0xC; the reply comes back in the words.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20 / +0x34 / +0x4C.
    u32& random; // DAT_0054DD7C.
};
void arc_attack_state_object_sizes(ObjectSizes& output); // Xbox 0x00088990
bool initialize_runtime_arc_attack_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeArcAttackStateContext& context); // Xbox 0x00088A10
void destroy_runtime_arc_attack_state(std::span<u8> state,
    RuntimeArcAttackStateContext& context); // Xbox 0x00088ED0
// `contact` is message 0x62's +0x1C record (a position, then the surface normal at +0xC), empty for the others.
void message_runtime_arc_attack_state(std::span<u8> state, std::span<u32> message, std::span<const u8> contact,
    RuntimeArcAttackStateContext& context); // Xbox 0x00089530
void tick_runtime_arc_attack_state(std::span<u8> state, RuntimeArcAttackStateContext& context); // Xbox 0x00089060
// Type 0x5F (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_arc_attack_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeArcAttackStateContext&(u32 body_address)> context_of);
}
