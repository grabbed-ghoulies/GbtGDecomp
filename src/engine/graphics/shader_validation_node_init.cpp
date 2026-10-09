#include "shader_validation_node_init.h"

namespace gbtg
{
// Xbox 0x0019018D
ShaderValidationNodeWords* initialize_shader_validation_object_node(ShaderValidationNodeWords& node, u32 value, u32 parent)
{
    node[3] = 0;
    node[4] = 0;
    node[1] = value;
    node[0] = 0x002037e0;
    node[2] = parent;
    node[5] = 1;
    return &node;
}

// Xbox 0x00190271
ShaderValidationNodeWords* initialize_shader_validation_diagnostic_node(ShaderValidationNodeWords& node, u32 value, u32 parent)
{
    node[3] = 0;
    node[4] = 0;
    node[1] = value;
    node[0] = 0x002037e8;
    node[2] = parent;
    node[5] = 1;
    return &node;
}

// Xbox 0x0019018D
ShaderValidationBalancedNode* initialize_shader_validation_object_node(ShaderValidationBalancedNode& node, u32 value, ShaderValidationBalancedNode* parent)
{
    node.first = nullptr;
    node.second = nullptr;
    node.value = value;
    node.vtable_address = 0x002037e0;
    node.parent = parent;
    node.height = 1;
    return &node;
}

// Xbox 0x00190271
ShaderValidationBalancedNode* initialize_shader_validation_diagnostic_node(ShaderValidationBalancedNode& node, u32 value, ShaderValidationBalancedNode* parent)
{
    node.first = nullptr;
    node.second = nullptr;
    node.value = value;
    node.vtable_address = 0x002037e8;
    node.parent = parent;
    node.height = 1;
    return &node;
}
}
