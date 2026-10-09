#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct ShaderRegisterRewriteState
{
    u8* references; // Retail +0x00; 0x1C bytes per instruction.
    u8* values; // +0x04; 10 bytes per value.
    u16 value_count; // +0x08.
    u8* lifetimes; // +0x0C; 0x12 bytes per lifetime.
    u16 lifetime_count; // +0x10.
    u8 cursor; // +0x12.
    u32* program; // +0x14.
};
void mark_runtime_shader_register_lifetime(ShaderRegisterRewriteState& state, u16 lifetime,
    u16 instruction, u32 mask, u8 vector); // Xbox 0x00186D09
bool runtime_shader_register_has_space(const ShaderRegisterRewriteState& state,
    const u8* occupied, u8 reg, u16 lifetime); // Xbox 0x00186D67
u32 release_runtime_shader_register_channels(const ShaderRegisterRewriteState& state,
    const u8* references, u8* occupied, u16 instruction); // Xbox 0x00186F20
u8 remap_runtime_shader_register_mask(u8 mask, const u8* lifetime); // Xbox 0x00186F9D
u32 apply_runtime_shader_register_mapping(ShaderRegisterRewriteState& state); // Xbox 0x0018A38E
u32 assign_runtime_shader_register_channels(ShaderRegisterRewriteState& state, u8* occupied, u8 reg, u16 lifetime); // Xbox 0x00186DE0
u32 allocate_runtime_shader_register(ShaderRegisterRewriteState& state, u8* occupied, u16 lifetime, u8 fixed); // Xbox 0x00186E96
void create_runtime_shader_register_value(ShaderRegisterRewriteState& state, u8 mask, u8 reg,
    u8* reference, u8* current, u32 instruction, u32 vector); // Xbox 0x0018A24E
u32 assign_runtime_shader_register_value(ShaderRegisterRewriteState& state, u16 value, u8* occupied, u16 instruction); // Xbox 0x0018A302
u32 allocate_runtime_shader_program_registers(ShaderRegisterRewriteState& state); // Xbox 0x0018B8D0
void collect_runtime_shader_register_sources(ShaderRegisterRewriteState& state, u8 mask, u8 type,
    u8 reg, u8 x, u8 y, u8 z, u8 w, u8* references, u8* current, u16 instruction); // Xbox 0x00186A7B
u32 collect_runtime_shader_program_values(ShaderRegisterRewriteState& state); // Xbox 0x0018B6CC
u32 rewrite_runtime_shader_program_registers(ShaderRegisterRewriteState& state, u32* program); // Xbox 0x0018BD12
u32 rewrite_runtime_shader_register_one(ShaderRegisterRewriteState& state, u32 option, u32* program); // Xbox 0x0018BF69
}
