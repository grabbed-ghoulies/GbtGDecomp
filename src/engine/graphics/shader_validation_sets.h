#pragma once
#include "shader_validation_constructors.h"
#include "shader_validation_tree_instances.h"
#include "shader_validation_instruction_ops.h"
#include <functional>

namespace gbtg
{
// Maps a retail instruction address stored in a set node to the actual instruction.
using ShaderValidationInstructionAt = std::function<const ShaderValidationInstruction*(u32)>;

bool shader_validation_owner_derives_from(const ShaderValidationOwner& owner, const ShaderValidationOwner* other); // Xbox 0x0018E78B
bool shader_validation_derived_owner_writes_channel(const ShaderValidationOwner& owner, u32 channel); // Xbox 0x0018F56F
bool shader_validation_channel_owner_writes_channel(const ShaderValidationOwner& owner, u32 channel); // Xbox 0x0018F549
void destroy_shader_validation_balanced_nodes_18f2aa(ShaderValidationBalancedNode* node,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x0018F2AA, as 0x0018F281
void destroy_shader_validation_balanced_nodes_18f2d3(ShaderValidationBalancedNode* node,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x0018F2D3, as 0x0018F281
void destroy_shader_validation_pass_nodes(ShaderValidationPassNode* node,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x0018F1BC
void clear_shader_validation_balanced_tree_190322(ShaderValidationBalancedChildTree& tree,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x00190322, as 0x001901D6
void clear_shader_validation_pass_tree(ShaderValidationPassTreeObject& tree,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x00190163
ShaderValidationBalancedChildTree* delete_shader_validation_balanced_tree(ShaderValidationBalancedChildTree& tree, u8 flags,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor,
    const std::function<void(void*, u32)>& release); // Xbox 0x00190AA0
ShaderValidationPassTreeObject* delete_shader_validation_pass_tree(ShaderValidationPassTreeObject& tree, u8 flags,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor,
    const std::function<void(void*, u32)>& release); // Xbox 0x00190AC6
void register_shader_validation_value_2037ec(ShaderValidationInsertContext& context, u32 value); // Xbox 0x00192772
void register_shader_validation_value_2037f4(ShaderValidationInsertContext& context, u32 value); // Xbox 0x0019287F
ShaderValidationPassNode* register_shader_validation_pass_value(ShaderValidationPassTree& tree,
    const std::function<void*(std::size_t, u32)>& allocate, u32 key, void* data); // Xbox 0x001924DA

bool shader_validation_instruction_precedes(u32 first, u32 second, const ShaderValidationInstructionAt& instruction_at); // Xbox 0x0018E94C
ShaderValidationBalancedNode* find_shader_validation_instruction_node(const ShaderValidationBalancedTree& tree, u32 instruction,
    const ShaderValidationInstructionAt& instruction_at); // Xbox 0x0018F174
ShaderValidationBalancedNode* insert_shader_validation_instruction_node(ShaderValidationInsertContext& context, u32 instruction,
    ShaderValidationBalancedNode* node, const ShaderValidationInstructionAt& instruction_at); // Xbox 0x001923BB
void register_shader_validation_instruction(ShaderValidationInsertContext& context, u32 instruction,
    const ShaderValidationInstructionAt& instruction_at); // Xbox 0x001927D1

// Maps between retail object addresses kept in validator trees and the actual objects.
using ShaderValidationOperandAt = std::function<ShaderValidationOperand*(u32)>;
using ShaderValidationAddressOf = std::function<u32(const void*)>;

void advance_shader_validation_arena_iterator(ShaderValidationBalancedNode*& node); // Xbox 0x0018F0EB
ShaderValidationPassNode** set_shader_validation_pass_value(ShaderValidationPassTree& tree,
    const std::function<void*(std::size_t, u32)>& allocate, ShaderValidationPassNode** result, u32 key, void* data); // Xbox 0x00192830
void copy_shader_validation_instruction_set(ShaderValidationInstruction& instruction, const ShaderValidationBalancedChildTree& source,
    const std::function<void*(std::size_t, u32)>& allocate, const ShaderValidationInstructionAt& instruction_at); // Xbox 0x001929E7
void release_shader_validation_instruction_set(ShaderValidationInstruction& instruction, ShaderValidationEraseContext& instructions,
    const ShaderValidationAddressOf& address_of,
    const std::function<void(ShaderValidationBalancedChildTree&, u32)>& deleting_destructor); // Xbox 0x00192A2B
bool shader_validation_owner_writers_in_set(const ShaderValidationOwner& owner, u32 unused, ShaderValidationChannelMask mask,
    const ShaderValidationBalancedTree& set, const ShaderValidationOwner* active_owner, const ShaderValidationAddressOf& address_of,
    const ShaderValidationInstructionAt& instruction_at); // Xbox 0x00190774
bool swap_shader_validation_owner_channels(ShaderValidationOwner& owner, u32 first, u32 second, u8 climb, u8 propagate,
    const ShaderValidationOperandAt& operand_at); // Xbox 0x00190FFE
}
