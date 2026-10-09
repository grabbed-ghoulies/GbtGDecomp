#pragma once
#include "gbtg/types.h"

namespace gbtg
{
inline constexpr u32 runtime_shader_write_storage_size = 0xE0;
void initialize_runtime_shader_writes(u8* writes); // Xbox 0x001866C9
u32 accumulate_runtime_shader_writes(u8* writes, const u32* instruction); // Xbox 0x00186724
bool runtime_shader_write_masks_overlap(const u8* first, const u8* second, u32 count); // Xbox 0x001866F8
bool runtime_shader_writes_conflict(const u8* first, const u8* second); // Xbox 0x00189C00
bool runtime_shader_operand_reads_writes(const u8* writes, const u32* instruction, u32 type, u32 reg,
    u32 negate, u32 x, u32 y, u32 z, u32 w, u8 mask); // Xbox 0x00186806
bool runtime_shader_first_operand_reads_writes(const u8* writes, const u32* instruction, u8 mask); // Xbox 0x00189C62
bool runtime_shader_second_operand_reads_writes(const u8* writes, const u32* instruction, u8 mask); // Xbox 0x00189CB3
bool runtime_shader_third_operand_reads_writes(const u8* writes, const u32* instruction, u8 mask); // Xbox 0x00189D06
u32 runtime_shader_vector_reads_writes(const u8* writes, const u32* instruction, u8* conflict); // Xbox 0x00189D56
u32 runtime_shader_scalar_reads_writes(const u8* writes, const u32* instruction, u8* conflict); // Xbox 0x00189EAD
bool runtime_shader_instruction_reads_writes(const u8* writes, const u32* instruction); // Xbox 0x0018B384
}
