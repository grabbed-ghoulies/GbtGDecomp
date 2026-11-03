#pragma once
#include "../scene/scene_point_lights.h"

#include "actor_factory_owner.h"
#include "actor_clock.h"
#include "actor_frame_update.h"
#include "actor_frame_render.h"
#include "gbtg/sound_runtime.h"
#include "../graphics/actor_effect_stream.h"
#include "../graphics/actor_snapshots.h"
#include "../graphics/glow_pass.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct RuntimeShaderTextureParameter;
// One actor's model and resource views as 0x0013AE30 reads and writes them.
struct RuntimeAvatarActorViews
{
    std::span<u8> bytes; // The object's bytes: +0x196, +0x19B, +0x1D0.
    Actor& actor;
    RuntimeActorStartupState& state; // +0x8C resource, +0x204 model.
};

// entityAvatarFxMotionBlur (type 15, getter 0x00066900, vtable 0x005049A0) over the avatar actor (type 2): the
// fading copy of a dying actor. Its bytes and parameters are `avatar_motion_blur_field` and
// `avatar_motion_blur_parameter` (avatar_fields.h). +0x1A0 / +0x1A4 are the draw hooks 0x00066B90 / 0x00066C00 with
// +0x1A8 the avatar.
struct RuntimeAvatarMotionBlur
{
    RuntimeAvatarActorViews self;
    std::function<RuntimeAvatarActorViews(u32 address)> actor_at; // A scene actor's views.
    RuntimeActorStartupContext& startup;
    ActorEffectStreamContext& effects;
    RuntimeActorSnapshots& snapshots; // 0x00116750.
    const char* default_name; // DAT_00546208.
    u32 address; // The avatar's native address (+0x1A8).
    std::function<u32(const void*)> resource_address; // A loaded resource's native address.
    std::span<const u8> resource; // +0x94C as a native view.
    const RuntimeActorClock& clock;
    RuntimeActorSoundQueues& sounds;
    ActorFrameState& frame; // 0x000420D0's views of the avatar.
    ActorFrameContext& frame_context;
    RuntimeScenePointLights& lights; // DAT_00511B70..; its count is DAT_005120F0.
    const u32& script_lights; // DAT_00512104.
};

// The draw hooks' views (actor +0x1A0 / +0x1A4).
struct RuntimeAvatarGlow
{
    RuntimeGlowPass& pass;
    const u32& mirror_pass; // DAT_0051120C.
    std::function<u32(u32 address)> word_at; // DAT_00314F48 / DAT_00314F4C, the glow colours.
};

void avatar_motion_blur_object_sizes(ObjectSizes& output); // Xbox 0x000668E0
// Before the copy's model draw: its screen rectangle (or the whole view) becomes the glow capture.
void begin_runtime_avatar_glow(const Actor& actor, RuntimeAvatarGlow& context); // Xbox 0x00066B90
// After it: the capture is composited with the two fading glow colours.
void end_runtime_avatar_glow(std::span<const u8> avatar, RuntimeAvatarGlow& context); // Xbox 0x00066C00
bool initialize_runtime_avatar_motion_blur(std::span<const u8> parameters,
    RuntimeAvatarMotionBlur& context); // Xbox 0x00066E20
void destroy_runtime_avatar_motion_blur(RuntimeAvatarMotionBlur& context); // Xbox 0x00066F50
void tick_runtime_avatar_motion_blur(RuntimeAvatarMotionBlur& context); // Xbox 0x00066FE0
// The phase 3 pulse: 1 elsewhere, else the double eased wave of the remaining time.
long double runtime_avatar_pulse(const RuntimeAvatarMotionBlur& context); // Xbox 0x00066940, EAX avatar
// Phase 1: the effect stream at +0x9D0 and the ONFIRE / BURNDITTY sounds in slots 4 and 5.
void ignite_runtime_avatar(RuntimeAvatarMotionBlur& context); // Xbox 0x00066F70, EAX avatar
void light_runtime_avatar(RuntimeAvatarMotionBlur& context); // Xbox 0x000669D0, EAX avatar
void release_runtime_avatar_light(RuntimeAvatarMotionBlur& context); // Xbox 0x00066A80, EAX avatar

// The source's world pose, scales, model copy, model words and +0x8C resource.
void copy_runtime_actor_appearance(RuntimeAvatarActorViews& actor, RuntimeAvatarActorViews& source,
    const char* name, RuntimeActorStartupContext& startup); // Xbox 0x0013AE30, EAX actor
// The model named by `name` (ESI) or else a copy of the source's live model.
// 0x00040C00 first runs with EDI `default_name` (DAT_00546208).
bool copy_runtime_actor_model(RuntimeAvatarActorViews& actor, RuntimeAvatarActorViews& source, const char* name,
    const char* default_name, RuntimeActorStartupContext& startup); // Xbox 0x00040CA0, EBX actor
// Effect stream `name` (ECX) bound to the avatar (EAX) with options id 0x4D and the given scale.
void start_runtime_avatar_effect(RuntimeAvatarMotionBlur& context, const char* name,
    float scale); // Xbox 0x00066AB0

// Type 16 (getter 0x000690A0, vtable 0x00504A54) over the avatar actor (type 2): a copy of an actor's look that
// fades over its life, the swing trail 0x0013B030 leaves. Its bytes and parameters are `avatar_afterimage_field` and
// `avatar_afterimage_parameter` (avatar_fields.h).
struct RuntimeAvatarAfterimage
{
    RuntimeAvatarActorViews self;
    std::function<RuntimeAvatarActorViews(u32 address)> actor_at; // A scene actor's views.
    RuntimeActorStartupContext& startup;
    const char* default_name; // DAT_00546208.
    const RuntimeActorClock& clock;
    ActorFrameState& frame; // 0x000420D0's views of the copy.
    ActorFrameContext& frame_context;
    ActorRenderRegistration& render; // 0x0003E850's probes.
    entity_detail::ShapeProbe& shape; // 0x0003C700's probe.
    entity_detail::ListHead& anchors; // Actor +0x1F4; its next (+0x1F8) is the first anchor.
    std::function<std::span<const u8>(u32)> bytes_at; // Retail words at an address (parameter +0x350's record).
    std::function<u32(const void*)> address_of; // A loaded texture's retail address.
    std::function<u8*(u32)> texture_bytes; // A texture's bytes by its address (0x000F86F0, 0x000F8640).
    RuntimeShaderTextureParameter* texture_parameter = nullptr; // +0x95C.
    std::span<const u8> texture{}; // +0x960.
};
void avatar_afterimage_object_sizes(ObjectSizes& output); // Xbox 0x00069080
bool initialize_runtime_avatar_afterimage(std::span<const u8> parameters,
    RuntimeAvatarAfterimage& context); // Xbox 0x000690F0
void destroy_runtime_avatar_afterimage(RuntimeAvatarAfterimage& context); // Xbox 0x00069220
void tick_runtime_avatar_afterimage(RuntimeAvatarAfterimage& context); // Xbox 0x00069250
// The part after the base actor message 0x0003EF70: 0x23 drops the copy's probes and model anchors.
void message_runtime_avatar_afterimage(u32 id, RuntimeAvatarAfterimage& context); // Xbox 0x00069540

struct RuntimeAvatarAfterimageDraw
{
    std::span<const u8> bytes;
    Actor& actor;
    ActorDrawModelView* model; // Actor +0x204.
    u32 material_second; // DAT_00427428.
    SceneModelDrawContext& draw;
    RuntimeGlowPass& pass; // With +0x964 set: 0x00062220 before the model, 0x00062490 after.
};
u32 draw_runtime_avatar_afterimage(i32 kind, RuntimeAvatarAfterimageDraw& context); // Xbox 0x00069410
}
