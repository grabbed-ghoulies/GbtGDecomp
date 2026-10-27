#pragma once

#include "gbtg/types.h"

#include <array>
#include <string>

namespace gbtg
{

// HLSL every gfx backend compiles (D3D11 with D3DCompile, Vulkan through DXC to SPIR-V). Register numbers are the
// D3D11 ones; the Vulkan backend maps them onto its descriptor bindings.
const char* gfx_hlsl_common();
const char* gfx_hlsl_alpha_test();
const char* gfx_hlsl_combiners();
const char* gfx_hlsl_native();
const char* gfx_hlsl_clear();

// The native pixel stage of a 60-word D3DPIXELSHADERDEF. Throws for a definition the combiner shader cannot run.
void gfx_check_native_pixel_definition(const u32* definition);
// The structural words a specialized shader bakes in; the colour constants (10..25, 43, 44) are zeroed, because
// the material buffer still supplies them.
std::array<u32, 60> gfx_native_pixel_key(const u32* definition);
// DOT_ZW on stage 3: the shader writes depth, entry ps_model_depth instead of ps_model.
bool gfx_native_pixel_writes_depth(const std::array<u32, 60>& key);
// The combiner source with `key`'s words as constants, the stage loop unrolled, and the native path's identity
// texture coordinates and v0/v1 vertex colours. Same equations as the generic ps_model.
std::string gfx_specialize_native_pixel_source(std::string source, const std::array<u32, 60>& key);

} // namespace gbtg
