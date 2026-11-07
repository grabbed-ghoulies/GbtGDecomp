#pragma once

#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct ActorMatrixSetup
{
    RuntimeActorMatrixState matrices;
    u8* allocation = nullptr; // actor +0x288
    float progress = 0.f; // +0x29C
    u32 flags = 0; // +0x2A0
};
// The allocation's records (the base release 0x0003EBA0 frees the block itself).
inline std::span<ActorMatrixRecord> actor_matrix_records(ActorMatrixSetup& setup)
{
    if (setup.allocation == nullptr)
    {
        return {};
    }
    return {reinterpret_cast<ActorMatrixRecord*>(setup.allocation), size_t(setup.matrices.count)};
}
using ActorMatrixBindings = std::unordered_map<const u8*, std::span<const u8>>;

i32 runtime_matrix_array_count(std::span<const u8> node); // Xbox 0x00155FE0
u32 runtime_matrix_array_matrix_offset(std::span<const u8> node,
    i32 index); // Xbox 0x00155FF0
u32 actor_model_link_head_offset(const Actor& actor); // Xbox 0x00040BE0
u32 runtime_model_animation_matrix_array(std::span<const u8> cpu); // Xbox 0x00053000
u32 runtime_matrix_array_binding(std::span<const u8> node, std::span<const u8> bindings,
    u32 index); // Xbox 0x00156050
bool initialize_actor_model_matrices(const Actor& actor, ActorMatrixSetup& state,
    std::span<const RuntimeModelNodeType> types,
    const ActorMatrixBindings& bindings); // Xbox 0x000400D0
}
