#include "shader_validation_containers.h"

namespace gbtg
{
namespace
{
ShaderValidationBalancedNode* initialize_balanced_node(ShaderValidationBalancedNode& node, u32 vtable_address, u32 value,
    ShaderValidationBalancedNode* parent)
{
    node.first = nullptr;
    node.second = nullptr;
    node.value = value;
    node.vtable_address = vtable_address;
    node.parent = parent;
    node.height = 1;
    return &node;
}
}

// Xbox 0x001902D9
ShaderValidationBalancedNode* initialize_shader_validation_node_2037ec(ShaderValidationBalancedNode& node, u32 value,
    ShaderValidationBalancedNode* parent)
{
    return initialize_balanced_node(node, 0x002037ec, value, parent);
}

// Xbox 0x001903E1
ShaderValidationBalancedNode* initialize_shader_validation_node_2037f0(ShaderValidationBalancedNode& node, u32 value,
    ShaderValidationBalancedNode* parent)
{
    return initialize_balanced_node(node, 0x002037f0, value, parent);
}

// Xbox 0x0019042A
ShaderValidationBalancedNode* initialize_shader_validation_node_2037f4(ShaderValidationBalancedNode& node, u32 value,
    ShaderValidationBalancedNode* parent)
{
    return initialize_balanced_node(node, 0x002037f4, value, parent);
}

// Xbox 0x00190473
ShaderValidationPassNode* initialize_shader_validation_pass_node(ShaderValidationPassNode& node, u32 value, void* data,
    ShaderValidationPassNode* parent)
{
    node.first = nullptr;
    node.second = nullptr;
    node.value = value;
    node.data = data;
    node.vtable_address = 0x002037f8;
    node.parent = parent;
    node.height = 1;
    return &node;
}

// Xbox 0x0019051C
i32 calculate_shader_validation_pass_node_height(const ShaderValidationPassNode& node)
{
    const ShaderValidationPassNode* first = node.first;
    const i32 first_height = first != nullptr ? first->height : 0;
    const ShaderValidationPassNode* second = node.second;
    const i32 second_height = second != nullptr ? second->height : 0;
    if (first_height > second_height)
    {
        if (first != nullptr)
        {
            return first->height + 1;
        }
    }
    else if (second != nullptr)
    {
        return second->height + 1;
    }
    return 1;
}

// Xbox 0x00190337
ShaderValidationBalancedNode** retreat_shader_validation_node_iterator(ShaderValidationBalancedNode** iterator)
{
    ShaderValidationBalancedNode* node = *iterator;
    ShaderValidationBalancedNode* child = node->first;
    if (child != nullptr)
    {
        node = child;
        while (node->second != nullptr)
        {
            node = node->second;
        }
        *iterator = node;
        return iterator;
    }
    ShaderValidationBalancedNode* parent = node->parent;
    if (parent == nullptr)
    {
        *iterator = nullptr;
        return iterator;
    }
    // Retail dereferences each parent without a null test while climbing first-child links.
    while (parent->first == node)
    {
        node = parent;
        parent = node->parent;
    }
    *iterator = node->parent;
    return iterator;
}

// Xbox 0x00190B12
ShaderValidationPassNode* find_shader_validation_pass_node(const ShaderValidationPassTree& tree, u32 key)
{
    // Retail forwards an uninitialized frame word as the callee's unused second argument.
    return find_shader_validation_pass_node_signed(tree, key);
}

// Xbox 0x00190CA6
void* last_shader_validation_tree_value(const ShaderValidationTreeStorage& storage)
{
    if (storage.count == 0)
    {
        return nullptr;
    }
    ShaderValidationTreeNode* last;
    return (*find_shader_validation_tree_last(storage, &last))->value;
}

// Xbox 0x001909B1
void destroy_shader_validation_list(ShaderValidationList& list)
{
    list.vtable_address = 0x002037c8;
    while (list.storage.count != 0)
    {
        erase_shader_validation_list_node(list.storage, list.storage.last);
    }
}

// Xbox 0x0018F314
ShaderValidationChannelMask* construct_shader_validation_single_channel_mask(ShaderValidationChannelMask& mask, u32 channel)
{
    mask.vtable_address = 0x00203790;
    // SAR uses only the low five bits of CL.
    decode_shader_validation_channel_mask(mask, static_cast<u32>(8 >> (channel & 0x1Fu)));
    return &mask;
}

// Xbox 0x0018F1E5
ShaderValidationListNode* construct_shader_validation_list_node(ShaderValidationListNode& node,
    const ShaderValidationListVtable* vtable, void* value, ShaderValidationListNode* previous,
    ShaderValidationListNode* next)
{
    // Retail installs vtable 0x002037A4; the host supplies the matching deleting destructor table.
    node.value = value;
    node.vtable = vtable;
    node.previous = previous;
    node.next = next;
    if (previous != nullptr)
    {
        previous->next = &node;
    }
    if (next != nullptr)
    {
        next->previous = &node;
    }
    return &node;
}

// Xbox 0x0019010E
void append_shader_validation_list_value(ShaderValidationList& list, const ShaderValidationListVtable* node_vtable,
    const std::function<void*(std::size_t, u32)>& allocate, void* value)
{
    auto* allocation = static_cast<ShaderValidationListNode*>(allocate(sizeof(ShaderValidationListNode), 0x24870000));
    ShaderValidationListNode* node = nullptr;
    if (allocation != nullptr)
    {
        node = construct_shader_validation_list_node(*allocation, node_vtable, value, list.storage.last, nullptr);
    }
    // A failed allocation still replaces the last link and increments the count.
    list.storage.last = node;
    if (list.storage.first == nullptr)
    {
        list.storage.first = node;
    }
    ++list.storage.count;
}

// Xbox 0x001929B7
ShaderValidationInstruction* construct_shader_validation_instruction_base(ShaderValidationInsertContext& context,
    ShaderValidationInstruction& instruction, u32 object_address)
{
    initialize_shader_validation_registered_object(context, instruction.base, object_address);
    instruction.base.vtable_address = 0x00203810;
    construct_shader_validation_channel_mask(instruction.output_mask, 0xF);
    construct_shader_validation_channel_mask(instruction.vector_mask, 0xF);
    construct_shader_validation_channel_mask(instruction.scalar_mask, 0xF);
    return &instruction;
}
}
