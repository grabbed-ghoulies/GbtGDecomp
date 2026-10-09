#include "shader_validation_erase.h"
#include "shader_validation_pass_traversal.h"

namespace gbtg
{
// Xbox 0x00190234
ShaderValidationBalancedNode** advance_shader_validation_balanced_iterator(ShaderValidationBalancedNode** iterator)
{
    auto* current = *iterator;
    auto* next = current->second;
    if (next != nullptr)
    {
        do
        {
            current = next;
            next = current->first;
        } while (next != nullptr);
        *iterator = current;
        return iterator;
    }
    next = current->parent;
    if (next == nullptr)
    {
        *iterator = nullptr;
        return iterator;
    }
    do
    {
        if (next->second != current)
        {
            break;
        }
        current = next;
        next = current->parent;
    } while (next != nullptr);
    *iterator = current->parent;
    return iterator;
}

// Xbox 0x0019158A
ShaderValidationBalancedNode* join_shader_validation_balanced_nodes([[maybe_unused]] ShaderValidationBalancedNode* parent,
    ShaderValidationBalancedNode* first, ShaderValidationBalancedNode* second)
{
    i32 second_height;
    if (second == nullptr)
    {
        second_height = 0;
    }
    else
    {
        second_height = second->height;
    }
    i32 first_height;
    if (first == nullptr)
    {
        first_height = 0;
    }
    else
    {
        first_height = first->height;
    }
    const bool second_taller = second_height > first_height;
    auto* root = second;
    if (!second_taller)
    {
        root = first;
    }
    if (root == nullptr)
    {
        return nullptr;
    }
    root->parent = nullptr;
    if (second_taller)
    {
        if (first != nullptr)
        {
            auto* inner = join_shader_validation_balanced_nodes(first, first->second, second->first);
            first->second = inner;
            if (inner != nullptr)
            {
                inner->parent = first;
            }
            second->first = first;
            first->parent = second;
            update_shader_validation_ancestor_heights(second);
            update_shader_validation_ancestor_heights(first);
            balance_shader_validation_ancestors(*first);
        }
    }
    else
    {
        if (second != nullptr)
        {
            auto* inner = join_shader_validation_balanced_nodes(second, first->second, second->first);
            second->first = inner;
            if (inner != nullptr)
            {
                inner->parent = second;
            }
            first->second = second;
            second->parent = first;
            update_shader_validation_ancestor_heights(first);
            update_shader_validation_ancestor_heights(second);
            balance_shader_validation_ancestors(*second);
        }
    }
    while (root->parent != nullptr)
    {
        root = root->parent;
    }
    root->height = calculate_shader_validation_node_height(*root);
    return root;
}

// Xbox 0x00191F4A
ShaderValidationBalancedNode* erase_shader_validation_balanced_node(ShaderValidationEraseContext& context, ShaderValidationBalancedNode* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    auto* successor = node;
    advance_shader_validation_balanced_iterator(&successor);
    auto* parent = node->parent;
    auto* replacement = join_shader_validation_balanced_nodes(parent, node->first, node->second);
    if (parent == nullptr)
    {
        context.tree.root = replacement;
    }
    else
    {
        if (replacement != nullptr)
        {
            replacement->parent = parent;
        }
        if (parent->first == node)
        {
            parent->first = replacement;
        }
        if (parent->second == node)
        {
            parent->second = replacement;
        }
        restore_shader_validation_tree_root(context.tree, *parent);
    }
    context.deleting_destructor(*node, 1);
    --context.tree.count;
    return successor;
}

// Xbox 0x0019269C
void unregister_shader_validation_object(ShaderValidationEraseContext& context, u32 object_address)
{
    auto* node = find_shader_validation_pass_node(context.tree, object_address);
    erase_shader_validation_balanced_node(context, node);
}

// Xbox 0x001928F6
void destroy_shader_validation_registered_object(ShaderValidationEraseContext& context, ShaderValidationRegisteredObject& object, u32 object_address)
{
    object.vtable_address = 0x00203808;
    unregister_shader_validation_object(context, object_address);
}
}
