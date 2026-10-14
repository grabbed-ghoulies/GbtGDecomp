#pragma once

#include "gbtg/model_relocation.h"
#include <string_view>
#include <vector>

namespace gbtg
{
// Host boundary for XDK XGAssembleShader, vertex shader input. Unsupported syntax raises.
std::vector<u32> assemble_xg_vertex_shader(std::string_view source);
struct RuntimeBootVertexShaders
{
    const RuntimeRelocationImage& image;
    std::function<i32(std::string_view, std::vector<u32>&)> assemble;
    std::function<i32(const i32*, const u32*, u32&, u32)> create;
    std::function<void(u32)> bind;
    std::function<void(u32, const float*, u32)> constants;
};
void create_runtime_boot_vertex_shaders(RuntimeBootVertexShaders& context); // Xbox 0x00065EE0
void bind_runtime_boot_vertex_shader(u32 index, RuntimeBootVertexShaders& context); // Xbox 0x00065F60
}
