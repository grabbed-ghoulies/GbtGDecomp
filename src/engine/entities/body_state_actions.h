#pragma once
#include "body_state_fields.h"
#include "gbtg/object_factory.h"
#include "gbtg/model_runtime.h"
#include "gbtg/sound_runtime.h"
#include "entity_types.h"
#include "body_animation_setup.h"
#include "player_attachment.h"
#include "body_action_selection.h"
#include "gbtg/retail_field.h"
#include <array>
#include <functional>

namespace gbtg
{
// Native views of the body a state action belongs to (state +8 is the body's address).
struct RuntimeBodyStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage*& bits; // Body +0x15C.
    AnimationController* animation; // Actor +0x50.
    entity_detail::ShapeProbe& shape; // Actor shape probe registered by 0x0003C770.
    RuntimeModelInstance* secondary; // Actor +0x310.
    i32& secondary_mode; // Actor +0x30C.
    RuntimeActorSoundQueues& sounds;
    std::span<u32> node_states; // DAT_005078D8.
    RuntimeBodyAttachedActor*& attached; // Body +0x4C.
    RuntimeBodyAnimationReceiver*& receiver; // Body +0x14.
    std::function<void(std::span<const u32>)> body_message; // Actual body vtable +0xC.
    RuntimeBodyActionSelection& actions; // Body animation selection (0x0001AED0, 0x0001B020).
    // XBE addresses held in executable parameters; size 0 views a NUL-terminated string.
    std::function<std::span<const u8>(u32 address, size_t size)> resolve;
    std::span<const u8> mind{}; // Actor +0x958, or empty.
    std::function<Vec3(u32)> actor_position{}; // 0x000412A0 of the actor at an address.
};
u8 set_runtime_body_shape_mode(u8 mode, RuntimeBodyStateContext& context); // Xbox 0x0001FEB0, BL mode, EDI body
void actor_move_object_sizes(ObjectSizes& output); // Xbox 0x00095530

// Type 0x87, entityStateStandAlert: the body stands, optionally holding still, pausing its animation, showing a model
// node or playing a sound, and takes its next key from the stick, the mind's alert or its own movement. State +0xC
// is the next key and +0x10 the state clock (`body_state_field`).
namespace stand_alert_parameter
{
constexpr RetailField<u16, 0x0C> input_key{};
constexpr RetailField<u16, 0x0E> alert_key{};
constexpr RetailField<u32, 0x10> moving_key{};
constexpr RetailField<u32, 0x14> raises_counters{};
constexpr RetailField<u32, 0x18> pauses_animation{};
constexpr RetailField<u32, 0x1C> holds{};
constexpr RetailField<u32, 0x20> node_slot{};
constexpr RetailField<u32, 0x24> keeps_node{};
constexpr RetailField<u32, 0x28> detaches_secondary{};
constexpr RetailField<u32, 0x2C> clears_secondary_mode{};
constexpr RetailField<u32, 0x30> settles{};
constexpr RetailField<std::array<char, 0x80>, 0x34> sound{}; // an inline cue name; empty plays none
constexpr RetailField<u32, 0xB4> body_bit{};
constexpr RetailField<u32, 0xB8> shape_mode{}; // 0 keeps the body's; otherwise its low byte is set
constexpr u32 kSize = 0xBC;
static_assert(retail_fits(kSize, shape_mode));
} // namespace stand_alert_parameter
struct StandAlertState
{
    BodyStateBase base{}; // +0
    u16 input_key = 0; // +0x18, the next key once the stick is pushed
    u16 alert_key = 0; // +0x1A, the next key while the mind is alert
    u32 moving_key = 0; // +0x1C, the body's moving action (vtable +0x40) is the next key while it moves
    u32 raises_counters = 0; // +0x20, body +0x309..+0x30C are raised
    u32 pauses_animation = 0; // +0x24, the actor's animation is paused
    u32 holds = 0; // +0x28, body +0x258 is raised; the turn is left alone
    u32 node_slot = 0; // +0x2C, a model node shown while the state lasts, or 0xFFFFFFFF
    u32 keeps_node = 0; // +0x30, the node stays shown afterwards
    u32 detaches_secondary = 0; // +0x34, the body's secondary attachment is let go at the end
    u32 settles = 0; // +0x38, the body's turn settles each tick (0x00134720)
    u32 clears_secondary_mode = 0; // +0x3C, actor +0x30C is 0 while the state lasts
    i32 saved_secondary_mode = 0; // +0x40
    u32 sound_started = 0; // +0x44
    u32 body_bit = 0; // +0x48, raised while the state lasts
    u32 saved_shape_mode = 0; // +0x4C, the body's shape mode at entry, in the low byte
};
static_assert(sizeof(StandAlertState) == 0x50);

// One of the walk alert's and the fly state's speed bands (0x10 bytes): the animation for the band, its durations at
// the band's slowest and fastest, and the band's top speed (over the body's step speed). The parameters' rows hold
// the animation's name (an XBE string) where the state's hold whether it was found.
namespace walk_alert_row_field
{
constexpr RetailField<u32, 0x0> animation{};
constexpr RetailField<float, 0x4> lower_duration{};
constexpr RetailField<float, 0x8> upper_duration{};
constexpr RetailField<float, 0xC> top_speed{};
constexpr u32 kSize = 0x10;
static_assert(retail_fits(kSize, top_speed));
} // namespace walk_alert_row_field

// Type 0x8E, entityStateWalkAlert: the stick walks the body through its speed bands. State +0xC is the next key and
// +0x10 the state clock (`body_state_field`).
namespace walk_alert_parameter
{
constexpr RetailField<i16, 0x0C> row_count{};
constexpr RetailField<u32, 0x10> rows{}; // the rows' XBE address
constexpr RetailField<std::array<char, 0x80>, 0x14> sound{}; // an inline cue name; empty plays none
constexpr RetailField<u16, 0x94> stop_key{};
constexpr RetailField<u16, 0x96> alert_key{};
constexpr RetailField<u32, 0x98> settles{};
constexpr RetailField<u32, 0x9C> stops_at_once{};
constexpr RetailField<float, 0xA0> stick_limit{};
constexpr RetailField<u32, 0xA4> holds_heading{};
constexpr RetailField<float, 0xA8> hold_heading_above{};
constexpr RetailField<u32, 0xAC> alert_when_calm{};
constexpr RetailField<u32, 0xB0> fixed_turn_ratio{};
constexpr RetailField<u32, 0xB4> body_bit{};
constexpr u32 kSize = 0xB8;
static_assert(retail_fits(kSize, body_bit));
} // namespace walk_alert_parameter
namespace walk_alert_field
{
constexpr RetailField<u16, 0x1A> stop_key{}; // the next key once the body stops
constexpr RetailField<u16, 0x1C> alert_key{}; // the next key while the mind is alert
constexpr RetailField<u32, 0x20> settles{}; // the body's turn settles while it walks (0x00134720)
constexpr RetailField<u32, 0x24> stops_at_once{}; // stop_key without waiting for the actor to slow down
constexpr RetailField<float, 0x28> stick_limit{}; // the most of the stick's push that counts
constexpr RetailField<i16, 0x2C> row_count{};
constexpr RetailField<u16, 0x2E> band{}; // the row playing now
constexpr RetailRecord<0x30, walk_alert_row_field::kSize, 4> rows{};
constexpr RetailField<u32, 0x70> sound_started{};
constexpr RetailField<u32, 0x74> holds_heading{}; // above hold_heading_above the actor keeps its own heading
constexpr RetailField<float, 0x78> hold_heading_above{};
constexpr RetailField<u32, 0x7C> alert_when_calm{}; // alert_key is taken while the mind is not alert instead
constexpr RetailField<u32, 0x80> fixed_turn_ratio{}; // 0x00133D50's fixed ratio
constexpr RetailField<u32, 0x84> body_bit{}; // raised while the state lasts
constexpr u32 kSize = 0x88;
static_assert(retail_fits(kSize, body_bit));
} // namespace walk_alert_field

// Type 0x6D, entityStateFly: the walk alert's rows steered in the air. State +0xC is the next key and +0x10 the
// state clock (`body_state_field`).
namespace fly_state_parameter
{
constexpr RetailField<i16, 0x0C> row_count{};
constexpr RetailField<u32, 0x10> rows{}; // the rows' XBE address
constexpr RetailField<std::array<char, 0x80>, 0x14> sound{}; // an inline cue name; empty plays none
constexpr RetailField<u16, 0x94> stop_key{};
constexpr RetailField<u16, 0x96> alert_key{};
constexpr RetailField<float, 0x98> stick_limit{};
constexpr RetailField<u32, 0x9C> holds_heading{};
constexpr RetailField<float, 0xA0> hold_heading_above{};
constexpr RetailField<u32, 0xA4> loop{}; // the looping animation's name, an XBE string
constexpr u32 kSize = 0xA8;
static_assert(retail_fits(kSize, loop));
} // namespace fly_state_parameter
namespace fly_state_field
{
constexpr RetailField<u16, 0x1A> stop_key{}; // the next key once the body stops
constexpr RetailField<u16, 0x1C> alert_key{}; // the next key while the mind is alert
constexpr RetailField<float, 0x20> stick_limit{}; // the most of the stick's push that counts
constexpr RetailField<u32, 0x24> holds_heading{}; // above hold_heading_above the actor keeps its own heading
constexpr RetailField<float, 0x28> hold_heading_above{};
constexpr RetailField<i16, 0x2C> row_count{};
constexpr RetailField<u16, 0x2E> band{}; // the row playing now
constexpr RetailRecord<0x30, walk_alert_row_field::kSize, 4> rows{};
constexpr RetailField<u32, 0x70> sound_started{};
constexpr RetailField<u32, 0x74> loop_found{};
constexpr u32 kSize = 0x78;
static_assert(retail_fits(kSize, loop_found));
} // namespace fly_state_field
// 0x000A5FD0 and the band code read both states' rows through the same offsets.
static_assert(fly_state_field::row_count.offset == walk_alert_field::row_count.offset
    && fly_state_field::band.offset == walk_alert_field::band.offset
    && fly_state_field::rows.offset == walk_alert_field::rows.offset);

void stand_alert_object_sizes(ObjectSizes& output); // Xbox 0x000A2510
bool initialize_runtime_stand_alert(StandAlertState& state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A2590
void destroy_runtime_stand_alert(StandAlertState& state, RuntimeBodyStateContext& context); // Xbox 0x000A2790
void walk_alert_object_sizes(ObjectSizes& output); // Xbox 0x000A55D0
bool initialize_runtime_walk_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A5650
void destroy_runtime_walk_alert(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A5950
// Type 0x6D, entityStateFly (getter 0x00090D30, vtable 0x00505880): `fly_state_field`, with a looping animation
// under the band's.
void fly_state_object_sizes(ObjectSizes& output); // Xbox 0x00090D10
bool initialize_runtime_fly_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x00090D90
void destroy_runtime_fly_state(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x00091110
// The animation record 0x000A5650 found for the walk alert's (or fly state's) speed band `index`. The record lives
// on the host, so the band's native word only says whether one was found.
const AnimationRecord* runtime_walk_alert_record(std::span<const u8> state, u16 index);
struct RuntimeHitStateLifecycle;
struct RuntimeDizzyStateContext;
struct RuntimeKnockdownContext;
struct RuntimeDieStateContext;
struct RuntimeAnimStateContext;
struct RuntimePickupStateContext;
struct RuntimePlayerDeathContext;
struct RuntimeGrabbedStateContext;
struct RuntimeThrowStateContext;
// Installs type 9 actorMove (getter 0x0002B5A0, vtable 0x004DD72C) and the state classes whose lifecycles
// are translated: 0x64 (getter 0x0008C940, vtable 0x00505760), 0x7A (getter 0x0009C570, vtable 0x00505A20), 0x7C (getter 0x0009DDA0, vtable 0x00505A60), 0x73 entityStateHit (getter 0x00095740, vtable 0x00505940), 0x74 entityStateHitFurnace
// (getter 0x000966B0, vtable 0x00505960), 0x87 entityStateStandAlert
// (getter 0x000A2530, vtable 0x00505BC0), 0x70 entityStateGrabbed (getter 0x000942F0, vtable 0x005058E0), 0x69 the
// player's death (getter 0x0008EED0, vtable 0x00505800), 0x88
// entityStateStruggle (getter 0x000A2B30, vtable 0x00505BE0) and
// 0x8E entityStateWalkAlert (getter 0x000A55F0, vtable
// 0x00505CA0), 0x8F, the skeletons' walk toward their target (getter 0x000A6220, vtable 0x00505CC0) and 0x8A, a
// timed hold (getter 0x000A34C0, vtable 0x00505C20), and 0x8B, the throw (getter 0x000A37F0, vtable 0x00505C40). The context resolvers map a state's body address (+8) to its native body views.
void install_runtime_body_state_classes(std::span<ObjectFactoryType> types,
    std::function<RuntimeBodyStateContext(u32)> context_of, std::function<RuntimeHitStateLifecycle(u32)> hit_of,
    std::function<RuntimeDizzyStateContext&(u32)> dizzy_of, std::function<RuntimeKnockdownContext&(u32)> knockdown_of,
    std::function<RuntimeDieStateContext&(u32)> die_of, std::function<RuntimeAnimStateContext&(u32)> anim_of,
    std::function<RuntimePickupStateContext&(u32)> pickup_of,
    std::function<RuntimePlayerDeathContext&(u32)> player_death_of,
    std::function<RuntimeGrabbedStateContext&(u32)> grabbed_of,
    std::function<RuntimeThrowStateContext&(u32)> throw_of = {});
}
