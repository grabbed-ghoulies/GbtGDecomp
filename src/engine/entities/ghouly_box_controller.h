#pragma once

#include "ghouly_requests.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct GhoulyPoseMessage
{
    u32 id = 0xA6;
    Vec3 position{};
    Vec3 euler{};
    u32 mode = 1;
    Vec3 output_position{};
    Vec3 output_euler{};
    u32 result = 0;
};
// Fields written by 0x0006BB70. The message and tick methods are separate
// retail functions; initializing this record does not create any actors.
struct GhoulyBoxController : GhoulyController
{
    Actor actor; // native base actor
    i32 spawned_940 = 0;
    void* anchor_934 = nullptr;
    void* load_938 = nullptr;
    void* source_93c = nullptr;
    char box_944[0x80]{};
    char attributes_9c4[0x80]{};
    u32 value_a48 = 0;
    u32 value_a4c = 0;
    u32 value_a50 = 0;
};

void tick_ghouly_box_controller(GhoulyBoxController& controller,
    u32& random_state); // Xbox 0x0006C0A0

void destroy_ghouly_box_controller(GhoulyBoxController& controller); // Xbox 0x0006BC70
bool query_ghouly_spawn_pose(GhoulyBoxController& controller, Vec3& source_position,
    Vec3& source_euler, Vec3& position, Vec3& euler); // Xbox 0x0006BCA0

bool initialize_ghouly_box_controller(GhoulyBoxController& controller,
    std::span<const u8> parameters, u32& random_state); // Xbox 0x0006BB70
}
