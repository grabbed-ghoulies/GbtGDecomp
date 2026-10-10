#include "shader_validation_pass_lifecycle.h"
#include "shader_validation_cleanup.h"
#include "shader_validation_destinations.h"
#include "shader_validation_instruction_lifecycle.h"
#include <bit>
#if defined(_WIN32)
#include <excpt.h>
#else
#error Shader validator constructor unwind requires the approved Windows SEH backend.
#endif

namespace gbtg
{
// Xbox 0x00195A15
void merge_shader_validation_program_instructions(ShaderValidationInstructionDestroyContext& context, ShaderValidationProgramPass& pass)
{
    auto& dependencies = context.dependencies;
    ShaderValidationEraseContext erase{pass.instruction_set.nodes, dependencies.sets.deleting_destructor};
    ShaderValidationBalancedNode* first;
    find_shader_validation_tree_last(pass.instruction_set.nodes, &first);
    while (first != nullptr)
    {
        auto& instruction = *const_cast<ShaderValidationInstruction*>(dependencies.sets.instruction_at(first->value));
        if (instruction.vector_opcode == 0 || instruction.scalar_opcode == 0)
        {
            convert_shader_validation_scalar_move(instruction);
            ShaderValidationBalancedNode* second = first;
            advance_shader_validation_arena_iterator(second);
            while (second != nullptr)
            {
                auto& other = *const_cast<ShaderValidationInstruction*>(dependencies.sets.instruction_at(second->value));
                convert_shader_validation_scalar_move(other);
                if (instruction.vector_opcode == other.vector_opcode && instruction.scalar_opcode == other.scalar_opcode &&
                    (instruction.output_register == nullptr) == (other.output_register == nullptr) &&
                    (instruction.vector_destination == nullptr) == (other.vector_destination == nullptr) &&
                    (instruction.scalar_destination == nullptr) == (other.scalar_destination == nullptr) &&
                    instruction.output_flag == other.output_flag && merge_shader_validation_instructions(context, instruction, other))
                {
                    second = erase_shader_validation_balanced_node_19234a(erase, second);
                }
                else
                {
                    advance_shader_validation_arena_iterator(second);
                }
            }
        }
        advance_shader_validation_arena_iterator(first);
    }
}

// Xbox 0x00195AFC
void merge_shader_validation_program_moves(ShaderValidationInstructionDestroyContext& context, ShaderValidationProgramPass& pass)
{
    auto& dependencies = context.dependencies;
    ShaderValidationEraseContext erase{pass.instruction_set.nodes, dependencies.sets.deleting_destructor};
    ShaderValidationBalancedNode* first;
    find_shader_validation_tree_last(pass.instruction_set.nodes, &first);
    while (first != nullptr)
    {
        auto& instruction = *const_cast<ShaderValidationInstruction*>(dependencies.sets.instruction_at(first->value));
        if (shader_validation_instruction_is_single_move(instruction) != 0)
        {
            convert_shader_validation_scalar_move(instruction);
            ShaderValidationBalancedNode* second = first;
            advance_shader_validation_arena_iterator(second);
            while (second != nullptr)
            {
                auto& other = *const_cast<ShaderValidationInstruction*>(dependencies.sets.instruction_at(second->value));
                if (shader_validation_instruction_is_single_move(other) != 0)
                {
                    convert_shader_validation_scalar_move(other);
                    if (merge_shader_validation_instructions(context, instruction, other))
                    {
                        second = erase_shader_validation_balanced_node_19234a(erase, second);
                        continue;
                    }
                }
                advance_shader_validation_arena_iterator(second);
            }
        }
        advance_shader_validation_arena_iterator(first);
    }
}

// Xbox 0x00196D71
void merge_shader_validation_list_candidates(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationListStorage& list, ShaderValidationListNode* first,
    ShaderValidationPass& pass, const ShaderValidationBalancedTree& readers)
{
    while (first != nullptr)
    {
        select_shader_validation_list_priority(context.dependencies, *first, nullptr);
        auto& other = *static_cast<ShaderValidationInstruction*>(first->value);
        if (shader_validation_instructions_merge_on_pass(context.dependencies, pass, instruction, other, readers))
        {
            if (merge_shader_validation_instructions(context, instruction, other))
            {
                auto* next = first->next;
                erase_shader_validation_list_node(list, first);
                first = next;
                continue;
            }
        }
        first = first->next;
    }
}

// Xbox 0x00196F09
u32 assign_shader_validation_instruction_registers(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationPass& pass, const ShaderValidationBalancedTree& readers,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    std::array<i32, 3> kinds;
    for (u32 slot = 0; slot < 3; ++slot)
    {
        auto* owner = instruction.operands[slot]->parent;
        kinds[slot] = -1;
        bool used = false;
        if (owner != nullptr && shader_validation_instruction_uses_slot(instruction, slot))
        {
            ShaderValidationChannelMask mask;
            shader_validation_instruction_read_mask(instruction, mask, slot);
            used = pack_shader_validation_channel_mask(mask) != 0;
            destroy_shader_validation_channel_mask(mask);
        }
        if (used)
        {
            if (owner->kind == 6)
            {
                kinds[slot] = find_shader_validation_owner_register_kind(pass, owner);
                if (kinds[slot] < 0)
                {
                    goto invalid;
                }
            }
            else if (owner->kind == 1)
            {
                kinds[slot] = std::bit_cast<i32>(owner->source);
            }
        }
    }
    for (u32 slot = 0; slot < 3; ++slot)
    {
        if (kinds[slot] >= 0)
        {
            retire_shader_validation_register(pass, kinds[slot], readers, deleting_destructor, context.constructors.allocate, context.sets);
        }
    }
    if (instruction.vector_opcode != 0)
    {
        if (instruction.scalar_destination != nullptr)
        {
            ShaderValidationChannelMask mask{0x00203790, instruction.scalar_mask.channels};
            if (!assign_shader_validation_owner_scalar_register(context, pass, *instruction.scalar_destination, readers, &instruction, mask, 0))
            {
                goto invalid;
            }
        }
        if (instruction.vector_destination != nullptr && instruction.vector_opcode != 13)
        {
            ShaderValidationChannelMask mask{0x00203790, instruction.vector_mask.channels};
            if (choose_shader_validation_owner_register(context, pass, *instruction.vector_destination, readers, instruction, mask, 0) < 0)
            {
                goto invalid;
            }
        }
    }
    else if (instruction.scalar_destination != nullptr)
    {
        ShaderValidationChannelMask mask{0x00203790, instruction.scalar_mask.channels};
        if (choose_shader_validation_owner_register(context, pass, *instruction.scalar_destination, readers, instruction, mask, 0) < 0)
        {
            goto invalid;
        }
    }
    goto special;
invalid:
    raise_invalid_shader_validation_operand(context.constructors.exceptions);
special:
    if (instruction.output_register != nullptr)
    {
        replace_shader_validation_special_owner(context, pass, *instruction.output_register, deleting_destructor);
    }
    return 1;
}

// Xbox 0x00195D42
ShaderValidationOwner* replace_shader_validation_special_owner(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, ShaderValidationOwner& owner,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    auto* node = find_shader_validation_pass_node(pass.special.tree.nodes, owner.source);
    auto* previous = node != nullptr ? static_cast<ShaderValidationOwner*>(node->data) : nullptr;
    if (previous != nullptr)
    {
        if (shader_validation_owner_derives_from(*previous, &owner))
        {
            return previous;
        }
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (owner.channels[channel] != 0)
            {
                bool read = false;
                for (auto* current = previous; current != nullptr; current = current->derived_owner)
                {
                    if (read || shader_validation_owner_diagnostic_reads_channel(*current, channel, context.sets.operand_at))
                    {
                        read = true;
                    }
                    if (current->channels[channel] != 0)
                    {
                        if (!read && current->channels[channel] != ~ShaderValidationChannelWriter{0})
                        {
                            set_shader_validation_channel_writer(*current, channel, 0);
                        }
                        break;
                    }
                }
            }
        }
    }
    ShaderValidationChannelMask first;
    ShaderValidationChannelMask second;
    construct_shader_validation_channel_mask(second, 0);
    construct_shader_validation_channel_mask(first, 0);
    merge_or_link_shader_validation_owners(context, owner, previous, 0, first, second);
    erase_shader_validation_pass_key(pass.special.tree.nodes, owner.source, deleting_destructor);
    ShaderValidationPassNode* inserted;
    if (owner.source == 0x100)
    {
        set_shader_validation_pass_value(pass.registers.tree.nodes, context.constructors.allocate, &inserted, 12, &owner);
    }
    set_shader_validation_pass_value(pass.special.tree.nodes, context.constructors.allocate, &inserted, owner.source, &owner);
    return &owner;
}

// Xbox 0x0019368D
void split_and_register_shader_validation_scalar_instruction(ShaderValidationConstructorContext& context,
    ShaderValidationProgramPass& pass, ShaderValidationInstruction& instruction,
    const ShaderValidationInstructionAt& instruction_at)
{
    if (instruction.vector_opcode != 0 && instruction.scalar_opcode != 0)
    {
        ShaderValidationInstruction* separated = split_shader_validation_scalar_instruction(context, instruction);
        ShaderValidationInsertContext insert{pass.instruction_set.nodes, context.allocate};
        register_shader_validation_instruction(insert, context.retail_address(separated), instruction_at);
    }
}

// Xbox 0x00193F8A
void split_shader_validation_program_scalar_instructions(ShaderValidationConstructorContext& context,
    ShaderValidationProgramPass& pass, const std::function<ShaderValidationInstruction*(u32)>& instruction_at)
{
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(pass.instruction_set.nodes, &node);
    while (node != nullptr)
    {
        split_and_register_shader_validation_scalar_instruction(context, pass, *instruction_at(node->value), instruction_at);
        advance_shader_validation_arena_iterator(node);
    }
}

// Xbox 0x001914B9
void destroy_shader_validation_pass_tree_object(ShaderValidationPassTreeObject& tree,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor)
{
    tree.vtable_address = 0x002037d8;
    clear_shader_validation_pass_tree(tree, deleting_destructor);
}

namespace
{
ShaderValidationOwner* create_constant_owner_body(ShaderValidationConstructorContext& context, const ShaderValidationPass& pass,
    u32 source, u8 use_pass_owner, i32& state, void*& allocation)
{
    allocation = context.allocate(sizeof(ShaderValidationOwner), 0x24870000);
    state = 0;
    if (allocation == nullptr)
    {
        return nullptr;
    }
    ShaderValidationOwner* parent = use_pass_owner != 0 ? pass.owner : nullptr;
    ShaderValidationChannelMask mask;
    construct_shader_validation_channel_mask(mask, 0xF);
    return construct_shader_validation_owner(context, *static_cast<ShaderValidationOwner*>(allocation), context.retail_address(allocation),
        3, source, parent, 0, mask);
}

void destroy_pass_body(ShaderValidationPassDestroyContext& context, ShaderValidationPass& pass, i32& state)
{
    pass.base.vtable_address = 0x00203818;
    state = 4;
    unregister_shader_validation_value_1926fb(context.expressions, context.retail_address(&pass.values.registered));
    unregister_shader_validation_value_1926fb(context.expressions, context.retail_address(&pass.registers.registered));
    unregister_shader_validation_value_1926fb(context.expressions, context.retail_address(&pass.special.registered));
    state = 3;
    pass.special.tree.vtable_address = 0x002037d8;
    clear_shader_validation_pass_tree(pass.special.tree, context.deleting_destructor);
    state = 2;
    pass.registers.tree.vtable_address = 0x002037d8;
    clear_shader_validation_pass_tree(pass.registers.tree, context.deleting_destructor);
    state = 1;
    pass.values.tree.vtable_address = 0x002037d8;
    clear_shader_validation_pass_tree(pass.values.tree, context.deleting_destructor);
    state = 0;
    destroy_shader_validation_list(pass.instructions);
    state = -1;
    pass.base.vtable_address = 0x00203808;
    unregister_shader_validation_object(context.objects, context.retail_address(&pass));
}
}

// Xbox 0x001935E6
ShaderValidationOwner* create_shader_validation_constant_owner(ShaderValidationConstructorContext& context,
    const ShaderValidationPass& pass, u32 source, u8 use_pass_owner)
{
    i32 state = -1;
    void* allocation = nullptr;
    ShaderValidationOwner* owner = nullptr;
    __try
    {
        owner = create_constant_owner_body(context, pass, source, use_pass_owner, state, allocation);
    }
    __finally
    {
        if (AbnormalTermination() && state == 0)
        {
            // Xbox 0x001709B7: FuncInfo 0x0032B8CC frees the owner allocation.
            release_shader_validation_allocation(allocation, context.release);
        }
    }
    return owner;
}

// Xbox 0x00193566
ShaderValidationOwner* get_shader_validation_input_owner(ShaderValidationConstructorContext& context,
    ShaderValidationPass& pass, u32 source)
{
    ShaderValidationPassNode* existing = find_shader_validation_pass_node(pass.values.tree.nodes, source);
    if (existing != nullptr)
    {
        return static_cast<ShaderValidationOwner*>(existing->data);
    }
    void* allocation = nullptr;
    ShaderValidationOwner* owner = nullptr;
    i32 state = -1;
    __try
    {
        allocation = context.allocate(sizeof(ShaderValidationOwner), 0x24870000);
        state = 0;
        if (allocation != nullptr)
        {
            ShaderValidationChannelMask mask;
            construct_shader_validation_channel_mask(mask, 0xF);
            owner = construct_shader_validation_owner(context, *static_cast<ShaderValidationOwner*>(allocation),
                context.retail_address(allocation), 2, source, nullptr, 0, mask);
        }
        state = -1;
        ShaderValidationPassNode* inserted;
        set_shader_validation_pass_value(pass.values.tree.nodes, context.allocate, &inserted, source, owner);
    }
    __finally
    {
        if (AbnormalTermination() && state == 0)
        {
            // Xbox 0x001709B7: shares FuncInfo 0x0032B8CC with 0x001935E6.
            release_shader_validation_allocation(allocation, context.release);
        }
    }
    return owner;
}

// Xbox 0x00193DF4
ShaderValidationOwner* select_shader_validation_owner(ShaderValidationConstructorContext& context, ShaderValidationPass& pass,
    u32 kind, u32 input_source, u32 constant_source, u32 register_kind, u8 use_pass_owner,
    ShaderValidationChannelMask mask)
{
    ShaderValidationOwner* result = nullptr;
    __try
    {
        if (kind == 1)
        {
            // 0x00191E8F pops an additional by-value mask but never reads it.
            const ShaderValidationChannelMask argument{0x00203790, mask.channels};
            (void)argument;
            result = find_shader_validation_register_owner(pass, register_kind);
        }
        else if (kind == 2)
        {
            result = get_shader_validation_input_owner(context, pass, input_source);
        }
        else if (kind == 3)
        {
            result = create_shader_validation_constant_owner(context, pass, constant_source, use_pass_owner);
        }
        else
        {
            result = nullptr;
        }
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Xbox 0x00170A4D: FuncInfo 0x0032BB74 destroys the by-value mask.
            destroy_shader_validation_channel_mask(mask);
        }
    }
    return result;
}

// Xbox 0x00191EC3
ShaderValidationOwner* set_shader_validation_pass_owner(ShaderValidationPass& pass, ShaderValidationOwner* owner,
    const ShaderValidationSetContext& context)
{
    ShaderValidationChannelMask other_mask;
    construct_shader_validation_channel_mask(other_mask, 0);
    ShaderValidationChannelMask owner_mask;
    construct_shader_validation_channel_mask(owner_mask, 0);
    link_shader_validation_owners(owner, pass.owner, 0, owner_mask, other_mask, context);
    // Retail ignores the link result, including a refused link.
    pass.owner = owner;
    return owner;
}

// Xbox 0x00193E52
ShaderValidationOwner* replace_shader_validation_register_owner(ShaderValidationPass& pass, ShaderValidationOwner& owner,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor,
    const ShaderValidationSetContext& context)
{
    ShaderValidationChannelMask other_mask;
    construct_shader_validation_channel_mask(other_mask, 0);
    ShaderValidationChannelMask owner_mask;
    construct_shader_validation_channel_mask(owner_mask, 0);
    ShaderValidationChannelMask lookup_mask;
    construct_shader_validation_channel_mask(lookup_mask, 0xF);
    (void)lookup_mask; // The extra 0x00191E8F argument is popped without being read.
    ShaderValidationOwner* previous = find_shader_validation_register_owner(pass, owner.source);
    link_shader_validation_owners(&owner, previous, 0, owner_mask, other_mask, context);
    // This path always replaces the +0x28 table, even for source 12 (lookup used +0x3C).
    erase_shader_validation_pass_key(pass.registers.tree.nodes, owner.source, deleting_destructor);
    ShaderValidationPassNode* inserted;
    set_shader_validation_pass_value(pass.registers.tree.nodes, context.allocate, &inserted, owner.source, &owner);
    return &owner;
}

// Xbox 0x00193649
void destroy_shader_validation_program_pass(ShaderValidationPassDestroyContext& context, ShaderValidationProgramPass& pass,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor)
{
    i32 state = -1;
    __try
    {
        pass.base.base.vtable_address = 0x00203820;
        state = 0;
        pass.instruction_set.vtable_address = 0x002037d0;
        clear_shader_validation_balanced_tree_190322(pass.instruction_set, deleting_destructor);
        state = -1;
        destroy_shader_validation_pass(context, pass.base);
    }
    __finally
    {
        if (AbnormalTermination() && state == 0)
        {
            // Xbox 0x001709CB: FuncInfo 0x0032BA58 destroys the base pass.
            destroy_shader_validation_pass(context, pass.base);
        }
    }
}

// Xbox 0x00193EBE
ShaderValidationProgramPass* delete_shader_validation_program_pass(ShaderValidationPassDestroyContext& context,
    ShaderValidationProgramPass* pass, u8 flags,
    const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor,
    const std::function<void(void*, u32)>& release)
{
    // The destructor dereferences pass before the retail post-destruction null test.
    destroy_shader_validation_program_pass(context, *pass, deleting_destructor);
    if ((flags & 1u) != 0 && pass != nullptr)
    {
        release(pass, 0x24870000);
    }
    return pass;
}

// Xbox 0x001934A5
void destroy_shader_validation_pass(ShaderValidationPassDestroyContext& context, ShaderValidationPass& pass)
{
    i32 state = -1;
    __try
    {
        destroy_pass_body(context, pass, state);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // FuncInfo 0x0032BA34 unwinds 4 -> 3 -> 2 -> 1 -> 0 -> -1.
            switch (state)
            {
            case 4:
                // Xbox 0x001709A2
                destroy_shader_validation_pass_tree_object(pass.special.tree, context.deleting_destructor);
                [[fallthrough]];
            case 3:
                // Xbox 0x00170997
                destroy_shader_validation_pass_tree_object(pass.registers.tree, context.deleting_destructor);
                [[fallthrough]];
            case 2:
                // Xbox 0x0017098C
                destroy_shader_validation_pass_tree_object(pass.values.tree, context.deleting_destructor);
                [[fallthrough]];
            case 1:
                // Xbox 0x00170981
                destroy_shader_validation_list(pass.instructions);
                [[fallthrough]];
            case 0:
                // Xbox 0x00170979
                destroy_shader_validation_registered_object(context.objects,
                    pass.base, context.retail_address(&pass));
                break;
            }
        }
    }
}
}
