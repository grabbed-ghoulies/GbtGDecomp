#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct ShaderValidationBalancedNode
{
    u32 vtable_address; // Retail +0x00.
    u32 value; // Retail +0x04.
    ShaderValidationBalancedNode* parent; // Retail +0x08.
    ShaderValidationBalancedNode* first; // Retail +0x0c.
    ShaderValidationBalancedNode* second; // Retail +0x10.
    i32 height; // Retail +0x14.
};

struct ShaderValidationBalancedTree
{
    ShaderValidationBalancedNode* root; // Retail storage +0x08.
    u32 count; // Retail storage +0x0c.
};

i32 calculate_shader_validation_node_height(const ShaderValidationBalancedNode& node); // Xbox 0x001904C3
void update_shader_validation_ancestor_heights(ShaderValidationBalancedNode* node); // Xbox 0x001904FB
void rotate_shader_validation_tree_left(ShaderValidationBalancedNode& node); // Xbox 0x00190575
void rotate_shader_validation_tree_right(ShaderValidationBalancedNode& node); // Xbox 0x001905C0
void balance_shader_validation_ancestors(ShaderValidationBalancedNode& node); // Xbox 0x00190B27
void restore_shader_validation_tree_root(ShaderValidationBalancedTree& tree, ShaderValidationBalancedNode& node); // Xbox 0x001917D9
}
