#pragma once
#include "actor_tracks.h"
#include "camera_sampling.h"
#include "event_assets.h"
#include "event_audio.h"
#include "event_pause.h"
#include "../camera/cutscene_impulse.h"
#include "../entities/actor_scare_requests.h"
#include "../graphics/actor_effect_stream.h"
#include "../audio/footstep_names.h"
#include "../video/movie_startup.h"
#include "gbtg/model_render.h"

namespace gbtg
{
struct RuntimeDialogueActivationContext;
struct RuntimeMovieStartupContext;
struct CutsceneResourceView
{
    std::span<const CutsceneActorTrackView> tracks;
    std::span<const CutsceneCameraView> cameras;
};
struct CutsceneInstanceRows
{
    std::vector<CutsceneActorRowView> actors;
    std::vector<std::span<const u8>> events;
    std::vector<Vec3> blend_positions;
    std::vector<Vec3> blend_scales;
};
using CutsceneEventCallback = std::function<void(RuntimeCutsceneInstance&, u32, u32)>;
struct CutsceneRuntimeContext
{
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    const std::unordered_map<const u8*, CutsceneResourceView>& views;
    std::unordered_map<RuntimeCutsceneInstance*, CutsceneInstanceRows> instances;
    CutsceneActorInitializationContext& initialization;
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> executable;
    const std::unordered_map<Actor*, FactoryObject*>& objects;
    const std::unordered_map<Actor*, ActorRenderRegistration*>& render;
    const std::unordered_map<Actor*, ActorAnimationState*>& animations;
    const std::unordered_map<Actor*, u32>& actor_addresses;
    const std::unordered_map<Actor*, std::function<void(Actor&)>>& updates; // Actor vtable+10.
    AudioBankStreams& audio;
    ActorEffectStreamContext& effects;
    ActorScareContext& scare;
    RuntimeModelRenderContext& renderer;
    RuntimeSceneFog& fog;
    CutsceneCameraImpulse& impulse;
    CutsceneCameraEventAsset& camera_asset;
    const FootstepSoundNames& footstep_names;
    std::span<const char* const> footsteps; // PTR_DAT_00423620.
    std::span<const u32> event_sizes; // DAT_00423630, bytes per kind.
    const char* dialogue_resources; // DAT_00423320, six names, stride 0x80.
    const Vec3& listener_position;
    u32& random;
    i32 language;
    u32& movie_flag_38;
    u32& movie_flag_3c;
    char* replacement_source;
    char* replacement_target;
    RuntimeDialogueActivationContext* dialogue;
    RuntimeMovieStartupContext* movies;
    std::array<RuntimeMovieEventSlot, 4>& movie_slots;
    i32& movie_mode; // DAT_0050DD8C.
    std::unordered_map<u32, CutsceneEventCallback> event_callbacks; // Retail +150 indirect call.
    std::unordered_map<u32, CutsceneResumeCallback> resume_callbacks; // Retail +160 indirect call.
    std::unordered_map<RuntimeCutsceneInstance*, const u8*> dialogue_events; // Native +164 pointer mapping.
    std::vector<FactoryObject> event_objects;
    void (*free_instance)(void*); // HeapFree boundary paired with engine_heap_allocate.
    std::unordered_map<Actor*, std::function<void(Actor&, i32, i32)>> draws; // Actor vtable+14.
};
void dispatch_runtime_cutscene_event(RuntimeCutsceneInstance* instance,
    std::span<u8> event_state, u8* event, Actor* actor, u32 phase,
    CutsceneRuntimeContext& context); // Xbox 0x0004C850
void prepare_runtime_cutscene_events(u8* events, CutsceneRuntimeContext& context); // Xbox 0x0004C560
const u8* load_runtime_cutscene_events(const char* name, std::span<u8> event_state,
    CutsceneRuntimeContext& context); // Xbox 0x0004C640
void load_runtime_cutscene_camera_events(RuntimeCutsceneInstance& instance,
    const char* name, CutsceneRuntimeContext& context); // Xbox 0x0004E020
void advance_runtime_cutscene_events(RuntimeCutsceneInstance& instance,
    std::span<u8> event_state, u8* events, Actor* actor, float current,
    float previous, i32 ending, i32 restricted,
    CutsceneRuntimeContext& context); // Xbox 0x0004D370
void end_runtime_cutscene_events(RuntimeCutsceneInstance& instance,
    std::span<u8> event_state, u8* events, Actor* actor, float current,
    float previous, CutsceneRuntimeContext& context); // Xbox 0x0004D3E0
void end_all_runtime_cutscene_events(RuntimeCutsceneInstance& instance,
    CutsceneRuntimeContext& context); // Xbox 0x0004E070
RuntimeCutsceneInstance* load_runtime_cutscene(const char* name, u32 flags,
    EntityCamNode* camera, CutsceneRuntimeContext& context); // Xbox 0x0004D680
void release_runtime_cutscene(RuntimeCutsceneInstance* instance,
    CutsceneRuntimeContext& context); // Xbox 0x0004D720
void seek_runtime_cutscene(RuntimeCutsceneInstance& instance, float position,
    float duration, i32 events, CutsceneRuntimeContext& context); // Xbox 0x0004E4A0
void update_runtime_cutscene_actors(RuntimeCutsceneInstance& instance,
    float blend, float position, CutsceneRuntimeContext& context); // Xbox 0x0004E1E0
bool tick_runtime_cutscene(RuntimeCutsceneInstance& instance, float dt,
    EntityCamNode* camera, CutsceneRuntimeContext& context); // Xbox 0x0004DA50
void draw_runtime_cutscene(RuntimeCutsceneInstance& instance, i32 view,
    CutsceneRuntimeContext& context); // Xbox 0x0004DBD0
bool cutscene_resource_exists(const char* name,
    AnimationResourceRegistry& resources); // Xbox 0x000356D0
}
