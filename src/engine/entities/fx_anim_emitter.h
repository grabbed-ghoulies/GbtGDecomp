#pragma once
#include "actor_base_messages.h"
#include "actor_frame_update.h"
#include "actor_frame_render.h"
#include "actor_runtime_queries.h"
#include "body_attachment_cleanup.h"
#include "player_messages.h"
#include "intro_notices.h"
#include "../camera/display_view.h"
#include "../graphics/billboard_queue.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
// A player the emitter can be collected by (DAT_00511B40 list, node at actor +0x93C).
struct RuntimeFxEmitterPlayer
{
    Actor& actor;
    std::span<const u8> actor_bytes; // +0x93C, +0xCDC, +0xCE0.
    std::span<const u8> body; // Actor +0x95C: +0x94, +0x884, +0x888.
    const Actor& body_actor; // Body +8.
    const Actor* case_actor; // Body +0x48.
    const RuntimeActorResourceReference& resource; // Actor +0x8C.
    ActorAttachmentEventContext& callouts;
};
// entityAvatarFxAnimEmitter's parameters (0x3C4 bytes) after the actor's 0x334: a powerup pickup.
namespace anim_emitter_parameter
{
constexpr RetailField<u32, 0x334> kind{}; // the powerup kind; 0x10 draws one of DAT_0030E220
constexpr RetailField<float, 0x338> amount{};
constexpr RetailField<u32, 0x33C> extra{};
constexpr RetailField<std::array<char, 0x80>, 0x340> sprite{}; // an AID, or empty for none
constexpr RetailField<u32, 0x3C0> id{}; // kind 1: the challenge bit that, once set, keeps it from appearing
constexpr u32 kSize = 0x3C4;
static_assert(retail_fits(kSize, id));
} // namespace anim_emitter_parameter

// entityAvatarFxAnimEmitter's own part (0x934..0x9E4). The list link (+0x934) and sprite (+0x994) are native views.
namespace anim_emitter_field
{
constexpr RetailField<u32, 0x93C> state{}; // 0 in flight, 1 waiting to be collected, 2 collected and fading
constexpr RetailField<u32, 0x940> kind{};
constexpr RetailField<float, 0x944> amount{};
constexpr RetailField<u32, 0x948> extra{};
constexpr RetailField<u32, 0x94C> id{};
// The callout tokens 0x0013B7C0 records for callouts 0x4A (a count and up to eight tokens, 0x24 bytes).
constexpr RetailRecord<0x950, 0x24> callout_tokens{};
constexpr RetailField<i32, 0x950> callout_count{};
constexpr RetailField<std::array<u32, 8>, 0x954> callout_ids{};
constexpr RetailField<Vec3, 0x974> origin{}; // where the flight started
constexpr RetailField<Vec3, 0x980> landing{}; // its y is the floor it bounces on
constexpr RetailField<float, 0x98C> flight_timer{};
constexpr RetailField<u32, 0x990> bounces{};
constexpr RetailField<float, 0x998> sprite_width{};
constexpr RetailField<float, 0x99C> sprite_height{};
constexpr RetailField<float, 0x9A0> pulse_phase{}; // wraps at 1
constexpr RetailField<float, 0x9A4> second_pulse_phase{}; // wraps at 1.5
constexpr RetailField<u32, 0x9A8> sprite_alpha{}; // its low byte is the sprite's alpha
constexpr RetailField<float, 0x9AC> fade_timer{};
constexpr u32 kSize = 0x9E4;
} // namespace anim_emitter_field

// entityAvatarFxAnimEmitter (type 0xD, vtable 0x0050E108) over the avatar actor (type 2).
struct RuntimeFxAnimEmitter
{
    std::span<u8> bytes;
    Actor& actor;
    u32 address;
    ActorBaseContext& base;
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message; // 0x0003EF70 word adapter.
    std::function<std::span<const u8>(u32)> memory_at; // Pointer words carried by messages.
    ActorAnimationState& animation; // Actor +0x48 table, +0x50 controller.
    ActorFrameState& frame;
    ActorFrameContext& frame_context;
    ActorMatrixSetup& matrices; // Actor +0x288..+0x2A0.
    ActorAttachmentList& attachments; // Actor +0x1AC.
    ActorAttachmentNode& pending; // Actor +0x1B4.
    entity_detail::ListHead& anchors; // Actor +0x1F4.
    const RuntimeActorResourceReference& resource; // Actor +0x8C.
    RuntimeBodyAttachmentLists& attachment_lists;
    const RuntimeActorSoundQueues& sounds;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    std::span<const u8>& sprite; // +0x994.
    entity_detail::ListHead& link; // +0x934.
    entity_detail::ListHead& emitters; // DAT_00511B2C.
    const RuntimeActorClock& clock;
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    std::span<const u8> challenge_bits; // *(DAT_00506C54+8).
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    const std::vector<RuntimeFxEmitterPlayer*>& players; // DAT_00511B40 order.
    const RuntimePlayerStrategyResume& resume; // DAT_0050E764, DAT_0030F144.
    u8& story_byte; // **(DAT_00506C6C+8).
    RuntimeIntroNotices& intro;
    ObjectFactoryRegistry& factory;
    std::function<FactoryObject*(FactoryObject)> keep; // Scene ownership of created objects.
    const char* default_name; // DAT_00546208, for 0x000411A0.
    // 0x00123E70: EDX the emitter id, then the collecting player, powerup kind, amount and extra word.
    std::function<void(u32, RuntimeFxEmitterPlayer&, u32, float, u32)> award;
};
void fx_anim_emitter_object_sizes(ObjectSizes& output); // Xbox 0x00109DB0
bool initialize_runtime_fx_anim_emitter(std::span<const u8> parameters,
    RuntimeFxAnimEmitter& context); // Xbox 0x00109FC0
void release_runtime_fx_emitter_attachments(RuntimeFxAnimEmitter& context); // Xbox 0x00109980, ESI emitter
void destroy_runtime_fx_anim_emitter(RuntimeFxAnimEmitter& context); // Xbox 0x0010A120
void launch_runtime_fx_anim_emitter(std::span<const u32, 3> landing,
    RuntimeFxAnimEmitter& context); // Xbox 0x00109D10
void dispatch_runtime_fx_anim_emitter_message(std::span<u32> message,
    RuntimeFxAnimEmitter& context); // Xbox 0x0010A180
void update_runtime_fx_emitter_flight(RuntimeFxAnimEmitter& context); // Xbox 0x001099C0
void build_runtime_emitter_burst_parameters(std::span<u8, 0x354> parameters, const Actor& actor, u32 address,
    float scale, const char* default_name); // Xbox 0x00069590, ESI actor, EAX parameters
u32 collect_runtime_fx_anim_emitter(const EntityCylinderBound& bounds, RuntimeFxEmitterPlayer& player,
    RuntimeFxAnimEmitter& context); // Xbox 0x0010A6D0
void scan_runtime_fx_emitter_players(RuntimeFxAnimEmitter& context); // Xbox 0x0010A590, EAX emitter
void update_runtime_fx_anim_emitter(RuntimeFxAnimEmitter& context); // Xbox 0x0010A2E0
struct RuntimeFxEmitterDraw
{
    ActorDrawState& state;
    const ActorFrameDrawContext& attachments;
    SceneModelDrawContext& scene;
    BillboardQueue& billboards;
};
bool runtime_fx_anim_emitter_visible(u32 pass, u32 slot, RuntimeFxAnimEmitter& context); // Xbox 0x00109E30
u32 draw_runtime_fx_anim_emitter(i32 kind, RuntimeFxEmitterDraw& draw,
    RuntimeFxAnimEmitter& context); // Xbox 0x00109EE0
}
