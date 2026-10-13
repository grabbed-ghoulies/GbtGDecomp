#pragma once

#include "gbtg/model_render.h"
#include <unordered_map>

namespace gbtg
{
// The node type registry (DAT_00434968: descriptors pointer, count). Descriptors are 0x54 bytes: an inline
// name, then the type's vtable (draw, relocate, ...) at +0x50.
struct RuntimeNodeTypeRegistry
{
    std::span<u8> descriptors; // DAT_00434970.
    u32& count; // DAT_0043496C.
};
void initialize_runtime_node_registry(RuntimeNodeTypeRegistry& registry); // Xbox 0x00158800
void register_runtime_node_type(RuntimeNodeTypeRegistry& registry, const char* name,
    u32 vtable); // Xbox 0x00158730
// Lighting vertex-shader fragments: 239 bucket sentinels (DAT_004439A8, stride 8) over 0x10-byte nodes
// (DAT_00444120) whose +8 is the key and +0xC the program.
struct RuntimeShaderFragmentTable
{
    std::array<RuntimeRenderLight, 239> buckets{};
    std::array<RuntimeRenderLight, 256> nodes{};
    i32 count = 0; // DAT_00445124.
    std::unordered_map<RuntimeRenderLight*, u32> programs; // Node +0xC.
};
void link_runtime_shader_fragment(RuntimeRenderLight& list, RuntimeRenderLight& node); // Xbox 0x00159A00
void register_runtime_shader_fragment(u32 key, u32 first, u32 second,
    RuntimeShaderFragmentTable& table); // Xbox 0x00158E00
void register_runtime_shader_fragments(RuntimeShaderFragmentTable& table); // Xbox 0x00159720
void initialize_runtime_lighting(RuntimeModelRenderContext& renderer,
    RuntimeShaderFragmentTable& table); // Xbox 0x00158CE0
// Render state defaults, the current frame's world matrix, the node type registry and lighting.
void initialize_runtime_model_renderer(RuntimeModelRenderContext& renderer, RuntimeNodeTypeRegistry& registry,
    RuntimeShaderFragmentTable& table); // Xbox 0x00155A10
}
