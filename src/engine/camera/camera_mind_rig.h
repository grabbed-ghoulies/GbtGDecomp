#pragma once
#include "../entities/actor_clock.h"
#include "../entities/camera_director.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/retail_field.h"
#include <array>
#include <functional>

namespace gbtg
{
// DAT_00434008..DAT_00434033, the camera view tunables: the retail .data words, then whatever 0x0013CBD0,
// 0x00140E30 and the script's type 0x61 store.
extern std::array<u32, 11> g_camera_view_tunables;
// The camera rig tunables: .data DAT_004274B8..DAT_0042753F and g_camera_view_tunables, by retail address.
struct RuntimeCameraRigTunables
{
    std::array<u8, 0xEC> rig{}; // DAT_004274B8..DAT_004275A4: the kind-1 rig, then the kinds 3, 4 and 0x12 words.
    float real(u32 address) const;
    u32 word(u32 address) const;
};
struct RuntimeCameraRigContext
{
    const RuntimeCameraRigTunables& tunables;
    const RuntimeDirectorTunables& director; // DAT_0042872C.
    const RuntimeActorClock& clock;
    const u8& view_flags; // **(DAT_00506C70 + 8); bit 2 keeps the stick direction.
    const u32& invert; // DAT_005122D8.
    const i32& render_width; // DAT_0054DD84.
    const i32& render_height; // DAT_0054DD80.
    std::function<RuntimeDirectorActor(u32)> actor_at;
    std::function<const RuntimeDialogueInputDevice*(u32)> input_device; // Actor +0x960 word.
    const RuntimeDialogueInputState& input;
};
// Each 0x20-byte camera target record: kind +0, position +4, height +0x14, width +0x1C.
float camera_input_turn(const RuntimeDirectorActor& actor,
    const RuntimeCameraRigContext& context); // Xbox 0x0013DC40, EAX actor
float camera_input_turn_step(const RuntimeDirectorActor& actor, const Actor& camera,
    const RuntimeCameraRigContext& context); // Xbox 0x0013DD00, EAX actor, ESI camera
float camera_floor_height(const RuntimeDirectorActor& actor, const Vec3& point, const Vec3& reference,
    float margin); // Xbox 0x0013D110, EAX actor, ECX point
void offset_camera_point(Vec3& point, float angle, float distance); // Xbox 0x001062A0, ESI point
float clamp_camera_heading(float heading, float first, float second); // Xbox 0x00101550
void aim_camera_offset(Vec3& output, const Vec3& from, const Vec3& to, u32 actor, float range, float near_distance,
    float far_distance, const RuntimeCameraRigContext& context); // Xbox 0x0013CE80, ECX from, EAX to, EDX actor, ESI output
u32 frame_camera_target(const Vec3& from, std::span<const u8, 0x20> record, Vec3& point, float& heading,
    float& distance, float& rise, const RuntimeCameraRigContext& context); // Xbox 0x000783A0, EAX from, ESI record, EDI point, EBX heading
float camera_stick_turn(const RuntimeDirectorActor& target, std::span<const u8> mind,
    const RuntimeCameraRigContext& context); // Xbox 0x000784B0, ESI target
void ease_camera_heading(std::span<u8> mind, const Actor& camera, float& heading, const RuntimeDirectorActor& target,
    const RuntimeCameraRigContext& context); // Xbox 0x00078630, ESI mind, EBX camera, EDI heading
void merge_camera_targets(std::span<const u8> records, i32 count, const Vec3& eye, float yaw,
    std::span<u8, 0x20> output); // Xbox 0x0013D640, ECX records, EAX count, ESI eye
void place_camera_rig(std::span<u8> mind, const Actor& camera, std::span<const u8> request, const Vec3& eye,
    Vec3& position, Vec3& focus, u32 target, float& distance,
    const RuntimeCameraRigContext& context); // Xbox 0x00078840
// The camera mind's own fields (type 0x36, 0xDC bytes from 0x0007AFC0), after the 0x7C-byte base mind
// (mind_field: its actor is the camera, its goal point the eye).
namespace camera_mind_field
{
// The actor the follow point tracks (the request's +0x10). A new one, or message 0x7C naming it, starts a blend
// from the old point (0x00079E80, 0x0007A550, 0x0007B040).
constexpr RetailField<u32, 0x7C> focus_actor{};
constexpr RetailField<Vec3, 0x80> blend_from{}; // The follow point when the focus actor changed.
constexpr RetailField<float, 0x8C> blend{}; // 1 when the focus actor changes, down to 0 by the actor's rate.
constexpr RetailField<Vec3, 0x90> follow_point{}; // The point 0x00079E80 last chose, after the blend.
// The target's body bit 0x30 holds the point it had when the bit came on (0x0007AA20).
constexpr RetailField<u32, 0x9C> target_point_held{};
constexpr RetailField<Vec3, 0xA0> held_target_point{};
// The heading goal 0x000791A0 sets: active, its heading, how long it holds, its turn speed and the caller's tag.
// 0x00078630 eases +0xB8 toward the goal and clears the goal once its hold runs out.
constexpr RetailField<u32, 0xAC> heading_goal_active{};
// Set when a goal starts without one active or ends; 0x00078840 then takes the framed heading into +0xB8.
constexpr RetailField<u32, 0xB0> heading_restart{};
constexpr RetailField<float, 0xB4> heading_goal{};
constexpr RetailField<float, 0xB8> heading{}; // The rig's heading 0x00078630 eases and clamps.
constexpr RetailField<float, 0xBC> heading_goal_hold{};
// Time the body has pushed the camera along its view (body bit 0x15, 0x00079720); past DAT_00427500 it moves.
constexpr RetailField<float, 0xC0> pushed_time{};
// 1 while the heading is held (DAT_00434030 below 0 and the target mind's +0x20 set, 0x00078630).
constexpr RetailField<u32, 0xC4> heading_held{};
// The rig's stored distance (0x00078840): a framed distance beyond both it and the planar gap is cut to the larger
// of the two; any other framed distance is stored here.
constexpr RetailField<float, 0xC8> distance{};
// 1 when the target record's route reaches the eye (0x00079320), so no heading is chosen.
constexpr RetailField<u32, 0xCC> route_reaches_eye{};
constexpr RetailField<float, 0xD0> heading_goal_speed{};
constexpr RetailField<u32, 0xD4> heading_goal_tag{};
constexpr RetailField<u32, 0xD8> above_floor{}; // 0x00078840: the rig's position is above the floor below it.
constexpr u32 kSize = 0xDC;
static_assert(retail_fits(kSize, above_floor));
} // namespace camera_mind_field
// The camera mind (type 0x36) and its body (type 0x28) as the kind-1 request reads them.
struct RuntimeCameraMindView
{
    std::span<u8> mind; // +8 camera actor, +0xC target, +0x70 eye, +0xAC..+0xD8 heading state.
    Actor& camera;
    std::span<u8> body; // Camera +0x95C: +0x1D8 push, +0x2FC focus, +0x318 aim.
    const EntityBitBlock* body_bits; // Body +0x15C.
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
};
void set_camera_heading_goal(std::span<u8> mind, float heading, float hold, u32 keep_side, float speed,
    u32 tag); // Xbox 0x000791A0, ESI mind
i32 slide_camera_eye(RuntimeCameraMindView& view, const Vec3& normal, const Vec3& start, const Vec3& eye,
    Vec3& center, float& heading, Vec3& slid,
    const RuntimeCameraRigContext& context); // Xbox 0x00078C70, EAX mind, ECX normal, EDX start, EDI eye
u32 sweep_camera_eye(RuntimeCameraMindView& view, const Vec3& eye, Vec3& start, Vec3& center, float& heading,
    i32 passes, const RuntimeCameraRigContext& context); // Xbox 0x000790B0, EDI eye, ESI start
void choose_camera_heading(RuntimeCameraMindView& view, std::span<const u8> request, const Vec3& eye,
    const RuntimeCameraRigContext& context); // Xbox 0x00079320, EAX eye, ECX request, EDI mind
u32 settle_camera_position(RuntimeCameraMindView& view, Vec3& eye, u32 target, Vec3& position,
    Vec3& focus, std::span<u8, 0x54> contact,
    const RuntimeCameraRigContext& context); // Xbox 0x00079450, ESI position, EDI focus
void track_camera_obstruction(RuntimeCameraMindView& view, Vec3& eye, Vec3& position, Vec3& focus,
    u32 target, float distance, const RuntimeCameraRigContext& context); // Xbox 0x00079720, EAX eye
// distance starts as the request's own address word, the stack slot retail reuses for it.
u32 update_camera_target_request(RuntimeCameraMindView& view, std::span<const u8> request, float distance,
    const RuntimeCameraRigContext& context); // Xbox 0x000781F0
}
