#pragma once
#include "scene_control_fields.h"
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
// The trophy-heads control's construction parameters (0x108 bytes, 0x000816A0).
namespace trophy_heads_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x08> cutscene{}; // the heads' clip, 0x00081710 starts it
constexpr RetailField<std::array<char, 0x80>, 0x88> head_events{}; // copied to TrophyHeadsControlState::head_events
constexpr u32 kSize = 0x108;
static_assert(retail_fits(kSize, head_events));
} // namespace trophy_heads_parameter

// The two fields 0x00082440 reads in each record of a cutscene events resource.
namespace cutscene_event_field
{
constexpr RetailField<u16, 0x0C> kind{}; // 0 ends the list; DAT_00423630 gives each kind's size
constexpr RetailField<u16, 0x10> value{}; // kind 0xD: a trophy head's id
} // namespace cutscene_event_field

// The trophy-heads control's state at its retail offsets (0x188 bytes, 0x000816A0; see docs/RETAIL_LAYOUTS.md).
struct TrophyHeadsControlState
{
    // +0x28: the heads' cutscene instance (0x0004EDA0); message 0x7F names it.
    RuntimeCutsceneInstance* cutscene = nullptr;
    // +0x2C: the player state 2 spawns (0x00134920), by address; 0 once message 0x8E 7 or 8 retires the player.
    u32 player = 0;
    i32 attacks = 0; // +0x30, attacks so far; 0x00082370 gets the count before this one
    // +0x34: the player's health, carried between attacks: 50 (10 in the alternate modes) at construction, less 10 for
    // each lost challenge (message 0x8E 8). State 2 gives it to the spawned player's body +0x94.
    i32 health = 0;
    // +0x40: A clip position the tick seeks to (0x0004E4A0) once, then clears. Inferred: nothing here writes it.
    float seek_time = 0.f;
    // +0x44: 0x00081CE0 sets it; 0x00081890 switches on it: 1 the heads' clip plays, 2 an attack, 3 the clip has ended
    // (event 0xAF each tick), 4 its end is near (the award, paused), 5 the last challenge lost.
    u16 phase = 0;
    // +0x46: the heads that may attack: up to four ids, -1 past the last (0x000821A0).
    std::array<std::int8_t, 4> chosen{};
    // +0x48: by head id (1..21): set once the head has attacked. Entry 0, which would overlap chosen, is never used.
    std::array<u32, 22> attacked{};
    // +0x9C: by head id (1..21): set while its cutscene row is live after an attack, cleared when the row goes quiet.
    std::array<u32, 22> roaring{};
    u32 head_id = 0; // +0xF4, the attacking head; read as a byte where a track id is wanted
    Actor* head = nullptr; // +0xF8, the attacking head's actor
    // +0x100: set while the control holds DAT_00510CEC and DAT_004DEA24 down (0x00081710); 0x00081AD0 and message 0x7F
    // give them back.
    u32 pause_held = 0;
    std::array<char, 0x80> head_events{}; // +0x104, trophy_heads_parameter::head_events
    // +0x184: set from challenge bit 1 at construction, and by state 4: the end-of-clip state 4 and its award are done.
    u32 challenge_won = 0;
};
void trophy_heads_control_object_sizes(ObjectSizes& output); // Xbox 0x000816A0
bool construct_runtime_trophy_heads_control(std::span<const u8> parameters,
    TrophyHeadsControlState& control, TrophyHeadsControlServices& services); // Xbox 0x00081710
void destroy_runtime_trophy_heads_control(TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services); // Xbox 0x00081AD0
// `reference` is the native pointer a 0x7E/0x7F message carries in its second word.
void message_runtime_trophy_heads_control(std::span<const u32> message,
    const void* reference, TrophyHeadsControlState& control, SceneControlInstance& instance,
    SceneControlLists& lists, TrophyHeadsControlServices& services); // Xbox 0x00081B00
void tick_runtime_trophy_heads_control(SceneControlBase& base, TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services); // Xbox 0x00081890
void set_runtime_trophy_heads_state(u8 next, TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services); // Xbox 0x00081CE0
void choose_runtime_trophy_heads(TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services); // Xbox 0x000821A0, ESI state
u32 accept_runtime_trophy_head(const TrophyHeadsControlState& control, std::int8_t id,
    TrophyHeadsControlServices& services); // Xbox 0x000820D0, AL id
u32 runtime_trophy_head_has_event(const TrophyHeadsControlState& control, std::int8_t id,
    TrophyHeadsControlServices& services); // Xbox 0x00082440, EAX state, EDI id
u32 runtime_trophy_head_chosen(const TrophyHeadsControlState& control,
    std::int8_t id); // Xbox 0x00082170, ECX state, DL id
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
void release_runtime_actor_animation_slot(ActorAnimationState& animation, i32 slot); // Xbox 0x0003B340, EAX actor, ECX slot
// The actor's 0x000426F0 bound in its own scale: +0x1C4 width, +0x1C8 top, +0x1CC bottom, +0x1D0 their reach.
void measure_runtime_actor_bound_size(Actor& actor, u32 mask, GhoulySpawnPool*& shapes); // Xbox 0x0003D670, EAX actor
}
