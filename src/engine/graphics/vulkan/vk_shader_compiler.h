#pragma once

#include "gbtg/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace gbtg::vk
{

// D3D11 register numbers in the shared HLSL (gfx_shader_sources.h) land on these set 0 bindings.
constexpr u32 kConstantBinding = 0; // b0..b3
constexpr u32 kTextureBinding = 10; // t0..t3
constexpr u32 kSamplerBinding = 20; // s0..s3

// The shared HLSL with its texture and sampler arrays split into single bindings; Vulkan cannot alias the
// stage_tex[4] array with g_tex / g_light the way D3D11 registers do.
std::string vulkan_hlsl(std::string source);

// SPIR-V for `entry` from DXC (dxcompiler.dll beside the executable). `profile` is "vs_6_0" or "ps_6_0";
// `strict` is IEEE strictness, D3D11's D3DCOMPILE_IEEE_STRICTNESS. An empty result is a logged compile failure.
std::vector<u32> compile_hlsl_spirv(std::string_view source, const char* entry, const char* profile, bool strict = false);

} // namespace gbtg::vk
