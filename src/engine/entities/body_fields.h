#pragma once
#include "gbtg/retail_field.h"

#include <array>
#include <span>

namespace gbtg
{
// The body record (actor +0x95C, built by 0x00024C50, the base's 0x894 bytes, 0x0001DA30) at its retail
// offsets. See docs/RETAIL_LAYOUTS.md. +8 is the body's actor.
// The parent player body's parameters' own part (0x00073450, 0x000734F0).
namespace player_body_parameter
{
constexpr RetailField<float, 0x7C8> collision_radius{}; // zero holds the body
constexpr RetailField<float, 0x7CC> collision_height{};
constexpr RetailRecord<0x7D0, 0xC, 6> joint_rows{}; // player_joint_row
constexpr RetailField<float, 0x818> joint_max_speed{};
constexpr RetailField<float, 0x81C> joint_acceleration{};
} // namespace player_body_parameter
// One tracking joint's row (0x00073A90).
namespace player_joint_row
{
constexpr RetailField<i32, 0x0> joint{}; // -1 for none
constexpr RetailField<float, 0x4> pitch_limit{}; // -1 leaves the axis alone
constexpr RetailField<float, 0x8> yaw_limit{}; // -1 leaves the axis alone
} // namespace player_joint_row

// A value pulsing about its base on a sine (0x00104DF0): the base plus up to amplitude of itself, for `duration` seconds at a time,
// `rate` times a second.
struct BodyOscillator
{
    float value = 0.f; // +0
    float base = 0.f; // +4
    float amplitude = 0.f; // +8
    float time_left = 0.f; // +0xC
    float duration = 0.f; // +0x10
    float restart_timer = 0.f; // +0x14
    float rate = 0.f; // +0x18, per second
};
static_assert(sizeof(BodyOscillator) == 0x1C);

struct BodyRecord
{
    u32 vtable = 0; // +0x0, the type's vtable address
    u16 type = 0; // +0x4, the factory type
    u16 unk_6 = 0; // +0x6
    u32 actor = 0; // +0x8, the actor the body moves, by address
    u32 unk_0C = 0; // +0xC
    u32 first_receiver = 0; // +0x10, by address; action keys go to it (0x00024B20)
    u32 second_receiver = 0; // +0x14, by address
    // +0x18: the body's world position: 0x000412A0 copies the actor's into it.
    Vec3 position{};
    Vec3 unk_24{}; // +0x24, the actor's position after its spheres are pushed (0x0002C2B0)
    // +0x30: raised while something holds the body still (states raise it with the +0x309..+0x30C counters and drop it
    // when they end). With it clear and move_hold_time at zero nothing moves the body (0x00024C20).
    u16 move_holds = 0;
    std::array<u8, 2> unk_32{}; // +0x32, padding
    // +0x34: seconds the body is held still: decays to zero (0x00025AE0), and a random hold only ever lengthens it
    // (0x0001AE50).
    float move_hold_time = 0.f;
    u8 unk_38 = 0; // +0x38, states set it and unk_39 together
    u8 unk_39 = 0; // +0x39
    std::array<u8, 2> unk_3A{}; // +0x3A, padding
    // +0x3C: the point the body faces (0x000197B0's heading runs from the actor to it): a target's position
    // (0x000412A0).
    Vec3 focus_point{};
    u32 attached = 0; // +0x48, the actor the body holds or carries (a held weapon, the Baron's cane), by address
    u32 unk_4C = 0; // +0x4C, an attached object, by address (the throw state's held object)
    u32 unk_50 = 0; // +0x50, by address; a message naming it clears it and unk_54
    u32 unk_54 = 0; // +0x54
    std::array<u8, 0x11> unk_58{}; // +0x58
    u8 unk_69 = 0; // +0x69, a counter states raise and drop
    u8 unk_6A = 0; // +0x6A, a counter states raise and drop; the camera director reads it
    u8 unk_6B = 0; // +0x6B, a counter states raise and drop
    u8 unk_6C = 0; // +0x6C
    u8 unk_6D = 0; // +0x6D, a counter states raise and drop
    u16 tag = 0; // +0x6E, parameters +0x1A4
    // +0x70: decays to zero (0x00025AE0); message 0x4E sets it (0x000253B0), and a target request waits while it is
    // positive (0x0001D180).
    float unk_70 = 0.f;
    std::array<u8, 4> unk_74{}; // +0x74
    float reach = 0.f; // +0x78, parameters +0x198, scaled by the actor's +0x38 radius (0x0001D3D0, 0x000ADCC0)
    float throw_lead = 0.f; // +0x7C, parameters +0x19C
    float unk_80 = 0.f; // +0x80, parameters +0x1A0; a thrown projectile's ballistic argument (0x00140D80)
    u32 hit_record = 0; // +0x84, the current hit record (its +0x18 is the hit's turn window), by address
    u32 unk_88 = 0; // +0x88
    float channel_timer = 0.f; // +0x8C
    u8 channel_keys = 0; // +0x90, picks the channel's key set
    u8 channel_key = 0; // +0x91, the key within the set, 12 bytes each
    u8 unk_92 = 0; // +0x92
    std::array<u8, 1> unk_93{}; // +0x93, padding
    u16 energy = 0; // +0x94
    u16 max_energy = 0; // +0x96, restored into energy on revival (0x000253B0)
    std::array<u8, 0xC> unk_98{}; // +0x98
    u8 overlay_state = 0; // +0xA4, the player overlay (0x0001DD00): 0 off, 1 fading in, 2 on, 3 fading out
    u8 overlay_alpha = 0; // +0xA5
    u8 overlay_enabled = 0; // +0xA6, parameters +0x1A8
    u8 overlay_held_off = 0; // +0xA7, raised while looking for a weapon: low energy doesn't call for it
    u8 overlay_flash = 0; // +0xA8, toggled every 0.1 s
    u8 overlay_second_flash = 0; // +0xA9, toggled every 0.75 s while overlay_second_flashes is set
    u8 overlay_second_flashes = 0; // +0xAA
    u8 unk_AB = 0; // +0xAB
    float overlay_flash_timer = 0.f; // +0xAC
    float overlay_second_flash_timer = 0.f; // +0xB0
    float overlay_hold_time = 0.f; // +0xB4, 2 after a furnace hit; it stays on while this runs (0x00096710)
    float overlay_fade_time = 0.f; // +0xB8, 0.4 s in, 0.5 s out
    BodyOscillator overlay_pulse{}; // +0xBC, the overlay's size
    std::array<u8, 4> unk_D8{}; // +0xD8
    float unk_DC = 0.f; // +0xDC
    u32 unk_E0 = 0; // +0xE0
    float aux_time_0 = 0.f; // +0xE4, the first of two rows, 12 bytes each
    u32 aux_value_0 = 0; // +0xE8
    u8 aux_level_0 = 0; // +0xEC
    std::array<u8, 3> unk_ED{}; // +0xED, padding
    float aux_time_1 = 0.f; // +0xF0
    u32 aux_value_1 = 0; // +0xF4
    u8 aux_level_1 = 0; // +0xF8
    std::array<u8, 3> unk_F9{}; // +0xF9, padding
    u32 unk_FC = 0; // +0xFC
    std::array<u8, 0x54> unk_100{}; // +0x100
    u32 hash_count = 0; // +0x154
    u32 alert_level = 0; // +0x158, the mind's alert, as 0x0002A650 sets it
    u32 bits_word = 0; // +0x15C, the bit block (EntityBitBlock), which the host keeps
    u16 unk_160 = 0; // +0x160
    u16 unk_162 = 0; // +0x162
    u32 unk_164 = 0; // +0x164, cleared with the turn once unk_168 is 0 (0x001346B0)
    float unk_168 = 0.f; // +0x168
    // +0x16C: the stagger meter: a hit empties it in steps (0x0001FF60); at zero the body can be finished.
    float stagger = 0.f;
    u32 unk_170 = 0; // +0x170
    float unk_174 = 0.f; // +0x174
    // +0x178: +0x178..+0x1CB is the ground contact's match block (0x000FCD70). The ground under the body while
    // has_ground is set; its y is the floor height 0x0001F060 reads.
    Vec3 ground_point{};
    Vec3 ground_normal{}; // +0x184
    std::array<u8, 0xE> unk_190{}; // +0x190
    u8 has_ground = 0; // +0x19E
    std::array<u8, 0x2D> unk_19F{}; // +0x19F
    std::array<u8, 0xC> last_contact{}; // +0x1CC, +0x1CC..+0x21F is the last contact's match block (0x000FCD70)
    Vec3 contact_push{}; // +0x1D8
    std::array<u8, 0x3C> unk_1E4{}; // +0x1E4
    float unk_220 = 0.f; // +0x220
    float unk_224 = 0.f; // +0x224
    float unk_228 = 0.f; // +0x228
    float unk_22C = 0.f; // +0x22C
    Vec3 unk_230{}; // +0x230
    float unk_23C = 0.f; // +0x23C, nonzero: a hit takes the secondary action (0x00133F50)
    float unk_240 = 0.f; // +0x240, nonzero: a hit takes the primary action (0x00133F50)
    u8 push_mode = 0; // +0x244
    u8 shape_mode = 0; // +0x245, parameters +0x1AA; the actor's shape_mode
    std::array<u8, 2> unk_246{}; // +0x246, padding
    // +0x248: the body's own motion; a moving body (nonzero, with flag bit 1) takes its moving key (0x000A2930,
    // 0x0008C310).
    Vec3 motion{};
    u8 unk_254 = 0; // +0x254
    std::array<u8, 3> unk_255{}; // +0x255, padding
    u32 holds = 0; // +0x258, counts holds on the body; the last release sends it message 0x4A
    float unk_25C = 0.f; // +0x25C
    u8 unk_260 = 0; // +0x260
    std::array<u8, 0xB> unk_261{}; // +0x261
    u32 unk_26C = 0; // +0x26C
    // +0x270: the actor that last hit the body, by address: a hit stream pushes the body away from it (0x00097500).
    u32 attacker = 0;
    u32 unk_274 = 0; // +0x274, a player's: the actor 0x00138B40 reports; messages 0x31 and 0x6A carry it
    u32 unk_278 = 0; // +0x278, set when the turn resets (0x001346B0)
    u32 damage_kind = 0; // +0x27C
    u32 damage_amount = 0; // +0x280
    u32 unk_284 = 0; // +0x284
    std::array<char, 0x44> variant{}; // +0x288, a name, such as the butler's while scared
    u32 contact_count = 0; // +0x2CC
    u8 unk_2D0 = 0; // +0x2D0, parameters +0x1BC
    u8 unk_2D1 = 0; // +0x2D1, parameters +0x1BD
    u8 unk_2D2 = 0; // +0x2D2
    u8 unk_2D3 = 0; // +0x2D3, parameters +0x1BE
    u8 unk_2D4 = 0; // +0x2D4, parameters +0x1BF
    std::array<u8, 3> unk_2D5{}; // +0x2D5, padding
    // +0x2D8: the callout tokens of the animation commands (a count and up to eight tokens).
    std::array<u8, 0x24> command_tokens{};
    Vec3 look_point{}; // +0x2FC
    u8 look_point_set = 0; // +0x308
    u8 unk_309 = 0; // +0x309, +0x309..+0x30C: counters states raise and drop together
    u8 unk_30A = 0; // +0x30A
    u8 unk_30B = 0; // +0x30B
    u8 unk_30C = 0; // +0x30C
    std::array<u8, 3> unk_30D{}; // +0x30D, padding
    u32 unk_310 = 0; // +0x310
    u32 unk_314 = 0; // +0x314
    Vec3 velocity{}; // +0x318, the velocity the actor's +0x3C eases toward, at ease_rate (move action 0x5A)
    float gravity = 0.f; // +0x324
    u8 move_class = 0; // +0x328, picks the class value step_speed scales (0x000259C0)
    std::array<u8, 3> unk_329{}; // +0x329, padding
    float step_speed = 0.f; // +0x32C, times the class value (0x000259C0) is the walking speed
    // +0x330: ease_rate as it was before a state swapped in its own (0x000957A0, 0x0007FD30); restored when the state
    // ends.
    float saved_ease_rate = 0.f;
    float ease_rate = 0.f; // +0x334
    u32 ease_rate_swaps = 0; // +0x338, raised with each ease_rate swap and lowered when it is given back
    float heading = 0.f; // +0x33C, the heading the body walks on
    u32 motion_sample = 0; // +0x340, the next of the 0x50 samples after it
    std::array<u8, 0x3C0> motion_samples{}; // +0x344, 12 bytes each (record_runtime_body_motion)
    Vec3 unk_704{}; // +0x704, a held body's offset from its actor (0x0002C2B0)
    std::array<u8, 4> unk_710{}; // +0x710
    u8 sphere_count = 0; // +0x714
    u8 unk_715 = 0; // +0x715
    u8 unk_716 = 0; // +0x716, cleared while the body is held (0x0002C2B0)
    u8 collision_kind = 0; // +0x717
    u32 rider = 0; // +0x718, what rides or holds this body (the jump on state), by address
    // +0x71C: three rates the body turns by, always written together: the constructor's 0.558, 12.57, 6 (0x00024C50),
    // the struggle state's parameters (0x000A2B90) and the hover state's 1.047, 2.269, 6 (0x00097B80).
    std::array<float, 3> turn_rates{};
    u32 unk_728 = 0; // +0x728, cleared while Cooper is bound to the head, set again after (Super Scary)
    Vec3 euler{}; // +0x72C, the body's euler angles (pitch, yaw, roll): 0x00041820 copies the actor's into it
    u32 unk_738 = 0; // +0x738
    u32 unk_73C = 0; // +0x73C
    u32 unk_740 = 0; // +0x740
    u32 unk_744 = 0; // +0x744
    u16 action_key = 0; // +0x748, the action the body is running
    u16 default_action_key = 0; // +0x74A
    u32 unk_74C = 0; // +0x74C
    u32 unk_750 = 0; // +0x750
    u32 hit_timer_running = 0; // +0x754
    float unk_758 = 0.f; // +0x758
    float unk_75C = 0.f; // +0x75C
    u32 hit_timer = 0; // +0x760, the hit timer's recipient, by address; message 5 stops it
    std::array<char, 0x80> hit_timer_name{}; // +0x764
    u8 unk_7E4 = 0; // +0x7E4
    u8 unk_7E5 = 0; // +0x7E5, a counter states raise and drop
    std::array<u8, 2> unk_7E6{}; // +0x7E6, padding
    float unk_7E8 = 0.f; // +0x7E8
    float unk_7EC = 0.f; // +0x7EC
    u32 second_timer = 0; // +0x7F0, the second timer's recipient, by address
    std::array<char, 0x80> second_timer_name{}; // +0x7F4
    // +0x874: the surface the body stands on, or surface_override while surface_override_frames counts down (0x00025AE0
    // clears the override when it reaches zero).
    u8 surface = 0;
    std::array<u8, 3> unk_875{}; // +0x875, padding
    u32 scaled_surfaces = 0; // +0x878, surfaces whose DAT_00217EC0 row scales the body's motion, one bit per surface
    u16 sound_tag = 0; // +0x87C
    u8 surface_override = 0; // +0x87E
    std::array<u8, 1> unk_87F{}; // +0x87F, padding
    u16 surface_override_frames = 0; // +0x880
    std::array<u8, 2> unk_882{}; // +0x882, padding
    // +0x884: the body's height and radius scales over the actor's +0x34 and +0x38 (parameters +0x190 and +0x194,
    // 0x00024C50).
    float height_scale = 0.f;
    float radius_scale = 0.f; // +0x888
    u32 unk_88C = 0; // +0x88C
    u32 unk_890 = 0; // +0x890
};
static_assert(sizeof(BodyRecord) == 0x894);

// One of a player body's six tracking joints (0x14 bytes).
struct PlayerBodyJoint
{
    u32 record_word = 0; // +0, the joint's record, which the host keeps
    u8 state = 0; // +4
    std::array<u8, 3> unk_5{}; // +5, padding
    float timer = 0.f; // +8
    float pitch_delta = 0.f; // +0xC
    float yaw_delta = 0.f; // +0x10
};
static_assert(sizeof(PlayerBodyJoint) == 0x14);

// The parent player body (factory type 0x27, 0x924 bytes, 0x00076180): the base and the tracking joints.
struct ParentPlayerBodyRecord
{
    BodyRecord base{}; // +0
    float collision_radius = 0.f; // +0x894, parameters +0x7C8, over the actor's radius
    u32 unk_898 = 0; // +0x898
    u32 selected_state_word = 0; // +0x89C, the host keeps the shapes' selected state
    u32 selected_word = 0; // +0x8A0, the host keeps the shapes' selection
    float collision_height = 0.f; // +0x8A4, parameters +0x7CC, over the actor's radius
    float joint_limit = 0.f; // +0x8A8, how far a joint tracks, plus DAT_0032B160
    std::array<PlayerBodyJoint, 6> joints{}; // +0x8AC
};
static_assert(sizeof(ParentPlayerBodyRecord) == 0x924);

// The camera body (factory type 0x28, 0x914 bytes, 0x00073170). The native camera node is the record at +0x894.
struct CameraBodyRecord
{
    BodyRecord base{}; // +0
    std::array<u8, 8> unk_894{}; // +0x894
    Vec3 view_position{}; // +0x89C
    Vec3 view_euler{}; // +0x8A8
    std::array<u8, 0x40> unk_8B4{}; // +0x8B4
    float unk_8F4 = 0.f; // +0x8F4
    std::array<u8, 0x18> unk_8F8{}; // +0x8F8
    float separation = 0.f; // +0x910
};
static_assert(sizeof(CameraBodyRecord) == 0x914);

// The player body (factory type 0x2A, 0x950 bytes, 0x00075AD0): the parent's, then its own.
struct PlayerBodyRecord
{
    ParentPlayerBodyRecord base{}; // +0
    // +0x924: the callout tokens control message 8 records (a count and up to eight tokens; 0x00075C60).
    std::array<u8, 0x24> control_tokens{};
    u32 saved_energy = 0; // +0x948, the energy control message 4 kept, restored when it ends
    u32 energy_saved = 0; // +0x94C
};
static_assert(sizeof(PlayerBodyRecord) == 0x950);

// A body's bytes viewed as its record.
inline BodyRecord& body_record(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(BodyRecord));
    return *reinterpret_cast<BodyRecord*>(bytes.data());
}

inline const BodyRecord& body_record(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(BodyRecord));
    return *reinterpret_cast<const BodyRecord*>(bytes.data());
}

inline ParentPlayerBodyRecord& parent_body_record(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(ParentPlayerBodyRecord));
    return *reinterpret_cast<ParentPlayerBodyRecord*>(bytes.data());
}

inline const ParentPlayerBodyRecord& parent_body_record(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(ParentPlayerBodyRecord));
    return *reinterpret_cast<const ParentPlayerBodyRecord*>(bytes.data());
}

inline CameraBodyRecord& camera_body_record(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(CameraBodyRecord));
    return *reinterpret_cast<CameraBodyRecord*>(bytes.data());
}

inline const CameraBodyRecord& camera_body_record(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(CameraBodyRecord));
    return *reinterpret_cast<const CameraBodyRecord*>(bytes.data());
}

inline PlayerBodyRecord& player_body_record(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(PlayerBodyRecord));
    return *reinterpret_cast<PlayerBodyRecord*>(bytes.data());
}

inline const PlayerBodyRecord& player_body_record(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(PlayerBodyRecord));
    return *reinterpret_cast<const PlayerBodyRecord*>(bytes.data());
}

// The states' common pair of writes, pitch (+0x72C) and roll (+0x734) to zero: the body stands level, keeping its
// yaw.
inline void level_body_euler(std::span<u8> body)
{
    body_record(body).euler.x = 0.f;
    body_record(body).euler.z = 0.f;
}
} // namespace gbtg
