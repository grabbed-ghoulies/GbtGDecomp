#pragma once
#include "shader_validation_arena.h"
#include "shader_validation_constructors.h"
#include "shader_validation_operand_order.h"

namespace gbtg
{
// Instruction object allocated by 0x00192DF1 (0x4C retail bytes).
// Mask and register names follow the microcode fields they are packed into.
struct ShaderValidationInstruction
{
    ShaderValidationRegisteredObject base; // Retail +0x00.
    std::array<ShaderValidationOperand*, 3> operands; // Retail +0x04, +0x08, +0x0c.
    u8 output_flag; // Retail +0x10; bit zero is packed.
    ShaderValidationChannelMask output_mask; // Retail +0x14.
    ShaderValidationChannelMask vector_mask; // Retail +0x1c; replaced by an empty mask for opcode 0xD.
    ShaderValidationChannelMask scalar_mask; // Retail +0x24.
    u32 vector_opcode; // Retail +0x2c.
    u32 scalar_opcode; // Retail +0x30.
    ShaderValidationOwner* output_register; // Retail +0x34; nine index bits are packed.
    ShaderValidationOwner* vector_destination; // Retail +0x38.
    ShaderValidationOwner* scalar_destination; // Retail +0x3c.
    std::array<u32, 3> retained40; // Retail +0x40..+0x48.
};

// Retained words of the 0x0018F8B7 frame at EBP-0x1C, EBP-0x18 and EBP-0x14.
// Retail reads these slots before writing them, then leaves the packed words
// there. The caller owns this storage; the serializer never initializes it.
struct ShaderValidationSerializerFrame
{
    u32 word0; // EBP-0x1C.
    u32 word1; // EBP-0x18.
    u32 word2; // EBP-0x14.
};

ShaderValidationChannelOrder* assign_shader_validation_channel_order(ShaderValidationChannelOrder& destination,
    const ShaderValidationChannelOrder& source); // Xbox 0x0018E7F6
ShaderValidationChannelOrder* replace_shader_validation_operand_channel_order(ShaderValidationChannelOrder& result,
    ShaderValidationOperand& operand, ShaderValidationChannelOrder value); // Xbox 0x0018F5D7
ShaderValidationChannelOrder* copy_shader_validation_operand_order(ShaderValidationChannelOrder& output,
    const ShaderValidationOperand& operand); // Xbox 0x0018E7D3
ShaderValidationOwner* find_shader_validation_foreign_parent(const ShaderValidationOwner& owner,
    const ShaderValidationOwner* active_owner); // Xbox 0x0018E973
u32 select_shader_validation_destination_index(const ShaderValidationInstruction& instruction); // Xbox 0x0018E816
u32* serialize_shader_validation_instruction(ShaderValidationInstruction& instruction, u32* output,
    ShaderValidationSerializerFrame& frame, const ShaderValidationOwner* active_owner); // Xbox 0x0018F8B7
bool serialize_shader_validation_program(ShaderValidationListNode* first, u32* program,
    ShaderValidationSerializerFrame& frame, const ShaderValidationOwner* active_owner); // Xbox 0x001914E8
}
