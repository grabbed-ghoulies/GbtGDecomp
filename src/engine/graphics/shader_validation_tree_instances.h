#pragma once
#include "shader_validation_containers.h"
#include "shader_validation_erase.h"
#include "shader_validation_insert.h"

namespace gbtg
{
void unregister_shader_validation_diagnostic(ShaderValidationEraseContext& context, u32 object_address); // Xbox 0x0019275A
// Retail instantiates the balanced-tree template once per node type. These addresses are
// byte-identical (after normalizing call targets) to the instance named in each comment.
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_19164f(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second); // Xbox 0x0019164F, as 0x0019158A
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_191714(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second); // Xbox 0x00191714, as 0x0019158A
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_191806(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second); // Xbox 0x00191806, as 0x0019158A
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_1918cb(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second); // Xbox 0x001918CB, as 0x0019158A
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_191b1c(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second); // Xbox 0x00191B1C, as 0x0019158A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19204a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node); // Xbox 0x0019204A, as 0x00191F4A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19214a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node); // Xbox 0x0019214A, as 0x00191F4A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19224a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node); // Xbox 0x0019224A, as 0x00191F4A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19234a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node); // Xbox 0x0019234A, as 0x00191F4A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_192529(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node); // Xbox 0x00192529, as 0x00191F4A
void unregister_shader_validation_value_1926fb(ShaderValidationEraseContext& context, u32 value); // Xbox 0x001926FB, as 0x0019269C
void unregister_shader_validation_value_1927b9(ShaderValidationEraseContext& context, u32 value); // Xbox 0x001927B9, as 0x0019269C
ShaderValidationBalancedNode* insert_shader_validation_node_2037ec(ShaderValidationInsertContext& context, u32 value,
    ShaderValidationBalancedNode* node); // Xbox 0x001922BB, as 0x00191FBB with node vtable 0x002037EC
ShaderValidationBalancedNode* insert_shader_validation_node_2037f4(ShaderValidationInsertContext& context, u32 value,
    ShaderValidationBalancedNode* node); // Xbox 0x0019259A, as 0x00191FBB with node vtable 0x002037F4
ShaderValidationPassNode* insert_shader_validation_pass_node(ShaderValidationPassTree& tree,
    const std::function<void*(std::size_t, u32)>& allocate, u32 key, void* data, ShaderValidationPassNode* node); // Xbox 0x00191A82
}
