#pragma once
#include "curve_sampling.h"
#include "instance_construction.h"
#include "../entities/actor_frame_update.h"
#include "../entities/actor_render_registration.h"

namespace gbtg
{
void set_cutscene_actor_replacement(char* replacement_source, char* replacement_target,
    const char* source, const char* target); // Xbox 0x0004E610
struct CutsceneActorTrackView
{
    const char* model; // +0
    std::span<const u8> animation; // +8
    u32 flags; // +C
    std::int8_t id; // +10
    const char* auxiliary; // +18
    CutsceneActorPositionView position; // +1C..+3F
    CutsceneActorScaleView scale; // +40..+63
};
struct CutsceneActorFactoryContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    const char* replacement_source; // DAT_004E1AC0
    const char* replacement_target; // DAT_004E1A40
};
FactoryObject create_cutscene_track_actor(const CutsceneActorTrackView* tracks,
    std::int8_t index, i32 suppress_secondary,
    CutsceneActorFactoryContext& context); // Xbox 0x0004DEC0
struct CutsceneActorBinding
{
    ActorFrameState& frame;
    ActorRenderRegistration& render;
};
struct CutsceneActorRowView
{
    Actor* actor; // +0, native pointer mapping
    u32 elapsed; // +4
    u32 previous; // +8
    u32 flags; // +C
    u32 listed; // +10
    u32 initialized; // +14
};
struct CutsceneActorInitializationContext
{
    CutsceneActorFactoryContext& factory;
    std::vector<FactoryObject>& objects;
    const std::unordered_map<const u8*, CutsceneActorBinding*>& bindings;
    AnimationResourceRegistry& animations;
    ResourceErrorState& error;
    u32& random;
    const SkeletonDispatchContext* callbacks;
    ModelNodeFlagCache& cache;
    const AnimationMatrix& reset;
};
struct CutsceneActorReleaseContext
{
    ObjectFactoryRegistry& factory;
    const std::function<FactoryObject*(Actor*)>& objects; // Factory object owning a track actor.
    const std::unordered_map<Actor*, ActorRenderRegistration*>& render;
    std::span<std::span<const u8>> events; // Native pointers for row+4 resources.
    AnimationResourceRegistry& resources;
};
void release_cutscene_actor_row(CutsceneActorRowView* rows, std::int8_t index,
    CutsceneActorReleaseContext& context); // Xbox 0x0004E9E0
void initialize_cutscene_actors(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, CutsceneActorRowView* rows,
    i32 suppress_secondary, i32 unlist,
    CutsceneActorInitializationContext& context); // Xbox 0x0004DC70
std::int8_t find_cutscene_actor_track(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, std::int8_t id); // Xbox 0x0004E950
Actor* find_cutscene_actor(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, const CutsceneActorRowView* rows,
    std::int8_t id); // Xbox 0x0004E980
void set_cutscene_actor_track_state(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, CutsceneActorRowView* rows,
    std::int8_t id, u32 state); // Xbox 0x0004E780
void pause_cutscene_actor(CutsceneActorRowView* rows, std::int8_t index, i32 pause,
    const std::unordered_map<Actor*, ActorAnimationState*>& animations); // Xbox 0x0004E6D0
}
