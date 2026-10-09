#include "shader_validation_sets.h"
#include "shader_validation_registers.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0018E78B
bool shader_validation_owner_derives_from(const ShaderValidationOwner& owner, const ShaderValidationOwner* other)
{
    if (&owner == other)
    {
        return true;
    }
    if (owner.derived_owner != nullptr && shader_validation_owner_derives_from(*owner.derived_owner, other))
    {
        return true;
    }
    return false;
}

// Xbox 0x0018F56F
bool shader_validation_derived_owner_writes_channel(const ShaderValidationOwner& owner, u32 channel)
{
    const ShaderValidationOwner* derived = owner.derived_owner;
    if (derived == nullptr)
    {
        return false;
    }
    if (derived->channels[channel] == 0 && !shader_validation_derived_owner_writes_channel(*derived, channel))
    {
        return false;
    }
    return true;
}

// Xbox 0x0018F549
bool shader_validation_channel_owner_writes_channel(const ShaderValidationOwner& owner, u32 channel)
{
    const ShaderValidationOwner* channel_owner = owner.channel_owner;
    if (channel_owner == nullptr)
    {
        return false;
    }
    if (channel_owner->channels[channel] == 0 && !shader_validation_channel_owner_writes_channel(*channel_owner, channel))
    {
        return false;
    }
    return true;
}

// Xbox 0x0018F2AA
void destroy_shader_validation_balanced_nodes_18f2aa(ShaderValidationBalancedNode* node,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_balanced_nodes_18f2aa(node->first, deleting_destructor);
        destroy_shader_validation_balanced_nodes_18f2aa(node->second, deleting_destructor);
        deleting_destructor(*node, 1);
    }
}

// Xbox 0x0018F2D3
void destroy_shader_validation_balanced_nodes_18f2d3(ShaderValidationBalancedNode* node,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_balanced_nodes_18f2d3(node->first, deleting_destructor);
        destroy_shader_validation_balanced_nodes_18f2d3(node->second, deleting_destructor);
        deleting_destructor(*node, 1);
    }
}

// Xbox 0x0018F1BC
void destroy_shader_validation_pass_nodes(ShaderValidationPassNode* node,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_pass_nodes(node->first, deleting_destructor);
        destroy_shader_validation_pass_nodes(node->second, deleting_destructor);
        deleting_destructor(*node, 1);
    }
}

// Xbox 0x00190322
void clear_shader_validation_balanced_tree_190322(ShaderValidationBalancedChildTree& tree,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    destroy_shader_validation_balanced_nodes_18f2aa(tree.nodes.root, deleting_destructor);
    tree.nodes.root = nullptr;
    tree.nodes.count = 0;
}

// Xbox 0x00190163
void clear_shader_validation_pass_tree(ShaderValidationPassTreeObject& tree,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    destroy_shader_validation_pass_nodes(tree.nodes.root, deleting_destructor);
    tree.nodes.root = nullptr;
    tree.nodes.count = 0;
}

// Xbox 0x00190AA0
ShaderValidationBalancedChildTree* delete_shader_validation_balanced_tree(ShaderValidationBalancedChildTree& tree, u8 flags,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor,
    const std::function<void(void*, u32)>& release)
{
    tree.vtable_address = 0x002037d0;
    clear_shader_validation_balanced_tree_190322(tree, deleting_destructor);
    if ((flags & 1u) != 0)
    {
        release(&tree, 0x24870000);
    }
    return &tree;
}

// Xbox 0x00190AC6
ShaderValidationPassTreeObject* delete_shader_validation_pass_tree(ShaderValidationPassTreeObject& tree, u8 flags,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor,
    const std::function<void(void*, u32)>& release)
{
    tree.vtable_address = 0x002037d8;
    clear_shader_validation_pass_tree(tree, deleting_destructor);
    if ((flags & 1u) != 0)
    {
        release(&tree, 0x24870000);
    }
    return &tree;
}

namespace
{
using BalancedInitializer = ShaderValidationBalancedNode* (*)(ShaderValidationBalancedNode&, u32, ShaderValidationBalancedNode*);

ShaderValidationBalancedNode* allocate_root(ShaderValidationInsertContext& context, u32 value, BalancedInitializer initialize)
{
    context.tree.count = 1;
    auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
    return allocation == nullptr ? nullptr : initialize(*allocation, value, nullptr);
}
}

// Xbox 0x00192772
void register_shader_validation_value_2037ec(ShaderValidationInsertContext& context, u32 value)
{
    if (context.tree.root == nullptr)
    {
        context.tree.root = allocate_root(context, value, initialize_shader_validation_node_2037ec);
    }
    else
    {
        insert_shader_validation_node_2037ec(context, value, context.tree.root);
    }
}

// Xbox 0x0019287F
void register_shader_validation_value_2037f4(ShaderValidationInsertContext& context, u32 value)
{
    if (context.tree.root == nullptr)
    {
        context.tree.root = allocate_root(context, value, initialize_shader_validation_node_2037f4);
    }
    else
    {
        insert_shader_validation_node_2037f4(context, value, context.tree.root);
    }
}

// Xbox 0x001924DA
ShaderValidationPassNode* register_shader_validation_pass_value(ShaderValidationPassTree& tree,
    const std::function<void*(std::size_t, u32)>& allocate, u32 key, void* data)
{
    // Declared void in retail; callers such as 0x00192830 use the node left in EAX.
    if (tree.root == nullptr)
    {
        tree.count = 1;
        auto* allocation = static_cast<ShaderValidationPassNode*>(allocate(sizeof(ShaderValidationPassNode), 0x24870000));
        tree.root = allocation == nullptr ? nullptr : initialize_shader_validation_pass_node(*allocation, key, data, nullptr);
        return tree.root;
    }
    return insert_shader_validation_pass_node(tree, allocate, key, data, tree.root);
}

// Xbox 0x0018E94C
bool shader_validation_instruction_precedes(u32 first, u32 second, const ShaderValidationInstructionAt& instruction_at)
{
    // Instructions order by their signed sequence, then by retail object address.
    const i32 first_sequence = std::bit_cast<i32>(instruction_at(first)->sequence);
    const i32 second_sequence = std::bit_cast<i32>(instruction_at(second)->sequence);
    if (first_sequence < second_sequence)
    {
        return true;
    }
    if (first_sequence > second_sequence)
    {
        return false;
    }
    return first < second;
}

// Xbox 0x0018F174
ShaderValidationBalancedNode* find_shader_validation_instruction_node(const ShaderValidationBalancedTree& tree, u32 instruction,
    const ShaderValidationInstructionAt& instruction_at)
{
    ShaderValidationBalancedNode* node = tree.root;
    while (node != nullptr)
    {
        const u32 value = node->value;
        if (shader_validation_instruction_precedes(instruction, value, instruction_at))
        {
            node = node->first;
        }
        else if (shader_validation_instruction_precedes(value, instruction, instruction_at))
        {
            node = node->second;
        }
        else
        {
            return node;
        }
    }
    return nullptr;
}

// Xbox 0x001923BB
ShaderValidationBalancedNode* insert_shader_validation_instruction_node(ShaderValidationInsertContext& context, u32 instruction,
    ShaderValidationBalancedNode* node, const ShaderValidationInstructionAt& instruction_at)
{
    if (node == nullptr)
    {
#ifdef _MSC_VER
        __debugbreak();
#else
        __asm__ volatile("int3");
#endif
    }
    const u32 value = node->value;
    ShaderValidationBalancedNode** link;
    if (shader_validation_instruction_precedes(instruction, value, instruction_at))
    {
        link = &node->first;
    }
    else
    {
        if (!shader_validation_instruction_precedes(value, instruction, instruction_at))
        {
            return node;
        }
        link = &node->second;
    }
    ShaderValidationBalancedNode* inserted;
    if (*link != nullptr)
    {
        inserted = insert_shader_validation_instruction_node(context, instruction, *link, instruction_at);
    }
    else
    {
        auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
        inserted = allocation == nullptr ? nullptr : initialize_shader_validation_node_2037f0(*allocation, instruction, node);
        *link = inserted;
        ++context.tree.count;
    }
    restore_shader_validation_tree_root(context.tree, *node);
    return inserted;
}

// Xbox 0x001927D1
void register_shader_validation_instruction(ShaderValidationInsertContext& context, u32 instruction,
    const ShaderValidationInstructionAt& instruction_at)
{
    if (context.tree.root == nullptr)
    {
        context.tree.root = allocate_root(context, instruction, initialize_shader_validation_node_2037f0);
    }
    else
    {
        insert_shader_validation_instruction_node(context, instruction, context.tree.root, instruction_at);
    }
}

// Xbox 0x0018F0EB
void advance_shader_validation_arena_iterator(ShaderValidationBalancedNode*& node)
{
    // Typed overload for balanced-node registries, which share the arena node link offsets.
    ShaderValidationBalancedNode* next = node->second;
    ShaderValidationBalancedNode* current = node;
    if (next != nullptr)
    {
        do
        {
            current = next;
            next = current->first;
        } while (next != nullptr);
        node = current;
        return;
    }
    ShaderValidationBalancedNode* previous;
    do
    {
        previous = current;
        current = previous->parent;
        if (current == nullptr)
        {
            break;
        }
    } while (current->second == previous);
    node = previous->parent;
}

// Xbox 0x00192830
ShaderValidationPassNode** set_shader_validation_pass_value(ShaderValidationPassTree& tree,
    const std::function<void*(std::size_t, u32)>& allocate, ShaderValidationPassNode** result, u32 key, void* data)
{
    ShaderValidationPassNode* node = find_shader_validation_pass_node(tree, key);
    if (node == nullptr)
    {
        node = register_shader_validation_pass_value(tree, allocate, key, data);
    }
    else
    {
        node->data = data;
    }
    *result = node;
    return result;
}

// Xbox 0x001929E7
void copy_shader_validation_instruction_set(ShaderValidationInstruction& instruction, const ShaderValidationBalancedChildTree& source,
    const std::function<void*(std::size_t, u32)>& allocate, const ShaderValidationInstructionAt& instruction_at)
{
    if (source.nodes.count == 0)
    {
        return;
    }
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(source.nodes, &node);
    while (node != nullptr)
    {
        ShaderValidationInsertContext context{instruction.instruction_set->nodes, allocate};
        register_shader_validation_instruction(context, node->value, instruction_at);
        advance_shader_validation_balanced_iterator(&node);
    }
    instruction.instruction_set_count = instruction.instruction_set->nodes.count;
}

// Xbox 0x00192A2B
void release_shader_validation_instruction_set(ShaderValidationInstruction& instruction, ShaderValidationEraseContext& instructions,
    const ShaderValidationAddressOf& address_of,
    const std::function<void(ShaderValidationBalancedChildTree&, u32)>& deleting_destructor)
{
    // instructions is the DAT_001a4d58 registry; deleting_destructor is vtable slot +0x04 of the set.
    if (instruction.instruction_set != nullptr)
    {
        unregister_shader_validation_value_1927b9(instructions, address_of(instruction.instruction_set));
        if (instruction.instruction_set != nullptr)
        {
            deleting_destructor(*instruction.instruction_set, 1);
        }
    }
    instruction.instruction_set = nullptr;
}

// Xbox 0x00190774
bool shader_validation_owner_writers_in_set(const ShaderValidationOwner& owner, u32 unused, ShaderValidationChannelMask mask,
    const ShaderValidationBalancedTree& set, const ShaderValidationOwner* active_owner, const ShaderValidationAddressOf& address_of,
    const ShaderValidationInstructionAt& instruction_at)
{
    // Retail pops an unused leading argument (RET 0x10). Writers are compared by retail address.
    (void)unused;
    for (std::size_t channel = 0; channel < 4; ++channel)
    {
        if (mask.channels[channel] == 0)
        {
            continue;
        }
        const ShaderValidationOwner* current = &owner;
        while (current->channels[channel] == 0)
        {
            current = current->derived_owner;
            if (current == nullptr)
            {
                break;
            }
        }
        if (current == nullptr)
        {
            continue;
        }
        const ShaderValidationChannelWriter writer = current->channels[channel];
        if (writer != shader_validation_unwritten_channel &&
            find_shader_validation_instruction_node(set, address_of(reinterpret_cast<const void*>(writer)), instruction_at) == nullptr)
        {
            return false;
        }
    }
    if (find_shader_validation_foreign_parent(owner, active_owner) != nullptr)
    {
        const ShaderValidationOwner* parent = find_shader_validation_foreign_parent(owner, active_owner);
        const ShaderValidationChannelWriter writer = parent->channels[0];
        if (find_shader_validation_instruction_node(set, address_of(reinterpret_cast<const void*>(writer)), instruction_at) == nullptr)
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00190FFE
bool swap_shader_validation_owner_channels(ShaderValidationOwner& owner, u32 first, u32 second, u8 climb, u8 propagate,
    const ShaderValidationOperandAt& operand_at)
{
    // Channel indices are 0..3 at every retail call site.
    ShaderValidationOwner* current = &owner;
    if (climb != 0)
    {
        while (current->derived_owner != nullptr)
        {
            propagate = 1;
            current = current->derived_owner;
        }
    }
    const ShaderValidationChannelWriter first_writer = current->channels[first];
    const ShaderValidationChannelWriter second_writer = current->channels[second];
    if (first_writer == shader_validation_unwritten_channel || second_writer == shader_validation_unwritten_channel)
    {
        return false;
    }
    if (propagate != 0 && current->channel_owner != nullptr &&
        !swap_shader_validation_owner_channels(*current->channel_owner, first, second, 0, 1, operand_at))
    {
        return false;
    }
    const auto swap_writer = [&](ShaderValidationChannelWriter writer)
    {
        return swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(writer), current, first, second, 0);
    };
    bool swapped = false;
    if (first_writer == second_writer)
    {
        swapped = second_writer != 0 ? swap_writer(second_writer) : true;
    }
    else if (second_writer == 0)
    {
        swapped = swap_writer(first_writer);
        if (swapped)
        {
            current->channels[second] = first_writer;
            current->channels[first] = 0;
        }
    }
    else if (first_writer == 0)
    {
        swapped = swap_writer(second_writer);
        if (swapped)
        {
            current->channels[first] = second_writer;
            current->channels[second] = 0;
        }
    }
    else if (swap_writer(second_writer))
    {
        swapped = swap_writer(first_writer);
        if (swapped)
        {
            current->channels[first] = second_writer;
            current->channels[second] = first_writer;
        }
        else
        {
            // Undo the second writer's swap.
            swap_writer(second_writer);
        }
    }
    if (swapped)
    {
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(current->diagnostics.nodes, &node);
        while (node != nullptr)
        {
            ShaderValidationOperand& operand = *operand_at(node->value);
            for (u32 index = 0; index <= 3; ++index)
            {
                const u32 channel = index == 4 ? 4u : operand.channel_order.channels[index];
                if (channel == first)
                {
                    operand.channel_order.channels[index] = second;
                }
                else if ((index == 4 ? 4u : operand.channel_order.channels[index]) == second)
                {
                    operand.channel_order.channels[index] = first;
                }
            }
            advance_shader_validation_arena_iterator(node);
        }
        return swapped;
    }
    if (propagate != 0 && current->channel_owner != nullptr)
    {
        swap_shader_validation_owner_channels(*current->channel_owner, first, second, 0, 1, operand_at);
    }
    return swapped;
}
}
