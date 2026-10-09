#pragma once
#include "gbtg/types.h"
#include "shader_validation_balance.h"
#include <array>

namespace gbtg
{
using ShaderValidationNodeWords = std::array<u32, 6>;

ShaderValidationNodeWords* initialize_shader_validation_object_node(ShaderValidationNodeWords& node, u32 value, u32 parent); // Xbox 0x0019018D
ShaderValidationNodeWords* initialize_shader_validation_diagnostic_node(ShaderValidationNodeWords& node, u32 value, u32 parent); // Xbox 0x00190271
ShaderValidationBalancedNode* initialize_shader_validation_object_node(ShaderValidationBalancedNode& node, u32 value, ShaderValidationBalancedNode* parent); // Xbox 0x0019018D
ShaderValidationBalancedNode* initialize_shader_validation_diagnostic_node(ShaderValidationBalancedNode& node, u32 value, ShaderValidationBalancedNode* parent); // Xbox 0x00190271
}
