#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// A damage record (0x68 bytes, built on the stack by the hit messages and handed to 0x00020DC0): what hit the body,
// where and how hard. See docs/RETAIL_LAYOUTS.md.
namespace body_damage_field
{
// How the damage is processed (0x00020DC0's switch): 1 a hit, 2 a reflected hit, 3 a knockdown, among others.
constexpr RetailField<u8, 0x0> kind{};
constexpr RetailField<u8, 0x1> key{}; // the damage table key (0x00022310); 0x3E also starts the hit timer
constexpr RetailField<u8, 0x2> push_key{};
constexpr RetailField<u8, 0x3> joint{};
constexpr RetailField<u32, 0x4> latch_mask{}; // the latches that can block it (0x0001F750)
constexpr RetailField<u32, 0x8> unk_8{}; // set for a knockdown
constexpr RetailField<u32, 0xC> forced{}; // applies even while player damage is off
constexpr RetailField<u32, 0x10> unblockable{}; // set: latches don't block it unless body bit 0x1B is set
constexpr RetailField<u32, 0x14> skip_script{}; // the notice skips the script (0x00024A00): a hit by a non-player
constexpr RetailField<Vec3, 0x18> position{}; // the contact point; its y gates contact blocking (0x00022870)
constexpr RetailField<Vec3, 0x24> normal{};
constexpr RetailField<u32, 0x30> contacts{}; // the contact pool, by address
constexpr RetailField<u32, 0x34> source{}; // the actor that dealt it
constexpr RetailField<u32, 0x38> attacker{}; // the actor the notices name (a thrown weapon's thrower)
constexpr RetailField<u32, 0x3C> holder{}; // the source body's unk_274, or the source
constexpr RetailField<Vec3, 0x40> push{};
constexpr RetailField<Vec3, 0x4C> direction{};
constexpr RetailField<u32, 0x58> starts_timer{}; // the body's hit timer starts (0x00020DC0)
constexpr RetailField<u32, 0x5C> unk_5C{}; // passed on to the notices
constexpr RetailField<u32, 0x60> unk_60{}; // passed on to the notices; 1 from the hit messages
constexpr RetailField<u16, 0x64> source_class{}; // the source's damage class (player_field::damage_class)
constexpr u32 kSize = 0x68;
static_assert(retail_fits(kSize, source_class));
} // namespace body_damage_field
} // namespace gbtg
