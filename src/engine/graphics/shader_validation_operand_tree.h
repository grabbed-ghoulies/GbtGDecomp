#pragma once
#include "shader_validation_insert.h"

namespace gbtg
{
ShaderValidationBalancedNode* initialize_shader_validation_operand_node(ShaderValidationBalancedNode& node, u32 value, ShaderValidationBalancedNode* parent); // Xbox 0x001901EB
ShaderValidationBalancedNode* insert_shader_validation_operand_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node); // Xbox 0x001920BB
void register_shader_validation_operand(ShaderValidationInsertContext& context, u32 value); // Xbox 0x001926B4
}
