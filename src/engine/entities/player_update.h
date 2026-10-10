#pragma once
#include "actor_clock.h"
#include "actor_frame_update.h"
#include "body_action_selection.h"
#include "body_collision.h"
#include "body_contacts.h"
#include "player_mind_events.h"
#include "../frontend/input_activation.h"
#include "../navigation/floor_tracking.h"
#include "../navigation/occupancy.h"

namespace gbtg
{
// Actor +0x974 and +0x978 count down while decay runs; each sends its message (0x55, 0x56) at zero.
void tick_runtime_player_status_timers(RuntimeBodyAttachedActor& self,
    const RuntimeActorClock& clock); // Xbox 0x00019520, ESI actor

struct RuntimePlayerRoomTracking
{
    RuntimeBodyAttachedActor& self;
    std::span<const u8> body; // Actor +0x95C; u16 +0x30 and float +0x34.
    const OccupancyRecord*& allowed; // Actor +0xCA4.
    const OccupancyRecord*& current; // Actor +0xCA8.
    bool strategy_present; // Actor +0x954 is nonzero.
};
void track_runtime_player_room(RuntimePlayerRoomTracking& context); // Xbox 0x0002B450, EAX actor

struct RuntimeWorldBounds
{
    const u8& enabled; // DAT_0050EA00.
    const float& min_x; // DAT_0050EA04.
    const float& min_y; // DAT_0050EA08.
    const float& min_z; // DAT_0050EA0C.
    const float& max_x; // DAT_0050EA10.
    const float& floor_y; // DAT_0050EA14.
    const float& max_z; // DAT_0050EA18.
};
struct RuntimePlayerBoundsCheck
{
    RuntimeBodyAttachedActor& self;
    const RuntimeWorldBounds& bounds;
    const EntityBitBlock* body_bits; // Body +0x15C.
    const RuntimePlayerGoalRing* ring; // Actor +0x980.
    std::span<const u8> address_zero;
};
void check_runtime_player_bounds(RuntimePlayerBoundsCheck& context); // Xbox 0x0002C980, EAX actor

struct RuntimePlayerUpdate
{
    RuntimeBodyAttachedActor& self; // Retail bytes, native actor and word messages.
    // Actor +0x960 input device and its activation state.
    std::span<u8> input;
    u32 input_address;
    RuntimeInputActivationContext& input_activation;
    const RuntimeInputReleaseContext& input_release;
    const u32& input_frame; // DAT_004DEA58.
    RuntimePlayerGoalRing*& ring; // Actor +0x980.
    std::span<const u8> address_zero;
    // Children; vtable slots are the actual child methods.
    FactoryObject& strategy; // Actor +0x954.
    FactoryObject& mind; // Actor +0x958.
    FactoryObject& body_object; // Actor +0x95C.
    std::function<void(FactoryObject&)> strategy_tick; // Vtable +0x10.
    std::function<void(FactoryObject&, u32)> mind_tick; // Vtable +0x10 with actor +0x960.
    std::function<void(FactoryObject&, u32)> body_tick; // Vtable +0x10 with actor +0x960.
    std::function<void(FactoryObject&)> body_animate; // Vtable +0x14.
    std::function<void(FactoryObject&)> body_settle; // Vtable +0x18.
    std::function<u32(FactoryObject&)> body_supported; // Vtable +0x1C.
    std::function<u16(FactoryObject&, u32)> action_key; // Body +0x744 vtable +0x1C with owner +0x960.
    entity_detail::NavigationPathSearch*& path_search; // Mind +0x1C.
    const u8*& floor_hint; // Mind +0x10; byte +0x19 is the preferred floor.
    // Body +0x95C state.
    RuntimeBodyCollision& body; // Body bytes, body +8 owner, +0x15C bits, +0x710 spheres.
    std::span<const u8> owner_bytes; // Body +8 retail bytes; input word at +0x960.
    EntityBitBlock*& body_bits; // Body +0x15C.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    RuntimeBodyActionSelection& actions; // Body +0x73C table and +0x744 action object.
    // Body contact walk inputs for 0x0013F2C0 with callback 0x0001F8C0.
    RuntimeBodyContactCallback contact_callback;
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor;
    const entity_detail::ListHead& players; // DAT_00511B1C.
    // Actor frame state.
    ActorAnimationState& animation; // Controller +0x50.
    ModelAnimationState& model;
    ActorAttachmentList& attachments; // Actor +0x1AC.
    ActorAttachmentNode& pending; // Actor +0x1B4.
    ActorFrameAttachment*& updates; // Actor +0x1D8.
    entity_detail::ShapeProbe& shape_probe; // Actor +0x110.
    ActorFrameContext& frame; // Animation, attachment and shader boundaries; ease DAT_00509B74.
    const RuntimeActorClock& clock;
    // Actor navigation and occupancy.
    entity_detail::NavigationCellRegistration*& navigation; // Actor +0xCE8.
    RuntimePlayerRoomTracking& room;
    const RuntimeWorldBounds& bounds;
};
void update_runtime_player(RuntimePlayerUpdate& context); // Xbox 0x0002C2B0
}
