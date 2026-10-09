#pragma once
#include "../cutscenes/event_runtime.h"
#include <memory>

namespace gbtg
{
// Relocated native pointer views of the scene's raw type-7 resource storage.
// This adapter owns the wider track/camera records; resource bytes stay owned
// by the real animation resource provider.
struct SceneCutsceneResourceStorage
{
    std::vector<CutsceneActorTrackView> tracks;
    std::vector<CutsceneCameraView> cameras;
};
struct SceneCutsceneResourceViews
{
    std::unordered_map<const u8*, std::unique_ptr<SceneCutsceneResourceStorage>> storage;
    std::unordered_map<const u8*, CutsceneResourceView> views;
};
void register_scene_cutscene_resource(std::span<const u8> resource,
    SceneCutsceneResourceViews& output);
void unregister_scene_cutscene_resource(const u8* resource,
    SceneCutsceneResourceViews& output);
// Host pointer adapter around the translated bundle provider's actual callbacks.
// The pools and output must outlive the registry's provider registration.
void register_scene_cutscene_bundle_provider(AnimationResourceRegistry& registry,
    std::span<AnimationBundlePool> pools, SceneCutsceneResourceViews& output,
    i32 category, const void* owner);
extern const std::array<u32, 33> retail_cutscene_event_sizes; // DAT_00423630.
extern const std::array<std::array<char, 128>, 6> retail_cutscene_dialogue_resources; // DAT_00423320.
}
