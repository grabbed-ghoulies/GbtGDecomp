#pragma once
#include "actor_lifecycle.h"
#include "../cutscenes/actor_tracks.h"

namespace gbtg
{
void install_runtime_base_object_class(std::span<ObjectFactoryType> types);
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
    ModelAnimationState pose;
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

private:
    std::unordered_map<const u8*, std::unique_ptr<NativeActorFactoryInstance>> instances_;
};
}
