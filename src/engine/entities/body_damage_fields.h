#pragma once
#include "gbtg/retail_field.h"

#include <cstddef>

namespace gbtg
{
// A damage record (0x68 bytes, built on the stack by the hit messages and handed to 0x00020DC0): what hit the body,
// where and how hard. See docs/RETAIL_LAYOUTS.md.
struct BodyDamage
{
    // How the damage is processed (0x00020DC0's switch): 1 a hit, 2 a reflected hit, 3 a knockdown, among others.
    u8 kind = 0; // +0
    u8 key = 0; // +1, the damage table key (0x00022310); 0x3E also starts the hit timer
    u8 push_key = 0; // +2
    u8 joint = 0; // +3
    u32 latch_mask = 0; // +4, the latches that can block it (0x0001F750)
    u32 unk_8 = 0; // +8, set for a knockdown
    u32 forced = 0; // +0xC, applies even while player damage is off
    u32 unblockable = 0; // +0x10, set: latches don't block it unless body bit 0x1B is set
    u32 skip_script = 0; // +0x14, the notice skips the script (0x00024A00): a hit by a non-player
    Vec3 position{}; // +0x18, the contact point; its y gates contact blocking (0x00022870)
    Vec3 normal{}; // +0x24
    u32 contacts = 0; // +0x30, the contact pool, by address
    u32 source = 0; // +0x34, the actor that dealt it
    u32 attacker = 0; // +0x38, the actor the notices name (a thrown weapon's thrower)
    u32 holder = 0; // +0x3C, the source body's unk_274, or the source
    Vec3 push{}; // +0x40
    Vec3 direction{}; // +0x4C
    u32 starts_timer = 0; // +0x58, the body's hit timer starts (0x00020DC0)
    u32 unk_5C = 0; // +0x5C, passed on to the notices
    u32 unk_60 = 0; // +0x60, passed on to the notices; 1 from the hit messages
    u16 source_class = 0; // +0x64, the source's damage class (player_field::damage_class)
    u16 unk_66 = 0; // +0x66, never read or written
};
static_assert(sizeof(BodyDamage) == 0x68);
// Message 0x1A carries the record's bytes, so its layout must stay the retail one.
static_assert(offsetof(BodyDamage, kind) == 0x0);
static_assert(offsetof(BodyDamage, key) == 0x1);
static_assert(offsetof(BodyDamage, push_key) == 0x2);
static_assert(offsetof(BodyDamage, joint) == 0x3);
static_assert(offsetof(BodyDamage, latch_mask) == 0x4);
static_assert(offsetof(BodyDamage, unk_8) == 0x8);
static_assert(offsetof(BodyDamage, forced) == 0xC);
static_assert(offsetof(BodyDamage, unblockable) == 0x10);
static_assert(offsetof(BodyDamage, skip_script) == 0x14);
static_assert(offsetof(BodyDamage, position) == 0x18);
static_assert(offsetof(BodyDamage, normal) == 0x24);
static_assert(offsetof(BodyDamage, contacts) == 0x30);
static_assert(offsetof(BodyDamage, source) == 0x34);
static_assert(offsetof(BodyDamage, attacker) == 0x38);
static_assert(offsetof(BodyDamage, holder) == 0x3C);
static_assert(offsetof(BodyDamage, push) == 0x40);
static_assert(offsetof(BodyDamage, direction) == 0x4C);
static_assert(offsetof(BodyDamage, starts_timer) == 0x58);
static_assert(offsetof(BodyDamage, unk_5C) == 0x5C);
static_assert(offsetof(BodyDamage, unk_60) == 0x60);
static_assert(offsetof(BodyDamage, source_class) == 0x64);
static_assert(offsetof(BodyDamage, unk_66) == 0x66);
} // namespace gbtg
