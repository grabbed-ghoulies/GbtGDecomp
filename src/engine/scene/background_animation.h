#pragma once

#include "../graphics/point_graph.h"
#include "../entities/actor_clock.h"

namespace gbtg
{
struct RuntimeBackgroundAnimation
{
    const std::span<const u8>* model_reference; // First background +0x204; referenced CPU may be null.
    std::span<const u8> animation_block; // CPU +0x10, bypass flag at +0x2C.
    std::span<const u8> header; // CPU +0x10 -> header +0x10.
    std::span<RuntimePointGraph> rows; // First background +0x950, descriptor stride 0x1E0.
    i32 matrix_count; // First background +0x28C.
    std::span<const u8> matrix_node; // First background +0x290.
};
void initialize_background_animation(RuntimeBackgroundAnimation* first,
    const std::function<std::span<const u8>(u32)>& resolve,
    const std::function<u32(u8*, u32)>& bind_allocation); // Xbox 0x00044490
// The first background (DAT_00511B20 node): actor +0x92C rate and +0x930 pause, its animation rows and block.
struct RuntimeBackgroundGraphPush
{
    const Actor& background;
    RuntimeBackgroundAnimation& animation;
    const RuntimeActorClock& clock;
    u32& random; // DAT_0054DD7C.
    const std::function<std::span<const u8>(u32, size_t)>& resolve;
};
// Pushes every point of one animation graph by the impulse (and its first point again by a random share),
// then steps the graph against the block's collision grid and recovers its transform.
void push_background_graph(const Vec3& impulse, u32 graph,
    RuntimeBackgroundGraphPush& context); // Xbox 0x00044A90, EAX impulse, EDX graph
}
