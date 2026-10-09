#include "shader_validation_registers.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00190554
void update_shader_validation_pass_ancestor_heights(ShaderValidationPassNode* node)
{
    while (node != nullptr)
    {
        const i32 height = calculate_shader_validation_pass_node_height(*node);
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

namespace
{
void replace_shader_validation_pass_child(ShaderValidationPassNode* parent, ShaderValidationPassNode& node,
    ShaderValidationPassNode& replacement)
{
    if (parent != nullptr)
    {
        // Both children are compared independently, as in retail.
        if (parent->first == &node)
        {
            parent->first = &replacement;
        }
        if (parent->second == &node)
        {
            parent->second = &replacement;
        }
    }
}
}

// Xbox 0x0019060B
void rotate_shader_validation_pass_tree_left(ShaderValidationPassNode& node)
{
    ShaderValidationPassNode* parent = node.parent;
    ShaderValidationPassNode& pivot = *node.second;
    ShaderValidationPassNode* moved = pivot.first;
    node.second = moved;
    if (moved != nullptr)
    {
        moved->parent = &node;
    }
    node.parent = &pivot;
    pivot.first = &node;
    pivot.parent = parent;
    replace_shader_validation_pass_child(parent, node, pivot);
    node.height = calculate_shader_validation_pass_node_height(node);
    pivot.height = calculate_shader_validation_pass_node_height(pivot);
}

// Xbox 0x00190656
void rotate_shader_validation_pass_tree_right(ShaderValidationPassNode& node)
{
    ShaderValidationPassNode* parent = node.parent;
    ShaderValidationPassNode& pivot = *node.first;
    ShaderValidationPassNode* moved = pivot.second;
    node.first = moved;
    if (moved != nullptr)
    {
        moved->parent = &node;
    }
    node.parent = &pivot;
    pivot.second = &node;
    pivot.parent = parent;
    replace_shader_validation_pass_child(parent, node, pivot);
    node.height = calculate_shader_validation_pass_node_height(node);
    pivot.height = calculate_shader_validation_pass_node_height(pivot);
}

// Xbox 0x00191E8F
ShaderValidationOwner* find_shader_validation_register_owner(const ShaderValidationPass& pass, u32 kind)
{
    // Retail pops two further unused arguments (RET 0xC).
    const ShaderValidationPassNode* node;
    if (kind == 0xC)
    {
        node = find_shader_validation_pass_node_signed(pass.special.tree.nodes, 0x100);
    }
    else
    {
        node = find_shader_validation_pass_node(pass.registers.tree.nodes, kind);
    }
    if (node == nullptr)
    {
        return nullptr;
    }
    return static_cast<ShaderValidationOwner*>(node->data);
}

// Xbox 0x00191F00
i32 find_shader_validation_owner_register_kind(const ShaderValidationPass& pass,
    const ShaderValidationOwner* owner)
{
    i32 kind = 0;
    do
    {
        const ShaderValidationPassNode* node = find_shader_validation_pass_node(pass.registers.tree.nodes, static_cast<u32>(kind));
        if (node != nullptr)
        {
            const auto* newest = static_cast<const ShaderValidationOwner*>(node->data);
            for (const ShaderValidationOwner* candidate = newest; candidate != nullptr; candidate = candidate->derived_owner)
            {
                if (candidate == owner)
                {
                    return kind;
                }
            }
            for (const ShaderValidationOwner* candidate = newest; candidate != nullptr; candidate = candidate->channel_owner)
            {
                if (candidate == owner)
                {
                    return kind;
                }
            }
        }
        ++kind;
    } while (kind <= 0xC);
    return -1;
}

// Xbox 0x0018F4A9
bool shader_validation_owners_equivalent(const ShaderValidationOwner* owner, const ShaderValidationOwner* other,
    const ShaderValidationOwner* active_owner)
{
    if (owner == other)
    {
        return true;
    }
    if (owner == nullptr || other == nullptr)
    {
        return false;
    }
    if (owner->kind == 3 && other->kind == 3 && owner->source == other->source)
    {
        // Retail resolves the second owner's parent first.
        const ShaderValidationOwner* other_parent = find_shader_validation_foreign_parent(*other, active_owner);
        if (find_shader_validation_foreign_parent(*owner, active_owner) == other_parent)
        {
            return true;
        }
    }
    return false;
}

namespace
{
i32 pass_height(const ShaderValidationPassNode* node)
{
    return node != nullptr ? node->height : 0;
}

i32 decrement(i32 value)
{
    return std::bit_cast<i32>(static_cast<u32>(value) - 1u);
}
}

// Xbox 0x00190BDD
void balance_shader_validation_pass_ancestors(ShaderValidationPassNode& node)
{
    ShaderValidationPassNode* current = &node;
    while (true)
    {
        ShaderValidationPassNode* first = current->first;
        ShaderValidationPassNode* parent = current->parent;
        const i32 first_height = pass_height(first);
        ShaderValidationPassNode* second = current->second;
        const i32 second_height = pass_height(second);
        if (decrement(first_height) > second_height)
        {
            // Retail dereferences the first child here without a null test.
            if (pass_height(first->first) < pass_height(first->second))
            {
                rotate_shader_validation_pass_tree_left(*first);
            }
            rotate_shader_validation_pass_tree_right(*current);
        }
        else if (decrement(second_height) > first_height)
        {
            if (pass_height(second->second) < pass_height(second->first))
            {
                rotate_shader_validation_pass_tree_right(*second);
            }
            rotate_shader_validation_pass_tree_left(*current);
        }
        if (parent == nullptr)
        {
            return;
        }
        update_shader_validation_pass_ancestor_heights(parent);
        current = parent;
    }
}

// Xbox 0x00192629
bool shader_validation_owner_chain_has_kind_one(const ShaderValidationPass& pass,
    const ShaderValidationOwner* owner)
{
    // Retail reads the derived link of an unregistered owner without a null test.
    i32 kind;
    do
    {
        kind = find_shader_validation_owner_register_kind(pass, owner);
        if (kind >= 0)
        {
            break;
        }
        owner = owner->derived_owner;
    } while (owner != nullptr);
    return kind == 1;
}

// Xbox 0x00191990
void rebalance_shader_validation_pass_tree(ShaderValidationPassTree& tree, ShaderValidationPassNode& node)
{
    update_shader_validation_pass_ancestor_heights(&node);
    balance_shader_validation_pass_ancestors(node);
    while (tree.root->parent != nullptr)
    {
        tree.root = tree.root->parent;
    }
}

// Xbox 0x001919BD
ShaderValidationPassNode* join_shader_validation_pass_subtrees(ShaderValidationPassNode* first,
    ShaderValidationPassNode* second)
{
    // Retail takes a third, leading stack argument that it only forwards to its recursive calls.
    const bool second_taller = pass_height(second) > pass_height(first);
    ShaderValidationPassNode* root = second_taller ? second : first;
    if (root == nullptr)
    {
        return nullptr;
    }
    root->parent = nullptr;
    if (second_taller)
    {
        if (first != nullptr)
        {
            ShaderValidationPassNode* joined = join_shader_validation_pass_subtrees(first->second, second->first);
            first->second = joined;
            if (joined != nullptr)
            {
                joined->parent = first;
            }
            second->first = first;
            first->parent = second;
            update_shader_validation_pass_ancestor_heights(second);
            update_shader_validation_pass_ancestor_heights(first);
            balance_shader_validation_pass_ancestors(*first);
        }
    }
    else if (second != nullptr)
    {
        ShaderValidationPassNode* joined = join_shader_validation_pass_subtrees(first->second, second->first);
        second->first = joined;
        if (joined != nullptr)
        {
            joined->parent = second;
        }
        first->second = second;
        second->parent = first;
        update_shader_validation_pass_ancestor_heights(first);
        update_shader_validation_pass_ancestor_heights(second);
        balance_shader_validation_pass_ancestors(*second);
    }
    while (root->parent != nullptr)
    {
        root = root->parent;
    }
    root->height = calculate_shader_validation_pass_node_height(*root);
    return root;
}

// Xbox 0x00192469
ShaderValidationPassNode* erase_shader_validation_pass_node(ShaderValidationPassTree& tree, ShaderValidationPassNode* node,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    ShaderValidationPassNode* next = node;
    next = *advance_shader_validation_pass_iterator(&next);
    ShaderValidationPassNode* parent = node->parent;
    ShaderValidationPassNode* joined = join_shader_validation_pass_subtrees(node->first, node->second);
    if (parent == nullptr)
    {
        tree.root = joined;
    }
    else
    {
        if (joined != nullptr)
        {
            joined->parent = parent;
        }
        if (parent->first == node)
        {
            parent->first = joined;
        }
        if (parent->second == node)
        {
            parent->second = joined;
        }
        rebalance_shader_validation_pass_tree(tree, *parent);
    }
    deleting_destructor(*node, 1);
    --tree.count;
    return next;
}

// Xbox 0x001936B5
ShaderValidationPassNode* erase_shader_validation_pass_key(ShaderValidationPassTree& tree, u32 key,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    // Retail discards the successor returned in EAX; it is forwarded here unchanged.
    return erase_shader_validation_pass_node(tree, find_shader_validation_pass_node(tree, key), deleting_destructor);
}
}
