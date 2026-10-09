#include "shader_validation_insert.h"
#include "shader_validation_node_init.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace gbtg
{
// Xbox 0x00191FBB
ShaderValidationBalancedNode* insert_shader_validation_object_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node)
{
    if (node == nullptr)
    {
#ifdef _MSC_VER
        __debugbreak();
#else
        __asm__ volatile("int3");
#endif
    }
    ShaderValidationBalancedNode* inserted;
    if (value < node->value)
    {
        auto* first = node->first;
        if (first != nullptr)
        {
            inserted = insert_shader_validation_object_node(context, value, first);
        }
        else
        {
            auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
            if (allocation == nullptr)
            {
                inserted = nullptr;
            }
            else
            {
                inserted = initialize_shader_validation_object_node(*allocation, value, node);
            }
            node->first = inserted;
            ++context.tree.count;
        }
    }
    else
    {
        if (value <= node->value)
        {
            return node;
        }
        auto* second = node->second;
        if (second != nullptr)
        {
            inserted = insert_shader_validation_object_node(context, value, second);
        }
        else
        {
            auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
            if (allocation == nullptr)
            {
                inserted = nullptr;
            }
            else
            {
                inserted = initialize_shader_validation_object_node(*allocation, value, node);
            }
            node->second = inserted;
            ++context.tree.count;
        }
    }
    restore_shader_validation_tree_root(context.tree, *node);
    return inserted;
}

// Xbox 0x001921BB
ShaderValidationBalancedNode* insert_shader_validation_diagnostic_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node)
{
    if (node == nullptr)
    {
#ifdef _MSC_VER
        __debugbreak();
#else
        __asm__ volatile("int3");
#endif
    }
    ShaderValidationBalancedNode* inserted;
    if (value < node->value)
    {
        auto* first = node->first;
        if (first != nullptr)
        {
            inserted = insert_shader_validation_diagnostic_node(context, value, first);
        }
        else
        {
            auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
            if (allocation == nullptr)
            {
                inserted = nullptr;
            }
            else
            {
                inserted = initialize_shader_validation_diagnostic_node(*allocation, value, node);
            }
            node->first = inserted;
            ++context.tree.count;
        }
    }
    else
    {
        if (value <= node->value)
        {
            return node;
        }
        auto* second = node->second;
        if (second != nullptr)
        {
            inserted = insert_shader_validation_diagnostic_node(context, value, second);
        }
        else
        {
            auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
            if (allocation == nullptr)
            {
                inserted = nullptr;
            }
            else
            {
                inserted = initialize_shader_validation_diagnostic_node(*allocation, value, node);
            }
            node->second = inserted;
            ++context.tree.count;
        }
    }
    restore_shader_validation_tree_root(context.tree, *node);
    return inserted;
}
}
