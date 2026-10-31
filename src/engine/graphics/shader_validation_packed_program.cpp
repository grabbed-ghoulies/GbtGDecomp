#include "shader_validation_packed_program.h"
#include "gbtg/host_seh.h"
#include "shader_validation_destinations.h"
#include "shader_validation_operand_tree.h"
#include <bit>

namespace gbtg
{
namespace
{
void construct_pass_body(ShaderValidationProgramConstructorContext& context, ShaderValidationPass& pass,
    const u32* packed_program, i32& state, void*& allocation)
{
    auto& constructors = context.dependencies.constructors;
    initialize_shader_validation_registered_object(constructors.objects, pass.base, constructors.retail_address(&pass));
    pass.base.vtable_address = 0x00203818;
    state = 0;
    pass.instructions.vtable_address = 0x002037C8;
    pass.instructions.storage = {nullptr, nullptr, 0};
    construct_shader_validation_registered_pass_tree(pass.values);
    construct_shader_validation_registered_pass_tree(pass.registers);
    construct_shader_validation_registered_pass_tree(pass.special);
    pass.owner = nullptr;
    for (u32 kind = 0; kind < 12; ++kind)
    {
        allocation = constructors.allocate(sizeof(ShaderValidationOwner), 0x24870000);
        state = 5;
        ShaderValidationOwner* owner;
        if (allocation == nullptr)
        {
            owner = nullptr;
        }
        else
        {
            ShaderValidationChannelMask full;
            construct_shader_validation_channel_mask(full, 0xF);
            owner = construct_shader_validation_owner(constructors, *static_cast<ShaderValidationOwner*>(allocation),
                constructors.retail_address(allocation), 1, kind, nullptr, shader_validation_unwritten_channel, full);
        }
        state = 4;
        ShaderValidationPassNode* inserted;
        set_shader_validation_pass_value(pass.registers.tree.nodes, constructors.allocate, &inserted, kind, owner);
        pass.initial_registers[kind] = owner;
    }
    allocation = constructors.allocate(sizeof(ShaderValidationOwner), 0x24870000);
    state = 6;
    ShaderValidationOwner* special;
    if (allocation == nullptr)
    {
        special = nullptr;
    }
    else
    {
        ShaderValidationChannelMask full;
        construct_shader_validation_channel_mask(full, 0xF);
        special = construct_shader_validation_owner(constructors, *static_cast<ShaderValidationOwner*>(allocation),
            constructors.retail_address(allocation), 4, 0x100, nullptr, shader_validation_unwritten_channel, full);
    }
    state = 4;
    ShaderValidationPassNode* inserted;
    set_shader_validation_pass_value(pass.special.tree.nodes, constructors.allocate, &inserted, 0x100, special);
    set_shader_validation_pass_value(pass.registers.tree.nodes, constructors.allocate, &inserted, 12, special);
    pass.initial_registers[12] = special;
    for (i32 index = 0; index < std::bit_cast<i32>(packed_program[0]); ++index)
    {
        allocation = constructors.allocate(sizeof(ShaderValidationInstruction), 0x24870000);
        state = 7;
        ShaderValidationInstruction* instruction;
        if (allocation == nullptr)
        {
            instruction = nullptr;
        }
        else
        {
            const auto* words = packed_program + 1 + static_cast<std::size_t>(index) * 3;
            instruction = construct_shader_validation_packed_instruction(context, *static_cast<ShaderValidationInstruction*>(allocation),
                words[0], words[1], words[2], static_cast<u32>(index), pass);
        }
        state = 4;
        append_shader_validation_list_value(pass.instructions, context.list_node_vtable, constructors.allocate, instruction);
    }
    register_shader_validation_operand(context.expressions, constructors.retail_address(&pass.values.registered));
    register_shader_validation_operand(context.expressions, constructors.retail_address(&pass.registers.registered));
    register_shader_validation_operand(context.expressions, constructors.retail_address(&pass.special.registered));
}

ShaderValidationOwner* allocate_packed_owner(ShaderValidationConstructorContext& constructors, u32 kind,
    u32 source, ShaderValidationInstruction& instruction, ShaderValidationChannelMask mask,
    i32 allocation_state, i32& state, void*& allocation)
{
    allocation = constructors.allocate(sizeof(ShaderValidationOwner), 0x24870000);
    state = allocation_state;
    ShaderValidationOwner* owner;
    if (allocation == nullptr)
    {
        owner = nullptr;
    }
    else
    {
        owner = construct_shader_validation_owner(constructors, *static_cast<ShaderValidationOwner*>(allocation),
            constructors.retail_address(allocation), kind, source, nullptr,
            reinterpret_cast<ShaderValidationChannelWriter>(&instruction), mask);
    }
    state = 3;
    return owner;
}

void construct_packed_body(ShaderValidationProgramConstructorContext& context, ShaderValidationInstruction& instruction,
    u32 word0, u32 word1, u32 word2, u32 sequence, ShaderValidationPass& pass, i32& state, void*& allocation)
{
    auto& constructors = context.dependencies.constructors;
    initialize_shader_validation_registered_object(constructors.objects, instruction.base, constructors.retail_address(&instruction));
    state = 0;
    instruction.base.vtable_address = 0x00203810;
    construct_shader_validation_channel_mask(instruction.output_mask, 0xF);
    construct_shader_validation_channel_mask(instruction.vector_mask, 0xF);
    construct_shader_validation_channel_mask(instruction.scalar_mask, 0xF);
    instruction.sequence = sequence;
    instruction.instruction_set_count = 0xFFFFFFFF;
    instruction.vector_opcode = (word2 >> 25) & 0xF;
    instruction.scalar_opcode = word2 >> 29;
    instruction.instruction_set = nullptr;
    ShaderValidationChannelMask mask;
    construct_shader_validation_channel_mask(mask, (word0 >> 12) & 0xF);
    copy_shader_validation_channel_mask(instruction.output_mask, mask);
    if (instruction.vector_opcode == 13)
    {
        construct_shader_validation_single_channel_mask(mask, 0);
    }
    else
    {
        construct_shader_validation_channel_mask(mask, (word0 >> 24) & 0xF);
    }
    copy_shader_validation_channel_mask(instruction.vector_mask, mask);
    construct_shader_validation_channel_mask(mask, (word0 >> 16) & 0xF);
    copy_shader_validation_channel_mask(instruction.scalar_mask, mask);
    instruction.output_flag = static_cast<u8>((word0 >> 2) & 1);
    std::array<ShaderValidationOwner*, 3> parents{constructors.active_owner, constructors.active_owner, constructors.active_owner};
    instruction.operands.fill(nullptr);
    const std::array<u32, 3> words{word2, word1, word1};
    const std::array<u32, 3> marker_shifts{12, 27, 12};
    const std::array<u32, 3> order_shifts{10, 25, 10};
    for (u32 slot = 0; slot < 3; ++slot)
    {
        allocation = constructors.allocate(sizeof(ShaderValidationOperand), 0x24870000);
        state = 4 + static_cast<i32>(slot);
        ShaderValidationOperand* operand;
        if (allocation == nullptr)
        {
            operand = nullptr;
        }
        else
        {
            operand = construct_shader_validation_operand(constructors, *static_cast<ShaderValidationOperand*>(allocation),
                constructors.retail_address(allocation), &instruction, nullptr, slot, parents[slot],
                static_cast<u8>((words[slot] >> marker_shifts[slot]) & 0xFFFFFF01));
        }
        instruction.operands[slot] = operand;
        for (u32 channel = 0; channel < 4; ++channel)
        {
            operand->channel_order.channels[channel] = (words[slot] >> (order_shifts[slot] - 2 * channel)) & 3;
        }
        state = 3;
        clear_unused_shader_validation_operand_channels(constructors, *operand);
        const bool uses = slot == 0 ? instruction.vector_opcode != 0 : shader_validation_instruction_uses_slot(instruction, slot);
        if (uses)
        {
            ShaderValidationChannelMask reads;
            ShaderValidationChannelMask sources;
            shader_validation_instruction_read_mask(instruction, reads, slot);
            shader_validation_operand_source_mask(*operand, reads, sources);
            const std::array<u32, 3> kinds{(word1 >> 28) & 3, (word1 >> 13) & 3, (word0 >> 28) & 3};
            const std::array<u32, 3> registers{word2 & 0xF, (word1 >> 15) & 0xF, word1 & 0xF};
            parents[slot] = select_shader_validation_owner(constructors, pass, kinds[slot], (word2 >> 13) & 0xF,
                (word2 >> 17) & 0xFF, registers[slot], static_cast<u8>((word0 >> 1) & 0xFFFFFF01), sources);
        }
        if (parents[slot] == nullptr)
        {
            parents[slot] = constructors.active_owner;
        }
    }
    for (u32 slot = 0; slot < 3; ++slot)
    {
        reparent_shader_validation_operand(constructors, *instruction.operands[slot], parents[slot]);
    }
    for (u32 slot = 0; slot < 3; ++slot)
    {
        if (instruction.operands[slot]->parent == nullptr)
        {
            raise_invalid_shader_validation_operand(constructors.exceptions);
        }
    }
    if (pack_shader_validation_channel_mask(instruction.output_mask) != 0)
    {
        ShaderValidationChannelMask argument{0x00203790, instruction.output_mask.channels};
        auto* owner = allocate_packed_owner(constructors, 4, (word0 >> 3) & 0x1FF, instruction, argument, 7, state, allocation);
        instruction.output_register = replace_shader_validation_special_owner(context.dependencies, pass, *owner, context.deleting_destructor);
    }
    else
    {
        instruction.output_register = nullptr;
    }
    if (instruction.vector_opcode == 13)
    {
        ShaderValidationChannelMask argument{0x00203790, instruction.vector_mask.channels};
        auto* owner = allocate_packed_owner(constructors, 5, 0, instruction, argument, 8, state, allocation);
        instruction.vector_destination = set_shader_validation_pass_owner(pass, owner, context.dependencies.sets);
    }
    else if (pack_shader_validation_channel_mask(instruction.vector_mask) != 0)
    {
        ShaderValidationChannelMask argument{0x00203790, instruction.vector_mask.channels};
        auto* owner = allocate_packed_owner(constructors, 1, (word0 >> 20) & 0xF, instruction, argument, 9, state, allocation);
        instruction.vector_destination = replace_shader_validation_register_owner(pass, *owner, context.deleting_destructor, context.dependencies.sets);
    }
    else
    {
        instruction.vector_destination = nullptr;
    }
    if (pack_shader_validation_channel_mask(instruction.scalar_mask) != 0)
    {
        const u32 source = instruction.vector_opcode == 0 ? (word0 >> 20) & 0xF : 1;
        ShaderValidationChannelMask argument{0x00203790, instruction.scalar_mask.channels};
        auto* owner = allocate_packed_owner(constructors, 1, source, instruction, argument, 10, state, allocation);
        instruction.scalar_destination = replace_shader_validation_register_owner(pass, *owner, context.deleting_destructor, context.dependencies.sets);
    }
    else
    {
        instruction.scalar_destination = nullptr;
    }
    clear_unused_shader_validation_instruction_channels(constructors, instruction);
}
}

// Xbox 0x00195FBE
ShaderValidationInstruction* construct_shader_validation_packed_instruction(ShaderValidationProgramConstructorContext& context,
    ShaderValidationInstruction& instruction, u32 word0, u32 word1, u32 word2, u32 sequence, ShaderValidationPass& pass)
{
    i32 state = -1;
    void* allocation = nullptr;
    GBTG_SEH_TRY_FINALLY
    {
        construct_packed_body(context, instruction, word0, word1, word2, sequence, pass, state, allocation);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state >= 0)
        {
            auto& constructors = context.dependencies.constructors;
            if (state >= 4)
            {
                release_shader_validation_allocation(allocation, constructors.release);
            }
            if (state >= 3)
            {
                destroy_shader_validation_channel_mask(instruction.scalar_mask);
            }
            if (state >= 2)
            {
                destroy_shader_validation_channel_mask(instruction.vector_mask);
            }
            if (state >= 1)
            {
                destroy_shader_validation_channel_mask(instruction.output_mask);
            }
            destroy_shader_validation_registered_object(constructors.object_erase, instruction.base, constructors.retail_address(&instruction));
        }
    }
    GBTG_SEH_FINALLY_END
    return &instruction;
}

// Xbox 0x00196545
ShaderValidationPass* construct_shader_validation_pass(ShaderValidationProgramConstructorContext& context,
    ShaderValidationPass& pass, const u32* packed_program)
{
    i32 state = -1;
    void* allocation = nullptr;
    GBTG_SEH_TRY_FINALLY
    {
        construct_pass_body(context, pass, packed_program, state, allocation);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state >= 0)
        {
            auto& constructors = context.dependencies.constructors;
            if (state >= 5)
            {
                release_shader_validation_allocation(allocation, constructors.release);
            }
            if (state >= 4)
            {
                destroy_shader_validation_pass_tree_object(pass.special.tree, context.deleting_destructor);
            }
            if (state >= 3)
            {
                destroy_shader_validation_pass_tree_object(pass.registers.tree, context.deleting_destructor);
            }
            if (state >= 2)
            {
                destroy_shader_validation_pass_tree_object(pass.values.tree, context.deleting_destructor);
            }
            if (state >= 1)
            {
                destroy_shader_validation_list(pass.instructions);
            }
            destroy_shader_validation_registered_object(constructors.object_erase, pass.base, constructors.retail_address(&pass));
        }
    }
    GBTG_SEH_FINALLY_END
    return &pass;
}

namespace
{
void construct_program_body(ShaderValidationProgramConstructorContext& context, ShaderValidationProgramPass& pass,
    const u32* packed_program, i32& state)
{
    auto& dependencies = context.dependencies;
    auto& constructors = dependencies.constructors;
    construct_shader_validation_pass(context, pass.base, packed_program);
    pass.base.base.vtable_address = 0x00203820;
    state = 0;
    pass.instruction_set.vtable_address = 0x002037D0;
    pass.instruction_set.nodes = {nullptr, 0};
    state = 1;
    ShaderValidationPassNode* node;
    first_shader_validation_pass_node(pass.base.registers.tree.nodes, &node);
    while (node != nullptr)
    {
        auto* owner = static_cast<ShaderValidationOwner*>(node->data);
        while (owner->derived_owner != nullptr)
        {
            move_shader_validation_readers_to_derived(dependencies, *owner, 1);
            if (owner->kind == 1)
            {
                owner->kind = 6;
            }
            owner = owner->derived_owner;
        }
        if (owner->kind == 1)
        {
            owner->kind = 6;
        }
        while ((owner = owner->channel_owner) != nullptr)
        {
            detach_unused_shader_validation_derived_owner(dependencies, *owner);
        }
        node = erase_shader_validation_pass_node(pass.base.registers.tree.nodes, node, context.deleting_destructor);
    }
    auto* list_node = pass.base.instructions.storage.first;
    while (list_node != nullptr)
    {
        ShaderValidationInsertContext insert{pass.instruction_set.nodes, constructors.allocate};
        register_shader_validation_instruction(insert, constructors.retail_address(list_node->value), dependencies.sets.instruction_at);
        auto* next = list_node->next;
        erase_shader_validation_list_node(pass.base.instructions.storage, list_node);
        list_node = next;
    }
}

void unwind_program_base(ShaderValidationProgramConstructorContext& context, ShaderValidationProgramPass& pass)
{
    auto& dependencies = context.dependencies;
    auto& constructors = dependencies.constructors;
    ShaderValidationEraseContext expressions{context.expressions.tree, dependencies.sets.deleting_destructor};
    ShaderValidationPassDestroyContext destroy_context{expressions, constructors.object_erase,
        context.deleting_destructor, constructors.retail_address};
    destroy_shader_validation_pass(destroy_context, pass.base);
}
}

// Xbox 0x00196739
ShaderValidationProgramPass* construct_shader_validation_program_pass(ShaderValidationProgramConstructorContext& context,
    ShaderValidationProgramPass& pass, const u32* packed_program)
{
    auto& dependencies = context.dependencies;
    i32 state = -1;
    GBTG_SEH_TRY_FINALLY
    {
        construct_program_body(context, pass, packed_program, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            if (state == 1)
            {
                pass.instruction_set.vtable_address = 0x002037D0;
                clear_shader_validation_balanced_tree_190322(pass.instruction_set, dependencies.sets.deleting_destructor);
            }
            if (state == 0 || state == 1)
            {
                unwind_program_base(context, pass);
            }
        }
    }
    GBTG_SEH_FINALLY_END
    return &pass;
}
}
