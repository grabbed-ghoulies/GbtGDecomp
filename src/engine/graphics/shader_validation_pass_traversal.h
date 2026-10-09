#pragma once
#include "gbtg/types.h"
#include "shader_validation_balance.h"

namespace gbtg
{
struct ShaderValidationPassNode
{
    u32 vtable_address; // Retail +0x00.
    u32 value; // +0x04, unsigned search key/address.
    void* data; // +0x08; the register tree stores ShaderValidationOwner pointers here.
    ShaderValidationPassNode* parent; // +0x0C.
    ShaderValidationPassNode* first; // +0x10.
    ShaderValidationPassNode* second; // +0x14.
    i32 height; // +0x18.
};

struct ShaderValidationPassTree
{
    ShaderValidationPassNode* root; // Retail tree +0x08.
    u32 count; // +0x0C.
};

ShaderValidationPassNode** first_shader_validation_pass_node(const ShaderValidationPassTree& tree,
    ShaderValidationPassNode** result); // Xbox 0x00190148
ShaderValidationBalancedNode* find_shader_validation_pass_node(const ShaderValidationBalancedTree& tree,
    u32 value); // Xbox 0x001902BA
ShaderValidationPassNode** advance_shader_validation_pass_iterator(ShaderValidationPassNode** iterator); // Xbox 0x0019038F
}
