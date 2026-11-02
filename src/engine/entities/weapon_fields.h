#pragma once
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// The weapon base (factory type 0xB over the actor, type 2): its own part, after the actor's 0x934 bytes, up to
// 0xE54 (0x001458E0's size). Every weapon class (the bob, candle, container, gun, projectiles, chair and the rest)
// builds on it, and its class's own fields follow at +0xE54. Pointer fields the host keeps natively (+0x998 markers,
// +0xAB8 grab shapes, +0xAC8 spawn points, +0xAD8 hand texture, +0xAE8 flagged markers, +0xC04 spawn record, +0xCFC
// cells, +0xD04 path search) are in RuntimeWeaponActor. See docs/RETAIL_LAYOUTS.md.
namespace weapon_field
{
// The low byte of the actor's mask word (0x00146230). A contact test passes only while it is clear.
constexpr RetailField<u8, 0x93C> mask{};
constexpr RetailField<u8, 0x93D> unk_93D{}; // parameters +0x337
constexpr RetailField<u8, 0x93F> unk_93F{}; // parameters +0x336; 0x001460C0 tests it
constexpr RetailField<u8, 0x940> firing{}; // the extinguisher and water pistol are spraying
constexpr RetailField<u8, 0x941> unk_941{}; // set by the bob and the chair
constexpr RetailField<u8, 0x942> unk_942{}; // set by the container and the chair; 0x00026C60 tests it
constexpr RetailField<u16, 0x944> accepted_class_count{};
constexpr RetailField<u16, 0x946> accepted_class_row{}; // the row animating with the holder (0x00144B80)
// The class's contact test, a retail function address (0x000C61D0 the bob's, 0x000CB040 the gun's, and so on).
constexpr RetailField<u32, 0x99C> contact_test{};
constexpr RetailField<u32, 0x9A0> spawn_model_blocked{}; // set once the weapon breaks (0x001454F0)
// Parameters +0x464 clear. Clear, a break awards its points and broadcasts message 0x8F.
constexpr RetailField<u32, 0x9A4> unk_9A4{};
constexpr RetailField<std::array<char, 0x80>, 0x9A8> name_9A8{}; // parameters +0x474
constexpr RetailField<std::array<char, 0x80>, 0xA28> name_A28{}; // parameters +0x4F4
constexpr RetailField<u16, 0xAA8> break_category{}; // what last broke it: 1 at start, the attacker's category
constexpr RetailField<u32, 0xAAC> was_held{}; // last bounds update's held state (0x00145A40)
constexpr RetailField<float, 0xAB0> prompt_pulse{}; // the grab prompt's pulse timer
constexpr RetailField<u32, 0xAB4> grab_shape_count{};
constexpr RetailField<u32, 0xAC4> spawn_point_count{};
constexpr RetailField<u32, 0xAD0> spawn_blocked{}; // the actor's spawn_blocked count
constexpr RetailField<u32, 0xAD4> prompt_wanted{}; // a player is near a grab point (0x00147430)
constexpr RetailField<u16, 0xADC> prompt_phase{}; // 0 hidden, 1 fading in, 2 shown, 3 fading out
constexpr RetailField<u8, 0xADE> prompt_alpha{};
constexpr RetailField<u8, 0xADF> prompt_visible{};
constexpr RetailField<float, 0xAE0> prompt_fade{}; // seconds left in the fading phases
constexpr RetailField<u16, 0xAE4> pickup_count{}; // pick-up states holding the weapon (0x0009DF90 raises it)
constexpr RetailField<u16, 0xAE6> flagged_marker_count{};
// The variant 0x00148430 asks for when the spawn trigger fires or the weapon breaks: its name and attributes.
constexpr RetailField<std::array<char, 0x80>, 0xAEC> variant_name{};
constexpr RetailField<std::array<char, 0x80>, 0xB6C> variant_attributes{};
constexpr RetailField<u32, 0xBEC> spawn_group{}; // parameters +0x454
// The ghouly spawn trigger (0x0013E980): its radius and chance; a radius of zero turns it off.
constexpr RetailField<float, 0xBF0> trigger_radius{};
constexpr RetailField<float, 0xBF4> trigger_chance{};
constexpr RetailField<u32, 0xBF8> unk_BF8{}; // set: a missed trigger (-1) turns the trigger off
constexpr RetailField<u32, 0xBFC> unk_BFC{}; // parameters +0x350
constexpr RetailField<u32, 0xC00> unk_C00{}; // set: the trigger needs a holderless, unblocked weapon with spawn points
constexpr RetailField<u32, 0xC08> group_owner{}; // the group's owner (0x00148600), cleared when it lets go
constexpr RetailField<u16, 0xC0C> state{};
constexpr RetailField<u16, 0xC0E> next_state{}; // taken at the end of the update (0x0014A2E0)
constexpr RetailField<u16, 0xC10> initial_state{}; // parameters +0x334
constexpr RetailField<float, 0xC14> state_time{};
constexpr RetailField<u32, 0xC18> starts_scene{}; // parameters +0x574: the scene (0x00144FD0) to start
constexpr RetailField<u32, 0xC1C> scene_background{};
constexpr RetailField<u32, 0xC20> scene_part{};
// The thrown motion (0x001487C0 / 0x001488D0): contact probing, sliding on a hit, quiet (no markers), gravity, the
// contact mask and whether it has hit.
constexpr RetailField<u32, 0xC24> probes_contacts{};
constexpr RetailField<u32, 0xC28> slides{};
constexpr RetailField<u32, 0xC2C> quiet{};
constexpr RetailField<float, 0xC30> gravity{};
constexpr RetailField<u32, 0xC40> contact_mask{};
constexpr RetailField<u32, 0xC44> hit{};
// The follow motion (0x001495C0 / 0x00149750 / 0x00149D20).
constexpr RetailField<u32, 0xCDC> starting{}; // the first step out of what it starts inside is still to come
constexpr RetailField<Vec3, 0xCE0> aim{};
constexpr RetailField<float, 0xCEC> slack{};
constexpr RetailField<float, 0xCF0> damping{}; // scales the planar velocity
constexpr RetailField<float, 0xCF4> rise{};
constexpr RetailField<u32, 0xCF8> ghost{}; // no world probes
constexpr RetailField<u32, 0xD00> floor{}; // the navigation floor
constexpr RetailField<std::array<char, 0x80>, 0xD08> voice_name{}; // parameters +0x578; empty for none
constexpr RetailField<u16, 0xD88> voice{}; // the playing voice, or 0xFFFF
constexpr RetailField<u32, 0xD8C> spawn_owner{}; // the actor that spawned it (native: actor.spawn_owner)
constexpr RetailField<u32, 0xD90> broken_groups{}; // piece groups broken off so far (0x001451C0)
constexpr RetailField<u32, 0xD94> broken_pieces{};
constexpr RetailField<u32, 0xD98> unk_D98{}; // no retail writer sets it
constexpr RetailField<u32, 0xD9C> unk_D9C{}; // no retail writer sets it
constexpr RetailField<u32, 0xDA0> fades_out{}; // parameters +0x460: its pieces end in a fade-out
constexpr RetailField<u32, 0xDA4> awards_points{}; // parameters +0x464
constexpr RetailField<u32, 0xDA8> unk_DA8{}; // parameters +0x46C
// The kind, strength and style a hit by the weapon reports (messages 0x1D and 0x19 +4..+0xC).
constexpr RetailField<u8, 0xDAC> hit_kind{};
constexpr RetailField<u8, 0xDAD> hit_strength{};
constexpr RetailField<u8, 0xDAE> hit_style{};
constexpr RetailField<u8, 0xDAF> pieces_finished{};
constexpr RetailField<std::array<char, 0x20>, 0xDB0> name{}; // parameters +0x33C's string
constexpr RetailField<std::array<char, 0x80>, 0xDD0> animation{}; // parameters +0xA8, an AID when it starts "aid_"
constexpr RetailField<u32, 0xE50> target_record{}; // a mind target record, by address
constexpr u32 kSize = 0xE54;
static_assert(retail_fits(kSize, target_record));
} // namespace weapon_field
} // namespace gbtg
