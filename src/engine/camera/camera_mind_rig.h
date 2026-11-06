#pragma once
#include <bit>
#include "../entities/mind_fields.h"
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
// The camera rig's tunables (.data DAT_004274B8..DAT_004275A3, 0xEC bytes, copied once at boot): the target request's rig,
// then the follow, pair and path requests'. Names come from what each feeds; `unk_` where it only passes on.
struct CameraRigTunables
{
    u32 probe_radius = 0; // DAT_004274B8, a float's bits, as the movement probes take it
    u32 unk_4274BC = 0; // DAT_004274BC
    float wall_turn_distance = 0.f; // DAT_004274C0, a wall nearer than this turns the heading (0x00079720)
    u32 checks_input_turn = 0; // DAT_004274C4
    u32 skips_slide = 0; // DAT_004274C8 (0x00072CB0)
    std::array<u32, 3> unk_4274CC{}; // DAT_004274CC
    float single_target_pitch = 0.f; // DAT_004274D8, else view tunable many_target_pitch
    float aim_range = 0.f; // DAT_004274DC
    float aim_far = 0.f; // DAT_004274E0
    u32 horizontal_pixels = 0; // DAT_004274E4
    u32 raises_eye = 0; // DAT_004274E8
    u32 pitches_eye = 0; // DAT_004274EC, else the eye rises by view tunable eye_raise
    float floor_margin = 0.f; // DAT_004274F0
    float heading_speed = 0.f; // DAT_004274F4
    float fixed_heading_speed = 0.f; // DAT_004274F8
    float fixed_heading_hold = 0.f; // DAT_004274FC
    float pushed_time_limit = 0.f; // DAT_00427500
    float push_gap_share = 0.f; // DAT_00427504, of the distance
    float push_pull = 0.f; // DAT_00427508
    float push_facing_dot = 0.f; // DAT_0042750C
    float wall_swing_degrees = 0.f; // DAT_00427510
    float heading_hold = 0.f; // DAT_00427514
    float side_offset = 0.f; // DAT_00427518
    u32 unk_51C = 0; // DAT_0042751C, passed to 0x00079320's reach test
    u32 unk_520 = 0; // DAT_00427520, passed to 0x00079320's reach test
    u32 slide_path_mode = 0; // DAT_00427524, passed to the path test
    u32 probes_slide = 0; // DAT_00427528, a probe rather than a path test
    u32 eye_path_mode = 0; // DAT_0042752C, passed to the path test
    u32 checks_eye_path = 0; // DAT_00427530
    float sweep_step_degrees = 0.f; // DAT_00427534
    u32 sweep_passes = 0; // DAT_00427538
    float min_distance = 0.f; // DAT_0042753C
    u32 follow_point_mode = 0; // DAT_00427540, request kind 3 (0x00079E80)
    float follow_height_share = 0.f; // DAT_00427544
    std::array<u32, 2> unk_427548{}; // DAT_00427548
    float follow_near = 0.f; // DAT_00427550
    float follow_far = 0.f; // DAT_00427554
    float follow_low_pitch = 0.f; // DAT_00427558
    float follow_high_pitch = 0.f; // DAT_0042755C
    u32 pair_point_mode = 0; // DAT_00427560, request kind 4 (0x0007A160)
    float pair_height_share = 0.f; // DAT_00427564
    u32 pair_levels_points = 0; // DAT_00427568, the second point takes the first's height
    u32 pair_fits_view = 0; // DAT_0042756C
    float pair_near = 0.f; // DAT_00427570
    float pair_far = 0.f; // DAT_00427574
    float pair_low_pitch = 0.f; // DAT_00427578
    float pair_high_pitch = 0.f; // DAT_0042757C
    float path_pitch_centre = 0.f; // DAT_00427580, the path request (0x0007AB00)
    float path_pitch_spread = 0.f; // DAT_00427584
    float path_low_distance = 0.f; // DAT_00427588
    float path_high_distance = 0.f; // DAT_0042758C
    float path_aim_range = 0.f; // DAT_00427590
    float path_aim_far = 0.f; // DAT_00427594
    u32 path_keeps_route = 0; // DAT_00427598, set: it tries to retarget the search's route first
    u32 path_holds_target_point = 0; // DAT_0042759C
    float path_floor_margin = 0.f; // DAT_004275A0
};
static_assert(sizeof(CameraRigTunables) == 0xEC);
// g_camera_view_tunables's words (DAT_00434008..DAT_00434033): scripts write them, so they stay words.
namespace camera_view_tunable
{
constexpr size_t many_target_pitch = 0; // DAT_00434008, script writes it
constexpr size_t near_range = 1; // DAT_0043400C, the director's
constexpr size_t vertical_pixels = 2; // DAT_00434010, script writes it
constexpr size_t eye_raise = 3; // DAT_00434014, script writes it
constexpr size_t unk_4 = 4; // DAT_00434018
constexpr size_t look_tilt_degrees = 5; // DAT_0043401C
constexpr size_t look_lift = 6; // DAT_00434020
constexpr size_t aim_lift = 7; // DAT_00434024
constexpr size_t aim_boost = 8; // DAT_00434028
constexpr size_t free_speed = 9; // DAT_0043402C, script writes it
constexpr size_t target_speed = 10; // DAT_00434030, script writes it; negative for free_speed
} // namespace camera_view_tunable
struct RuntimeCameraRigTunables
{
    CameraRigTunables rig{};
    float view_real(size_t index) const
    {
        return std::bit_cast<float>(g_camera_view_tunables.at(index));
    }
    u32 view_word(size_t index) const
    {
        return g_camera_view_tunables.at(index);
    }
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
struct CameraMindRecord
{
    MindRecord base{}; // +0
    // +0x7C: The actor the follow point tracks (the request's +0x10). A new one, or message 0x7C naming it, starts a
    // blend from the old point (0x00079E80, 0x0007A550, 0x0007B040).
    u32 focus_actor = 0;
    Vec3 blend_from{}; // +0x80, the follow point when the focus actor changed
    float blend = 0.f; // +0x8C, 1 when the focus actor changes, down to 0 by the actor's rate
    Vec3 follow_point{}; // +0x90, the point 0x00079E80 last chose, after the blend
    // +0x9C: The target's body bit 0x30 holds the point it had when the bit came on (0x0007AA20).
    u32 target_point_held = 0;
    Vec3 held_target_point{}; // +0xA0
    // +0xAC: The heading goal 0x000791A0 sets: active, its heading, how long it holds, its turn speed and the caller's
    // tag. 0x00078630 eases +0xB8 toward the goal and clears the goal once its hold runs out.
    u32 heading_goal_active = 0;
    // +0xB0: Set when a goal starts without one active or ends; 0x00078840 then takes the framed heading into +0xB8.
    u32 heading_restart = 0;
    float heading_goal = 0.f; // +0xB4
    float heading = 0.f; // +0xB8, the rig's heading 0x00078630 eases and clamps
    float heading_goal_hold = 0.f; // +0xBC
    // +0xC0: Time the body has pushed the camera along its view (body bit 0x15, 0x00079720); past DAT_00427500 it
    // moves.
    float pushed_time = 0.f;
    // +0xC4: 1 while the heading is held (DAT_00434030 below 0 and the target mind's +0x20 set, 0x00078630).
    u32 heading_held = 0;
    // +0xC8: The rig's stored distance (0x00078840): a framed distance beyond both it and the planar gap is cut to the
    // larger of the two; any other framed distance is stored here.
    float distance = 0.f;
    // +0xCC: 1 when the target record's route reaches the eye (0x00079320), so no heading is chosen.
    u32 route_reaches_eye = 0;
    float heading_goal_speed = 0.f; // +0xD0
    u32 heading_goal_tag = 0; // +0xD4
    u32 above_floor = 0; // +0xD8, 0x00078840: the rig's position is above the floor below it
};
static_assert(sizeof(CameraMindRecord) == 0xDC);

GBTG_MIND_VIEW(camera_mind_record, CameraMindRecord)
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
