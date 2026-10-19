#pragma once
#include "cutscene_scene_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "../audio/event_voices.h"
#include "../camera/camera_pulse.h"
#include "../entities/powerup_awards.h"

namespace gbtg
{
struct GhoulySpawnPool;
struct SceneControlInstance;
struct SceneControlLists;
// One row of DAT_00301920 (stride 0x18): a trophy head's animation and animevents suffixes (+0, +4), its attack
// cutscene prefix (+8) and the float at +0xC.
struct TrophyHeadRow
{
    const char* animation; // +0
    const char* events; // +4
    const char* cutscene; // +8
    float turn; // +0xC
};
// What the trophy-heads control (factory type 0x92, entitySceneControlChapter1Scene2) reaches outside its
// object. SceneActorServices binds it when the native scene starts.
struct TrophyHeadsControlServices
{
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    SceneCutsceneRuntime& cutscenes; // DAT_004E1B40.. and the clips' track views and actor rows.
    AudioEventVoices& voices; // DAT_004E0CE8
    u32& random; // DAT_0054DD7C
    i32& pause_count; // DAT_00510CEC
    i32& transition_pause_count; // DAT_004DEA24
    u32& runtime_flags; // DAT_0050EA9C
    const u8& scene_flags; // **(DAT_00506C74+8)
    u8& story_byte; // Byte +4 of **(DAT_00506C6C+8)
    std::span<const u8> challenge_bits; // *(DAT_00506C54+8)
    i32& chapter_mode; // DAT_00430D0C
    RuntimeCameraPulse& pulse; // DAT_0050447C
    std::span<const TrophyHeadRow> heads; // DAT_00301920
    float& gameplay_time; // DAT_004312D0
    i32& gameplay_mark; // DAT_004312D4
    const i32& gameplay_tick; // DAT_004312D8
    const u32& prompt_owner; // DAT_004EE1EC
    const Vec3& default_euler; // DAT_0050DD74
    const float& freeze; // DAT_004312DC
    const float& dt; // DAT_005096C0
    const float& scene_scale; // DAT_004312C4
    std::function<GhoulySpawnPool*&(Actor&)> shapes; // Actor +0x8FC.
    std::function<u32(Actor&)> actor_address; // A clip actor's retail address.
    std::function<void(std::span<const u8>)> queue_event; // 0x00116810
    std::function<u32()> spawn_player; // 0x00134920 at DAT_0050DD80; the scene keeps the object.
    std::function<std::span<u8>(u32)> player_body; // Player +0x95C.
    std::function<void(u32)> retire_actor; // 0x00112CC0, ESI the actor.
    std::function<void(std::span<const u8>)> create_object; // 0x00107520; the scene keeps the object.
    RuntimePowerupAwardContext& powerups; // 0x00123E70
};
// Native state of a trophy-heads control whose retail words are pointers.
struct TrophyHeadsControlState
{
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x28
    u32 player = 0; // +0x2C, the spawned player's address.
    Actor* head = nullptr; // +0xF8, the attacking head's actor.
};
void trophy_heads_control_object_sizes(ObjectSizes& output); // Xbox 0x000816A0
bool construct_runtime_trophy_heads_control(std::span<u8> state, std::span<const u8> parameters,
    TrophyHeadsControlState& control, TrophyHeadsControlServices& services); // Xbox 0x00081710
void destroy_runtime_trophy_heads_control(std::span<u8> state,
    TrophyHeadsControlServices& services); // Xbox 0x00081AD0
// `reference` is the native pointer a 0x7E/0x7F message carries in its second word.
void message_runtime_trophy_heads_control(std::span<u8> state, std::span<const u32> message,
    const void* reference, TrophyHeadsControlState& control, SceneControlInstance& instance,
    SceneControlLists& lists, TrophyHeadsControlServices& services); // Xbox 0x00081B00
void tick_runtime_trophy_heads_control(std::span<u8> state, TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services); // Xbox 0x00081890
void set_runtime_trophy_heads_state(std::span<u8> state, u8 next, TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services); // Xbox 0x00081CE0
void choose_runtime_trophy_heads(std::span<u8> state,
    TrophyHeadsControlServices& services); // Xbox 0x000821A0, ESI state
u32 accept_runtime_trophy_head(std::span<const u8> state, std::int8_t id,
    TrophyHeadsControlServices& services); // Xbox 0x000820D0, AL id
u32 runtime_trophy_head_has_event(std::span<const u8> state, std::int8_t id,
    TrophyHeadsControlServices& services); // Xbox 0x00082440, EAX state, EDI id
u32 runtime_trophy_head_chosen(std::span<const u8> state, std::int8_t id); // Xbox 0x00082170, ECX state, DL id
void start_runtime_trophy_head_attack(i32 count, i32 id, Actor* head,
    TrophyHeadsControlServices& services); // Xbox 0x00082370, ECX count, EAX id
// The scene-control parameters 0x000873A0 builds for type 0x98 (0x1D8 bytes): EAX the block, ECX the first
// name, EDX the actor (by its retail address), EDI the second name.
void build_runtime_scene_challenge_parameters(std::span<u8> parameters, const char* first, u32 actor,
    float scale_x, float scale_y, const Vec3* position, const Vec3* euler, u32 word_134, const Vec3* offset,
    u32 word_14c, u32 word_150, u32 word_1d4, const char* second, u32& runtime_flags); // Xbox 0x000873A0
std::int8_t find_runtime_cutscene_track(const SceneCutsceneRuntime& cutscenes, std::int8_t id); // Xbox 0x0004F010
// A clip actor's slot-1 animation: started with its events (and slot 0 held), or cleared (slot 0 released and the
// actor's euler reset to DAT_0050DD74) when `animation` is null. DL the track id.
void set_runtime_cutscene_actor_animation(RuntimeCutsceneInstance& instance, std::int8_t id, const char* animation,
    const char* events, const Vec3& default_euler, CutsceneRuntimeContext& context); // Xbox 0x0004E7D0
void hold_runtime_actor_animation_slot(ActorAnimationState& animation, i32 slot); // Xbox 0x0003B310, EAX actor, ECX slot
// The actor's 0x000426F0 bound in its own scale: +0x1C4 width, +0x1C8 top, +0x1CC bottom, +0x1D0 their reach.
void measure_runtime_actor_bound_size(Actor& actor, u32 mask, GhoulySpawnPool*& shapes); // Xbox 0x0003D670, EAX actor
}
