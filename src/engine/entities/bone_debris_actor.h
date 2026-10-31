#pragma once

#include "avatar_motion_blur.h"
#include "actor_attachment_events.h"
#include "actor_frame_render.h"
#include "actor_frame_update.h"
#include "actor_matrix_sampling.h"
#include "actor_matrix_setup.h"
#include "actor_render_registration.h"
#include "model_sound_lifetime.h"
#include "../graphics/actor_effect_nodes.h"
#include "../graphics/actor_effect_stream.h"
#include "../graphics/scene_model_draw.h"

namespace gbtg
{
// Factory type 0x12 (getter 0x00067E60, vtable 0x00504A18) over the actor (type 2): the pieces a dead actor falls
// into. The copy takes the dead actor's model and pose; each group of joints in the model's part header becomes a
// debris particle of its own effect node (+0x934), and the particles' poses drive the joints' +0x288 rows until every
// particle rests. Bytes: +0x938 the dead actor, +0x93C the time the pieces stay once at rest, +0x940 / +0x944 an
// optional fade timer and its length, +0x948 the fade alpha (0xFF while opaque), +0x94C the landing call made, +0x950
// set once every particle rests, +0x954 the name of an effect each piece may carry (chance +0x9D4), +0x9D8 / +0xA64 /
// +0x9DC an optional shader texture (its parameter, its texture and a copy of its record), +0xA68 / +0xA88 / +0xAA8
// the carried effects, their particles and their count.
// The bone debris' own parameters (0x55C bytes in all) and instance fields (0xAAC bytes in all, 0x00067E40), after
// the actor's. +0x934 the effect node is RuntimeBoneDebris::effect.
namespace bone_debris_parameter
{
constexpr RetailField<u32, 0x334> dead_actor{};
constexpr RetailField<std::array<char, 0x80>, 0x338> effect{}; // the debris effect (0x000ED990)
constexpr RetailField<std::array<char, 0x80>, 0x3B8> appearance{}; // 0x0013AE30's model source name
constexpr RetailField<std::array<char, 0x80>, 0x438> carried_effect{};
constexpr RetailField<float, 0x4B8> carry_chance{};
constexpr RetailField<float, 0x4BC> fade_length{};
constexpr RetailField<float, 0x4C0> stay_time{};
constexpr RetailField<std::array<char, 0x80>, 0x4C4> sound{}; // played on the actor's +0x3EC sound (0x0012A260)
constexpr RetailField<u32, 0x544> has_push_source{};
constexpr RetailField<Vec3, 0x548> push_source{}; // the pieces are pushed away from it
constexpr RetailField<float, 0x554> push_strength{};
// An optional shader texture record (0x88 bytes): +0 the shader's name, +4 the parameter's name, +8 the texture's AID.
constexpr RetailField<u32, 0x558> shader_texture{};
constexpr u32 kSize = 0x55C;
static_assert(retail_fits(kSize, shader_texture));
} // namespace bone_debris_parameter
namespace bone_debris_field
{
constexpr RetailField<u32, 0x938> dead_actor{}; // the dead actor; message 0x7C naming it clears it
constexpr RetailField<float, 0x93C> stay_time{}; // seconds the pieces stay once at rest, then fade (0x0013AF80)
constexpr RetailField<float, 0x940> fade_timer{}; // an optional fade, ticking down
constexpr RetailField<float, 0x944> fade_length{};
constexpr RetailField<u32, 0x948> fade_alpha{}; // 0xFF while opaque; under it the fade draw runs (0x00068610)
constexpr RetailField<u32, 0x94C> landed{}; // the landing callouts (0x16) have run
constexpr RetailField<u32, 0x950> resting{}; // every particle rests
constexpr RetailField<std::array<char, 0x80>, 0x954> carried_effect{}; // an effect each piece may carry
constexpr RetailField<float, 0x9D4> carry_chance{};
constexpr RetailField<u32, 0x9D8> texture_parameter{}; // the shader texture's parameter (0x000F8540)
// The shader texture record copied by 0x00140560; 0x0013AF80 passes it to the fade's after-image (its +0x350).
constexpr RetailField<std::array<u8, 0x88>, 0x9DC> shader_texture{};
constexpr RetailField<u32, 0xA64> texture{}; // the shader texture's texture (0x00035B60)
constexpr RetailField<std::array<i32, 8>, 0xA68> carried_effects{}; // the carried effects' node slots, -1 once let go
constexpr RetailField<std::array<u32, 8>, 0xA88> carried_particles{}; // the particle each carried effect follows
constexpr RetailField<u32, 0xAA8> carried_count{};
constexpr u32 kSize = 0xAAC;
static_assert(retail_fits(kSize, carried_count));
} // namespace bone_debris_field
struct RuntimeBoneDebris
{
    RuntimeAvatarActorViews self;
    u32 address; // The copy's native address.
    std::function<RuntimeAvatarActorViews(u32 address)> actor_at; // A scene actor's views.
    RuntimeActorStartupContext& startup;
    std::span<const RuntimeModelNodeType> model_types;
    std::function<const ActorMatrixBindings&(const Actor&)> matrix_bindings;
    const RuntimeShaderMatrixBoundaries& boundaries; // 0x00040200's matrix helpers.
    ActorMatrixSetup matrices{}; // Actor +0x288..+0x2A0.
    void*& actor_matrices; // The actor's +0x288 block, which the base release 0x0003EBA0 frees.
    ActorEffectStreamContext& effects;
    ActorEffectNode* effect = nullptr; // Actor +0x934.
    std::function<u32(const ActorEffectNode&)> effect_address; // The node's retail address (particle owner).
    std::function<u32(const void*)> resource_address; // A loaded resource's retail address.
    std::span<float, 12> sampling_scratch; // DAT_0050D7A0.
    float*& sampling_active; // DAT_0050DD70.
    const Vec3& origin; // DAT_0050DD80.
    std::function<std::span<const u8>(u32)> model_source; // Retail words in this actor's model space.
    u32& random; // DAT_0054DD7C.
    const i32& pal; // DAT_005096E0.
    const i32& pal_override; // DAT_005096E4.
    const RuntimeActorClock& clock;
    ActorFrameState& frame; // 0x000420D0's views.
    ActorFrameContext& frame_context;
    std::function<void(ActorEffectNode&, float)> tick_effect; // 0x000EF2D0 with the scene's particle simulation.
    const Vec3& collision_normal; // DAT_00506924: the last particle collision's normal.
    ActorAttachmentEventContext& callouts; // 0x0003BE20 over the actor's +0x8C records.
    ObjectFactoryRegistry& factory;
    std::function<FactoryObject*(FactoryObject)> keep; // The scene keeps created objects.
    const char* default_name; // DAT_00546208.
    std::function<RuntimeModelSoundRelease&()> effect_release; // 0x000ED2E0's views.
    ActorRenderRegistration& render; // 0x0003E850's probes.
    entity_detail::ShapeProbe& shape; // 0x0003C700's probe.
    entity_detail::ListHead& anchors; // Actor +0x1F4; its next (+0x1F8) is the first anchor.
    RuntimeActorSoundQueues& sounds;
    std::function<std::span<const u8>(u32)> bytes_at; // Retail words at an address (parameter +0x558's record).
    std::function<u8*(u32)> texture_bytes; // A texture's bytes by its address (0x000F86F0, 0x000F8640).
    RuntimeShaderTextureParameter* texture_parameter = nullptr; // +0x9D8.
    std::span<const u8> texture{}; // +0xA64.
};
void bone_debris_object_sizes(ObjectSizes& output); // Xbox 0x00067E40
// One group of the part header: its joints' rows and a debris particle for the group (or for each joint), with an
// optional push away from `source` and the carried effect (ECX header, EAX group).
void spawn_runtime_bone_debris_group(std::span<const u8> header, i32 group, i32& rows, i32& particles,
    const Vec3* source, float strength, RuntimeBoneDebris& context); // Xbox 0x00068970
bool create_runtime_bone_debris_effect(std::span<const u8> parameters,
    RuntimeBoneDebris& context); // Xbox 0x00068850, EAX actor
bool initialize_runtime_bone_debris(std::span<const u8> parameters, RuntimeBoneDebris& context); // Xbox 0x00067EB0
void destroy_runtime_bone_debris(RuntimeBoneDebris& context); // Xbox 0x00068010
// Moves the dead actor's playing sounds (slots other than 4 and 7) to the copy (ECX the dead actor).
void take_runtime_actor_sounds(RuntimeAvatarActorViews& source, RuntimeBoneDebris& context); // Xbox 0x00041D90
// The part after the base actor message 0x0003EF70.
void message_runtime_bone_debris(std::span<const u32> message, RuntimeBoneDebris& context); // Xbox 0x00068700
// The views 0x0013AF80 reads: the actor (EAX), its bytes (+0x19B) and address, and the scene's factory.
struct RuntimeActorFade
{
    const Actor& actor;
    std::span<const u8> bytes;
    u32 address;
    ObjectFactoryRegistry& factory;
    const std::function<FactoryObject*(FactoryObject)>& keep;
    const char* default_name; // DAT_00546208.
};
// An after-image of the actor that fades over `duration`; `tint` (EDI) is three words for parameters +0x340, or null.
bool spawn_runtime_actor_fade(float duration, u32 sound, u32 glow, const float* tint,
    RuntimeActorFade& context); // Xbox 0x0013AF80
void update_runtime_bone_debris(RuntimeBoneDebris& context); // Xbox 0x00068060
struct RuntimeBoneDebrisDraw
{
    ActorDrawModelView* model; // Actor +0x204.
    u32 material_second; // DAT_00427424.
    SceneModelDrawContext& draw;
};
u32 draw_runtime_bone_debris_fade(i32 kind, RuntimeBoneDebrisDraw& draw,
    RuntimeBoneDebris& context); // Xbox 0x00068610, EAX actor, ECX kind
u32 draw_runtime_bone_debris(i32 kind, RuntimeBoneDebrisDraw& draw, RuntimeBoneDebris& context); // Xbox 0x000686A0
}
