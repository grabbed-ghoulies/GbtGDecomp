#pragma once
#include "gbtg/retail_field.h"
#include "body_message_dispatch.h"
#include "body_source_contacts.h"
#include "body_collision.h"
#include "player_body_motion.h"
#include "actor_render_registration.h"
#include "body_animation_commands.h"
#include "actor_model_anchors.h"

namespace gbtg
{
// A body message (an actor message's words): its id at +0, then per-id arguments. See docs/RETAIL_LAYOUTS.md.
namespace body_message_field
{
constexpr RetailField<u32, 0x0> id{};
} // namespace body_message_field

// The control messages' start and end (0x00075C60, 0x00075DD0): for the body whose actor is `actor`.
namespace control_message
{
constexpr RetailField<u32, 0x04> actor{};
constexpr RetailField<u32, 0x08> kind{}; // 3 adds energy, 4 holds it at 1, 8 records callouts, 9 hides the body
constexpr RetailField<u32, 0x0C> amount{};
constexpr RetailField<u32, 0x10> unk_10{}; // kinds 8 and 9 act only when it is 0
} // namespace control_message

// Messages 0x2A (a direct hit), 0x4C and 0x60 (a push): a point; 0x43 and 0x44: an actor.
namespace point_message
{
constexpr RetailField<Vec3, 0x08> point{};
constexpr RetailField<u32, 0x08> actor{};
} // namespace point_message

// Message 0x45: a collision hit; the handler writes its result and whether it was handled back.
namespace collision_hit_message
{
constexpr RetailField<u32, 0x04> result{};
constexpr RetailField<u32, 0x08> handled{};
constexpr RetailField<Vec3, 0x10> normal{};
constexpr RetailField<Vec3, 0x1C> position{};
constexpr RetailField<u8, 0x29> joint{};
constexpr RetailField<u16, 0x2A> kind{};
} // namespace collision_hit_message

// Message 0x19: an impact; a hit from behind is answered in its reply words.
namespace impact_message
{
constexpr RetailField<u32, 0x10> unk_10{};
constexpr RetailField<u32, 0x14> unk_14{};
constexpr RetailField<u32, 0x18> unk_18{};
constexpr RetailField<Vec3, 0x20> direction{};
} // namespace impact_message

// Message 0x9E: movement added to the body.
namespace movement_message
{
constexpr RetailField<Vec3, 0x04> movement{};
} // namespace movement_message

struct RuntimePlayerBodyControlMessages
{
    Actor& actor;
    u8& hidden; // Actor +0x197.
    entity_detail::ListHead& anchors; // Actor +0x1F4, first link at +0x1F8.
    ActorLiveAttachment*& attachment; // Actor +0x8F8.
    const RuntimeActorResourceReference& resource;
    ActorAttachmentEventContext& events;
    RuntimeBodyAttachmentLists& lists;
    EntityBitBlock& flags;
    u32 body_address;
};
void begin_runtime_player_body_control(std::span<u8> body, std::span<const u8> message,
    RuntimePlayerBodyControlMessages& context); // Xbox 0x00075C60
void end_runtime_player_body_control(std::span<u8> body, std::span<const u8> message,
    RuntimePlayerBodyControlMessages& context); // Xbox 0x00075DD0
struct RuntimePlayerBodyMessageViews
{
    RuntimeBodyMessageViews parent;
    RuntimeBodySourceContact* source_contact;
    RuntimeBodyAttachedActor* source;
    std::span<const u8> source_body;
    std::span<const u8> bound;
    const RuntimeBodyContactHitViews* impact = nullptr; // Ids 0x19 and 0x1B: the contacts and their sender.
};
struct RuntimePlayerBodyMessages
{
    RuntimeBodyMessageDispatch& parent;
    RuntimeBodyDamageEntry& damage;
    RuntimePlayerGoalRing*& goals;
    std::span<const u8> address_zero;
    const EntityMaskHost& mask;
    const i32& floor; // Actor +0xCEC.
    ActorRenderRegistration& registration;
    const RuntimeActorClock& clock;
    const u8* tag_rows;
    const char* const* tag_names;
    std::span<const char* const, 4> first_health_names;
    std::span<const char* const, 4> second_health_names;
    std::span<const u8>& settings; // Body +0x268.
    std::span<const u8>& hit; // Body +0x84.
    u32& scene_flags; // DAT_0050EA9C.
    u32& restart_requested; // DAT_00430D0C.
};
void dispatch_runtime_player_body_message(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views,
    RuntimePlayerBodyMessages& context); // Xbox 0x00074080
// entityBodyVampire (type 0x2C): a moving contact (0x19) from an attacker whose +0xDAC is 0xC lands only from
// in front (-20..75 degrees off the actor's yaw, actor +0x18) while the bit block's byte 1 bit 0 is clear; any
// other such contact is answered {1, 0, 1} at +0x10. Every other message goes to the 0x27 parent's 0x00074080.
void dispatch_runtime_vampire_body_message(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views, RuntimePlayerBodyMessages& context, const EntityBitBlock& bits,
    std::span<const u8> actor, u32 body_address); // Xbox 0x00075FC0
void dispatch_runtime_player_body(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views, RuntimePlayerBodyMessages& context,
    RuntimePlayerBodyControlMessages& controls); // Xbox 0x00075B60
}
