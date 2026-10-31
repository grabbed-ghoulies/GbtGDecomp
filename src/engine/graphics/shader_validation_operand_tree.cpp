#include "shader_validation_operand_tree.h"
#ifdef _MSC_VER
#include "gbtg/host_intrin.h"
#endif

namespace gbtg
{
// Xbox 0x001901EB
ShaderValidationBalancedNode* initialize_shader_validation_operand_node(ShaderValidationBalancedNode& node, u32 value, ShaderValidationBalancedNode* parent)
{
    node.first = nullptr;
    node.second = nullptr;
    node.value = value;
    node.vtable_address = 0x002037e4;
    node.parent = parent;
    node.height = 1;
    return &node;
}

// Xbox 0x001920BB
ShaderValidationBalancedNode* insert_shader_validation_operand_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node)
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
            inserted = insert_shader_validation_operand_node(context, value, first);
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
                inserted = initialize_shader_validation_operand_node(*allocation, value, node);
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
            inserted = insert_shader_validation_operand_node(context, value, second);
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
                inserted = initialize_shader_validation_operand_node(*allocation, value, node);
            }
            node->second = inserted;
            ++context.tree.count;
        }
    }
    restore_shader_validation_tree_root(context.tree, *node);
    return inserted;
}

// Xbox 0x001926B4
void register_shader_validation_operand(ShaderValidationInsertContext& context, u32 object_address)
{
    if (context.tree.root == nullptr)
    {
        context.tree.count = 1;
        auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
        ShaderValidationBalancedNode* node;
        if (allocation == nullptr)
        {
            node = nullptr;
        }
        else
        {
            node = initialize_shader_validation_operand_node(*allocation, object_address, nullptr);
        }
        context.tree.root = node;
    }
    else
    {
        insert_shader_validation_operand_node(context, object_address, context.tree.root);
    }
}

}
