#pragma once
#include "gbtg/retail_field.h"
#include "actor_fields.h"
#include <span>

#include <array>

namespace gbtg
{
// The player (factory type 5 over the actor, type 2; 0x00027F40's size 0xCF8): its own part after the actor's 0x934
// bytes. Every character's actor (a body's +8, a mind's +8) is one, so the body, mind and strategy code read it
// through this. A weapon (type 0xB) also follows the actor but has its own fields at the same offsets
// (`WeaponBase`). See docs/RETAIL_LAYOUTS.md.
struct PlayerRecord
{
    ActorBase base{}; // +0
    std::array<u8, 8> unk_934{}; // +0x934
    // +0x93C: the actor's node on the case list (DAT_00511B3C / DAT_00511B40), nonzero for a player (native: mask_flag).
    u32 case_link = 0;
    u32 unk_940 = 0; // +0x940
    std::array<u8, 0x10> unk_944{}; // +0x944
    u32 unk_954 = 0; // +0x954
    u32 mind = 0; // +0x958
    u32 body = 0; // +0x95C
    u32 input_device = 0; // +0x960, the pad that drives the actor, or 0
    u32 unk_964 = 0; // +0x964
    std::array<u8, 1> unk_968{}; // +0x968
    u8 provoked = 0; // +0x969, with countdown_56 running, the actor counts as provoked
    std::array<u8, 2> unk_96A{}; // +0x96A, padding
    u8 unk_96C = 0; // +0x96C
    u8 unk_96D = 0; // +0x96D
    u8 unk_96E = 0; // +0x96E
    u8 unk_96F = 0; // +0x96F
    u8 unk_970 = 0; // +0x970
    std::array<u8, 3> unk_971{}; // +0x971, padding
    float countdown_55 = 0.f; // +0x974, runs while decay runs; sends message 0x55 at zero (0x00019520)
    float countdown_56 = 0.f; // +0x978, runs while decay runs; sends message 0x56 at zero (0x00019520)
    u16 hostile_class_0 = 0; // +0x97C
    u16 hostile_class_1 = 0; // +0x97E
    u32 unk_980 = 0; // +0x980
    u32 unk_984 = 0; // +0x984, set by 0x00139010, cleared by the goal cleanup (0x00028240)
    u8 unk_988 = 0; // +0x988
    u8 unk_989 = 0; // +0x989
    u8 unk_98A = 0; // +0x98A
    u8 unk_98B = 0; // +0x98B
    u32 goal_volume = 0; // +0x98C, an occupancy record, by address
    u16 goal_latch = 0; // +0x990
    std::array<u8, 2> unk_992{}; // +0x992, padding
    float goal_delay = 0.f; // +0x994, counted down before the next goal (0x000285B0)
    std::array<char, 0x200> goal_names{}; // +0x998, the goal's effect and sound names; +0x180 is the sound
    u32 spawn_record = 0; // +0xB98, the ghouly spawn record the body takes ghoulies from
    u16 ghouly_kind = 0; // +0xB9C, 0 a template clone, 1 or 2 0x00112C50
    u16 ghouly_count = 0; // +0xB9E, ghoulies made and not yet gone (0x00093BC0 / 0x00093DF0)
    std::array<char, 0x80> ghouly_box{}; // +0xBA0, the box's AID, or empty
    std::array<char, 0x80> ghouly_attributes{}; // +0xC20
    u32 unk_CA0 = 0; // +0xCA0
    std::array<u8, 8> unk_CA4{}; // +0xCA4
    Vec3 saved_position{}; // +0xCAC, stored by 0x0002B450 / 0x0002B540
    u32 left_room = 0; // +0xCB8, set once the player stands in a room other than the allowed one (0x0002B450)
    std::array<u8, 4> unk_CBC{}; // +0xCBC
    Vec3 start_position{}; // +0xCC0, the constructor's world position; revival returns there
    Vec3 start_euler{}; // +0xCCC
    u32 pause_count = 0; // +0xCD8, messages 9 and 10 raise and drop it (native: parked_gate)
    u16 mode = 0; // +0xCDC, parameters +0x334; 2 is a controlled player (native: mask_mode)
    u16 owner_class = 0; // +0xCDE, parameters +0x336; the class target requests filter on (0x0001D180)
    u32 live = 0; // +0xCE0
    u32 unk_CE4 = 0; // +0xCE4
    u32 cells = 0; // +0xCE8, the navigation cell registration
    u32 floor = 0; // +0xCEC, the navigation floor
    u32 outer_cells = 0; // +0xCF0, registers the outer navigation cells too (0x001104E0)
    u16 damage_class = 0; // +0xCF4, the tag's row (native: damage_class)
    u16 tag = 0; // +0xCF6, parameters +0x338
};
static_assert(sizeof(PlayerRecord) == 0xCF8);

// An actor's bytes viewed as the player's record. Every character's actor is one.
inline PlayerRecord& player_record(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(PlayerRecord));
    return *reinterpret_cast<PlayerRecord*>(bytes.data());
}

inline const PlayerRecord& player_record(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(PlayerRecord));
    return *reinterpret_cast<const PlayerRecord*>(bytes.data());
}
} // namespace gbtg
