#pragma once
#include "shader_validation_balance.h"
#include <cstddef>
#include <functional>

namespace gbtg
{
struct ShaderValidationInsertContext
{
    ShaderValidationBalancedTree& tree;
    std::function<void*(std::size_t, u32)> allocate; // XMemAlloc, host node size and retail allocation tag.
};

ShaderValidationBalancedNode* insert_shader_validation_object_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node); // Xbox 0x00191FBB
ShaderValidationBalancedNode* insert_shader_validation_diagnostic_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node); // Xbox 0x001921BB
}
