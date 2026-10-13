#pragma once
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
    std::span<u8> fields; // Retail row, stride 0x150.
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
    CutsceneRuntimeContext& cutscenes;
    EntityCamNode* camera; // DAT_004EE2BC.
    const std::function<u32(const RuntimeCutsceneInstance*)>& instance_address;
    const std::unordered_map<Actor*, RuntimeDoorActorModels>& models;
    std::span<u32> shared_model_states;
};
void load_runtime_door_clips(RuntimeDoorClips& door, const char* clip,
    const char* camera, RuntimeDoorContext& context); // Xbox 0x0005B960
void register_runtime_door(const Vec3& position, const Vec3& euler, const Vec3& scale,
    i32 shape, u16 ordinal, RuntimeDoorContext& context); // Xbox 0x0005B2D0
}
