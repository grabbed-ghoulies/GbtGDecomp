#pragma once
#include "shader_validation_instruction_constructor.h"
#include "shader_validation_registers.h"
#include "shader_validation_sets.h"
#include "shader_validation_owner_sets.h"
#include "shader_validation_dependencies.h"

namespace gbtg
{
struct ShaderValidationInstructionDestroyContext;
// The 0x98-byte pass derived from the base pass at +0x00.
// Base fields +0x54..+0x84 retain the initial register owners.
struct ShaderValidationProgramPass
{
    ShaderValidationPass base; // Retail +0x00.
    ShaderValidationBalancedChildTree instruction_set; // Retail +0x88.
};

struct ShaderValidationPassDestroyContext
{
    ShaderValidationEraseContext& expressions; // DAT_001a4d48 registry.
    ShaderValidationEraseContext& objects; // DAT_001a4d38 registry.
    std::function<void(ShaderValidationPassNode&, u32)> deleting_destructor; // Pass-node vtable slot +0x00.
    std::function<u32(const void*)> retail_address;
};
void merge_shader_validation_program_instructions(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationProgramPass& pass); // Xbox 0x00195A15
void merge_shader_validation_program_moves(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationProgramPass& pass); // Xbox 0x00195AFC
void merge_shader_validation_list_candidates(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationListStorage& list, ShaderValidationListNode* first,
    ShaderValidationPass& pass, const ShaderValidationBalancedTree& readers); // Xbox 0x00196D71
u32 assign_shader_validation_instruction_registers(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationPass& pass, const ShaderValidationBalancedTree& readers,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x00196F09

void split_and_register_shader_validation_scalar_instruction(ShaderValidationConstructorContext& context,
    ShaderValidationProgramPass& pass, ShaderValidationInstruction& instruction,
    const ShaderValidationInstructionAt& instruction_at); // Xbox 0x0019368D
void split_shader_validation_program_scalar_instructions(ShaderValidationConstructorContext& context,
    ShaderValidationProgramPass& pass, const std::function<ShaderValidationInstruction*(u32)>& instruction_at); // Xbox 0x00193F8A

void destroy_shader_validation_pass_tree_object(ShaderValidationPassTreeObject& tree,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x001914B9
ShaderValidationOwner* create_shader_validation_constant_owner(ShaderValidationConstructorContext& context,
    const ShaderValidationPass& pass, u32 source, u8 use_pass_owner); // Xbox 0x001935E6
void destroy_shader_validation_pass(ShaderValidationPassDestroyContext& context, ShaderValidationPass& pass); // Xbox 0x001934A5
ShaderValidationOwner* get_shader_validation_input_owner(ShaderValidationConstructorContext& context,
    ShaderValidationPass& pass, u32 source); // Xbox 0x00193566
ShaderValidationOwner* select_shader_validation_owner(ShaderValidationConstructorContext& context, ShaderValidationPass& pass,
    u32 kind, u32 input_source, u32 constant_source, u32 register_kind, u8 use_pass_owner,
    ShaderValidationChannelMask mask); // Xbox 0x00193DF4
ShaderValidationOwner* set_shader_validation_pass_owner(ShaderValidationPass& pass, ShaderValidationOwner* owner,
    const ShaderValidationSetContext& context); // Xbox 0x00191EC3
ShaderValidationOwner* replace_shader_validation_register_owner(ShaderValidationPass& pass, ShaderValidationOwner& owner,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor,
    const ShaderValidationSetContext& context); // Xbox 0x00193E52
ShaderValidationOwner* replace_shader_validation_special_owner(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, ShaderValidationOwner& owner,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x00195D42
void destroy_shader_validation_program_pass(ShaderValidationPassDestroyContext& context, ShaderValidationProgramPass& pass,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x00193649
ShaderValidationProgramPass* delete_shader_validation_program_pass(ShaderValidationPassDestroyContext& context,
    ShaderValidationProgramPass* pass, u8 flags,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor,
    const std::function<void(void*, u32)>& release); // Xbox 0x00193EBE
}
