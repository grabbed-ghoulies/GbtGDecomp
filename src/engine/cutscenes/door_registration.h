#pragma once
#include <functional>
#include "event_runtime.h"
#include "../navigation/cell_registration.h"

namespace gbtg
{
struct DoorRoomAlias
{
    const char* source;
    const char* target;
};
struct RuntimeDoorNames
{
    const char* room; // DAT_00545D40.
    std::span<const DoorRoomAlias, 2> aliases; // PTR_DAT_00434034, stride 8.
    std::span<const char* const> variants; // PTR_DAT_00424970.
    std::span<const char* const> shapes; // PTR_DAT_00425118.
    std::span<const u8> dimensions; // DAT_00424994, stride 0x94, X and Z at +0/+4.
};
const char* runtime_door_room_suffix(const char* room,
    std::span<const DoorRoomAlias, 2> aliases); // Xbox 0x001410F0
bool build_runtime_door_clip_names(char* clip, char* camera, u16 first_variant,
    u16 second_variant, i32 shape, u16 ordinal, const RuntimeDoorNames& names,
    AnimationResourceRegistry& resources); // Xbox 0x0005B890
struct RuntimeDoorClips
{
    std::span<u8> fields; // Retail row, stride 0x154.
    RuntimeCutsceneInstance* primary; // +0x140.
    RuntimeCutsceneInstance* camera; // +0x144.
};
struct RuntimeDoorActorModels
{
    const std::span<const u8>* reference; // Actor +0x204.
    RuntimeModelInstance* primary; // Instance referenced by +0x204.
    RuntimeModelInstance* secondary; // Instance referenced by +0x310.
};
struct RuntimeDoorContext
{
    entity_detail::NavigationCellRegistration*& pool; // DAT_004EE2B8.
    std::vector<RuntimeDoorClips>& rows; // Native pointer mappings for pool rows.
    const RuntimeDoorNames& names;
    CutsceneRuntimeContext* cutscenes; // Raises at use while the scene has no cutscene runtime.
    EntityCamNode* camera; // DAT_004EE2BC.
    const std::function<u32(const RuntimeCutsceneInstance*)>& instance_address;
    const std::function<RuntimeDoorActorModels(Actor*)>& models; // Views of a door track actor's models.
    std::span<u32> shared_model_states;
    std::function<void(Actor&, std::span<u32>)> actor_message; // Actor vtable +0xC, for 0x0005BDB0's notices.
    // 0x0005BF30's scene hold while a door clip runs with its camera.
    u32* camera_registered = nullptr; // DAT_004EE31C.
    u32* paused = nullptr; // DAT_004EE320.
    u32* active_strategies = nullptr; // DAT_00510CEC.
    u32* camera_hold = nullptr; // DAT_004DEA24.
    u32* runtime_flags = nullptr; // DAT_0050EA9C.
    const char* camera_name = nullptr; // 0x00323C40.
    // The vectors 0x0005B540 gives the door it adds when the ordinal has no row (0x0005B2D0's EDX, ECX and EAX).
    const Vec3* added_position = nullptr; // DAT_0050DD80.
    const Vec3* added_euler = nullptr; // DAT_0050DD74.
    const Vec3* added_scale = nullptr; // DAT_0042A840.
};
struct RuntimeDoorRelease
{
    RuntimeDoorContext& doors;
    u32& camera_registered; // DAT_004EE31C.
    u32& paused; // DAT_004EE320.
    u32& active_strategies; // DAT_00510CEC.
    u32& camera_hold; // DAT_004DEA24.
    u32& runtime_flags; // DAT_0050EA9C.
};
void release_runtime_doors(RuntimeDoorRelease& context); // Xbox 0x0005B1E0
void load_runtime_door_clips(RuntimeDoorClips& door, const char* clip,
    const char* camera, RuntimeDoorContext& context); // Xbox 0x0005B960
void register_runtime_door(const Vec3& position, const Vec3& euler, const Vec3& scale,
    i32 shape, u16 ordinal, RuntimeDoorContext& context); // Xbox 0x0005B2D0
// SI ordinal: whether the event door has reached its event state 5 (door 2), or is moving to it past the
// DAT_0032B0F4 fraction of its clip.
bool runtime_door_at_event(u16 ordinal, RuntimeDoorContext& context); // Xbox 0x0005B7C0
// The straight-line distance between two points (x87, as the dump computes it).
long double runtime_point_distance(const Vec3& from, const Vec3& to); // Xbox 0x00146CE0, EAX from, ECX to
// The door's clip (+0x140) first track actor's position and its distance from `from`; false without that actor.
bool runtime_door_distance(u16 ordinal, const Vec3& from, Vec3& door_position, float& distance,
    RuntimeDoorContext& context); // Xbox 0x0005BEC0, CX ordinal, EAX door position
// The door whose +4 ordinal is `ordinal` (SI): its +6 state in `state` and true, or false when none matches.
bool runtime_door_state(u16 ordinal, u16& state, RuntimeDoorContext& context); // Xbox 0x0005B780
// AX ordinal: moves the door to `state` (6 removes it, 7 closes its clips), loading the clips the move needs;
// `hold` and `last` are kept at +0x14C / +0x150.
void set_runtime_door_state(u16 ordinal, u16 state, u32 hold, u32 last,
    RuntimeDoorContext& context); // Xbox 0x0005B540
// ESI door, AX state: sets +6 and +8 at once, blocks or frees the door's navigation regions and sends the clip's
// actors 0x1F (state 1) or 0x1E.
void snap_runtime_door_state(RuntimeDoorClips& door, u16 state, RuntimeDoorContext& context); // Xbox 0x0005BDB0

// ECX door, EAX active: marks the door's clip running (+0xB8); the first running door whose clip has a camera
// holds the scene's strategies and binds DAT_004EE2BC as the view, and clearing it releases the hold.
void hold_runtime_door(RuntimeDoorClips& door, u32 active, RuntimeDoorContext& context); // Xbox 0x0005BF30

// The scene tick's door pass: each running door's clip advances by `step` (the frame step); a finished clip
// releases the hold and settles the door at its +8 target, and the door camera is unbound when a camera clip
// ends outside the script's last action.
void update_runtime_doors(RuntimeDoorContext& context, float step); // Xbox 0x0005AA90

// Each door's navigation regions blocked unless its +6 state is 0.
void apply_runtime_door_regions(RuntimeDoorContext& context); // Xbox 0x0005C020

// DAT_004EE2B8's scene context while a scene has doors; null between scenes.
extern RuntimeDoorContext* g_runtime_doors;
}
