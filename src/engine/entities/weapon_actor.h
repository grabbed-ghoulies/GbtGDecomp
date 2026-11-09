#pragma once

#include "weapon_fields.h"
#include "actor_factory_owner.h"
#include "actor_matrix_setup.h"
#include "actor_base_messages.h"
#include "actor_frame_update.h"
#include "actor_trail_requests.h"
#include "actor_spawn_query.h"
#include "body_contacts.h"
#include "body_resources.h"
#include "camera_director.h"
#include "../camera/display_view.h"
#include "../scene/scene_marker_handlers.h"
#include "../frontend/scoreboard_hud.h"
#include "../audio/event_voices.h"
#include "../scripts/script_actor_query.h"
#include "ghouly_requests.h"
#include "../navigation/cell_registration.h"
#include "../navigation/floor_tracking.h"
#include "../scene/background_parts.h"
#include "gbtg/resource_error.h"
#include "actor_lifecycle.h"
#include "ghouly_spawn_record.h"
#include "../../gbtg/text/localtext_resources.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace gbtg
{
struct RuntimeMindTarget;
struct RuntimeCutsceneInstance;
struct CutsceneRuntimeContext;
// Weapon base, factory type 0x0B (getter 0x00145900) over the actor (type 2), and its subclass the cook's chair,
// type 0xAA (getter 0x000CEF70, vtable 0x00506438).
void weapon_actor_object_sizes(ObjectSizes& sizes); // Xbox 0x001458E0
void chair_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000CEF50

// DAT_005461F4..DAT_005461FC: the weapon stat table (0x44-byte rows: the tag name, then the points at +0x40), its
// row count, and the loctext table of its captions. Loaded at boot by 0x001486C0.
struct RuntimeWeaponStatTables
{
    std::span<const u8> stats;
    u32 count = 0;
    std::span<const u8> text;
};
void load_runtime_weapon_stat_tables(RuntimeWeaponStatTables& tables, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x001486C0
struct RuntimeCandleServices;
// Scene services every weapon shares.
struct RuntimeDisplaySetupContext;
struct RuntimeWeaponServices
{
    RuntimeWeaponStatTables& stats;
    std::function<const char*(u16)> tag_name; // DAT_0042A850[tag].
    LocalTextResourceContext& text; // 0x000FF120.
    std::span<char16_t> award_text; // DAT_00507E78.
    std::function<void(i32, u32, const char16_t*, i32)> award; // 0x0012DE90: points, category, text, quiet.
    u32& broken_parts; // DAT_00545978.
    RuntimeActorSoundQueues& sounds; // 0x00039690's voices.
    std::span<float, 12> sampling_scratch; // DAT_0050D7A0.
    float*& sampling_active; // DAT_0050DD70.
    std::function<std::span<const u8>(const Actor&, u32)> resolve; // A retail word in an actor model's space.
    std::function<void(Actor&, ActorFrameAttachment*)> release_frames; // 0x0003DE90 over actor +0x1D8.
    u32& runtime_flags; // DAT_0050EA9C.
    std::function<u32(const Actor&)> actor_address; // A scene actor's retail address.
    std::function<std::span<const u8>(u32)> body_of; // The actor's +0x95C object (players).
    RuntimeMarkerObjects& objects; // The factory, the objparams pools and DAT_0051108C.
    std::function<FactoryObject*(FactoryObject)> keep; // The scene keeps created objects.
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    float occupancy_initial;
    const Vec3& default_euler; // DAT_0050DD74.
    RuntimeScoreboardGlobals& scoreboard; // DAT_00432688..DAT_004326E4.
    std::span<const u8> damage_preset; // DAT_00312F8C.
    std::function<void(u32)> detach_from_holder; // 0x0001B4C0 on the holder's body (+0x95C).
    std::function<u32(const void*, size_t)> bind_address; // A retail word for a native block in a message.
    std::function<void(const void*)> unbind_address;
    std::function<void*(u32)> bound_at; // The native block behind a bound word.
    std::span<u32> contact_totals; // DAT_0053E528, 0x3D words per tag.
    std::span<u32> contact_session; // DAT_00524E2C.
    std::function<ActorAnimationState*(Actor&)> animation_of; // A scene actor's +0x50 views.
    const std::unordered_map<ActorFrameAttachment*, const u8*>& frame_objects; // Actor +0x1D8 objects.
    AudioEventVoices& voices; // DAT_004E0CE8.
    const u32& story_flags; // DAT_0050EA98.
    std::function<CutsceneRuntimeContext&()> cutscenes; // 0x0004D680's scene cutscene runtime.
    // Holder body +0x4C and +0x14 (with their native secondary and receiver views) set to a weapon, or cleared for 0.
    std::function<void(const Actor& holder, u32 weapon)> hold_secondary{};
    // 0x0001B680 on the holder's body (ECX): empty its hand (+0x48, +0x10, +0x288 and bit 0x22), native views too.
    std::function<void(const Actor& holder)> empty_hand{};
    // The large projectile's flight: its trail views (0x0013B030), the actor startup views (0x00041F90) and the PAL
    // words (DAT_005096E0 / DAT_005096E4).
    std::function<ActorTrailContext(u32 actor_address)> trail{};
    std::function<RuntimeActorStartupContext&()> startup{};
    const i32* pal = nullptr;
    const i32* pal_override = nullptr;
    std::function<FactoryObject*(const u8*)> object_of{}; // The scene object holding an object's bytes.
    std::function<RuntimeCandleServices&()> candle{}; // Type 0xA1's lights, billboards and burst callout.
    ActorEffectStreamContext* effects = nullptr; // 0x00053170's context (type 0xA6's droplet effects).
    std::function<RuntimeDisplaySetupContext&()> display{}; // Type 0xA6's strip draw (0x000659F0).
    std::function<std::vector<BackgroundListEntry>()> backgrounds{}; // DAT_00511B20 order, for 0x0013B980.
    // A mind target record (a mind's +0x10, a weapon's +0xE50) by its retail address; its +0x10 is the actor.
    std::function<RuntimeMindTarget*(u32)> target_at{};
    // A player actor's cell registration (actor +0xCE8, grid +0xCEC) by the actor's retail address.
    std::function<entity_detail::NavigationCellRegistration**(u32)> player_cells{};
};
// Message 0x1D from a weapon's contact test (0x00141890) to the actor it hit.
struct RuntimeWeaponHitNotice
{
    u32 id = 0x1D;
    u32 kind = 0; // +4, weapon +0xDAC.
    u32 strength = 0; // +8, weapon +0xDAD.
    u32 style = 0; // +0xC, weapon +0xDAE.
    u32 reply = 0; // +0x10.
    Actor* weapon = nullptr; // +0x14.
    Vec3 velocity{}; // +0x18.
    GhoulySpawnPool* contacts = nullptr; // +0x24.
    Vec3 point{}; // +0x28.
};

// Message 0x19 from the large projectile's contact test (0x00141A30) to the actor it hit.
struct RuntimeProjectileHitNotice
{
    u32 id = 0x19;
    u32 kind = 0; // +4, weapon +0xDAC.
    u32 strength = 0; // +8, weapon +0xDAD.
    u32 style = 0; // +0xC, weapon +0xDAE.
    u32 reply = 0; // +0x10.
    u32 result = 0;
    u32 standing = 0;
    Actor* weapon = nullptr; // +0x1C.
    Vec3 velocity{}; // +0x20.
    GhoulySpawnPool* contacts = nullptr; // +0x2C.
    Vec3 point{}; // +0x30.
    u32 tail = 0; // +0x3C, the caller's last argument (0).
};

// One weapon instance: the retail-layout bytes, its actor, and native storage behind the pointer fields.
// The chair (factory type 0xAA, 0xF08 bytes), the one class weapon_actor.cpp keeps to itself.
// The chair's parameters after the weapon's (0x000CEFE0).
namespace chair_weapon_parameter
{
constexpr RetailField<u32, 0x5F8> taker_uses{};
constexpr RetailField<u32, 0x5FC> player_uses{};
constexpr RetailField<u32, 0x600> unk_600{}; // set: the weapon's unk_942 is 1
} // namespace chair_weapon_parameter
struct ChairWeaponState
{
    WeaponBase base{}; // +0
    u32 taker_uses = 0; // +0xE54, parameters +0x5F8: a taker that is no player gets these
    u32 player_uses = 0; // +0xE58, parameters +0x5FC: a player gets these; 0 sets unk_941
    u32 uses = 0; // +0xE5C, left; a landed swing takes one
    u32 total_uses = 0; // +0xE60, taken at the pick-up (message 0x91)
    // +0xE64: this swing has counted (message 0x5A); message 0x92 clears it, and the contact needs it clear.
    u32 struck = 0;
    u32 spent = 0; // +0xE68, no uses left, and the next message 0x92 drops it
    u32 effect = 0; // +0xE6C, the object made from effect_name, by address
    std::array<char, 0x80> effect_name{}; // +0xE70, parameters +0x604
    std::array<u8, 0x18> unk_EF0{}; // +0xEF0, never read or written by translated code
};
static_assert(sizeof(ChairWeaponState) == 0xF08);

struct RuntimeWeaponActor
{
    std::span<u8> bytes;
    Actor& actor;
    NativeActorFactoryInstance& view;
    u16 type; // The factory type, which picks vtable +0x3C / +0x40.
    GhoulyController& controller; // The DAT_00512B74 entry's object.
    u32& random; // DAT_0054DD7C.
    std::function<std::span<const u8>(u32, size_t)> executable; // Objparams pointers (XBE addresses).
    AnimationResourceRegistry& registry; // 0x00035E90.
    ResourceErrorState& error; // 0x00032700.
    std::span<const RuntimeModelNodeType> model_types;
    std::function<const ActorMatrixBindings&(const Actor&)> matrix_bindings;
    const RuntimeShaderMatrixBoundaries& matrix_boundaries;
    entity_detail::ListHead& global_attachments; // DAT_00545984, 0x0012AFE0's second list.
    // 0x00107720 / 0x00107520 with +8 this actor's address and the patch applied; the scene keeps the object.
    std::function<u32(const char*, const std::function<void(std::span<u8>)>&)> create_object;
    std::function<void(std::span<u32>)> base_message; // 0x0003EF70.
    std::function<void(std::span<u32>)> message; // Vtable +0xC of this weapon.
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // DAT_00511AA4 node - 0x900, as an address.
    std::function<std::span<u8>(u32)> bytes_at; // An object's bytes by address.
    std::function<void(u32, std::span<u32>)> send; // Vtable +0xC of the object at an address.
    std::function<u32()> self_address; // This weapon's retail address.
    ActorFrameContext& frame; // The base update's scene values (0x000420D0's steps).
    const i32& blocked_players; // DAT_004E1C34, the prompt lock.
    std::function<std::vector<RuntimeDirectorActor>()> marks; // DAT_00511B38 nodes in list order.
    // DAT_00511B40 actors in list order, as 0x00129420 reads them; only the 0x1A000 tests are viewed.
    std::function<std::vector<RuntimeScriptActorQueryView>()> case_actors;
    const RuntimeScriptActorQueryContext& query;
    const u32& runtime_flags; // DAT_0050EA9C.
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor; // 0x0013F2C0's views.
    const entity_detail::ListHead& players; // DAT_00511B1C.
    const RuntimeActorClock& clock;
    std::function<Actor*(u32)> actor_at; // The actor at a retail address; raises when absent.
    std::function<ActorBaseContext&()> base; // The actor base's views (callouts for 0x0003BE20).
    const Vec3& default_euler; // DAT_0050DD74.
    u8* const& navigation_nodes; // DAT_0050E24C.
    RuntimeWeaponServices& services;
    ActorSpawnLink spawn_link{nullptr, nullptr, &actor}; // +0x934 on DAT_00511B24.
    std::vector<u8> pieces; // +0x2A4, 0x0003F170.
    std::array<std::span<const u8>, 5> accepted_resources{}; // +0x948 rows' +8.
    const AnimationTableResourceViews* animation_tables = nullptr; // The animtable views behind the row +8 AIDs.
    std::vector<RuntimeBodyModelNode> model_nodes; // Actor +0x204's node table, as 0x0013F150 reads it.
    RuntimeBodyCollisionState* markers = nullptr; // +0x998, 0x0013F150; +0x99C is the class's contact test.
    std::vector<const u8*> grab_shapes; // +0xAB8, count +0xAB4.
    std::span<const u8> hand_texture; // +0xAD8.
    std::vector<const EntityShapeNode*> spawn_points; // +0xAC8, count +0xAC4; the actor's spawn_points.
    std::vector<const u8*> flagged_markers; // +0xAE8, count +0xAE6.
    entity_detail::NavigationCellRegistration* cells = nullptr; // +0xCFC, grid +0xD00.
    entity_detail::NavigationPathSearch* path_search = nullptr; // +0xD04.
    GhoulySpawnRecord* spawn_record = nullptr; // +0xC04, 0x00148430's ghoulybox.
    GhoulySpawnPool* collision_overrides = nullptr; // +0xE7C of 0xA9, +0xE8C of 0xA8, +0xF40 of 0xA7: 0x0013EBE0's pool.
    const AnimationRecord* flight_animation = nullptr; // +0xE64 of types 0xA7 and 0xA8, 0x0003AAA0's record.
    RuntimeBodyAttachmentLists* attachment_lists = nullptr; // Actor +0x1B8 / +0x1B0 and DAT_00506930 for 0x0013B810.
    RuntimeCutsceneInstance* cutscene = nullptr; // +0xFE0 of type 0xA0, 0x0004D680's instance.
    const u8* candle_binding = nullptr; // +0xE60 of type 0xA1, 0x000408D0's binding 0.
    std::unique_ptr<std::array<u8, 0x98>> candle_light; // +0xE64 of type 0xA1: its DAT_00509B7C record.
    std::span<const u8> candle_flame; // +0xE6C of type 0xA1.
    std::span<const u8> candle_glow; // +0xE70 of type 0xA1.
    std::function<std::span<const u8>(u32)> model_source; // Retail words in this actor's model space.
    std::function<RuntimeSegmentContactActor&(u32)> contact_actor_at; // The contact view of the actor at an address.
    std::function<u32(const FactoryObject&)> object_address; // A scene object's retail address.
    Actor* actor_or_null(u32 address) const // A retail actor word: zero is no actor.
    {
        return address == 0 ? nullptr : actor_at(address);
    }
    ActorMatrixSetup matrices; // +0x288..+0x2A0.
};

bool initialize_runtime_weapon_actor(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x00145C80
bool initialize_runtime_chair_weapon(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000CEFE0
void block_runtime_weapon_spawn(RuntimeWeaponActor& weapon); // Xbox 0x00147860, EBX actor
// Script type 0x39 over the weapons (DAT_00511B24, node at +0x934): a zero `unblock` blocks each one's spawn
// (0x00147860); any other lifts one block, and a weapon left with none registers its model anchors again.
void step_runtime_weapon_spawn_blocks(i32 unblock,
    const std::function<RuntimeWeaponActor&(Actor&)>& weapon_of); // Xbox 0x0012D050, EDI unblock
void set_runtime_weapon_mask(RuntimeWeaponActor& weapon, u8 value); // Xbox 0x00146230, ESI actor, EAX value
void register_runtime_weapon_cells(RuntimeWeaponActor& weapon); // Xbox 0x00145BC0, EAX actor
void start_runtime_weapon_idle(RuntimeWeaponActor& weapon); // Xbox 0x00144D00, ESI actor
bool start_runtime_weapon_animation(RuntimeWeaponActor& weapon, const char* name); // Xbox 0x00144D40, EAX actor
void attach_runtime_weapon_volume(RuntimeWeaponActor& weapon); // Xbox 0x00146310, EAX actor
void change_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x0014A290, ESI actor, EDI state
void start_runtime_weapon_scene(RuntimeWeaponActor& weapon); // Xbox 0x00144FD0, EAX actor
void bind_runtime_weapon_group(RuntimeWeaponActor& weapon); // Xbox 0x00148600, ESI actor
void release_runtime_weapon_holder(RuntimeWeaponActor& weapon, u32 actor); // Xbox 0x001462B0, ESI weapon, EAX actor
void reset_runtime_weapon_state(RuntimeWeaponActor& weapon); // Xbox 0x0014A0D0, ESI actor
void destroy_runtime_weapon_actor(RuntimeWeaponActor& weapon); // Xbox 0x00145E80
// Vtable +0x10 of the weapon classes.
void update_runtime_weapon_actor(RuntimeWeaponActor& weapon); // Xbox 0x0014A2E0
void update_runtime_chair_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CF050, vtable +0x44
void update_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x0014A220, vtable +0x44
void notify_runtime_weapon_grab_points(RuntimeWeaponActor& weapon); // Xbox 0x00147430, EAX actor
void update_runtime_weapon_bounds(RuntimeWeaponActor& weapon); // Xbox 0x00145A40, EAX actor
void update_runtime_weapon_markers(RuntimeWeaponActor& weapon); // Xbox 0x00144E30, EAX actor
void update_runtime_weapon_variant(RuntimeWeaponActor& weapon); // Xbox 0x00148510, EAX actor
void update_runtime_weapon_pieces(RuntimeWeaponActor& weapon); // Xbox 0x00145740, ESI actor
void finish_runtime_weapon_pieces(RuntimeWeaponActor& weapon); // Xbox 0x00145820, EAX actor
void check_runtime_weapon_world_bounds(RuntimeWeaponActor& weapon); // Xbox 0x0014A6D0, EAX actor
void update_runtime_weapon_prompt(RuntimeWeaponActor& weapon); // Xbox 0x00147160, EAX actor
void update_runtime_weapon_voice(RuntimeWeaponActor& weapon); // Xbox 0x0014A770, EAX actor
// Vtable +0x14 of the weapon classes after the actor draw 0x0003C9E0: whether the draw is skipped for a held
// weapon, and the hand prompt icons 0x00147560 for pass 2.
bool runtime_weapon_draw_hidden(const RuntimeWeaponActor& weapon); // Xbox 0x001459C0's holder test
void draw_runtime_weapon_prompt(RuntimeWeaponActor& weapon, i32 pass); // Xbox 0x00147560, EAX pass, EBX actor
void tick_actor_model_pieces(const Actor& actor, std::span<u8> actor_bytes, std::vector<u8>& pieces, ActorMatrixSetup& matrices,
    float step); // Xbox 0x0003FB80
bool actor_model_pieces_settled(const Actor& actor, const std::vector<u8>& pieces); // Xbox 0x00040040, EAX actor
Actor* find_nearest_runtime_case_actor(const Vec3* point, Actor& requester, u32 flags, float* distance,
    const std::vector<RuntimeScriptActorQueryView>& candidates,
    const RuntimeScriptActorQueryContext& context); // Xbox 0x001292F0, EAX point, ECX flags
bool runtime_frame_timer_elapsed(float& timer, const Actor& actor,
    const ActorFrameContext& frame); // Xbox 0x00042360, EAX timer, ECX actor
// Message 0x28 from the holder's body (+0x10): plays the weapon's animation for the holder's new action.
void animate_runtime_weapon_with_holder(RuntimeWeaponActor& weapon,
    std::span<const u32> message); // Xbox 0x00144B80, EAX message
void dispatch_runtime_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x001463D0
// The weapon breaks when hit: by the player at message +0x10 (8), or by an attacker (EDI) for a category.
void break_runtime_weapon_by_player(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x00146A60, ESI actor, EBX message
u32 break_runtime_weapon_by_attacker(RuntimeWeaponActor& weapon, Actor* attacker, u16 category); // Xbox 0x00146B80, ESI actor, EDI attacker
// The weapon's ghoulybox controller requests (0xA7 / 0xA9 / 0xAA): a spawned actor, a template at the weapon, or a
// template beside it that hops away (0x96). Each returns the new object's address, or 0.
u32 spawn_runtime_weapon_body(RuntimeWeaponActor& weapon, const char* name, const char* third,
    const char* second, const char* fourth, bool face_view); // Xbox 0x00147FC0
u32 spawn_runtime_weapon_template(RuntimeWeaponActor& weapon, const char* name); // Xbox 0x00148270, EAX actor
u32 spawn_runtime_weapon_hopper(RuntimeWeaponActor& weapon, const char* name); // Xbox 0x00148300, EAX actor
// The chair's contact test (+0x99C): reports the hit to the actor touched, its holder and the chair.
u32 test_runtime_chair_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000CF5F0
u32 report_runtime_weapon_contact(RuntimeWeaponActor& weapon, Actor* holder, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word, u8 kind, u8 strength, u8 style,
    bool count); // Xbox 0x00141890, EAX velocity, EBX point
// A spray's report: message 0x18 to the actor touched, then {0x3F, weapon, velocity, actor, contacts, point, reply} to
// the holder and the weapon; with `count` an answered hit adds to the holder's contact totals.
u32 report_runtime_weapon_spray_contact(RuntimeWeaponActor& weapon, Actor* holder, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u8 kind, u8 strength, u8 style,
    bool count); // Xbox 0x00141C00, EAX velocity, EBX point, EDX style
// The large projectile's report: message 0x19 to the actor hit, then {0x49, ...} to the holder and the weapon.
u32 report_runtime_projectile_contact(RuntimeWeaponActor& weapon, Actor* holder, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word, u8 kind, u8 strength, u8 style,
    bool count, u32 tail); // Xbox 0x00141A30, EAX velocity, EBX point, EDX style
// The large projectile's contact test: report the hit, count it, and keep the actor out of further tests.
u32 test_runtime_projectile_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000CE5D0
// Breaks off piece groups until the remaining fraction is at most the given one.
void chip_runtime_weapon(RuntimeWeaponActor& weapon, const Vec3& direction, const Vec3& velocity,
    float fraction); // Xbox 0x001451C0, ECX direction, EDX actor, EAX velocity
void drop_runtime_chair_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000CF670, ESI actor
bool start_runtime_weapon_prewait(RuntimeWeaponActor& weapon); // Xbox 0x00144CC0, EDI actor
// A held weapon's flagged markers push the holder off walls (message 0x9E).
void push_runtime_weapon_holder(RuntimeWeaponActor& weapon, Actor& holder); // Xbox 0x00147B30, EAX actor
void push_runtime_weapon_marker(RuntimeWeaponActor& weapon, Actor& holder, std::span<const u8, 0x14> shape,
    Vec3& push); // Xbox 0x00147CD0, EAX holder
void show_runtime_weapon_uses(RuntimeWeaponActor& weapon, i32 uses, u32 caption); // Xbox 0x0012DC80, EAX uses
void update_runtime_weapon_cells(RuntimeWeaponActor& weapon, i32 preferred); // Xbox 0x00145C30, EAX actor
// The object at +0xE6C among the actor's +0x1D8 updates: told 0x14 through the actor, then released.
bool release_runtime_actor_frame_object(RuntimeWeaponActor& weapon, u32 object); // Xbox 0x0003DE30, ECX actor, ESI object
void dispatch_runtime_chair_weapon_message(RuntimeWeaponActor& weapon,
    std::span<u32> message); // Xbox 0x000CF0D0
// Vtable +0x3C and +0x40 of the weapon classes.
void enter_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 previous); // Xbox 0x0014A120
void leave_runtime_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Xbox 0x0014A180
void enter_runtime_chair_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 previous); // Xbox 0x000CF400
void leave_runtime_chair_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Xbox 0x000CF510
}
