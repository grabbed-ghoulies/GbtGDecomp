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
};
i32 update_camera_mind_request(RuntimePlayerMindRequest& request,
    RuntimeCameraMindRequests& context); // Xbox 0x00078170, vtable +0x14
i32 initialize_camera_mind_request(RuntimePlayerMindRequest& request,
    RuntimeCameraMindRequests& context); // Xbox 0x0007AEF0, vtable +0x18
}
