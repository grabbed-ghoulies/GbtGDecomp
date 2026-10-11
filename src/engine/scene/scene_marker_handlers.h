#pragma once
#include "scene_markers.h"
#include "../entities/player_spawn.h"
#include "../navigation/occupancy.h"

namespace gbtg
{
// Player camera start pose (DAT_00545AD0..DAT_00545AE8), read by 0x0013CC70.
struct RuntimeMarkerCameraStart
{
    Vec3 position; // DAT_00545AD0.
    Vec3 euler; // DAT_00545ADC.
    u32 valid; // DAT_00545AE8.
};
OccupancyRecord& add_runtime_occupancy_record(u16 kind, u8 value, u16 id, u32 first, u32 second, u32 third,
    u32 fourth, u32 mask, u16 group); // Xbox 0x001246A0, EDI mask, SI group
void add_runtime_marker_sphere(const u8* record, const char* const* tags); // Xbox 0x00100210, kind 8
void add_runtime_marker_box(const u8* record, const char* const* tags); // Xbox 0x001002B0, kind 0x14
void set_runtime_marker_camera_start(const u8* record, RuntimeMarkerCameraStart& start); // Xbox 0x000FF880, kind 0x13
void resolve_runtime_marker_chain_pose(const u8* record, const u8* records, std::span<const u8> address_zero,
    Vec3& position, Vec3& euler); // Xbox 0x00100110, EAX record, EDI position, ESI euler
void send_runtime_marker_chain_goals(const u8* record, const Vec3& position, const Vec3& euler, const u8* records,
    std::span<const u8> address_zero,
    const std::function<void(std::span<const u32>)>& message); // Xbox 0x000FFFD0, EBX actor, ECX position, EAX euler
struct RuntimeMarkerPlayerStart
{
    RuntimePlayerSpawnContext& spawn;
    const u8*& records; // DAT_0051108C.
    std::span<const u8> address_zero;
    const u32& player_start_enabled; // DAT_005462FC.
    const i32& chapter_mode; // DAT_00430D0C.
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    std::function<bool()> script_end_gate; // 0x00123B00.
    std::vector<FactoryObject>& actors; // Scene ownership of the created actor.
    // Actual actor vtable +0xC of the created actor.
    std::function<void(FactoryObject&, std::span<const u32>)> actor_message;
};
void spawn_runtime_marker_player(const u8* record, RuntimeMarkerPlayerStart& context); // Xbox 0x000FFEC0, kind 2
// Factory objects the object markers create; the scene keeps ownership of every created object.
struct RuntimeMarkerObjects
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    const u8*& records; // DAT_0051108C.
    std::span<const u8> address_zero;
    const char* default_name; // DAT_00546208, for 0x000411A0.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    std::vector<FactoryObject>& objects;
    std::function<ActorAnimationState&(FactoryObject&)> animation_of; // Actor +0x50 of a created actor.
    std::function<void(FactoryObject&, std::span<const u32>)> message; // Actual object vtable +0xC.
};
void create_runtime_marker_prop(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FF680, kind 3
void send_runtime_marker_actor_goals(u16 first, u16 goal, FactoryObject& actor,
    RuntimeMarkerObjects& context); // Xbox 0x00069F30, AX first, BX goal, EDI actor
FactoryObject* create_runtime_marker_actor(const char* name, const Vec3& position, const Vec3& euler, float scale,
    const char* second_override, const char* fourth_override, const char* fifth_override, u32 goal, u32 tag,
    const Vec3& goal_position, const Vec3& goal_euler, const u8* goal_names, u32 chained, u16 key,
    RuntimeMarkerObjects& context); // Xbox 0x001349E0, ECX fifth, EDX fourth
void create_runtime_marker_spawn_actor(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FF4F0, kind 4
void create_runtime_marker_template(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x001001A0, kind 6
void build_runtime_marker_trigger_parameters(std::span<u8, 0x65C> parameters, const Vec3& position, const Vec3& euler,
    u16 id, u16 next, u32 a2, u32 a3, u32 a4, u16 a5, const u8* block, u32 a7, u32 a8, const char* first_name,
    const char* second_name, float a11, u32 a12, float a13, u32 a14, const char* default_name); // Xbox 0x0006A010
void create_runtime_marker_trigger(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFAD0, kind 0xB
void build_runtime_marker_area_parameters(std::span<u8, 0x348> parameters, const Vec3& position, const Vec3& euler,
    u16 id, u16 next, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, const char* default_name); // Xbox 0x0006E110
void create_runtime_marker_area(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFDC0, kind 0x10
}
