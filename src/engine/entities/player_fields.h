#pragma once
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// The player (factory type 5 over the actor, type 2; 0x00027F40's size 0xCF8): its own part after the actor's 0x934
// bytes. Every character's actor (a body's +8, a mind's +8) is one, so the body, mind and strategy code read it
// through these. A weapon (type 0xB) also follows the actor but has its own fields at the same offsets
// (`weapon_field`). See docs/RETAIL_LAYOUTS.md.
namespace player_field
{
// The actor's node on the case list (DAT_00511B3C / DAT_00511B40), nonzero for a player (native: mask_flag).
constexpr RetailField<u32, 0x93C> case_link{};
constexpr RetailField<u32, 0x958> mind{};
constexpr RetailField<u32, 0x95C> body{};
constexpr RetailField<u32, 0x960> input_device{}; // the pad that drives the actor, or 0
constexpr RetailField<u8, 0x969> provoked{}; // with countdown_56 running, the actor counts as provoked
// Countdowns that run while decay runs; each sends its message (0x55, 0x56) at zero (0x00019520).
constexpr RetailField<float, 0x974> countdown_55{};
constexpr RetailField<float, 0x978> countdown_56{};
constexpr RetailField<u32, 0x984> unk_984{}; // set by 0x00139010, cleared by the goal cleanup (0x00028240)
constexpr RetailField<u32, 0x98C> goal_volume{}; // an occupancy record, by address
constexpr RetailField<u16, 0x990> goal_latch{};
constexpr RetailField<float, 0x994> goal_delay{}; // counted down before the next goal (0x000285B0)
constexpr RetailField<u32, 0xB98> spawn_record{}; // the ghouly spawn record the body takes ghoulies from
constexpr RetailField<u16, 0xB9C> ghouly_kind{}; // 0 a template clone, 1 or 2 0x00112C50
constexpr RetailField<u16, 0xB9E> ghouly_count{}; // ghoulies made and not yet gone (0x00093BC0 / 0x00093DF0)
constexpr RetailField<std::array<char, 0x80>, 0xBA0> ghouly_box{}; // the box's AID, or empty
constexpr RetailField<Vec3, 0xCAC> saved_position{}; // stored by 0x0002B450 / 0x0002B540
constexpr RetailField<Vec3, 0xCC0> start_position{}; // the constructor's world position; revival returns there
constexpr RetailField<Vec3, 0xCCC> start_euler{};
constexpr RetailField<u32, 0xCD8> pause_count{}; // messages 9 and 10 raise and drop it (native: parked_gate)
constexpr RetailField<u16, 0xCDC> mode{}; // parameters +0x334; 2 is a controlled player (native: mask_mode)
constexpr RetailField<u16, 0xCDE> owner_class{}; // parameters +0x336; the class target requests filter on (0x0001D180)
constexpr RetailField<u32, 0xCE0> live{};
constexpr RetailField<u32, 0xCE8> cells{}; // the navigation cell registration
constexpr RetailField<u32, 0xCEC> floor{}; // the navigation floor
constexpr RetailField<u16, 0xCF4> damage_class{}; // the tag's row (native: damage_class)
constexpr RetailField<u16, 0xCF6> tag{}; // parameters +0x338
constexpr u32 kSize = 0xCF8;
static_assert(retail_fits(kSize, tag));
} // namespace player_field
} // namespace gbtg
