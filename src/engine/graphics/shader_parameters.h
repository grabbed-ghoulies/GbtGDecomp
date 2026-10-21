#pragma once

#include "gbtg/types.h"

#include <functional>

namespace gbtg
{
struct RuntimeModelInstance;
struct RuntimeShaderInstance;
// A model shader's texture parameter (0x18 bytes, 0x000F8540): the model, the shader's instance (model +0x2A4 + the
// node's offset), the parameter's texture index, the texture bound to it and how that texture's frames play.
struct RuntimeShaderTextureParameter
{
    RuntimeModelInstance* model = nullptr; // +0
    RuntimeShaderInstance* shader = nullptr; // +4
    u32 index = 0; // +8
    u32 texture = 0; // +0xC, a texture's native address.
    u32 mode = 0; // +0x10: 0 plays the frames once over a second, 1 loops them at the texture's rate.
    float time = 0.f; // +0x14, seconds since the texture was bound.
};
// The named shader's texture parameter of that name, or null.
RuntimeShaderTextureParameter* create_shader_parameter(RuntimeModelInstance& model, const char* shader,
    const char* name); // Xbox 0x000F8540, EBX the model
void set_shader_texture(RuntimeShaderInstance& shader, u32 index, u32 texture); // Xbox 0x0015B2B0
void release_shader_parameter(RuntimeShaderTextureParameter* parameter); // Xbox 0x000F85D0, EAX the parameter
// `texture_bytes` gives a texture's bytes by its native address (+0x10 rate, +0x11 frames, +0x13 frame shown).
void set_shader_parameter_texture(RuntimeShaderTextureParameter& parameter, u32 texture, u32 mode,
    const std::function<u8*(u32)>& texture_bytes); // Xbox 0x000F86F0, EAX parameter, ECX texture, EDX mode
// The frame a looping texture shows `time` seconds in.
u8 runtime_texture_loop_frame(const u8* texture, float time); // Xbox 0x00053640, ESI the texture
void advance_shader_parameter(RuntimeShaderTextureParameter& parameter, float step,
    const std::function<u8*(u32)>& texture_bytes); // Xbox 0x000F8640, EAX the parameter, XMM0 the step
}
