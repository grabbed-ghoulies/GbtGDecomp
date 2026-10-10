#pragma once
#include "shader_validation_owner_sets.h"
#include "shader_validation_instruction_constructor.h"

namespace gbtg
{
struct ShaderValidationDependencyContext
{
    ShaderValidationConstructorContext& constructors;
    ShaderValidationInsertContext& instruction_sets; // DAT_001a4d58.
    const ShaderValidationSetContext& sets;
    u8 include_operand_writers; // DAT_001a4d30.
};

ShaderValidationBalancedChildTree& collect_shader_validation_owner_dependencies(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, ShaderValidationBalancedChildTree& dependencies, ShaderValidationChannelMask mask); // Xbox 0x0019390D
ShaderValidationBalancedChildTree& merge_shader_validation_instruction_dependencies(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationBalancedChildTree& dependencies); // Xbox 0x001931E4
void build_shader_validation_instruction_dependencies(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction); // Xbox 0x00193C44
bool shader_validation_instruction_has_dependency(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, u32 other); // Xbox 0x00193FE4
bool shader_validation_instruction_priority_precedes(ShaderValidationDependencyContext& context,
    u32 first, u32 second); // Xbox 0x00194002
void* select_shader_validation_list_priority(ShaderValidationDependencyContext& context,
    ShaderValidationListNode& first, ShaderValidationListNode* end); // Xbox 0x00194047
bool shader_validation_operand_accepts_parent(ShaderValidationDependencyContext& context,
    ShaderValidationOperand& operand, ShaderValidationOwner& parent, ShaderValidationChannelMask mask); // Xbox 0x001940AC
void move_shader_validation_readers_to_derived(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, u8 climb); // Xbox 0x00194D12
bool detach_unused_shader_validation_derived_owner(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner); // Xbox 0x00195BE1
bool rotate_shader_validation_kind_six_owner(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner); // Xbox 0x00195CA0
bool assign_shader_validation_owner_register(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    u32 kind, ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask mask, u8 query_only); // Xbox 0x00196814
bool assign_shader_validation_owner_scalar_register(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask mask, u8 query_only); // Xbox 0x001969C0
i32 choose_shader_validation_owner_register(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction& instruction, ShaderValidationChannelMask mask, u8 query_only); // Xbox 0x00196A14
bool shader_validation_instruction_registers_available(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, const ShaderValidationInstruction& instruction,
    const ShaderValidationBalancedTree& readers, i32* available); // Xbox 0x00197075
bool shader_validation_instruction_ready(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, const ShaderValidationInstruction& instruction,
    const ShaderValidationBalancedTree& writers, const ShaderValidationBalancedTree& readers); // Xbox 0x00196DD1
bool merge_shader_validation_owners(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, ShaderValidationOwner* other, u8 query_only,
    ShaderValidationChannelMask owner_mask, ShaderValidationChannelMask other_mask); // Xbox 0x00194E5E
bool merge_or_link_shader_validation_owners(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, ShaderValidationOwner* other, u8 query_only,
    ShaderValidationChannelMask owner_mask, ShaderValidationChannelMask other_mask); // Xbox 0x00195C1A
bool shader_validation_instructions_merge_compatible(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationInstruction& other); // Xbox 0x001954F3
// Retail reads the retained low byte of EBP-0x14 when source channel 4 is tested.
struct ShaderValidationMoveFrame
{
    u8 retained_iterator_byte;
};
bool rewrite_shader_validation_move(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, const ShaderValidationMoveFrame& frame); // Xbox 0x00193222
bool shader_validation_instructions_merge_on_pass(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, ShaderValidationInstruction& instruction, ShaderValidationInstruction& other,
    const ShaderValidationBalancedTree& readers); // Xbox 0x00196C74

}
