#pragma once
#include "shader_validation_balance.h"
#include "shader_validation_registration.h"
#include <functional>

namespace gbtg
{
struct ShaderValidationEraseContext
{
    ShaderValidationBalancedTree& tree;
    std::function<void(ShaderValidationBalancedNode&, u32)> deleting_destructor; // Genuine node vtable slot +0x00.
};

ShaderValidationBalancedNode** advance_shader_validation_balanced_iterator(ShaderValidationBalancedNode** iterator); // Xbox 0x00190234
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes(ShaderValidationBalancedNode* parent, ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second); // Xbox 0x0019158A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node(ShaderValidationEraseContext& context, ShaderValidationBalancedNode* node); // Xbox 0x00191F4A
void unregister_shader_validation_object(ShaderValidationEraseContext& context, u32 object_address); // Xbox 0x0019269C
void destroy_shader_validation_registered_object(ShaderValidationEraseContext& context, ShaderValidationRegisteredObject& object, u32 object_address); // Xbox 0x001928F6
}
