#pragma once
#include "actor_lifecycle.h"
#include "../cutscenes/actor_tracks.h"

namespace gbtg
{
struct GhoulySpawnPool;
void install_runtime_base_object_class(std::span<ObjectFactoryType> types);
struct NativeActorFactoryInstance;
// Host view of the actor's current model instance (actor +0x204) as the pose routines read it: poses
// +0x164/+0x168, skeleton +0x1BC (joints +0x24, count +0x20), auxiliary targets +0x1CC, node flags +0x294
// and the CPU type-1 node mask record. Node flags written through the view are stored back afterwards.
void refresh_actor_model_animation_view(NativeActorFactoryInstance& instance);
void store_actor_model_animation_view(NativeActorFactoryInstance& instance);
struct NativeActorFactoryInstance
{
    std::span<const u8> bytes;
    ButtonBatScreen bat;
    ActorAnimationState animation;
    RuntimeActorStartupState startup{bat.actor, animation};
    ActorLink bind;
    entity_detail::ShapeProbe shape;
    entity_detail::SegmentProbe segment;
    ActorRenderRegistration render{bat.actor, shape, segment, startup.live};
    ActorAttachmentNode pending{};
    entity_detail::ListHead body_node{}; // Native identity of actor +0x934 for contact-list comparisons.
    entity_detail::ListHead anchors{};
    entity_detail::ListHead live_attachments{};
    void* allocation_8fc = nullptr;
    void* matrices = nullptr;
    GhoulySpawnPool* prop_contacts = nullptr; // Type 0x1A actor +0x934.
    ModelAnimationState pose;
    std::vector<ModelResourceBlock> pose_blocks; // Model CPU records behind pose.resource_blocks.
    RuntimeModelInstance* pose_model = nullptr; // Model the view was loaded from.
    std::array<u8, 8> pose_flags_loaded{}; // Its +0x294 node flags at load.
    ActorFrameState frame{bat.actor, animation, pose, startup.attachments,
        pending, nullptr, anchors};
    CutsceneActorBinding cutscene{frame, render};
};
struct NativeActorFactoryBindings
{
    RuntimeActorStartupContext& startup;
    ObjectFactoryRegistry& factory;
    RuntimeActorSoundQueues& sounds;
    const std::unordered_map<ActorFrameAttachment*, FactoryObject*>& frame_objects;
    std::function<void(Actor&, const void*)> message; // Actual actor vtable+0xC mapping.
    const char* empty_name; // DAT_00546208.
    std::function<u32(const void*)> resource_address;
    std::function<u32(const void*)> pose_address;
    std::function<void(Actor&, const Actor&, const Vec3&, const Vec3&, u16)> shape_message;
};
class NativeActorFactoryOwner
{
public:
    explicit NativeActorFactoryOwner(NativeActorFactoryBindings bindings);
    ~NativeActorFactoryOwner();
    NativeActorFactoryOwner(const NativeActorFactoryOwner&) = delete;
    NativeActorFactoryOwner& operator=(const NativeActorFactoryOwner&) = delete;
    void install(std::span<ObjectFactoryType> types);
    NativeActorFactoryInstance& construct_view(FactoryObject& object);
    NativeActorFactoryInstance& view(const FactoryObject& object);
    void release_view(FactoryObject& object);
    NativeActorFactoryInstance& view(const entity_detail::SegmentProbe& probe);
    NativeActorFactoryInstance& view(const entity_detail::ShapeProbe& probe);
    NativeActorFactoryBindings bindings;
    std::unordered_map<const u8*, Actor*> actors;
    std::unordered_map<Actor*, ActorAnimationState*> animations;
    std::unordered_map<Actor*, ActorRenderRegistration*> render;
    std::unordered_map<const u8*, CutsceneActorBinding*> cutscene_bindings;
    std::function<void(Actor&)> released; // Host bookkeeping keyed by the view's actor, told before it goes.

private:
    std::unordered_map<const u8*, std::unique_ptr<NativeActorFactoryInstance>> instances_;
};
}
