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
// +0x9DC an optional sound, +0xA68 / +0xA88 / +0xAA8 the carried effects, their particles and their count.
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
// An after-image of the actor that fades over `duration` (EAX actor, EDI an optional colour).
bool spawn_runtime_actor_fade(float duration, u32 sound, u32 glow, RuntimeBoneDebris& context); // Xbox 0x0013AF80
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
