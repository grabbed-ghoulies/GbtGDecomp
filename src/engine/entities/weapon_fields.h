#pragma once
#include "gbtg/retail_field.h"
#include "actor_fields.h"

#include <array>

namespace gbtg
{
// The weapon base (factory type 0xB over the actor, type 2): its own part, after the actor's 0x934 bytes, up to
// 0xE54 (0x001458E0's size). Every weapon class (the bob, candle, container, gun, projectiles, chair and the rest)
// builds on it, and its class's own fields follow at +0xE54. Pointer fields the host keeps natively are in
// RuntimeWeaponActor; their words here are the ones named `_word`. See docs/RETAIL_LAYOUTS.md.
// The weapon's parameter block (0x001458E0's 0x5F8 bytes, after the actor's 0x334): what 0x00145C80 and its helpers
// copy into the weapon base. The marker handlers' template patches (0x001007F0, 0x00146C80) write it too.
namespace weapon_parameter
{
constexpr RetailField<u16, 0x334> initial_state{};
constexpr RetailField<u8, 0x336> unk_93F{}; // into +0x93F
constexpr RetailField<u8, 0x337> unk_93D{}; // into +0x93D
constexpr RetailField<u8, 0x338> hit_kind{};
constexpr RetailField<u8, 0x339> hit_strength{};
constexpr RetailField<u8, 0x33A> hit_style{};
constexpr RetailField<u32, 0x33C> name{}; // an XBE string, or 0
constexpr RetailField<float, 0x340> variant_chance{}; // the variant and its trigger are kept when a roll is no more
constexpr RetailField<float, 0x344> trigger_radius{};
constexpr RetailField<float, 0x348> trigger_chance{};
constexpr RetailField<u32, 0x34C> unk_BF8{};
constexpr RetailField<u32, 0x350> unk_BFC{};
constexpr RetailField<std::array<char, 0x80>, 0x354> variant_name{};
constexpr RetailField<std::array<char, 0x80>, 0x3D4> variant_attributes{};
constexpr RetailField<u32, 0x454> spawn_group{};
constexpr RetailField<u32, 0x458> accepted_class_count{}; // the copy reads its low half
constexpr RetailField<u32, 0x45C> accepted_classes{}; // the rows' XBE address (0x8C bytes each)
constexpr RetailField<u32, 0x460> fades_out{};
constexpr RetailField<u32, 0x464> awards_points{};
constexpr RetailField<u32, 0x468> blocks_spawn{}; // set: 0x00147860 raises the actor's spawn block
constexpr RetailField<u32, 0x46C> unk_DA8{};
constexpr RetailField<u32, 0x470> controller_key{}; // set: the weapon's ghouly controller is registered under it
constexpr RetailField<std::array<char, 0x80>, 0x474> name_9A8{};
constexpr RetailField<std::array<char, 0x80>, 0x4F4> name_A28{};
constexpr RetailField<u32, 0x574> starts_scene{};
constexpr RetailField<std::array<char, 0x80>, 0x578> voice_name{};
constexpr u32 kSize = 0x5F8;
static_assert(retail_fits(kSize, voice_name));
} // namespace weapon_parameter
struct WeaponBase
{
    ActorBytes base{}; // +0
    std::array<u8, 8> unk_934{}; // +0x934, never read or written by translated code
    // +0x93C: the low byte of the actor's mask word (0x00146230). A contact test passes only while it is clear.
    u8 mask = 0;
    u8 unk_93D = 0; // +0x93D, parameters +0x337
    std::array<u8, 1> unk_93E{}; // +0x93E, never read or written by translated code
    u8 unk_93F = 0; // +0x93F, parameters +0x336; 0x001460C0 tests it
    u8 firing = 0; // +0x940, the extinguisher and water pistol are spraying
    u8 unk_941 = 0; // +0x941, set by the bob and the chair
    u8 unk_942 = 0; // +0x942, set by the container and the chair; 0x00026C60 tests it
    std::array<u8, 1> unk_943{}; // +0x943, padding
    u16 accepted_class_count = 0; // +0x944
    u16 accepted_class_row = 0; // +0x946, the row animating with the holder (0x00144B80)
    // +0x948: the rows' classes the holder may be, which actor.accepted_classes points at (ActorAcceptedClass, 0x10
    // bytes each).
    std::array<u8, 0x50> accepted_classes{};
    u32 markers_word = 0; // +0x998, the host keeps RuntimeWeaponActor::markers
    // +0x99C: the class's contact test, a retail function address (0x000C61D0 the bob's, 0x000CB040 the gun's, and so
    // on).
    u32 contact_test = 0;
    u32 spawn_model_blocked = 0; // +0x9A0, set once the weapon breaks (0x001454F0)
    // +0x9A4: parameters +0x464 clear. Clear, a break awards its points and broadcasts message 0x8F.
    u32 unk_9A4 = 0;
    std::array<char, 0x80> name_9A8{}; // +0x9A8, parameters +0x474
    std::array<char, 0x80> name_A28{}; // +0xA28, parameters +0x4F4
    u16 break_category = 0; // +0xAA8, what last broke it: 1 at start, the attacker's category
    std::array<u8, 2> unk_AAA{}; // +0xAAA, padding
    u32 was_held = 0; // +0xAAC, last bounds update's held state (0x00145A40)
    float prompt_pulse = 0.f; // +0xAB0, the grab prompt's pulse timer
    u32 grab_shape_count = 0; // +0xAB4
    u32 grab_shapes_word = 0; // +0xAB8, the host keeps RuntimeWeaponActor::grab_shapes
    std::array<u8, 4> unk_ABC{}; // +0xABC, never read or written by translated code
    std::array<u8, 4> kept_spawn_points{}; // +0xAC0, the nearest point by spawn point, 0xFF for none
    u32 spawn_point_count = 0; // +0xAC4
    u32 spawn_points_word = 0; // +0xAC8, the host keeps the spawn points in RuntimeWeaponActor
    std::array<u8, 4> unk_ACC{}; // +0xACC, never read or written by translated code
    u32 spawn_blocked = 0; // +0xAD0, the actor's spawn_blocked count
    u32 prompt_wanted = 0; // +0xAD4, a player is near a grab point (0x00147430)
    u32 hand_texture_word = 0; // +0xAD8, the host keeps the hand texture in RuntimeWeaponActor
    u16 prompt_phase = 0; // +0xADC, 0 hidden, 1 fading in, 2 shown, 3 fading out
    u8 prompt_alpha = 0; // +0xADE
    u8 prompt_visible = 0; // +0xADF
    float prompt_fade = 0.f; // +0xAE0, seconds left in the fading phases
    u16 pickup_count = 0; // +0xAE4, pick-up states holding the weapon (0x0009DF90 raises it)
    u16 flagged_marker_count = 0; // +0xAE6
    u32 flagged_markers_word = 0; // +0xAE8, the host keeps RuntimeWeaponActor::flagged_markers
    // +0xAEC: the variant 0x00148430 asks for when the spawn trigger fires or the weapon breaks: its name and
    // attributes.
    std::array<char, 0x80> variant_name{};
    std::array<char, 0x80> variant_attributes{}; // +0xB6C
    u32 spawn_group = 0; // +0xBEC, parameters +0x454
    // +0xBF0: the ghouly spawn trigger (0x0013E980): its radius and chance; a radius of zero turns it off.
    float trigger_radius = 0.f;
    float trigger_chance = 0.f; // +0xBF4
    u32 unk_BF8 = 0; // +0xBF8, set: a missed trigger (-1) turns the trigger off
    u32 unk_BFC = 0; // +0xBFC, parameters +0x350
    u32 unk_C00 = 0; // +0xC00, set: the trigger needs a holderless, unblocked weapon with spawn points
    u32 spawn_record_word = 0; // +0xC04, the host keeps RuntimeWeaponActor::spawn_record
    u32 group_owner = 0; // +0xC08, the group's owner (0x00148600), cleared when it lets go
    u16 state = 0; // +0xC0C
    u16 next_state = 0; // +0xC0E, taken at the end of the update (0x0014A2E0)
    u16 initial_state = 0; // +0xC10, parameters +0x334
    std::array<u8, 2> unk_C12{}; // +0xC12, padding
    float state_time = 0.f; // +0xC14
    u32 starts_scene = 0; // +0xC18, parameters +0x574: the scene (0x00144FD0) to start
    u32 scene_background = 0; // +0xC1C
    u32 scene_part = 0; // +0xC20
    // +0xC24: the thrown motion (0x001487C0 / 0x001488D0): contact probing, sliding on a hit, quiet (no markers),
    // gravity, the contact mask and whether it has hit.
    u32 probes_contacts = 0;
    u32 slides = 0; // +0xC28
    u32 quiet = 0; // +0xC2C
    float gravity = 0.f; // +0xC30
    Vec3 motion_euler{}; // +0xC34, the thrown motion's, DAT_0050DD74 unless the caller gives one
    u32 contact_mask = 0; // +0xC40
    u32 hit = 0; // +0xC44
    std::array<u8, 0x54> contact_match{}; // +0xC48, the contact's match block (0x000FCD70)
    std::array<u8, 0x20> hop{}; // +0xC9C, from, to, timer, done (0x00149230)
    std::array<u8, 0x20> drop{}; // +0xCBC, the same, for the drop (0x00149580)
    // +0xCDC: the follow motion (0x001495C0 / 0x00149750 / 0x00149D20).
    u32 starting = 0; // the first step out of what it starts inside is still to come
    Vec3 aim{}; // +0xCE0
    float slack = 0.f; // +0xCEC
    float damping = 0.f; // +0xCF0, scales the planar velocity
    float rise = 0.f; // +0xCF4
    u32 ghost = 0; // +0xCF8, no world probes
    u32 cells_word = 0; // +0xCFC, the host keeps RuntimeWeaponActor::cells
    i32 floor = 0; // +0xD00, the navigation floor
    u32 path_search_word = 0; // +0xD04, the host keeps RuntimeWeaponActor::path_search
    std::array<char, 0x80> voice_name{}; // +0xD08, parameters +0x578; empty for none
    u16 voice = 0; // +0xD88, the playing voice, or 0xFFFF
    std::array<u8, 2> unk_D8A{}; // +0xD8A, padding
    u32 spawn_owner = 0; // +0xD8C, the actor that spawned it (native: actor.spawn_owner)
    u32 broken_groups = 0; // +0xD90, piece groups broken off so far (0x001451C0)
    u32 broken_pieces = 0; // +0xD94
    u32 unk_D98 = 0; // +0xD98, no retail writer sets it
    u32 unk_D9C = 0; // +0xD9C, no retail writer sets it
    u32 fades_out = 0; // +0xDA0, parameters +0x460: its pieces end in a fade-out
    u32 awards_points = 0; // +0xDA4, parameters +0x464
    u32 unk_DA8 = 0; // +0xDA8, parameters +0x46C
    // +0xDAC: the kind, strength and style a hit by the weapon reports (messages 0x1D and 0x19 +4..+0xC).
    u8 hit_kind = 0;
    u8 hit_strength = 0; // +0xDAD
    u8 hit_style = 0; // +0xDAE
    u8 pieces_finished = 0; // +0xDAF
    std::array<char, 0x20> name{}; // +0xDB0, parameters +0x33C's string
    std::array<char, 0x80> animation{}; // +0xDD0, parameters +0xA8, an AID when it starts "aid_"
    u32 target_record = 0; // +0xE50, a mind target record, by address
};
static_assert(sizeof(WeaponBase) == 0xE54);

// A weapon's bytes viewed as the base every class builds on.
inline WeaponBase& weapon_base(std::span<u8> bytes)
{
    return actor_record_view<WeaponBase>(bytes);
}

inline const WeaponBase& weapon_base(std::span<const u8> bytes)
{
    return actor_record_view<WeaponBase>(bytes);
}
} // namespace gbtg
