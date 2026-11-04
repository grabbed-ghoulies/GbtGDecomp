#include "camera_actor_classes.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_mind_goals.h"
#include "../camera/camera_mind_rig.h"
#include "../camera/view_initialization.h"
#include <cstring>

namespace gbtg
{
u32 g_camera_actor = 0; // DAT_00510F70

// Pointer-width adapter over the camera body sizes.
void camera_body_object_sizes(ObjectSizes& output)
{
    u32 sizes[2];
    entity_body_camera_sizeof(sizes);
    output.instance = sizes[0];
    output.parameters = sizes[1];
}

// Xbox 0x000731F0
bool initialize_runtime_camera_body(RuntimeCameraBodyContext& context)
{
    auto body = context.body;
    context.bits->bytes[3] &= 0xDF;
    std::memcpy(&g_camera_actor, body.data() + 8, 4);
    initialize_camera_view(context.view, "cameron", 1); // String 0x00323E30.
    // The inlined list insertion and first-bind of 0x00048C20 on slot 0 (DAT_004E1680).
    link_camera_view(0, context.view);
    camera_bind_named_view(0, "cameron");
    const Vec3 position = actor_world_position(context.actor);
    const Vec3 euler = actor_world_euler(context.actor);
    std::memcpy(body.data() + 0x89C, &position, sizeof(position));
    std::memcpy(body.data() + 0x8A8, &euler, sizeof(euler));
    // The native camera node is the record embedded at body +0x894 (position +8, Euler rotation +0x14).
    context.view.position = position;
    context.view.rotation.x = euler.x;
    context.view.rotation.y = euler.y;
    context.view.rotation.z = euler.z;
    return true;
}

// Xbox 0x00073300
void destroy_runtime_camera_body(RuntimeCameraBodyContext& context)
{
    camera_unlink_view(&context.view);
    g_camera_actor = 0;
}

// Xbox 0x0007AFC0
void camera_mind_object_sizes(ObjectSizes& output)
{
    output.instance = 0xDC;
    output.parameters = 0xA0;
}

// Xbox 0x000AAE40
void director_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = 0x44;
    output.parameters = 0x18;
}

// Xbox 0x000AAEB0
bool initialize_runtime_director_strategy(std::span<u8> strategy)
{
    const u32 zero = 0;
    const u32 one = 1;
    std::memcpy(strategy.data() + 0x38, &zero, 4);
    std::memcpy(strategy.data() + 0x3C, &one, 4);
    std::memcpy(strategy.data() + 0x40, &one, 4);
    return true;
}

// Xbox 0x000AA3A0
void enter_runtime_director_state(u16 state, RuntimeDirectorStrategy& context)
{
    switch (state)
    {
    case 0x3F6:
    {
        auto* request = allocate_runtime_mind_request(context.requests, 3, context.request_sizes);
        std::memcpy(request->bytes.data() + 0x10, context.strategy.data() + 0x38, 4);
        return;
    }
    case 0x3F7:
        allocate_runtime_mind_request(context.requests, 4, context.request_sizes);
        return;
    case 0x401:
        allocate_runtime_mind_request(context.requests, 0x12, context.request_sizes);
        return;
    case 0x40C:
        return;
    default:
        enter_runtime_base_strategy_state(context.entry, state);
        return;
    }
}

// Xbox 0x000AAED0
void dispatch_runtime_director_message(std::span<const u32> message, RuntimeDirectorMessages& context)
{
    switch (message[0])
    {
    case 0x26:
    case 0x27:
        return;
    case 0x7D:
    {
        Vec3 position;
        Vec3 euler;
        if (context.camera_start(position, euler))
        {
            actor_set_world_position(context.actor, position);
            normalize_euler_angles(context.actor.euler, euler);
            context.actor.xform_valid = 0;
        }
        context.actor_update();
        break;
    }
    case 0xBA:
        set(context.mind, mind_field::target, message[1]);
        change_runtime_player_strategy_state(context.strategy, 0x40C, context.setup);
        break;
    case 0xBB:
        std::memcpy(context.strategy.bytes.get() + 0x38, &message[1], 4);
        change_runtime_player_strategy_state(context.strategy, 0x3F6, context.setup);
        break;
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, context.base);
}

// Xbox 0x0007B040
void dispatch_runtime_camera_mind_message(std::span<const u32> message, RuntimeCameraMindMessages& context)
{
    if (message[0] == 0x7C)
    {
        if (message[1] == get(context.mind, camera_mind_field::focus_actor))
        {
            const u32 zero = 0;
            set(context.mind, camera_mind_field::focus_actor, zero);
        }
    }
    else if (message[0] == 0x7D)
    {
        initialize_runtime_player_mind_navigation(context.mind, context.floor, context.grids, context.nodes,
            context.storage, context.address_of);
        dispatch_runtime_parent_mind_message(context.events, message, context.target);
        return;
    }
    dispatch_runtime_parent_mind_message(context.events, message, context.target);
}

// Xbox 0x000AA450
void leave_runtime_director_state(std::span<u8> strategy, u16 state)
{
    if (state != 0x3F7 && (state == 0x3FC || state == 0x3FF || state == 0x415))
    {
        --strategy_base(strategy).pause_count;
    }
}
}
