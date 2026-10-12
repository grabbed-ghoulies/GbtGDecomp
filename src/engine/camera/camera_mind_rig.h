#pragma once
#include "../entities/actor_clock.h"
#include "../entities/camera_director.h"
#include "../frontend/dialogue_input.h"
#include <array>
#include <functional>

namespace gbtg
{
// The camera rig tunables: .data DAT_004274B8..DAT_0042753F and DAT_00434008..DAT_00434033, by retail address.
struct RuntimeCameraRigTunables
{
    std::array<u8, 0x88> rig{};
    std::array<u8, 0x2C> view{};
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
