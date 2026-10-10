#include "shader_validation_tree_instances.h"
#include "shader_validation_registers.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0019275A
void unregister_shader_validation_diagnostic(ShaderValidationEraseContext& context, u32 object_address)
{
    auto* node = find_shader_validation_pass_node(context.tree, object_address);
    erase_shader_validation_balanced_node_19214a(context, node);
}

// Xbox 0x0019164F
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_19164f(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second)
{
    return join_shader_validation_balanced_nodes(parent, first, second);
}

// Xbox 0x00191714
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_191714(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second)
{
    return join_shader_validation_balanced_nodes(parent, first, second);
}

// Xbox 0x00191806
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_191806(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second)
{
    return join_shader_validation_balanced_nodes(parent, first, second);
}

// Xbox 0x001918CB
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_1918cb(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second)
{
    return join_shader_validation_balanced_nodes(parent, first, second);
}

// Xbox 0x00191B1C
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes_191b1c(ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second)
{
    return join_shader_validation_balanced_nodes(parent, first, second);
}

// Xbox 0x0019204A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19204a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node)
{
    return erase_shader_validation_balanced_node(context, node);
}

// Xbox 0x0019214A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19214a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node)
{
    return erase_shader_validation_balanced_node(context, node);
}

// Xbox 0x0019224A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19224a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node)
{
    return erase_shader_validation_balanced_node(context, node);
}

// Xbox 0x0019234A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_19234a(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node)
{
    return erase_shader_validation_balanced_node(context, node);
}

// Xbox 0x00192529
ShaderValidationBalancedNode* erase_shader_validation_balanced_node_192529(ShaderValidationEraseContext& context,
    ShaderValidationBalancedNode* node)
{
    return erase_shader_validation_balanced_node(context, node);
}

// Xbox 0x001926FB
void unregister_shader_validation_value_1926fb(ShaderValidationEraseContext& context, u32 value)
{
    unregister_shader_validation_object(context, value);
}

// Xbox 0x001927B9
void unregister_shader_validation_value_1927b9(ShaderValidationEraseContext& context, u32 value)
{
    unregister_shader_validation_object(context, value);
}

namespace
{
using BalancedInitializer = ShaderValidationBalancedNode* (*)(ShaderValidationBalancedNode&, u32, ShaderValidationBalancedNode*);

ShaderValidationBalancedNode* insert_balanced_node(ShaderValidationInsertContext& context, u32 value, ShaderValidationBalancedNode* node,
    BalancedInitializer initialize)
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
    ShaderValidationBalancedNode** link;
    if (value < node->value)
    {
        link = &node->first;
    }
    else
    {
        if (value <= node->value)
        {
            return node;
        }
        link = &node->second;
    }
    if (*link != nullptr)
    {
        inserted = insert_balanced_node(context, value, *link, initialize);
    }
    else
    {
        auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
        inserted = allocation == nullptr ? nullptr : initialize(*allocation, value, node);
        *link = inserted;
        ++context.tree.count;
    }
    restore_shader_validation_tree_root(context.tree, *node);
    return inserted;
}
}

// Xbox 0x001922BB
ShaderValidationBalancedNode* insert_shader_validation_node_2037ec(ShaderValidationInsertContext& context, u32 value,
    ShaderValidationBalancedNode* node)
{
    return insert_balanced_node(context, value, node, initialize_shader_validation_node_2037ec);
}

// Xbox 0x0019259A
ShaderValidationBalancedNode* insert_shader_validation_node_2037f4(ShaderValidationInsertContext& context, u32 value,
    ShaderValidationBalancedNode* node)
{
    return insert_balanced_node(context, value, node, initialize_shader_validation_node_2037f4);
}

// Xbox 0x00191A82
ShaderValidationPassNode* insert_shader_validation_pass_node(ShaderValidationPassTree& tree,
    const std::function<void*(std::size_t, u32)>& allocate, u32 key, void* data, ShaderValidationPassNode* node)
{
    if (node == nullptr)
    {
#ifdef _MSC_VER
        __debugbreak();
#else
        __asm__ volatile("int3");
#endif
    }
    const i32 signed_key = std::bit_cast<i32>(key);
    const i32 node_key = std::bit_cast<i32>(node->value);
    ShaderValidationPassNode** link;
    if (node_key > signed_key)
    {
        link = &node->first;
    }
    else
    {
        if (node_key >= signed_key)
        {
            return node;
        }
        link = &node->second;
    }
    ShaderValidationPassNode* inserted;
    if (*link != nullptr)
    {
        inserted = insert_shader_validation_pass_node(tree, allocate, key, data, *link);
    }
    else
    {
        auto* allocation = static_cast<ShaderValidationPassNode*>(allocate(sizeof(ShaderValidationPassNode), 0x24870000));
        inserted = allocation == nullptr ? nullptr : initialize_shader_validation_pass_node(*allocation, key, data, node);
        *link = inserted;
        ++tree.count;
    }
    rebalance_shader_validation_pass_tree(tree, *node);
    return inserted;
}
}
