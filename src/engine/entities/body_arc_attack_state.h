#pragma once
#include <array>

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
struct ArcAttackState
{
    BodyStateBase base{}; // +0
    u32 target = 0; // +0x18, the mind's target record (actor +0x958 -> +0x14) at entry, or 0
    u32 phase = 0; // +0x1C, 1 take-off, 2 rise, 3 hold, 4 leap, 5 retreat, 0 done
    float spin_rate = 0.f; // +0x20, radians per second about the actor's x axis
    float launch_yaw = 0.f; // +0x24, the leap's direction (0x00105F90)
    std::array<u8, 4> unk_28{}; // +0x28, never read or written by translated code
    float trail_timer = 0.f; // +0x2C
    float trail_scale = 0.f; // +0x30
    u32 retreated = 0; // +0x34
    u32 bounce = 0; // +0x38
    u16 bounce_key = 0; // +0x3C
    std::array<u8, 2> unk_3E{}; // +0x3E, padding
    float speed = 0.f; // +0x40, the parameters', or the actor's reply to message 0x58
    float spin_degrees = 0.f; // +0x44
    float gravity = 0.f; // +0x48
    float landing_gravity = 0.f; // +0x4C
    u32 settled = 0; // +0x50, set once body +0x324 took the landing gravity
    float hold = 0.f; // +0x54, counted down by the hold phase
    float settle = 0.f; // +0x58
    float saved_gravity = 0.f; // +0x5C, body +0x324 at entry, restored by the destructor
    u32 shape_mode = 0; // +0x60, 0x0001FEB0's previous mode, restored by the destructor
    Vec3 start{}; // +0x64, the actor's world position at entry
    u32 zero_gravity = 0; // +0x70
    float take_off_time = 0.f; // +0x74
    u16 shape_tag = 0; // +0x78
    std::array<u8, 2> unk_7A{}; // +0x7A, padding
    float shape_scale = 0.f; // +0x7C
    u32 shapes_active = 0; // +0x80, set while the leap's contact shapes are live
    std::array<char, 0x80> launch_sound{}; // +0x84
    std::array<char, 0x80> flight_sound{}; // +0x104
    float settle_timer = 0.f; // +0x184
    u32 latch = 0; // +0x188
    u32 latched = 0; // +0x18C, set while the leap holds the body latches
    u32 retreat = 0; // +0x190
    u32 reflect = 0; // +0x194
    float retreat_speed = 0.f; // +0x198
    float retreat_spin_degrees = 0.f; // +0x19C
    float retreat_gravity = 0.f; // +0x1A0
    u32 damage_end = 0; // +0x1A4
    u16 end_key = 0; // +0x1A8
    std::array<u8, 2> unk_1AA{}; // +0x1AA, padding
    Vec3 lift_origin{}; // +0x1AC, the actor's world position when the rise starts
    float lift_height = 0.f; // +0x1B8
    float lift_start = 0.f; // +0x1BC
    std::array<char, 0x80> take_off_sound{}; // +0x1C0
    u32 flat = 0; // +0x240
    float height_scale = 0.f; // +0x244
    // +0x248: Five animation records (0x0003AAA0); set when the record was found. 0x00089700 starts phase 1's +0x248,
    // 2's +0x250, 3's +0x24C, 4's +0x254 and 5's +0x258.
    u32 take_off_animation = 0;
    u32 flight_animation = 0; // +0x24C
    u32 lift_animation = 0; // +0x250
    u32 leap_animation = 0; // +0x254
    u32 retreat_animation = 0; // +0x258
    u32 keep_pitch = 0; // +0x25C
    // +0x260: Read by the hurt (0x00089DB0) to price the hit; no dump writes it, so it keeps the factory's zero fill.
    u32 unk_260 = 0;
    u32 land_on_contact = 0; // +0x264
};
static_assert(sizeof(ArcAttackState) == 0x268);
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
bool initialize_runtime_arc_attack_state(ArcAttackState& state, std::span<const u8> parameters,
    RuntimeArcAttackStateContext& context); // Xbox 0x00088A10
void destroy_runtime_arc_attack_state(ArcAttackState& state,
    RuntimeArcAttackStateContext& context); // Xbox 0x00088ED0
// `contact` is message 0x62's +0x1C record (a position, then the surface normal at +0xC), empty for the others.
void message_runtime_arc_attack_state(ArcAttackState& state, std::span<u32> message, std::span<const u8> contact,
    RuntimeArcAttackStateContext& context); // Xbox 0x00089530
void tick_runtime_arc_attack_state(ArcAttackState& state, RuntimeArcAttackStateContext& context); // Xbox 0x00089060
// Type 0x5F (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_arc_attack_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeArcAttackStateContext&(u32 body_address)> context_of);
}
