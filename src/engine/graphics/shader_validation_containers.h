#pragma once
#include "shader_validation_accessors.h"
#include "shader_validation_registration.h"

namespace gbtg
{
// Tree object owning a pass-layout tree (vtable 0x002037D8).
struct ShaderValidationPassTreeObject
{
    u32 vtable_address; // Retail +0x00.
    u32 retained; // Retail +0x04.
    ShaderValidationPassTree nodes; // Retail root +0x08, count +0x0c.
};

struct ShaderValidationList
{
    u32 vtable_address; // Retail +0x00.
    ShaderValidationListStorage storage; // Retail +0x04..+0x0c.
};

ShaderValidationBalancedNode* initialize_shader_validation_node_2037ec(ShaderValidationBalancedNode& node, u32 value,
    ShaderValidationBalancedNode* parent); // Xbox 0x001902D9
ShaderValidationBalancedNode* initialize_shader_validation_node_2037f0(ShaderValidationBalancedNode& node, u32 value,
    ShaderValidationBalancedNode* parent); // Xbox 0x001903E1
ShaderValidationBalancedNode* initialize_shader_validation_node_2037f4(ShaderValidationBalancedNode& node, u32 value,
    ShaderValidationBalancedNode* parent); // Xbox 0x0019042A
ShaderValidationPassNode* initialize_shader_validation_pass_node(ShaderValidationPassNode& node, u32 value, void* data,
    ShaderValidationPassNode* parent); // Xbox 0x00190473
i32 calculate_shader_validation_pass_node_height(const ShaderValidationPassNode& node); // Xbox 0x0019051C
ShaderValidationBalancedNode** retreat_shader_validation_node_iterator(ShaderValidationBalancedNode** iterator); // Xbox 0x00190337
ShaderValidationPassNode* find_shader_validation_pass_node(const ShaderValidationPassTree& tree, u32 key); // Xbox 0x00190B12
void* last_shader_validation_tree_value(const ShaderValidationTreeStorage& storage); // Xbox 0x00190CA6
void destroy_shader_validation_list(ShaderValidationList& list); // Xbox 0x001909B1
ShaderValidationChannelMask* construct_shader_validation_single_channel_mask(ShaderValidationChannelMask& mask,
    u32 channel); // Xbox 0x0018F314
ShaderValidationListNode* construct_shader_validation_list_node(ShaderValidationListNode& node,
    const ShaderValidationListVtable* vtable, void* value, ShaderValidationListNode* previous,
    ShaderValidationListNode* next); // Xbox 0x0018F1E5
void append_shader_validation_list_value(ShaderValidationList& list, const ShaderValidationListVtable* node_vtable,
    const std::function<void*(std::size_t, u32)>& allocate, void* value); // Xbox 0x0019010E
ShaderValidationInstruction* construct_shader_validation_instruction_base(ShaderValidationInsertContext& context,
    ShaderValidationInstruction& instruction, u32 object_address); // Xbox 0x001929B7
}
