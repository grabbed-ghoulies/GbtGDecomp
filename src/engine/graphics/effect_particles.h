#pragma once
#include "actor_effect_nodes.h"
#include "../entities/bounded_channels.h"
#include "gbtg/retail_field.h"
#include <array>

namespace gbtg
{
// One particle of an actor effect: 0x150-byte slots from the effect's +0x390 (0x000EFE30), run by 0x000F18E0. The
// debris (0x000EBDB0) and trail (0x000EB6C0) pools hold records of their own and are not these.
// A particle's oscillation (0x3C bytes at +0xB8; 0x000F1AF0 sets it, 0x00106A80 ticks it): each axis's phase
// advances at its rate and wraps at a turn, and its value is sin(phase) * amplitude + center.
struct EffectOscillation
{
    Vec3 rate{}; // +0, radians per second
    Vec3 phase{}; // +0xC
    Vec3 center{}; // +0x18
    Vec3 amplitude{}; // +0x24
    Vec3 value{}; // +0x30, the draws scale the particle's size by it
};
static_assert(sizeof(EffectOscillation) == 0x3C);

struct EffectParticle
{
    Vec3 position{}; // +0
    Vec3 euler{}; // +0xC
    Vec3 velocity{}; // +0x18
    Vec3 spin{}; // +0x24, euler per second; a bounce may redraw it (0x000ECC30)
    std::array<u32, 3> unk_30{}; // +0x30, cleared at birth (0x000EF8C0) and never read
    float collision_radius = 0.f; // +0x3C, times size, the world probe's radius (0x000ECBD0)
    float bounce = 0.f; // +0x40, the velocity's factor after a bounce
    float spin_bounce = 0.f; // +0x44, the spin's factor after a bounce
    float size = 0.f; // +0x48, eases toward target_size at size_rate
    float gravity = 0.f; // +0x4C, added to velocity.y each second
    float drag = 0.f; // +0x50
    // +0x54: a stopped particle no longer moves (0x000ECE50). Set by a world hit while unk_55 is clear
    // (0x000ECBD0) or by leaving the effect's bounds.
    u8 stopped = 0;
    u8 unk_55 = 0; // +0x55, 1 at birth; 0x000ECE50 raises it and a world hit clears it
    // +0x56: 1 probes the world, 4 redraws the spin on a bounce, 0x20 the probe's mode, 2 / 8 face the velocity;
    // 0x000ECE50 also tests 0x10.
    u8 motion_flags = 0;
    u8 unk_57 = 0; // +0x57, cleared at birth and never read
    // +0x58: 4 runs the oscillation, 8 the bounded channels. The channel setup writes its low byte, the draws read
    // it as a word.
    u16 channel_flags = 0;
    u16 next_free = 0; // +0x5A, the next free slot; 0xFFFE while live, 0xFFFD detached
    u32 collisions = 0; // +0x5C
    std::array<u8, 0x3C> colour_channels{}; // +0x60, effect_channel_field: alpha and colour, approached each tick
    float size_rate = 0.f; // +0x9C
    float target_size = 0.f; // +0xA0
    float size_scale = 0.f; // +0xA4, the emitter's size with its variance drawn
    float life = 0.f; // +0xA8, seconds left, or -1 for no end
    // +0xAC: the last duration range a colour command drew from, and the duration it drew.
    float duration_low = 0.f;
    float duration_high = 0.f; // +0xB0
    float duration = 0.f; // +0xB4
    EffectOscillation oscillation{}; // +0xB8
    // +0xF4: the animation frames, first (lower) to last (upper); the mode's bit 1 counts up, bit 2 down.
    BoundedChannel frames{};
    float unk_108 = 0.f; // +0x108, counts down to zero; a command condition tests it
    std::array<BoundedChannel, 3> bounded_channels{}; // +0x10C, added to the euler by the draws
    u32 owner = 0; // +0x148, the owning effect's address; the draws read the effect's scale through it
    u32 command = 0; // +0x14C, the command the particle runs next, by address
};
static_assert(sizeof(EffectParticle) == 0x150);

// Particle `index` of an effect.
EffectParticle& actor_effect_particle(ActorEffectNode& effect, size_t index);
u32 actor_effect_emission_count(ActorEffectNode& effect, float step); // Xbox 0x000EF1D0
void initialize_actor_effect_particle(EffectParticle& particle, const ActorEffectNode& effect,
    u32& random); // Xbox 0x000EF8C0
void initialize_actor_effect_particle_frames(EffectParticle& particle, const ActorEffectNode& effect,
    u32& random, i32 pal, i32 pal_override); // Xbox 0x000EFB20
EffectParticle* allocate_actor_effect_particle(ActorEffectNode& effect, u32 owner_address, u32 user,
    u32& random, i32 pal, i32 pal_override); // Xbox 0x000EFE30
}
