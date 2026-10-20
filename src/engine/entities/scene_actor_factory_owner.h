#pragma once
#include "actor_factory_owner.h"
#include "trigger_actor.h"
#include "actor_word_messages.h"
#include "fx_anim_emitter.h"
#include "avatar_motion_blur.h"
#include "weapon_actor.h"
#include "projectile_weapon.h"
#include "bob_weapon.h"
#include "ghouly_cutscene_setup.h"
#include "bone_debris_actor.h"
#include "../graphics/effect_collision_response.h"

namespace gbtg
{
// Scene services for the FX anim emitter (type 0xD) beyond the shared actor bindings.
struct NativeSceneEmitterBindings
{
    ActorFrameContext& frame; // 0x000420D0.
    std::span<u8*> timed_attachments; // DAT_00506930.
    const RuntimeActorClock& clock;
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    std::span<const u8> challenge_bits; // *(DAT_00506C54+8).
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    const std::vector<RuntimeFxEmitterPlayer*>& players; // DAT_00511B40 order.
    const RuntimePlayerStrategyResume& resume;
    u8& story_byte; // **(DAT_00506C6C+8).
    RuntimeIntroNotices& intro;
    entity_detail::ListHead& emitters; // DAT_00511B2C.
    std::function<void(u32, RuntimeFxEmitterPlayer&, u32, float, u32)> award; // 0x00123E70.
};
struct NativeSceneActorClassBindings
{
    NativeActorFactoryOwner& actors;
    RuntimeMarkerObjects& objects;
    ActorEffectStreamContext& effects;
    const i32& prompt_lock;
    const RuntimeAreaDialoguePool*& dialogue;
    u32& random;
    float occupancy_initial;
    std::function<ActorBaseContext&(FactoryObject&)> base_of;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<u32(const void*)> address_of;
    std::function<void(u32, std::span<u32>)> message;
    std::function<FactoryObject*(FactoryObject)> keep;
    const RuntimeActorWordBindings& word_messages;
    NativeSceneEmitterBindings& emitter;
    std::function<FactoryObject*(u32)> object_at; // A scene object by native address, or null.
    std::function<RuntimeActorSnapshots&()> snapshots; // DAT_005462F4 and 0x00116530's views.
    const u32& lights_enabled; // DAT_005120F0.
    const u32& script_lights; // DAT_00512104.
    std::function<u32(const void*, size_t)> heap_address; // A stable retail address for a native heap block.
    // The weapon classes (types 0x0B and 0xAA): objparams pointers (XBE addresses; size 0 gives the C string), the
    // model node types and matrix bindings 0x000400D0 reads, and 0x00040200's matrix boundaries.
    std::function<std::span<const u8>(u32, size_t)> executable;
    std::span<const RuntimeModelNodeType> model_types;
    std::function<const ActorMatrixBindings&(const Actor&)> matrix_bindings;
    const RuntimeShaderMatrixBoundaries& matrix_boundaries;
    std::function<void(FactoryObject&)> refresh_shape_poses; // The model instance's poses at +0x164 / +0x168.
    entity_detail::ListHead& global_attachments; // DAT_00545984.
    std::span<ExecutableResourcePool> pools; // 0x00107720's templates.
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // The object a DAT_00511AA4 node sits in.
    std::function<std::vector<RuntimeDirectorActor>()> marks; // DAT_00511B38 nodes in list order.
    std::function<std::vector<RuntimeScriptActorQueryView>()> case_actors; // DAT_00511B40 actors in list order.
    const RuntimeScriptActorQueryContext& query;
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor; // 0x0013F2C0's views.
    const Vec3& default_euler; // DAT_0050DD74.
    u8* const& navigation_nodes; // DAT_0050E24C.
    RuntimeWeaponServices& weapon_services;
    // The bone debris (type 0x12): 0x000EF2D0 on its own effect node with the scene's particle simulation, the last
    // particle collision (DAT_00506918..), 0x000ED2E0's views and the PAL words particles read.
    std::function<void(ActorEffectNode&, float)> tick_owned_effect;
    const RuntimeEffectCollision& effect_collision;
    std::function<RuntimeModelSoundRelease&()> effect_release;
    const i32& pal; // DAT_005096E0.
    const i32& pal_override; // DAT_005096E4.
};
// Native contexts for the retail area/trigger classes, over the existing
// actor lifecycle owner. All referenced scene services must outlive this owner.
class NativeSceneActorClassOwner
{
public:
    explicit NativeSceneActorClassOwner(NativeSceneActorClassBindings bindings);
    ~NativeSceneActorClassOwner();
    NativeSceneActorClassOwner(const NativeSceneActorClassOwner&) = delete;
    NativeSceneActorClassOwner& operator=(const NativeSceneActorClassOwner&) = delete;
    void install(std::span<ObjectFactoryType> types);
    bool construct_area(FactoryObject& object, std::span<const u8> parameters);
    bool construct_trigger(FactoryObject& object, std::span<const u8> parameters);
    bool construct_cutscene_setup(FactoryObject& object, std::span<const u8> parameters); // Type 0x25.
    void release_cutscene_setup(FactoryObject& object); // Type 0x25, Xbox 0x0006B350.
    void update_cutscene_setup(FactoryObject& object); // Type 0x25, Xbox 0x0006B8B0.
    bool construct_emitter(FactoryObject& object, std::span<const u8> parameters);
    bool construct_motion_blur(FactoryObject& object, std::span<const u8> parameters);
    bool construct_weapon(FactoryObject& object, std::span<const u8> parameters); // Type 0x0B, Xbox 0x00145C80.
    bool construct_chair_weapon(FactoryObject& object, std::span<const u8> parameters); // Type 0xAA, Xbox 0x000CEFE0.
    bool construct_large_projectile(FactoryObject& object,
        std::span<const u8> parameters); // Type 0xA9, Xbox 0x000CD6B0.
    void release_large_projectile(FactoryObject& object); // Type 0xA9, Xbox 0x000CDAA0.
    bool construct_bob_weapon(FactoryObject& object, std::span<const u8> parameters); // Type 0xA0, Xbox 0x000C5C20.
    void release_bob_weapon(FactoryObject& object); // Type 0xA0, Xbox 0x000C5D30.
    void release_weapon(FactoryObject& object); // Type 0x0B, Xbox 0x00145E80.
    void update_weapon(FactoryObject& object); // Vtable +0x10, Xbox 0x0014A2E0.
    bool weapon_draw_hidden(FactoryObject& object); // Vtable +0x14 before 0x0003C9E0, Xbox 0x001459C0.
    void draw_weapon_prompt(FactoryObject& object, i32 pass); // Vtable +0x14 after 0x0003C9E0, Xbox 0x00147560.
    bool defer_release = false; // Keep message views through the actor-base destructor.
    void release_view(const u8* bytes); // Native ownership cleanup after all retail destructors.
    void release_area(FactoryObject& object);
    void release_trigger(FactoryObject& object);
    void release_emitter(FactoryObject& object);
    void release_motion_blur(FactoryObject& object);
    void message(FactoryObject& object, std::span<u32> message);
    void update_trigger(FactoryObject& object);
    void update_emitter(FactoryObject& object); // Vtable +0x10, Xbox 0x0010A2E0.
    u32 draw_emitter(FactoryObject& object, i32 kind, RuntimeFxEmitterDraw& draw); // Vtable +0x14, Xbox 0x00109EE0.
    bool emitter_visible(FactoryObject& object, u32 pass, u32 slot); // Vtable +0x18, Xbox 0x00109E30.
    void update_motion_blur(FactoryObject& object); // Type 0xF vtable +0x10, Xbox 0x00066FE0.
    bool construct_afterimage(FactoryObject& object, std::span<const u8> parameters); // Type 0x10, Xbox 0x000690F0.
    void release_afterimage(FactoryObject& object); // Xbox 0x00069220.
    void update_afterimage(FactoryObject& object); // Type 0x10 vtable +0x10, Xbox 0x00069250.
    void message_afterimage(FactoryObject& object, u32 id); // Type 0x10 vtable +0xC after 0x0003EF70, Xbox 0x00069540.
    bool construct_bone_debris(FactoryObject& object, std::span<const u8> parameters); // Type 0x12, Xbox 0x00067EB0.
    void release_bone_debris(FactoryObject& object); // Xbox 0x00068010.
    void update_bone_debris(FactoryObject& object); // Type 0x12 vtable +0x10, Xbox 0x00068060.
    // Type 0x12 vtable +0xC after 0x0003EF70, Xbox 0x00068700.
    void message_bone_debris(FactoryObject& object, std::span<const u32> message);
    u32 draw_bone_debris(FactoryObject& object, i32 kind, RuntimeBoneDebrisDraw& draw); // Vtable +0x14, 0x000686A0.
    ActorMatrixSetup& bone_debris_matrices(FactoryObject& object); // Actor +0x288..+0x2A0 of a type 0x12 actor.
    // A weapon's +0x998 collision state slot (0x0013F150); `bytes` must be a weapon of this owner.
    RuntimeBodyCollisionState*& weapon_markers(const u8* bytes);
    bool owns_weapon(const u8* bytes) const;
    // Each trigger's ghouly controller and the trigger's actor, for the spawn queues' distance tests.
    void controller_actors(std::unordered_map<const GhoulyController*, Actor*>& output);

private:
    struct TriggerInstance;
    struct CutsceneSetupInstance;
    struct EmitterInstance;
    struct WeaponInstance;
    NativeSceneActorClassBindings bindings_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeAreaActorContext>> areas_;
    std::unordered_map<const u8*, std::unique_ptr<TriggerInstance>> triggers_;
    std::unordered_map<const u8*, std::unique_ptr<CutsceneSetupInstance>> cutscene_setups_;
    std::unordered_map<const u8*, std::unique_ptr<EmitterInstance>> emitters_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeAvatarMotionBlur>> motion_blurs_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeAvatarAfterimage>> afterimages_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBoneDebris>> bone_debris_;
    std::unordered_map<const u8*, std::unique_ptr<WeaponInstance>> weapons_;
};
}
