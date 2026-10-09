#include "shader_validation_constructors.h"
#if defined(_WIN32)
#include <excpt.h>
#else
#error Shader validator constructor unwind requires the approved Windows SEH backend.
#endif

namespace gbtg
{
// Xbox 0x0018F281
void destroy_shader_validation_child_nodes(ShaderValidationBalancedNode* node,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_child_nodes(node->first, deleting_destructor);
        destroy_shader_validation_child_nodes(node->second, deleting_destructor);
        deleting_destructor(*node, 1);
    }
}

// Xbox 0x001901D6
void clear_shader_validation_child_tree(ShaderValidationBalancedChildTree& tree,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    destroy_shader_validation_child_nodes(tree.nodes.root, deleting_destructor);
    tree.nodes.root = nullptr;
    tree.nodes.count = 0;
}

// Xbox 0x001909A6
void destroy_shader_validation_child_tree(ShaderValidationBalancedChildTree& tree,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    tree.vtable_address = 0x002037b8;
    clear_shader_validation_child_tree(tree, deleting_destructor);
}

namespace
{
// Xbox 0x00170A0C
void unwind_operand_base(ShaderValidationConstructorContext& context, ShaderValidationOperand& operand, u32 address)
{
    destroy_shader_validation_registered_object(context.object_erase, operand.base, address);
}

// Xbox 0x00170A14
void unwind_operand_order(ShaderValidationOperand& operand)
{
    destroy_shader_validation_channel_order(operand.channel_order);
}

// Xbox 0x00170861
void unwind_owner_mask(ShaderValidationChannelMask& mask)
{
    destroy_shader_validation_channel_mask(mask);
}

// Xbox 0x00170869
void unwind_owner_base(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner, u32 address)
{
    destroy_shader_validation_registered_object(context.object_erase, owner.base, address);
}

// Xbox 0x00170871
void unwind_owner_children(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner)
{
    destroy_shader_validation_child_tree(owner.diagnostics, context.deleting_destructor);
}

// Xbox 0x0017087C
void unwind_owner_allocation(ShaderValidationConstructorContext& context, void* allocation)
{
    release_shader_validation_allocation(allocation, context.release);
}

void construct_operand_body(ShaderValidationConstructorContext& context, ShaderValidationOperand& operand,
    u32 object_address, u32 source, ShaderValidationOwner* owner, u32 kind, ShaderValidationOwner* parent,
    u8 marker, i32& state)
{
    initialize_shader_validation_registered_object(context.objects, operand.base, object_address);
    state = 0;
    operand.owner = owner;
    operand.base.vtable_address = 0x0020380c;
    operand.source = source;
    operand.kind = kind;
    operand.parent = parent;
    initialize_shader_validation_channel_order(operand.channel_order);
    state = 1;
    if ((source == 0) && (kind != 3))
    {
        raise_invalid_shader_validation_operand(context.exceptions);
    }
    if ((parent != nullptr) && (parent != context.active_owner))
    {
        register_shader_validation_diagnostic(context.diagnostic_registry(*parent), object_address);
    }
    operand.marker = marker;
}

void construct_owner_body(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner,
    u32 object_address, u32 kind, u32 source, ShaderValidationOwner* parent, u32 channel_value,
    ShaderValidationChannelMask& mask, i32& state, void*& allocation)
{
    state = 0;
    initialize_shader_validation_registered_object(context.objects, owner.base, object_address);
    owner.base.vtable_address = 0x00203814;
    owner.diagnostics.vtable_address = 0x002037b8;
    owner.diagnostics.nodes.root = nullptr;
    owner.diagnostics.nodes.count = 0;
    if (kind == 5)
    {
        ShaderValidationChannelMask temporary;
        temporary.vtable_address = 0x00203790;
        decode_shader_validation_channel_mask(temporary, 8);
        copy_shader_validation_channel_mask(mask, temporary);
    }
    owner.kind = kind;
    owner.source = source;
    if (parent == nullptr)
    {
        parent = context.active_owner;
    }
    allocation = context.allocate(sizeof(ShaderValidationOperand), 0x24870000);
    state = 3;
    ShaderValidationOperand* operand;
    if (allocation == nullptr)
    {
        operand = nullptr;
    }
    else
    {
        operand = construct_shader_validation_operand(context, *static_cast<ShaderValidationOperand*>(allocation),
            context.retail_address(allocation), 0, &owner, 3, parent, 0);
    }
    owner.operand = operand;
    u32 index = 0;
    do
    {
        owner.channels[index] = (0u - static_cast<u32>(mask.channels[index] != 0)) & channel_value;
        ++index;
    } while (index < 4);
    owner.channel_owner = nullptr;
    owner.derived_owner = nullptr;
}
}

// Xbox 0x00192934
ShaderValidationOperand* construct_shader_validation_operand(ShaderValidationConstructorContext& context, ShaderValidationOperand& operand,
    u32 object_address, u32 source, ShaderValidationOwner* owner, u32 kind, ShaderValidationOwner* parent, u8 marker)
{
    i32 state = -1;
    __try
    {
        construct_operand_body(context, operand, object_address, source, owner, kind, parent, marker, state);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            switch (state)
            {
            case 1:
                unwind_operand_order(operand);
                [[fallthrough]];
            case 0:
                unwind_operand_base(context, operand, object_address);
                break;
            }
        }
    }
    return &operand;
}

// Xbox 0x00192A54
ShaderValidationOwner* construct_shader_validation_owner(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner,
    u32 object_address, u32 kind, u32 source, ShaderValidationOwner* parent, u32 channel_value, ShaderValidationChannelMask mask)
{
    i32 state = -1;
    void* allocation = nullptr;
    __try
    {
        construct_owner_body(context, owner, object_address, kind, source, parent, channel_value, mask, state, allocation);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            switch (state)
            {
            case 3:
                unwind_owner_allocation(context, allocation);
                [[fallthrough]];
            case 2:
                unwind_owner_children(context, owner);
                [[fallthrough]];
            case 1:
                unwind_owner_base(context, owner, object_address);
                [[fallthrough]];
            case 0:
                unwind_owner_mask(mask);
                break;
            }
        }
    }
    return &owner;
}
}
