#pragma once
#include "cutscene_scene_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
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
