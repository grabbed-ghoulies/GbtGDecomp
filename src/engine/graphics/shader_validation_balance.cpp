#include "shader_validation_balance.h"
#include <bit>

namespace gbtg
{
// Xbox 0x001904C3
i32 calculate_shader_validation_node_height(const ShaderValidationBalancedNode& node)
{
    auto* first = node.first;
    i32 first_height;
    if (first == nullptr)
    {
        first_height = 0;
    }
    else
    {
        first_height = first->height;
    }
    auto* second = node.second;
    i32 second_height;
    if (second == nullptr)
    {
        second_height = 0;
    }
    else
    {
        second_height = second->height;
    }
    if (second_height < first_height)
    {
        if (first != nullptr)
        {
            return std::bit_cast<i32>(static_cast<u32>(first->height) + 1u);
        }
    }
    else if (second != nullptr)
    {
        return std::bit_cast<i32>(static_cast<u32>(second->height) + 1u);
    }
    return 1;
}

// Xbox 0x001904FB
void update_shader_validation_ancestor_heights(ShaderValidationBalancedNode* node)
{
    while (node != nullptr)
    {
        i32 height = calculate_shader_validation_node_height(*node);
        if (node->height == height)
        {
            node = nullptr;
        }
        else
        {
            node->height = height;
            node = node->parent;
        }
    }
}

// Xbox 0x00190575
void rotate_shader_validation_tree_left(ShaderValidationBalancedNode& node)
{
    auto* parent = node.parent;
    auto* second = node.second;
    auto* middle = second->first;
    node.second = middle;
    if (middle != nullptr)
    {
        middle->parent = &node;
    }
    node.parent = second;
    second->first = &node;
    second->parent = parent;
    if (parent != nullptr)
    {
        if (parent->first == &node)
        {
            parent->first = second;
        }
        if (parent->second == &node)
        {
            parent->second = second;
        }
    }
    node.height = calculate_shader_validation_node_height(node);
    second->height = calculate_shader_validation_node_height(*second);
}

// Xbox 0x001905C0
void rotate_shader_validation_tree_right(ShaderValidationBalancedNode& node)
{
    auto* parent = node.parent;
    auto* first = node.first;
    auto* middle = first->second;
    node.first = middle;
    if (middle != nullptr)
    {
        middle->parent = &node;
    }
    node.parent = first;
    first->second = &node;
    first->parent = parent;
    if (parent != nullptr)
    {
        if (parent->first == &node)
        {
            parent->first = first;
        }
        if (parent->second == &node)
        {
            parent->second = first;
        }
    }
    node.height = calculate_shader_validation_node_height(node);
    first->height = calculate_shader_validation_node_height(*first);
}

// Xbox 0x00190B27
void balance_shader_validation_ancestors(ShaderValidationBalancedNode& node)
{
    auto* current = &node;
    while (true)
    {
        auto* first = current->first;
        auto* parent = current->parent;
        i32 first_height;
        if (first == nullptr)
        {
            first_height = 0;
        }
        else
        {
            first_height = first->height;
        }
        auto* second = current->second;
        i32 second_height;
        if (second == nullptr)
        {
            second_height = 0;
        }
        else
        {
            second_height = second->height;
        }
        if (second_height < std::bit_cast<i32>(static_cast<u32>(first_height) - 1u))
        {
            if (first->first == nullptr)
            {
                first_height = 0;
            }
            else
            {
                first_height = first->first->height;
            }
            i32 inner_height;
            if (first->second == nullptr)
            {
                inner_height = 0;
            }
            else
            {
                inner_height = first->second->height;
            }
            if (first_height < inner_height)
            {
                rotate_shader_validation_tree_left(*first);
            }
            rotate_shader_validation_tree_right(*current);
        }
        else
        {
            if (second == nullptr)
            {
                first_height = 0;
            }
            else
            {
                first_height = second->height;
            }
            i32 outer_height;
            if (first == nullptr)
            {
                outer_height = 0;
            }
            else
            {
                outer_height = first->height;
            }
            if (outer_height < std::bit_cast<i32>(static_cast<u32>(first_height) - 1u))
            {
                if (second->second == nullptr)
                {
                    outer_height = 0;
                }
                else
                {
                    outer_height = second->second->height;
                }
                if (second->first == nullptr)
                {
                    first_height = 0;
                }
                else
                {
                    first_height = second->first->height;
                }
                if (outer_height < first_height)
                {
                    rotate_shader_validation_tree_right(*second);
                }
                rotate_shader_validation_tree_left(*current);
            }
        }
        if (parent == nullptr)
        {
            break;
        }
        update_shader_validation_ancestor_heights(parent);
        current = parent;
    }
}

// Xbox 0x001917D9
void restore_shader_validation_tree_root(ShaderValidationBalancedTree& tree, ShaderValidationBalancedNode& node)
{
    update_shader_validation_ancestor_heights(&node);
    balance_shader_validation_ancestors(node);
    while (tree.root->parent != nullptr)
    {
        tree.root = tree.root->parent;
    }
}
}
