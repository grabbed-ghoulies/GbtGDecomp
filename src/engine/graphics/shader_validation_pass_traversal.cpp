#include "shader_validation_pass_traversal.h"

namespace gbtg
{
// Xbox 0x00190148
ShaderValidationPassNode** first_shader_validation_pass_node(const ShaderValidationPassTree& tree,
    ShaderValidationPassNode** result)
{
    ShaderValidationPassNode* node = tree.root;
    ShaderValidationPassNode* previous = tree.root;
    while (node != nullptr)
    {
        previous = node;
        node = node->first;
    }
    *result = previous;
    return result;
}

// Xbox 0x001902BA
ShaderValidationBalancedNode* find_shader_validation_pass_node(const ShaderValidationBalancedTree& tree,
    u32 value)
{
    ShaderValidationBalancedNode* node = tree.root;
    while (true)
    {
        while (true)
        {
            if (node == nullptr)
            {
                return node;
            }
            if (node->value <= value)
            {
                break;
            }
            node = node->first;
        }
        if (value <= node->value)
        {
            break;
        }
        node = node->second;
    }
    return node;
}

// Xbox 0x0019038F
ShaderValidationPassNode** advance_shader_validation_pass_iterator(ShaderValidationPassNode** iterator)
{
    ShaderValidationPassNode* node = *iterator;
    ShaderValidationPassNode* next = node->second;
    if (next != nullptr)
    {
        do
        {
            node = next;
            next = node->first;
        } while (next != nullptr);
        *iterator = node;
        return iterator;
    }
    next = node->parent;
    if (next == nullptr)
    {
        *iterator = nullptr;
        return iterator;
    }
    do
    {
        if (next->second != node)
        {
            break;
        }
        node = next;
        next = node->parent;
    } while (next != nullptr);
    *iterator = node->parent;
    return iterator;
}
}
