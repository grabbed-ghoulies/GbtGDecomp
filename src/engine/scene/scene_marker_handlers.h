#pragma once
#include "scene_markers.h"
#include "../entities/player_spawn.h"
#include "../navigation/occupancy.h"
#include "../entities/actor_live_attachments.h"

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
bool read_runtime_marker_camera_start(Vec3& position, Vec3& euler,
    const RuntimeMarkerCameraStart& start); // Xbox 0x0013CC70, ECX position, EAX euler
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
void build_runtime_marker_bg_break_key_parameters(std::span<u8, 0x3B4> parameters, const Vec3& position,
    const Vec3& euler, u16 id, u16 next, const char* name, const char* default_name); // Xbox 0x0006D410
void create_runtime_marker_bg_break_key(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FF830, kind 0xE
void create_runtime_marker_companion(const char* name, const Vec3& position, const Vec3& euler,
    float scale, RuntimeMarkerObjects& context); // Xbox 0x00100770
void set_runtime_marker_template_names(std::span<u8> parameters, const char* first, const char* second,
    u32 value); // Xbox 0x00146C80
void set_runtime_marker_template_chain_name(std::span<u8> parameters, const char* name); // Xbox 0x00146CC0
void create_runtime_marker_modified_template(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x001007F0, kind 5
void build_runtime_marker_trigger_parameters(std::span<u8, 0x65C> parameters, const Vec3& position, const Vec3& euler,
    u16 id, u16 next, u32 a2, u32 a3, u32 a4, u16 a5, const u8* block, u32 a7, u32 a8, const char* first_name,
    const char* second_name, float a11, u32 a12, float a13, u32 a14, const char* default_name); // Xbox 0x0006A010
void create_runtime_marker_trigger(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFAD0, kind 0xB
void build_runtime_marker_area_parameters(std::span<u8, 0x348> parameters, const Vec3& position, const Vec3& euler,
    u16 id, u16 next, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, const char* default_name); // Xbox 0x0006E110
void create_runtime_marker_area(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFDC0, kind 0x10
struct RuntimeMarkerAdditionalBindings
{
    std::span<u8> names; // DAT_0051254C, 0x90-byte rows.
    i32& name_count; // DAT_00512548.
    const u8* scare_templates; // DAT_0030CB18, 0x80-byte names.
    const u8* dropper_templates; // DAT_0030D098, 0x80-byte names.
    Vec3& nearest_retained_position; // Retained local stack bytes in 0x0011EFA0.
    Vec3& dropper_retained_position; // Retained local stack bytes in 0x00100600.
};
// Kind 0x19's player shape.
struct RuntimeMarkerPlayerShapeBindings
{
    Actor*& player; // DAT_00511B20 points to actor +0x934.
    entity_detail::ListHead& player_attachments; // Actor +0x8F4.
    entity_detail::ListHead& global_attachments; // DAT_00545984.
    const Vec3& attachment_origin; // DAT_0050DD80.
    u8 attachment_retained_byte; // Unwritten shape +3 stack byte in 0x000FFA30.
};
void register_runtime_marker_name(const Vec3& position, const char* name,
    RuntimeMarkerAdditionalBindings& context); // Xbox 0x0012A190
void register_runtime_marker_chain_name(const u8* record, RuntimeMarkerObjects& objects,
    RuntimeMarkerAdditionalBindings& context); // Xbox 0x00100500, kind 7
void create_runtime_marker_camera(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FF8F0, kind 0xA
void create_runtime_marker_bg_break_setup(u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFC00, kind 0xD
void create_runtime_marker_cutscene_setup(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFCD0, kind 0xF
void create_runtime_marker_swap_setup(const u8* record, RuntimeMarkerObjects& context); // Xbox 0x000FFD30, kind 0x11
void create_runtime_marker_super_scary_shock(const u8* record,
                                             RuntimeMarkerObjects& context); // Xbox 0x00100570, kind 0x15
void create_runtime_marker_scare(const u8* record, RuntimeMarkerObjects& objects,
    RuntimeMarkerAdditionalBindings& context); // Xbox 0x00100380, kind 0x16
void create_runtime_marker_dropper(const u8* record, RuntimeMarkerObjects& objects,
    RuntimeMarkerAdditionalBindings& context); // Xbox 0x00100600, kind 0x17
void attach_runtime_marker_player_shape(const u8* record,
    RuntimeMarkerPlayerShapeBindings& context); // Xbox 0x000FFA30, kind 0x19
void build_runtime_marker_bg_break_parameters(std::span<u8, 0x460> parameters, const Vec3& position,
    const Vec3& euler, u16 id, u16 next, u32 value, const char* first, const char* second, u16 key,
    std::span<const u32, 9> values, const char* default_name); // Xbox 0x0006AF70
void build_runtime_marker_cutscene_setup_parameters(std::span<u8, 0x3C4> parameters, const Vec3& position,
    const Vec3& euler, u16 id, u16 next, u32 value, u16 key, const char* name,
    u32 first, u32 second, const char* default_name); // Xbox 0x0006BA70
void build_runtime_marker_swap_setup_parameters(std::span<u8, 0x4C8> parameters, const Vec3& position,
    const Vec3& euler, u16 id, u16 next, u32 value, const char* first, const char* second,
    std::span<const u32, 4> values, const char* default_name); // Xbox 0x0006C170
void build_runtime_marker_super_scary_shock_parameters(std::span<u8, 0x754> parameters, const Vec3& position,
    const Vec3& euler, std::span<const u32, 5> values, std::span<const char* const, 8> names,
    const char* default_name); // Xbox 0x000725E0
struct RuntimeSceneMarkerBindings
{
    RuntimeMarkerPlayerStart& player;
    RuntimeMarkerObjects& objects;
    const char* const* tags; // DAT_0042A854.
    RuntimeMarkerCameraStart& camera;
    RuntimeMarkerAdditionalBindings* additional = nullptr;
    RuntimeMarkerPlayerShapeBindings* player_shape = nullptr;
};
// Native mapping of DAT_0042A720. Bindings must outlive the returned callbacks.
RuntimeSceneMarkerHandlers bind_runtime_scene_marker_handlers(RuntimeSceneMarkerBindings& bindings);
}
