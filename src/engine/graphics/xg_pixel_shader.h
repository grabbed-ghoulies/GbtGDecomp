#pragma once

#include "gbtg/model_relocation.h"
#include <string_view>

namespace gbtg
{
using XboxPixelShaderDefinition = std::array<u32, 60>;
// XDK XGAssembleShader host boundary. Compiles the boot table's xps.1.1 instructions to NV2A combiners.
XboxPixelShaderDefinition assemble_xg_pixel_shader(std::string_view source);
struct RuntimeBootPixelShaders
{
    const RuntimeRelocationImage& image;
    std::function<i32(std::string_view, XboxPixelShaderDefinition&)> assemble;
    std::function<i32(const XboxPixelShaderDefinition&, u32&)> create;
    std::function<u32(u32)> factory;
};
u32 create_runtime_boot_texture_pixel_shader(RuntimeBootPixelShaders& context); // Xbox 0x00063F40
void create_runtime_boot_pixel_shaders(RuntimeBootPixelShaders& context); // Xbox 0x00063FB0
}
