#pragma once
#include "cutscene_scene_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"
#include "../audio/bank_streams.h"
#include "../audio/event_voices.h"
#include "../entities/actor_clock.h"
#include "../frontend/dialogue_input.h"
#include "../frontend/input_activation.h"

namespace gbtg
{
struct SceneControlInstance;
struct SceneControlLists;
struct RuntimeActorPauseGroups;
struct RuntimeDialogueActivationContext;
struct RuntimeDialogueSpriteQueue;
struct RuntimeDialogueOuterDrawContext;
struct RuntimePlayerHealthDisplay;
struct ButtonBatScreen;
namespace entity_detail
{
struct DisplayView;
}
// A scene actor the challenge drives (Cooper at +0x28, the trophy head at +0x2C): its native views.
struct SuperScaryActorView
{
    Actor& actor;
    ActorAnimationState& animation;
    i32& secondary_mode; // Actor +0x30C.
    bool secondary_model; // Actor +0x310 is set.
};
// The button bat at +0x30 (0x0013B580): its native views.
struct SuperScaryBatView
{
    u32 address;
    ButtonBatScreen& bat;
    ActorAnimationState& animation;
};
// What the Super Scary challenge (factory type 0x98, entitySceneControlSuperScary) reaches outside its object.
// SceneActorServices binds it when the native scene starts.
struct SuperScaryControlServices
{
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    SceneCutsceneRuntime& cutscenes; // DAT_004E1B40.. and the clips' track views and actor rows.
    AudioEventVoices& voices; // DAT_004E0CE8
    AudioBankStreams& streams;
    u32& random; // DAT_0054DD7C
    i32& pause_count; // DAT_00510CEC
    u32& runtime_flags; // DAT_0050EA9C
    const u8& scene_flags; // **(DAT_00506C74+8)
    std::span<u8> story_bits; // *(DAT_00506C6C+8)
    const i32& frontend_state; // DAT_004E17C8
    const u32& prompt_owner; // DAT_004EE1EC
    const u32& auto_presses; // DAT_005122F0
    const u32& override_health; // DAT_00512230
    std::span<const u8> button_rows; // 0x00302198, rows of 0x50 bytes.
    i32 button_count; // DAT_00427958
    u32 panel_color; // DAT_0042795C
    u32 button_color; // DAT_00314F48
    u32 highlight_color; // DAT_00314F30
    const std::array<u16, 2>& screen; // DAT_005096E8
    entity_detail::DisplayView*& display_view; // DAT_005077A8
    const char* scene_name; // DAT_00510E58
    const RuntimeActorClock& clock; // DAT_004312DC, DAT_005096C0, DAT_004312C4
    std::function<void(u8, i32)> pause_actors; // 0x00117400 over DAT_00511B38 (mask, release).
    RuntimeInputActivationContext& input;
    const RuntimeInputReleaseContext& input_release;
    std::unordered_map<u32, std::span<u8>>& input_addresses;
    std::unordered_map<u32, RuntimeDialogueInputDevice>& input_views;
    RuntimeDialogueInputState& input_state;
    const u32& frame; // DAT_004DEA58
    RuntimeDialogueActivationContext& dialogue; // 0x000582A0
    RuntimeDialogueSpriteQueue& sprites; // 0x001221B0
    RuntimeDialogueOuterDrawContext& panels; // 0x00061360
    RuntimePlayerHealthDisplay& health; // 0x00130710, 0x0012FE10
    float& health_intensity; // DAT_00433C14
    std::function<u32()> scene_player; // DAT_00511B40's player, by address (0 for none).
    std::function<SuperScaryActorView(u32)> actor_view; // A scene actor by address.
    std::function<std::span<u8>(u32)> player_body; // Player +0x95C.
    std::function<const Actor*(u32)> attached_actor; // Body +0x48 of a player, or null.
    std::function<void(u32)> release_attachment; // 0x0001B810(0, 0) on a player.
    std::function<void(u32, std::span<u32>)> actor_message; // Actor vtable +0xC.
    std::function<void(u32)> retire_actor; // 0x00112CC0, ESI the actor.
    std::function<void(u32)> unregister_render; // 0x0003CBB0, EDI the actor.
    std::function<void(u32)> register_render; // 0x0003CAE0
    std::function<u32(const std::array<float, 2>&, float, float)> create_bat; // 0x0013B580; the scene keeps it.
    std::function<SuperScaryBatView(u32)> bat_view;
    std::function<void(u32, i32, i32)> draw_bat; // The bat's vtable +0x14 (view, pass).
    std::function<void(std::span<const u32>)> broadcast_pulse; // 0x00122C70
    std::function<void(std::span<const u32>)> message_players; // 0x00122D10 over DAT_00511B40.
    std::function<void(const char*, const Vec3&, float)> camera_shake; // 0x000FB120
    std::function<float(const Vec3&)> floor_height; // 0x00110BF0(-1, 0), 0x00110EF0 and DAT_0050E240.
    std::function<void(u32)> refresh_body_position; // Body +0x18 from 0x000412A0 on the body's actor (+8).
    void* const& scene_name_text; // DAT_00512484
    std::function<bool()> scene_end_flag; // 0x00049D10
    std::function<i32()> player_character; // 0x00087830
};
// The Super Scary challenge's construction parameters (0x1D8 bytes, 0x000842D0), as 0x00084340 reads them.
namespace super_scary_parameter
{
constexpr RetailField<u32, 0x008> head{}; // the attacking actor's address
constexpr RetailField<std::array<char, 0x80>, 0x00C> first_clip{}; // the head's first scare clip
constexpr RetailField<std::array<char, 0x80>, 0x08C> second_clip{}; // the head's second scare clip
constexpr RetailField<float, 0x10C> first_clip_time{}; // 0 to use the clip's duration
constexpr RetailField<float, 0x110> second_clip_time{};
constexpr RetailField<Vec3, 0x114> clip_position{};
constexpr RetailField<u32, 0x120> has_clip_position{};
constexpr RetailField<Vec3, 0x124> player_spawn{};
constexpr RetailField<u32, 0x130> spawn_under_view{}; // set: the spawn is the floor under the view (0x00086CE0)
constexpr RetailField<u32, 0x134> move_player{};
constexpr RetailField<Vec3, 0x138> clip_euler{};
constexpr RetailField<u32, 0x144> has_clip_euler{};
constexpr RetailField<i32, 0x148> button_pattern{}; // 0x00087170's count: 0 random, 1..4 fixed sets, 5 a table row
constexpr RetailField<u32, 0x14C> unk_14C{}; // copied to state +0x290, which nothing here reads
constexpr RetailField<i32, 0x150> scare_mode{};
constexpr RetailField<std::array<char, 0x80>, 0x154> clip_list{}; // the aidlist resource's name
constexpr RetailField<u32, 0x1D4> swap_render{};
constexpr u32 kSize = 0x1D8;
static_assert(retail_fits(kSize, swap_render));
} // namespace super_scary_parameter

// The Super Scary challenge's state fields at their retail offsets (0x2C8 bytes, 0x000842D0; see
// docs/RETAIL_LAYOUTS.md). +0x20 is the scene-control base's scene_control_field::elapsed. The words that hold
// pointers are SuperScaryControlState members, noted here by offset.
namespace super_scary_field
{
constexpr u32 kSize = 0x2C8;
// The actors' addresses, mirrored in SuperScaryControlState: Cooper (0 once message 0x7C names him), the head,
// and the button bat.
constexpr RetailField<u32, 0x028> player{};
constexpr RetailField<u32, 0x02C> head{};
constexpr RetailField<u32, 0x030> bat{};
// 0x000856E0 sets it; 0x00084960 and 0x00085380 switch on it. 1 and 3 the head's scare clips, 2 and 4 Cooper's
// recoils, 5/6 the button presses, 7/9 the success/failure page, 8/10 the outcome clips, 11 disposal.
constexpr RetailField<i32, 0x034> phase{};
// +0x38 input
constexpr RetailField<u32, 0x03C> input_pushed{}; // set once +0x38 is pushed (0x00109340); released when set
// +0x40 camera (EntityCamNode, to +0xA0)
constexpr RetailField<u32, 0x0A0> camera_linked{}; // set once +0x40 is linked; unlinked (0x00048C70) when set
// Phases 1 to 4: the next shock (0.45 s apart, 0x00086990). Phase 11: the wait before disposal.
constexpr RetailField<float, 0x0A4> shock_timer{};
constexpr RetailField<float, 0x0A8> heartbeat_timer{}; // phases 5/6: the next heartbeat, quicker as they run out
constexpr RetailField<float, 0x0AC> focal_jitter_timer{}; // 0x00084E50 jitters the focal length while it runs
constexpr RetailField<float, 0x0B0> wrong_press_timer{}; // 1 s after a wrong press: presses ignored, highlight shown
constexpr RetailField<float, 0x0B4> unk_B4{}; // set to 0 by 0x00084340; nothing here reads it
constexpr RetailField<std::array<char, 0x80>, 0x0B8> first_clip{}; // super_scary_parameter::first_clip
constexpr RetailField<std::array<char, 0x80>, 0x138> second_clip{}; // super_scary_parameter::second_clip
constexpr RetailField<float, 0x1B8> first_clip_timer{}; // phase 1, then the shocks wait until it is under 1
constexpr RetailField<float, 0x1BC> second_clip_timer{}; // phase 3
constexpr RetailField<float, 0x1C0> auto_press_timer{}; // with DAT_005122F0 set, a right press when it runs out
constexpr RetailField<i32, 0x1C4> music{}; // 0x00086FD0: 0 none, 1 BUTTONPRESS, 2 SUCCEED, 3 BUTTONNODIE
constexpr RetailField<Vec3, 0x1C8> player_spawn{};
constexpr RetailField<Vec3, 0x1D4> clip_position{}; // used while has_clip_position
constexpr RetailField<Vec3, 0x1E0> clip_euler{}; // used while has_clip_euler
constexpr RetailField<Vec3, 0x1EC> shock_camera_position{}; // phase 5: the display view's position
constexpr RetailField<float, 0x1F8> shock_camera_pitch{}; // phase 5: facing Cooper's joint 0
constexpr RetailField<float, 0x1FC> shock_camera_yaw{};
constexpr RetailField<float, 0x200> shock_camera_roll{};
constexpr RetailField<i32, 0x204> shocks_left{}; // 3 at phase 1
constexpr RetailField<i32, 0x208> heartbeats_left{}; // 8 at construction; under 0 the challenge fails
constexpr RetailField<i32, 0x20C> presses{}; // right presses so far; the challenge succeeds at button_count
constexpr RetailField<i32, 0x210> button_count{}; // DAT_00427958
constexpr RetailField<std::array<u32, 20>, 0x214> buttons{}; // each press's button, 0..3 for A, B, X, Y
constexpr RetailField<i16, 0x264> shiver_voice{}; // XACT_SOUNDBANK_GBOY_SHIVER's voice, -1 for none
constexpr RetailField<u32, 0x268> has_clip_euler{};
constexpr RetailField<u32, 0x26C> paused_cutscene{}; // a cutscene was running at construction; phase 1 pauses it
constexpr RetailField<u32, 0x270> move_player{}; // 0x00086E80 moves Cooper to player_spawn
constexpr RetailField<u32, 0x274> has_clip_position{};
constexpr RetailField<u32, 0x278> spawn_under_view{};
// Modes 2 and 3: Cooper's position when bound to the head, where 0x000874F0 puts him back.
constexpr RetailField<Vec3, 0x27C> bound_position{};
constexpr RetailField<float, 0x280> bound_position_y{};
constexpr RetailField<u32, 0x288> head_binding{}; // modes 2 and 3: the head's model binding 2 (0x000408D0)
// 0..3 from the parameters. 2 and 3 bind Cooper to the head, and drop to 0 and 1 with no head. 0 becomes 1 and 2
// becomes 3 when his attached actor is unavailable (0x001460C0). It picks the animation rows and the camera
// (0x00084E50).
constexpr RetailField<i32, 0x28C> scare_mode{};
constexpr RetailField<u32, 0x290> unk_290{}; // super_scary_parameter::unk_14C; nothing here reads it
// +0x294 clips, +0x298 the five button textures
// Modes 2 and 3 after the challenge: Cooper settles from settle_start_y to settle_floor_y over 0.5 s (0x00087720).
constexpr RetailField<float, 0x2AC> settle_timer{};
constexpr RetailField<float, 0x2B0> settle_start_y{};
constexpr RetailField<float, 0x2B4> settle_floor_y{};
constexpr RetailField<u32, 0x2B8> player_released{}; // message 10 has gone to Cooper
// From the parameters: Cooper and the head take turns in the render bins (0x0003CBB0 / 0x0003CAE0).
constexpr RetailField<u32, 0x2BC> swap_render{};
constexpr RetailField<u32, 0x2C0> player_hidden{};
constexpr RetailField<u32, 0x2C4> head_hidden{};
static_assert(retail_fits(kSize, head_hidden));
static_assert(retail_fits(0x264, buttons));
} // namespace super_scary_field

// Native state of a Super Scary challenge whose retail words are pointers.
struct SuperScaryControlState
{
    u32 player = 0; // +0x28, the scared player's address.
    u32 head = 0; // +0x2C, the attacking actor's address.
    u32 bat = 0; // +0x30, the button bat's address.
    std::span<u8> input; // +0x38
    u32 input_address = 0;
    std::span<const u8> clips; // +0x294, the aidlist: rows of 0x80-byte names.
    std::array<std::span<const u8>, 5> buttons{}; // +0x298, the aidlist's rows +0x1880.. (button textures).
    EntityCamNode camera{}; // +0x40
};
void super_scary_control_object_sizes(ObjectSizes& output); // Xbox 0x000842D0
bool construct_runtime_super_scary_control(std::span<u8> state, std::span<const u8> parameters,
    SuperScaryControlState& control, SuperScaryControlServices& services); // Xbox 0x00084340
void destroy_runtime_super_scary_control(std::span<u8> state, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x000846E0
void message_runtime_super_scary_control(std::span<u8> state, std::span<const u32> message,
    SuperScaryControlState& control, SceneControlInstance& instance, SceneControlLists& lists); // Xbox 0x00084930
void tick_runtime_super_scary_control(std::span<u8> state, SuperScaryControlState& control,
    SceneControlInstance& instance, SceneControlLists& lists, SuperScaryControlServices& services); // Xbox 0x00084960
void draw_runtime_super_scary_control(std::span<u8> state, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00085380
void set_runtime_super_scary_state(std::span<u8> state, i32 next, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x000856E0
void finish_runtime_super_scary(std::span<u8> state, i32 outcome, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00086BC0, ESI state
void end_runtime_super_scary_challenge(std::span<u8> state, i32 succeeded, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x000874F0
void choose_runtime_super_scary_buttons(i32 count, std::span<u8> state,
    SuperScaryControlServices& services); // Xbox 0x00087170, EAX count, EDX state
void shuffle_runtime_super_scary_buttons(std::span<u8> buttons, i32 count, u32& random); // Xbox 0x00087080, ECX
// {0x8E, step} to the active scene controls (DAT_0050E438), then 0x00122C70.
void announce_runtime_super_scary_step(u32 step, SuperScaryControlServices& services); // Xbox 0x00086C90, EAX
float start_runtime_super_scary_clip(const Vec3* position, const Vec3* euler, u32 actor, const char* name,
    SuperScaryControlServices& services); // Xbox 0x000854E0, EAX ECX EDX; XMM0 the clip's duration
void play_runtime_super_scary_animation(u32 actor, const char* animation, const char* events, i16 mode,
    float duration, float fade, float position, SuperScaryControlServices& services); // Xbox 0x00086860
void place_runtime_super_scary_camera(std::span<u8> state, float shake, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00084E50, EAX state
// The view's floor point under the camera into +0x1C8 (ESI), and in ECX the euler facing back along the view.
void aim_runtime_super_scary_camera(std::span<u8> state, Vec3& euler,
    SuperScaryControlServices& services); // Xbox 0x00086CE0, ESI +0x1C8, ECX
u32 spawn_runtime_super_scary_player(std::span<u8> state, const Vec3& position, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00086E80, EAX state
void set_runtime_super_scary_music(std::span<u8> state, i32 next, SuperScaryControlServices& services); // Xbox 0x00086FD0
void tick_runtime_super_scary_shocks(std::span<u8> state, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00086990, ESI state
u32 runtime_super_scary_right_button(std::span<u8> state, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00086A40, ECX state, EDI the input
u32 runtime_super_scary_wrong_button(std::span<const u8> state, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00086B30, EAX state
void shake_runtime_super_scary_camera(SuperScaryControlState& control, SuperScaryControlServices& services); // Xbox 0x00086B70
void settle_runtime_super_scary_player(std::span<u8> state, SuperScaryControlState& control,
    SuperScaryControlServices& services); // Xbox 0x00087720, EBX state
void runtime_super_scary_button_position(std::array<float, 2>& output, std::span<const u8> state, i32 index,
    SuperScaryControlServices& services); // Xbox 0x00085190, ECX output, EDX state
void draw_runtime_super_scary_panel(std::span<const u8> state, SuperScaryControlServices& services); // Xbox 0x00085280, ESI
void wobble_runtime_button_bat(ButtonBatScreen& bat, ActorAnimationState& animation,
    SuperScaryControlServices& services); // Xbox 0x0006DAD0, ESI the bat
}
