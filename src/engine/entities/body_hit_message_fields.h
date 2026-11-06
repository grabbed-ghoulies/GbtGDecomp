#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The hit messages a body receives (0x00074080): an actor message's words, with its id at +0. Their senders build them
// at their own lengths, and only some of the handlers' reads prove a length.

// Message 0xF, a timer contact (0x00022A40).
namespace timer_contact_message
{
constexpr RetailField<Vec3, 0x08> push{};
constexpr RetailField<Vec3, 0x14> position{};
} // namespace timer_contact_message

// Messages 0x16 (a health hit, 0x000233A0) and 0x17 (a knockdown, 0x00023580) from a source contact.
namespace contact_hit_message
{
constexpr RetailField<u32, 0x04> result{}; // written
constexpr RetailField<u32, 0x08> source{};
constexpr RetailField<Vec3, 0x10> position{};
} // namespace contact_hit_message

// Message 0x18, a projectile's contact (0x00022CF0).
namespace projectile_contact_message
{
constexpr RetailField<u8, 0x04> kind{};
constexpr RetailField<u8, 0x08> key{};
constexpr RetailField<u8, 0x0C> push_key{};
constexpr RetailField<u32, 0x10> result{}; // written
constexpr RetailField<u32, 0x14> source{};
constexpr RetailField<Vec3, 0x28> position{};
} // namespace projectile_contact_message

// Message 0x19, an impact (0x00022E40, 0x00022FB0; sent as RuntimeProjectileHitNotice by 0x00141A30). The handler answers
// in +0x10..+0x18.
namespace impact_message
{
constexpr RetailField<u8, 0x04> kind{}; // the sender's weapon's hit kind, strength and style
constexpr RetailField<u8, 0x08> key{};
constexpr RetailField<u8, 0x0C> push_key{};
constexpr RetailField<u32, 0x10> handled{}; // written
constexpr RetailField<u32, 0x14> result{}; // written, the damage's third result word
constexpr RetailField<u32, 0x18> standing{}; // written, set when that word is zero
constexpr RetailField<u32, 0x1C> source{};
constexpr RetailField<Vec3, 0x20> velocity{};
constexpr RetailField<Vec3, 0x30> position{};
constexpr RetailField<u32, 0x3C> no_reply{}; // set: 0x00020F80 isn't called
} // namespace impact_message

// Message 0x1B (0x000231C0).
namespace thrown_hit_message
{
constexpr RetailField<u32, 0x04> result{}; // written
constexpr RetailField<u32, 0x08> source{};
constexpr RetailField<u32, 0x0C> starts_timer{};
constexpr RetailField<Vec3, 0x14> position{};
} // namespace thrown_hit_message

// Message 0x1C (0x00023740): energy lost, and which of two sets of animations to choose from.
namespace health_notice_message
{
constexpr RetailField<u32, 0x04> handled{}; // written
constexpr RetailField<u32, 0x08> amount{};
constexpr RetailField<u32, 0x0C> kind{}; // 0..3, the animation
constexpr RetailField<u32, 0x10> second_set{};
} // namespace health_notice_message
} // namespace gbtg
