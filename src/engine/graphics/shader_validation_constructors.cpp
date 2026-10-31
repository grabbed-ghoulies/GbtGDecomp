#include "shader_validation_constructors.h"
#include "gbtg/host_seh.h"
#include "shader_validation_serializer.h"
#include "shader_validation_tree_instances.h"
#include "shader_validation_instruction_ops.h"

namespace gbtg
{
namespace
{
void move_unwritten_readers_body(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner,
    const std::function<ShaderValidationOperand*(u32)>& operand_at, ShaderValidationChannelMask& accumulated, i32& state)
{
    bool allowed = owner.derived_owner != nullptr;
    accumulated.vtable_address = 0x00203790;
    decode_shader_validation_channel_mask(accumulated, 0);
    state = 0;
    ShaderValidationOwner* current = &owner;
    do
    {
        if (!allowed)
        {
            return;
        }
        ShaderValidationChannelMask written;
        shader_validation_owner_channel_mask(*current, written);
        ShaderValidationChannelMask combined;
        unite_shader_validation_channel_masks(accumulated, written, combined);
        copy_shader_validation_channel_mask(accumulated, combined);
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(current->diagnostics.nodes, &node);
        while (node != nullptr)
        {
            if (shader_validation_operand_reads_masked_channel(*operand_at(node->value),
                    ShaderValidationChannelMask{0x00203790, accumulated.channels}))
            {
                ShaderValidationChannelMask unwritten;
                invert_shader_validation_channel_mask(accumulated, unwritten);
                if (shader_validation_operand_reads_masked_channel(*operand_at(node->value), unwritten))
                {
                    allowed = false;
                }
            }
            advance_shader_validation_balanced_iterator(&node);
        }
        current = current->channel_owner;
    } while (current != nullptr);
    if (allowed)
    {
        current = &owner;
        ShaderValidationChannelMask empty;
        empty.vtable_address = 0x00203790;
        decode_shader_validation_channel_mask(empty, 0);
        copy_shader_validation_channel_mask(accumulated, empty);
        do
        {
            ShaderValidationChannelMask written;
            shader_validation_owner_channel_mask(*current, written);
            ShaderValidationChannelMask combined;
            unite_shader_validation_channel_masks(accumulated, written, combined);
            copy_shader_validation_channel_mask(accumulated, combined);
            ShaderValidationBalancedNode* node;
            find_shader_validation_tree_last(current->diagnostics.nodes, &node);
            while (node != nullptr)
            {
                if (!shader_validation_operand_reads_masked_channel(*operand_at(node->value),
                        ShaderValidationChannelMask{0x00203790, accumulated.channels}))
                {
                    reparent_shader_validation_operand(context, *operand_at(node->value), owner.derived_owner);
                    // Reparenting mutates the current registry; retail restarts its iterator.
                    find_shader_validation_tree_last(current->diagnostics.nodes, &node);
                }
                else
                {
                    advance_shader_validation_balanced_iterator(&node);
                }
            }
            current = current->channel_owner;
        } while (current != nullptr);
    }
}
}

// Xbox 0x00193A40
void move_shader_validation_unwritten_readers_to_derived(ShaderValidationConstructorContext& context,
    ShaderValidationOwner& owner, const std::function<ShaderValidationOperand*(u32)>& operand_at)
{
    ShaderValidationChannelMask accumulated;
    i32 state = -1;
    GBTG_SEH_TRY_FINALLY
    {
        move_unwritten_readers_body(context, owner, operand_at, accumulated, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 0)
        {
            // FuncInfo 0x0032BACC: 0x001709FA destroys EBP-0x30.
            destroy_shader_validation_channel_mask(accumulated);
        }
    }
    GBTG_SEH_FINALLY_END
}

// Xbox 0x00192C9E
ShaderValidationOwner* reparent_shader_validation_operand(ShaderValidationConstructorContext& context,
    ShaderValidationOperand& operand, ShaderValidationOwner* parent)
{
    if (operand.parent != nullptr && operand.parent != context.active_owner)
    {
        ShaderValidationEraseContext erase{operand.parent->diagnostics.nodes, context.deleting_destructor};
        unregister_shader_validation_diagnostic(erase, context.retail_address(&operand));
        // Retail reloads the parent after the virtual registry erase.
        ShaderValidationOwner* previous = operand.parent;
        if (previous->kind == 3)
        {
            u32 last_value = 0;
            if (previous->diagnostics.nodes.count != 0)
            {
                ShaderValidationBalancedNode* last;
                find_shader_validation_tree_last(previous->diagnostics.nodes, &last);
                last_value = last->value;
            }
            if (last_value == 0 && find_shader_validation_foreign_parent(*previous, context.active_owner) != nullptr)
            {
                reparent_shader_validation_owner_operand(context, *previous, nullptr);
            }
        }
    }
    operand.parent = parent;
    if (parent != nullptr && parent != context.active_owner)
    {
        register_shader_validation_diagnostic(context.diagnostic_registry(*parent), context.retail_address(&operand));
    }
    if (operand.parent == nullptr)
    {
        operand.parent = context.active_owner;
    }
    return operand.parent;
}

// Xbox 0x00192B20
void reparent_shader_validation_owner_operand(ShaderValidationConstructorContext& context,
    ShaderValidationOwner& owner, ShaderValidationOwner* parent)
{
    void* allocation = nullptr;
    i32 state = -1;
    GBTG_SEH_TRY_FINALLY
    {
        if (owner.operand == nullptr)
        {
            allocation = context.allocate(sizeof(ShaderValidationOperand), 0x24870000);
            state = 0;
            ShaderValidationOperand* operand;
            if (allocation == nullptr)
            {
                operand = nullptr;
            }
            else
            {
                operand = construct_shader_validation_operand(context, *static_cast<ShaderValidationOperand*>(allocation),
                    context.retail_address(allocation), nullptr, &owner, 3, context.active_owner, 0);
            }
            state = -1;
            owner.operand = operand;
        }
        reparent_shader_validation_operand(context, *owner.operand, parent);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 0)
        {
            // FuncInfo 0x0032B8CC, state 0: 0x001709B7 releases EBP-0x10.
            release_shader_validation_allocation(allocation, context.release);
        }
    }
    GBTG_SEH_FINALLY_END
}

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
    u32 object_address, ShaderValidationInstruction* instruction, ShaderValidationOwner* owner, u32 kind, ShaderValidationOwner* parent,
    u8 marker, i32& state)
{
    initialize_shader_validation_registered_object(context.objects, operand.base, object_address);
    state = 0;
    operand.owner = owner;
    operand.base.vtable_address = 0x0020380c;
    operand.instruction = instruction;
    operand.kind = kind;
    operand.parent = parent;
    initialize_shader_validation_channel_order(operand.channel_order);
    state = 1;
    if ((instruction == nullptr) && (kind != 3))
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
    u32 object_address, u32 kind, u32 source, ShaderValidationOwner* parent, ShaderValidationChannelWriter channel_value,
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
        owner.channels[index] = (ShaderValidationChannelWriter{0} - (mask.channels[index] != 0 ? 1u : 0u)) & channel_value;
        ++index;
    } while (index < 4);
    owner.channel_owner = nullptr;
    owner.derived_owner = nullptr;
}
}

// Xbox 0x00192934
ShaderValidationOperand* construct_shader_validation_operand(ShaderValidationConstructorContext& context, ShaderValidationOperand& operand,
    u32 object_address, ShaderValidationInstruction* instruction, ShaderValidationOwner* owner, u32 kind, ShaderValidationOwner* parent,
    u8 marker)
{
    i32 state = -1;
    GBTG_SEH_TRY_FINALLY
    {
        construct_operand_body(context, operand, object_address, instruction, owner, kind, parent, marker, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
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
    GBTG_SEH_FINALLY_END
    return &operand;
}

// Xbox 0x00192A54
ShaderValidationOwner* construct_shader_validation_owner(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner,
    u32 object_address, u32 kind, u32 source, ShaderValidationOwner* parent, ShaderValidationChannelWriter channel_value,
    ShaderValidationChannelMask mask)
{
    i32 state = -1;
    void* allocation = nullptr;
    GBTG_SEH_TRY_FINALLY
    {
        construct_owner_body(context, owner, object_address, kind, source, parent, channel_value, mask, state, allocation);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
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
    GBTG_SEH_FINALLY_END
    return &owner;
}
}
