#include "shader_validation_instruction_lifecycle.h"
#include "gbtg/host_seh.h"
#include "shader_validation_destinations.h"

namespace gbtg
{
// Xbox 0x00193BBC
void destroy_shader_validation_operand(ShaderValidationConstructorContext& context, ShaderValidationOperand& operand)
{
    operand.base.vtable_address = 0x0020380C;
    i32 state = 1;
    GBTG_SEH_TRY_FINALLY
    {
        if (operand.parent != nullptr)
        {
            reparent_shader_validation_operand(context, operand, nullptr);
        }
        if (operand.instruction != nullptr)
        {
            operand.instruction->operands[operand.kind] = nullptr;
        }
        if (operand.owner != nullptr)
        {
            operand.owner->operand = nullptr;
        }
        state = -1;
        destroy_shader_validation_channel_order(operand.channel_order);
        destroy_shader_validation_registered_object(context.object_erase, operand.base, context.retail_address(&operand));
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 1)
        {
            destroy_shader_validation_channel_order(operand.channel_order);
            destroy_shader_validation_registered_object(context.object_erase, operand.base, context.retail_address(&operand));
        }
    }
    GBTG_SEH_FINALLY_END
}

// Xbox 0x00193FC0
ShaderValidationOperand* delete_shader_validation_operand(ShaderValidationConstructorContext& context,
    ShaderValidationOperand* operand, u8 flags)
{
    destroy_shader_validation_operand(context, *operand);
    if ((flags & 1) != 0 && operand != nullptr)
    {
        context.release(operand, 0x24870000);
    }
    return operand;
}

namespace
{
void destroy_owner_body(ShaderValidationOwnerDestroyContext& context, ShaderValidationOwner& owner, i32& state)
{
    auto& dependencies = context.instructions.dependencies;
    auto& constructors = dependencies.constructors;
    owner.base.vtable_address = 0x00203814;
    state = 1;
    while (owner.diagnostics.nodes.count != 0)
    {
        const u32 operand = last_shader_validation_tree_value(owner.diagnostics.nodes);
        reparent_shader_validation_operand(constructors, *dependencies.sets.operand_at(operand), owner.derived_owner);
    }
    if (owner.channel_owner != nullptr)
    {
        owner.channel_owner->derived_owner = owner.derived_owner;
    }
    if (owner.derived_owner != nullptr)
    {
        owner.derived_owner->channel_owner = owner.channel_owner;
    }
    owner.channel_owner = nullptr;
    owner.derived_owner = nullptr;
    for (u32 channel = 0; channel < 4; ++channel)
    {
        const auto writer = owner.channels[channel];
        if (writer != 0 && writer != shader_validation_unwritten_channel)
        {
            auto* instruction = reinterpret_cast<ShaderValidationInstruction*>(writer);
            if (instruction->output_register == &owner)
            {
                set_shader_validation_output_destination(*instruction, nullptr);
            }
            if (owner.channels[channel] != 0)
            {
                instruction = reinterpret_cast<ShaderValidationInstruction*>(owner.channels[channel]);
                if (instruction->vector_destination == &owner)
                {
                    set_shader_validation_vector_destination(*instruction, nullptr);
                }
                if (owner.channels[channel] != 0)
                {
                    instruction = reinterpret_cast<ShaderValidationInstruction*>(owner.channels[channel]);
                    if (instruction->scalar_destination == &owner)
                    {
                        set_shader_validation_scalar_destination(*instruction, nullptr);
                    }
                }
            }
            owner.channels[channel] = 0;
        }
    }
    if (owner.operand != nullptr)
    {
        delete_shader_validation_operand(constructors, owner.operand, 1);
    }
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(context.expressions, &node);
    while (node != nullptr)
    {
        context.unregister_owner(node->value, owner);
        advance_shader_validation_arena_iterator(node);
    }
    state = 0;
    owner.diagnostics.vtable_address = 0x002037B8;
    clear_shader_validation_child_tree(owner.diagnostics, dependencies.sets.deleting_destructor);
    state = -1;
    destroy_shader_validation_registered_object(constructors.object_erase, owner.base, constructors.retail_address(&owner));
}

void destroy_instruction_body(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction& instruction, i32& state)
{
    auto& dependencies = context.dependencies;
    auto& constructors = dependencies.constructors;
    instruction.base.vtable_address = 0x00203810;
    state = 3;
    ShaderValidationEraseContext registry{dependencies.instruction_sets.tree, dependencies.sets.deleting_destructor};
    release_shader_validation_instruction_set(instruction, registry, dependencies.sets.address_of,
        [&](ShaderValidationBalancedChildTree& set, u32 flags)
        {
            delete_shader_validation_balanced_tree(set, static_cast<u8>(flags), dependencies.sets.deleting_destructor, constructors.release);
        });
    for (u32 slot = 0; slot < 3; ++slot)
    {
        if (instruction.operands[slot] != nullptr)
        {
            delete_shader_validation_operand(constructors, instruction.operands[slot], 1);
        }
    }
    const std::array<ShaderValidationOwner**, 3> destinations{&instruction.output_register,
        &instruction.vector_destination, &instruction.scalar_destination};
    for (auto* destination : destinations)
    {
        if (*destination != nullptr)
        {
            for (u32 channel = 0; channel < 4; ++channel)
            {
                remove_shader_validation_destination_channel(instruction, channel, *destination);
            }
        }
    }
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(dependencies.instruction_sets.tree, &node);
    while (node != nullptr)
    {
        ShaderValidationBalancedChildTree& set = *context.set_at(node->value);
        ShaderValidationEraseContext erase{set.nodes, dependencies.sets.deleting_destructor};
        unregister_shader_validation_instruction_dependency(erase, dependencies.sets.address_of(&instruction), dependencies.sets.instruction_at);
        advance_shader_validation_arena_iterator(node);
    }
    state = -1;
    destroy_shader_validation_channel_mask(instruction.scalar_mask);
    destroy_shader_validation_channel_mask(instruction.vector_mask);
    destroy_shader_validation_channel_mask(instruction.output_mask);
    destroy_shader_validation_registered_object(constructors.object_erase, instruction.base, constructors.retail_address(&instruction));
}
}

// Xbox 0x001937FC
void destroy_shader_validation_owner(ShaderValidationOwnerDestroyContext& context, ShaderValidationOwner& owner)
{
    i32 state = -1;
    GBTG_SEH_TRY_FINALLY
    {
        destroy_owner_body(context, owner, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            auto& dependencies = context.instructions.dependencies;
            if (state == 1)
            {
                destroy_shader_validation_child_tree(owner.diagnostics, dependencies.sets.deleting_destructor);
            }
            if (state == 0 || state == 1)
            {
                auto& constructors = dependencies.constructors;
                destroy_shader_validation_registered_object(constructors.object_erase, owner.base, constructors.retail_address(&owner));
            }
        }
    }
    GBTG_SEH_FINALLY_END
}

// Xbox 0x00194088
ShaderValidationOwner* delete_shader_validation_owner(ShaderValidationOwnerDestroyContext& context, ShaderValidationOwner* owner, u8 flags)
{
    destroy_shader_validation_owner(context, *owner);
    if ((flags & 1) != 0 && owner != nullptr)
    {
        context.instructions.dependencies.constructors.release(owner, 0x24870000);
    }
    return owner;
}

// Xbox 0x00192D13
void destroy_shader_validation_instruction(ShaderValidationInstructionDestroyContext& context, ShaderValidationInstruction& instruction)
{
    i32 state = -1;
    GBTG_SEH_TRY_FINALLY
    {
        destroy_instruction_body(context, instruction, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 3)
        {
            destroy_shader_validation_channel_mask(instruction.scalar_mask);
            destroy_shader_validation_channel_mask(instruction.vector_mask);
            destroy_shader_validation_channel_mask(instruction.output_mask);
            auto& constructors = context.dependencies.constructors;
            destroy_shader_validation_registered_object(constructors.object_erase, instruction.base, constructors.retail_address(&instruction));
        }
    }
    GBTG_SEH_FINALLY_END
}

// Xbox 0x001937D8
ShaderValidationInstruction* delete_shader_validation_instruction(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction* instruction, u8 flags)
{
    destroy_shader_validation_instruction(context, *instruction);
    if ((flags & 1) != 0 && instruction != nullptr)
    {
        context.dependencies.constructors.release(instruction, 0x24870000);
    }
    return instruction;
}

// Xbox 0x001956D1
bool merge_shader_validation_instructions(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationInstruction& other)
{
    auto& dependencies = context.dependencies;
    auto& constructors = dependencies.constructors;
    if (!shader_validation_instructions_merge_compatible(dependencies, instruction, other))
    {
        return false;
    }
    for (u32 slot = 0; slot < 3; ++slot)
    {
        auto& destination = *instruction.operands[slot];
        auto& source = *other.operands[slot];
        if (!shader_validation_instruction_uses_slot(instruction, slot))
        {
            reparent_shader_validation_operand(constructors, destination, source.parent);
            destination.marker = source.marker;
            for (u32 channel = 0; channel < 4; ++channel)
            {
                destination.channel_order.channels[channel] = source.channel_order.channels[channel];
            }
        }
        else if (shader_validation_instruction_uses_slot(other, slot))
        {
            for (u32 channel = 0; channel < 4; ++channel)
            {
                if (!shader_validation_instruction_reads_channel(instruction, slot, static_cast<i32>(channel)))
                {
                    destination.channel_order.channels[channel] = source.channel_order.channels[channel];
                }
                else if (shader_validation_instruction_reads_channel(other, slot, static_cast<i32>(channel)))
                {
                    if (destination.channel_order.channels[channel] != source.channel_order.channels[channel])
                    {
                        raise_invalid_shader_validation_operand(constructors.exceptions);
                    }
                }
            }
        }
        reparent_shader_validation_operand(constructors, source, constructors.active_owner);
    }
    if (instruction.vector_opcode == 0)
    {
        instruction.vector_opcode = other.vector_opcode;
    }
    if (instruction.scalar_opcode == 0)
    {
        instruction.scalar_opcode = other.scalar_opcode;
    }
    if (instruction.output_register == nullptr)
    {
        instruction.output_register = other.output_register;
        instruction.output_flag = other.output_flag;
    }
    const std::array<ShaderValidationOwner**, 2> first_destinations{&instruction.vector_destination, &instruction.scalar_destination};
    const std::array<ShaderValidationOwner**, 2> second_destinations{&other.vector_destination, &other.scalar_destination};
    for (u32 index = 0; index < 2; ++index)
    {
        if (*first_destinations[index] == nullptr)
        {
            *first_destinations[index] = *second_destinations[index];
        }
        else if (*second_destinations[index] != nullptr && *first_destinations[index] != *second_destinations[index])
        {
            ShaderValidationChannelMask first;
            ShaderValidationChannelMask second;
            if ((*second_destinations[index])->kind == 1)
            {
                construct_shader_validation_channel_mask(second, 0);
                construct_shader_validation_channel_mask(first, 0);
                merge_shader_validation_owners(dependencies, **second_destinations[index], *first_destinations[index], 0, first, second);
                *first_destinations[index] = *second_destinations[index];
            }
            else
            {
                shader_validation_owner_channel_mask(**second_destinations[index], second);
                shader_validation_owner_channel_mask(**first_destinations[index], first);
                merge_shader_validation_owners(dependencies, **first_destinations[index], *second_destinations[index], 0, first, second);
            }
        }
    }
    const std::array<ShaderValidationChannelMask*, 3> first_masks{&instruction.output_mask, &instruction.vector_mask, &instruction.scalar_mask};
    const std::array<ShaderValidationChannelMask*, 3> second_masks{&other.output_mask, &other.vector_mask, &other.scalar_mask};
    for (u32 index = 0; index < 3; ++index)
    {
        ShaderValidationChannelMask argument{0x00203790, second_masks[index]->channels};
        ShaderValidationChannelMask combined;
        unite_shader_validation_channel_masks(*first_masks[index], argument, combined);
        copy_shader_validation_channel_mask(*first_masks[index], combined);
    }
    const std::array<ShaderValidationOwner**, 3> writer_destinations{&other.output_register, &other.vector_destination, &other.scalar_destination};
    for (u32 index = 0; index < 3; ++index)
    {
        if (*writer_destinations[index] != nullptr)
        {
            ShaderValidationChannelMask mask{0x00203790, second_masks[index]->channels};
            set_shader_validation_masked_writers(**writer_destinations[index], mask,
                reinterpret_cast<ShaderValidationChannelWriter>(&instruction));
        }
    }
    other.scalar_destination = nullptr;
    other.vector_destination = nullptr;
    other.output_register = nullptr;
    if (instruction.vector_destination != nullptr)
    {
        merge_shader_validation_duplicate_channels(*instruction.vector_destination, dependencies.sets.operand_at);
    }
    if (instruction.scalar_destination != nullptr)
    {
        merge_shader_validation_duplicate_channels(*instruction.scalar_destination, dependencies.sets.operand_at);
    }
    if (instruction.instruction_set == nullptr)
    {
        instruction.instruction_set = other.instruction_set;
        instruction.instruction_set_count = other.instruction_set_count;
    }
    else if (other.instruction_set != nullptr)
    {
        copy_shader_validation_instruction_set(instruction, *other.instruction_set, constructors.allocate, dependencies.sets.instruction_at);
    }
    delete_shader_validation_instruction(context, &other, 1);
    return true;
}
}
