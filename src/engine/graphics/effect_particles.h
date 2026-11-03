#pragma once
#include "actor_effect_nodes.h"
#include "gbtg/retail_field.h"

namespace gbtg
{
// One particle of an actor effect: 0x150-byte slots from the effect's +0x390 (0x000EFE30), run by 0x000F18E0. The
// debris (0x000EBDB0) and trail (0x000EB6C0) pools hold records of their own and are not these.
namespace effect_particle_field
{
constexpr RetailField<Vec3, 0x00> position{};
constexpr RetailField<Vec3, 0x0C> euler{};
constexpr RetailField<Vec3, 0x18> velocity{};
constexpr RetailField<Vec3, 0x24> spin{}; // euler per second; a bounce may redraw it (0x000ECC30)
constexpr RetailField<float, 0x3C> collision_radius{}; // times size, the world probe's radius (0x000ECBD0)
constexpr RetailField<float, 0x40> bounce{}; // the velocity's factor after a bounce
constexpr RetailField<float, 0x44> spin_bounce{}; // the spin's factor after a bounce
constexpr RetailField<float, 0x48> size{}; // eases toward target_size at size_rate
constexpr RetailField<float, 0x4C> gravity{}; // added to velocity.y each second
constexpr RetailField<float, 0x50> drag{};
constexpr RetailField<u8, 0x54> unk_54{}; // set by a world hit while unk_55 is clear (0x000ECBD0)
constexpr RetailField<u8, 0x55> unk_55{}; // 1 at birth; 0x000ECE50 raises it and a world hit clears it
// 1 probes the world, 4 redraws the spin on a bounce, 0x20 the probe's mode, 2 / 8 face the velocity; 0x000ECE50
// also tests 0x10.
constexpr RetailField<u8, 0x56> motion_flags{};
constexpr RetailField<u8, 0x58> channel_flags{}; // 4 runs the oscillation, 8 the bounded channels
constexpr RetailField<u16, 0x5A> next_free{}; // the next free slot; 0xFFFE while live, 0xFFFD detached
constexpr RetailField<u32, 0x5C> collisions{};
constexpr RetailRecord<0x60, 0x3C> colour_channels{}; // alpha and colour, approached each tick
constexpr RetailField<float, 0x9C> size_rate{};
constexpr RetailField<float, 0xA0> target_size{};
constexpr RetailField<float, 0xA4> size_scale{}; // the emitter's size with its variance drawn
constexpr RetailField<float, 0xA8> life{}; // seconds left, or -1 for no end
// The last duration range a colour command drew from, and the duration it drew.
constexpr RetailField<float, 0xAC> duration_low{};
constexpr RetailField<float, 0xB0> duration_high{};
constexpr RetailField<float, 0xB4> duration{};
constexpr RetailRecord<0xB8, 0x3C> oscillation{};
// The animation frames: first and last, the speed, the frame now and its mode (bit 1 counts up, bit 2 down).
constexpr RetailRecord<0xF4, 0x14> frames{};
constexpr RetailField<float, 0xF4> first_frame{};
constexpr RetailField<float, 0xF8> last_frame{};
constexpr RetailField<float, 0xFC> frame_speed{};
constexpr RetailField<float, 0x100> frame{};
constexpr RetailField<u8, 0x104> frame_mode{};
constexpr RetailField<float, 0x108> unk_108{}; // counts down to zero; a command condition tests it
constexpr RetailRecord<0x10C, 0x3C> bounded_channels{}; // three 0x14-byte channels
constexpr RetailField<u32, 0x148> owner{}; // the owning actor's address
constexpr RetailField<u32, 0x14C> command{}; // the command the particle runs next, by address
constexpr u32 kSize = 0x150;
static_assert(retail_fits(kSize, command));
} // namespace effect_particle_field
u32 actor_effect_emission_count(ActorEffectNode& effect, float step); // Xbox 0x000EF1D0
void initialize_actor_effect_particle(std::span<u8> particle, const ActorEffectNode& effect,
    u32& random); // Xbox 0x000EF8C0
void initialize_actor_effect_particle_frames(std::span<u8> particle, const ActorEffectNode& effect,
    u32& random, i32 pal, i32 pal_override); // Xbox 0x000EFB20
u8* allocate_actor_effect_particle(ActorEffectNode& effect, u32 owner_address, u32 user,
    u32& random, i32 pal, i32 pal_override); // Xbox 0x000EFE30
}
