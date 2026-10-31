#pragma once
#include "camera_mind_rig.h"
#include "../entities/body_action_selection.h"
#include "../entities/player_children.h"

namespace gbtg
{
// The camera mind (type 0x36) request methods over its kind-1 camera rig.
struct RuntimeCameraMindRequests
{
    RuntimeCameraMindView& view;
    const RuntimeCameraRigContext& rig;
    RuntimeBodyActionSelection& actions; // Body +0x73C table, selection 0x000276A0.
    u32& random; // DAT_0054DD7C.
    std::function<u32(const RuntimePlayerMindRequest&)> request_address; // Retail address of the request.
    std::function<i32(const RuntimePlayerMindRequest&)> parent_update; // 0x000295E0 for the other kinds.
    entity_detail::NavigationPathSearch* path_search = nullptr; // Mind +0x1C (0x00136700's search), or none.
};
// The follow request's point: the request's +0x10 actor (by DAT_00427540: its position, or its cylinder raised by
// DAT_00427544 of its height), eased (mind +0x8C, cosine) from the last point (mind +0x80) toward it; the near and
// far distances are DAT_00427550 and DAT_00427554. Returns 0.
u32 track_camera_follow_point(std::span<u8> mind, std::span<const u8> request, const Actor& camera, Vec3& point,
    float& near_distance, float& far_distance,
    const RuntimeCameraRigContext& context); // Xbox 0x00079E80, EBX mind, EAX request
// The camera's eye `eye` kept within the distance and the pitch and yaw ranges of `point`, moved only past a 0.001
// dead zone; a nonzero `curve` (an input device) turns the yaw by its analog difference (0x00108D20).
void place_camera_follow_eye(const Vec3& point, Vec3& eye, const Actor& camera, float near_distance,
    float far_distance, float low_pitch, float high_pitch, float low_yaw, float high_yaw, u32 curve,
    const RuntimeCameraRigContext& context); // Xbox 0x00079C40, EAX point, EDI eye, EBX camera
i32 update_camera_follow_request(RuntimeCameraMindView& view, std::span<const u8> request,
    const RuntimeCameraRigContext& context); // Xbox 0x00079B80, EBX mind
// An actor's framing point: its position (or, by DAT_00427560, its cylinder) raised by DAT_00427564 of its height;
// `heading` (when given) takes its yaw (+0x18) in degrees.
void camera_actor_point(const RuntimeDirectorActor& actor, Vec3& point, float* heading,
    const RuntimeCameraRigContext& context); // Xbox 0x0007A160, ECX actor, EAX point
// The two-player camera's point (only under the "cameron" camera): the mind's target, or the midpoint of it and its
// own target, eased (mind +0x8C) from the last point; the near and far distances fit both players. Returns the
// target's input device (+0x960), or 0 with no target.
u32 track_camera_pair_point(std::span<u8> mind, const Actor& camera, Vec3& point, float& near_distance,
    float& far_distance, float& heading,
    const RuntimeCameraRigContext& context); // Xbox 0x0007A550, ESI mind, EBX point
// The yaw turned by the input device's first analog level toward the side it lies from `heading`, or reset behind
// `heading` once it would pass the opposite side.
void turn_camera_yaw_by_input(const Actor& camera, float& yaw, u32 device, float heading,
    const RuntimeCameraRigContext& context); // Xbox 0x0007A420, EAX camera, ESI yaw
// 0x00079C40's placement in degrees, with the device's turn (0x0007A420) applied to the yaw.
void place_camera_pair_eye(const Vec3& point, Vec3& eye, const Actor& camera, float near_distance, float far_distance,
    float low_pitch, float high_pitch, float low_yaw, float high_yaw, float heading, u32 device,
    const RuntimeCameraRigContext& context); // Xbox 0x0007A1F0, EBX point, EDI eye
i32 update_camera_pair_request(RuntimeCameraMindView& view,
    const RuntimeCameraRigContext& context); // Xbox 0x0007A080
// The path request's point: the mind's target (`target`) raised by half its height (`rise`); with DAT_0042759C and
// the target's body bit 0x30 set, the first such point is held (mind +0x9C, +0xA0). Returns 0 with no target.
u32 camera_path_target_point(std::span<u8> mind, const RuntimeDirectorActor& target, Vec3& point, float& rise,
    const RuntimeCameraRigContext& context); // Xbox 0x0007AA20, ECX target, EAX point, EBX mind
// The eye `wanted` from `eye` toward `point`: within DAT_00427588..DAT_0042758C of it and the DAT_00427580 /
// DAT_00427584 pitch, or `eye` itself within 0.001.
void camera_path_eye(const Vec3& point, const Vec3& eye, Vec3& wanted,
    const RuntimeCameraRigContext& context); // Xbox 0x0007AB00, EAX wanted, EBX point, EDI eye
// Whether the floor grid's cell `cell` sees `point` (0x0012CC50 from the cell's centre).
bool navigation_cell_sees_point(i32 cell, i32 floor, const Vec3& point, i32 region_mode, i32 allow_tagged,
    i32 reject_marked); // Xbox 0x0012C9C0, EAX cell, ECX floor
// With the target's sight to the eye blocked, the first cell of the floor route from the eye to the point that sees
// the point takes the wanted eye (0x0007AB00); 1 when one does.
// `camera` is the mind's +8, whose step (0x00042320) advances the search; `search` is the mind's +0x1C.
u32 route_camera_path_eye(const Actor& camera, const RuntimeDirectorActor& target, const Vec3& eye,
    const Vec3& point, Vec3& wanted, entity_detail::NavigationPathSearch* search,
    const RuntimeCameraRigContext& context); // Xbox 0x0007ACB0
// The wanted eye (0x0007AB00), routed round (0x0007ACB0) when the target cannot see it, then 0x0013D110's floor check.
void place_camera_path_eye(const Actor& camera, const RuntimeDirectorActor& target, const Vec3& point,
    const Vec3& eye, Vec3& wanted, entity_detail::NavigationPathSearch* search,
    const RuntimeCameraRigContext& context); // Xbox 0x0007AE70, EAX point, ECX target
i32 update_camera_path_request(RuntimeCameraMindView& view, entity_detail::NavigationPathSearch* search,
    const RuntimeCameraRigContext& context); // Xbox 0x0007A910, EAX mind
i32 update_camera_mind_request(RuntimePlayerMindRequest& request,
    RuntimeCameraMindRequests& context); // Xbox 0x00078170, vtable +0x14
i32 initialize_camera_mind_request(RuntimePlayerMindRequest& request,
    RuntimeCameraMindRequests& context); // Xbox 0x0007AEF0, vtable +0x18
}
