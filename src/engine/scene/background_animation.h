#pragma once

#include "../graphics/point_graph.h"

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
}
