#include "shader_validation_optimizer.h"
#include "shader_validation_destinations.h"
#include "shader_validation_cleanup.h"
#include "shader_validation_operand_tree.h"
#include <excpt.h>

namespace gbtg
{
namespace
{
struct OptimizationLocals
{
    ShaderValidationChannelOrder source_order;
    ShaderValidationChannelOrder inverse_order;
    ShaderValidationChannelOrder composed_order;
    ShaderValidationChannelMask mask;
    ShaderValidationChannelMask remaining;
    ShaderValidationChannelMask vector_intersection;
};

void enqueue_owner(ShaderValidationDependencyContext& context, ShaderValidationBalancedChildTree& owners,
    ShaderValidationOwner* owner)
{
    ShaderValidationInsertContext insert{owners.nodes, context.constructors.allocate};
    register_shader_validation_value_2037f4(insert, context.sets.address_of(owner));
}

void enqueue_source_writers(ShaderValidationDependencyContext& context, ShaderValidationBalancedChildTree& owners,
    ShaderValidationBalancedChildTree& instructions, ShaderValidationOwner& source, ShaderValidationOperand& operand)
{
    enqueue_owner(context, owners, &source);
    ShaderValidationInsertContext insert{instructions.nodes, context.constructors.allocate};
    for (const auto writer : source.channels)
    {
        if (writer != 0 && writer != shader_validation_unwritten_channel)
        {
            register_shader_validation_instruction(insert, context.sets.address_of(reinterpret_cast<void*>(writer)), context.sets.instruction_at);
        }
    }
    clear_unused_shader_validation_operand_channels(context.constructors, operand);
}

bool clear_shadowed_output(ShaderValidationDependencyContext& context, ShaderValidationOwner& output,
    ShaderValidationInstruction& instruction)
{
    bool changed = false;
    for (u32 channel = 0; channel < 4; ++channel)
    {
        if (output.channels[channel] == reinterpret_cast<ShaderValidationChannelWriter>(&instruction))
        {
            if (!shader_validation_owner_channel_has_reader(output, channel, context.sets.operand_at))
            {
                if (shader_validation_channel_owner_writes_channel(output, channel))
                {
                    set_shader_validation_channel_writer(output, channel, 0);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

void redirect_move_readers(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner,
    ShaderValidationOwner& source, ShaderValidationOperand& move_operand, OptimizationLocals& locals,
    i32& state, i32 composed_state, i32 mask_state)
{
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
    ShaderValidationEraseContext erase{owner.diagnostics.nodes, context.sets.deleting_destructor};
    while (node != nullptr)
    {
        auto& operand = *context.sets.operand_at(node->value);
        ShaderValidationChannelOrder order;
        copy_shader_validation_operand_order(order, operand);
        ShaderValidationChannelOrder argument{0x00203794, locals.source_order.channels};
        compose_shader_validation_channel_orders(order, argument, locals.composed_order);
        state = composed_state;
        destroy_shader_validation_channel_order(order);
        ShaderValidationChannelMask inverse;
        invert_shader_validation_channel_mask(locals.mask, inverse);
        bool redirected = false;
        if (!shader_validation_operand_reads_masked_channel(operand, inverse))
        {
            ShaderValidationChannelMask used;
            shader_validation_channel_order_used_mask(locals.composed_order, used);
            if (shader_validation_operand_accepts_parent(context, operand, source, used))
            {
                node = erase_shader_validation_balanced_node_19214a(erase, node);
                reparent_shader_validation_operand(context.constructors, operand, &source);
                ShaderValidationChannelOrder result;
                ShaderValidationChannelOrder replacement{0x00203794, locals.composed_order.channels};
                replace_shader_validation_operand_channel_order(result, operand, replacement);
                destroy_shader_validation_channel_order(result);
                if (move_operand.marker != 0)
                {
                    operand.marker = operand.marker == 0;
                }
                redirected = true;
            }
        }
        if (!redirected)
        {
            advance_shader_validation_arena_iterator(node);
        }
        state = mask_state;
        destroy_shader_validation_channel_order(locals.composed_order);
    }
}

void normalize_instruction_body(ShaderValidationOptimizationContext& optimization, ShaderValidationProgramPass& pass,
    ShaderValidationInstruction* instruction, ShaderValidationBalancedChildTree& owners,
    ShaderValidationBalancedChildTree& instructions, OptimizationLocals& locals, i32& state)
{
    auto& context = optimization.owners.instructions.dependencies;
    if (instruction == nullptr || reinterpret_cast<std::uintptr_t>(instruction) == shader_validation_unwritten_channel)
    {
        return;
    }
    if (shader_validation_instruction_destinations_unused(*instruction, context.sets.operand_at))
    {
        for (auto* operand : instruction->operands)
        {
            if (operand->parent != nullptr)
            {
                enqueue_owner(context, owners, operand->parent);
            }
        }
    }
    else
    {
        if (clear_unused_shader_validation_instruction_channels(context.constructors, *instruction))
        {
            for (auto* operand : instruction->operands)
            {
                if (operand->parent != nullptr)
                {
                    enqueue_owner(context, owners, operand->parent);
                }
            }
        }
        auto* output = instruction->output_register;
        auto* vector = instruction->vector_destination;
        if (!shader_validation_instruction_is_single_move(*instruction))
        {
            return;
        }
        const bool rewritten = rewrite_shader_validation_move(context, *instruction, optimization.move_frame);
        auto& operand = *instruction->operands[0];
        auto& source = *operand.parent;
        copy_shader_validation_operand_order(locals.source_order, operand);
        state = 0;
        ShaderValidationChannelOrder order;
        copy_shader_validation_operand_order(order, operand);
        invert_shader_validation_channel_order(order, locals.inverse_order);
        state = 1;
        destroy_shader_validation_channel_order(order);
        if (find_shader_validation_foreign_parent(source, context.constructors.active_owner) == nullptr)
        {
            bool replaced_output = false;
            if (output != nullptr && instruction->output_flag == 0)
            {
                replaced_output = rewritten;
                if (rewritten)
                {
                    std::array<ShaderValidationChannelWriter, 4> seen{};
                    for (u32 channel = 0; channel < 4; ++channel)
                    {
                        ShaderValidationChannelMask selected{0x00203790, instruction->output_mask.channels};
                        destroy_shader_validation_channel_mask(selected);
                        if (selected.channels[channel] != 0)
                        {
                            const auto writer = source.channels[channel];
                            bool duplicate = false;
                            for (const auto prior : seen)
                            {
                                if (prior == writer)
                                {
                                    duplicate = true;
                                }
                            }
                            seen[channel] = writer;
                            if (!duplicate && writer != 0 && writer != shader_validation_unwritten_channel)
                            {
                                auto& producer = *reinterpret_cast<ShaderValidationInstruction*>(writer);
                                if (producer.output_register == nullptr)
                                {
                                    const bool vector_source = producer.vector_destination == &source;
                                    producer.output_flag = vector_source ? 0 : 1;
                                    ShaderValidationChannelMask argument{0x00203790,
                                        vector_source ? producer.vector_mask.channels : producer.scalar_mask.channels};
                                    auto& protected_mask = vector_source ? locals.vector_intersection : locals.remaining;
                                    protected_mask = ShaderValidationChannelMask{0x00203790, instruction->output_mask.channels};
                                    state = vector_source ? 2 : 3;
                                    ShaderValidationChannelMask intersection;
                                    intersect_shader_validation_channel_masks(protected_mask, argument, intersection);
                                    replace_shader_validation_output_destination(producer, output, intersection);
                                    destroy_shader_validation_channel_mask(protected_mask);
                                    state = 1;
                                    auto* destination = vector_source ? producer.vector_destination : producer.scalar_destination;
                                    if (destination != nullptr)
                                    {
                                        enqueue_owner(context, owners, destination);
                                    }
                                }
                                else
                                {
                                    replaced_output = false;
                                }
                            }
                        }
                    }
                }
                if (!replaced_output && output->source == 0x100)
                {
                    if (clear_shadowed_output(context, *output, *instruction))
                    {
                        enqueue_source_writers(context, owners, instructions, source, operand);
                    }
                    locals.mask = ShaderValidationChannelMask{0x00203790, instruction->output_mask.channels};
                    state = 4;
                    redirect_move_readers(context, *output, source, operand, locals, state, 5, 4);
                    enqueue_owner(context, owners, &source);
                    if (clear_shadowed_output(context, *output, *instruction))
                    {
                        enqueue_source_writers(context, owners, instructions, source, operand);
                    }
                    state = 1;
                }
            }
            if (vector != nullptr)
            {
                locals.mask = ShaderValidationChannelMask{0x00203790, instruction->vector_mask.channels};
                state = 6;
                bool redirect_writers = rewritten && (source.kind == 1 || source.kind == 6);
                if (redirect_writers)
                {
                    auto* descendant = vector;
                    do
                    {
                        descendant = descendant->derived_owner;
                        if (descendant == nullptr)
                        {
                            break;
                        }
                        for (u32 channel = 0; channel < 4; ++channel)
                        {
                            ShaderValidationChannelOrder current_order;
                            copy_shader_validation_operand_order(current_order, operand);
                            const bool used = shader_validation_channel_order_contains(current_order, channel);
                            destroy_shader_validation_channel_order(current_order);
                            if (used)
                            {
                                const auto writer = source.channels[channel];
                                if (writer != 0 && writer != shader_validation_unwritten_channel)
                                {
                                    auto& producer = *reinterpret_cast<ShaderValidationInstruction*>(writer);
                                    for (const auto other : descendant->channels)
                                    {
                                        if (other != 0 && other != shader_validation_unwritten_channel)
                                        {
                                            if (shader_validation_instruction_has_dependency(context, producer,
                                                context.sets.address_of(reinterpret_cast<void*>(other))))
                                            {
                                                redirect_writers = false;
                                            }
                                        }
                                    }
                                    ShaderValidationBalancedNode* node;
                                    find_shader_validation_tree_last(descendant->diagnostics.nodes, &node);
                                    while (node != nullptr)
                                    {
                                        auto* reader = context.sets.operand_at(node->value)->instruction;
                                        if (reader != nullptr)
                                        {
                                            if (shader_validation_instruction_has_dependency(context, producer, context.sets.address_of(reader)))
                                            {
                                                redirect_writers = false;
                                            }
                                        }
                                        advance_shader_validation_arena_iterator(node);
                                    }
                                }
                            }
                        }
                    } while (redirect_writers);
                    if (redirect_writers)
                    {
                        for (u32 channel = 0; channel < 4; ++channel)
                        {
                            if (locals.mask.channels[channel] != 0)
                            {
                                const auto writer = source.channels[channel];
                                if (writer == 0 || writer == shader_validation_unwritten_channel)
                                {
                                    redirect_writers = false;
                                }
                                else
                                {
                                    for (u32 other = 0; other < 4; ++other)
                                    {
                                        if (locals.mask.channels[other] == 0 && source.channels[other] == writer)
                                        {
                                            redirect_writers = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                locals.remaining = ShaderValidationChannelMask{0x00203790, locals.mask.channels};
                state = 7;
                auto* current = &source;
                while (pack_shader_validation_channel_mask(locals.remaining) != 0)
                {
                    ShaderValidationBalancedNode* node;
                    find_shader_validation_tree_last(current->diagnostics.nodes, &node);
                    ShaderValidationEraseContext erase{current->diagnostics.nodes, context.sets.deleting_destructor};
                    if (redirect_writers)
                    {
                        while (node != nullptr)
                        {
                            auto& reader = *context.sets.operand_at(node->value);
                            if (reader.instruction == instruction || !shader_validation_operand_reads_masked_channel(reader, locals.remaining))
                            {
                                advance_shader_validation_arena_iterator(node);
                            }
                            else
                            {
                                ShaderValidationChannelMask inverse;
                                invert_shader_validation_channel_mask(locals.remaining, inverse);
                                if (shader_validation_operand_reads_masked_channel(reader, inverse))
                                {
                                    redirect_writers = false;
                                    break;
                                }
                                ShaderValidationChannelMask full;
                                construct_shader_validation_channel_mask(full, 0xF);
                                if (!shader_validation_operand_accepts_parent(context, reader, *vector, full))
                                {
                                    redirect_writers = false;
                                    break;
                                }
                                node = erase_shader_validation_balanced_node_19214a(erase, node);
                                reparent_shader_validation_operand(context.constructors, reader, vector);
                            }
                        }
                    }
                    current = current->channel_owner;
                    if (current == nullptr)
                    {
                        ShaderValidationChannelMask empty;
                        construct_shader_validation_channel_mask(empty, 0);
                        copy_shader_validation_channel_mask(locals.remaining, empty);
                        destroy_shader_validation_channel_mask(empty);
                    }
                    else
                    {
                        for (u32 channel = 0; channel < 4; ++channel)
                        {
                            if (current->channels[channel] != 0)
                            {
                                locals.remaining.channels[channel] = 0;
                            }
                        }
                    }
                }
                if (!redirect_writers)
                {
                    redirect_move_readers(context, *vector, source, operand, locals, state, 8, 7);
                }
                else
                {
                    for (u32 channel = 0; channel < 4; ++channel)
                    {
                        const auto writer = source.channels[channel];
                        if (locals.mask.channels[channel] != 0 && writer != 0 && writer != shader_validation_unwritten_channel)
                        {
                            auto& producer = *reinterpret_cast<ShaderValidationInstruction*>(writer);
                            if (producer.vector_destination == &source)
                            {
                                set_shader_validation_vector_destination(producer, vector);
                            }
                            if (producer.scalar_destination == &source)
                            {
                                set_shader_validation_scalar_destination(producer, vector);
                            }
                        }
                    }
                }
                enqueue_owner(context, owners, &source);
                state = 1;
            }
        }
        bool changed = false;
        if (vector != nullptr)
        {
            for (u32 channel = 0; channel < 4; ++channel)
            {
                if (vector->channels[channel] == reinterpret_cast<ShaderValidationChannelWriter>(instruction))
                {
                    if (!shader_validation_owner_channel_has_reader(*vector, channel, context.sets.operand_at))
                    {
                        set_shader_validation_channel_writer(*vector, channel, 0);
                        changed = true;
                    }
                }
            }
            if (changed)
            {
                enqueue_source_writers(context, owners, instructions, source, operand);
            }
        }
        if (!shader_validation_instruction_destinations_unused(*instruction, context.sets.operand_at))
        {
            return;
        }
    }
    ShaderValidationEraseContext erase{pass.instruction_set.nodes, context.sets.deleting_destructor};
    unregister_shader_validation_instruction_dependency(erase, context.sets.address_of(instruction), context.sets.instruction_at);
    delete_shader_validation_instruction(optimization.owners.instructions, instruction, 1);
}
}

// Xbox 0x001943CD
void normalize_shader_validation_instruction(ShaderValidationOptimizationContext& context, ShaderValidationProgramPass& pass,
    ShaderValidationInstruction* instruction, ShaderValidationBalancedChildTree& owners, ShaderValidationBalancedChildTree& instructions)
{
    OptimizationLocals locals;
    i32 state = -1;
    __try
    {
        normalize_instruction_body(context, pass, instruction, owners, instructions, locals, state);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            if (state == 5 || state == 8)
            {
                destroy_shader_validation_channel_order(locals.composed_order);
                state = state == 5 ? 4 : 7;
            }
            if (state == 2)
            {
                destroy_shader_validation_channel_mask(locals.vector_intersection);
            }
            if (state == 3 || state == 7)
            {
                destroy_shader_validation_channel_mask(locals.remaining);
            }
            if (state == 4 || state == 6 || state == 7)
            {
                destroy_shader_validation_channel_mask(locals.mask);
            }
            if (state >= 1)
            {
                destroy_shader_validation_channel_order(locals.inverse_order);
            }
            if (state >= 0)
            {
                destroy_shader_validation_channel_order(locals.source_order);
            }
        }
    }
}

// Xbox 0x00195B89
void normalize_shader_validation_instruction_worklist(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass, ShaderValidationBalancedChildTree& owners, ShaderValidationBalancedChildTree& instructions)
{
    auto& dependencies = context.owners.instructions.dependencies;
    merge_shader_validation_program_moves(context.owners.instructions, pass);
    ShaderValidationEraseContext erase{instructions.nodes, dependencies.sets.deleting_destructor};
    while (instructions.nodes.count != 0)
    {
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(instructions.nodes, &node);
        const u32 address = node->value;
        erase_shader_validation_balanced_node_19234a(erase, node);
        if (find_shader_validation_instruction_node(pass.instruction_set.nodes, address, dependencies.sets.instruction_at) != nullptr)
        {
            normalize_shader_validation_instruction(context, pass,
                const_cast<ShaderValidationInstruction*>(dependencies.sets.instruction_at(address)), owners, instructions);
        }
    }
}

// Xbox 0x00195E43
void clean_shader_validation_owner_worklist(ShaderValidationOptimizationContext& optimization,
    ShaderValidationProgramPass& pass, ShaderValidationBalancedChildTree& owners, ShaderValidationBalancedChildTree& instructions)
{
    auto& context = optimization.owners.instructions.dependencies;
    ShaderValidationEraseContext erase_owners{owners.nodes, context.sets.deleting_destructor};
    ShaderValidationEraseContext erase_instructions{instructions.nodes, context.sets.deleting_destructor};
    ShaderValidationEraseContext erase_program{pass.instruction_set.nodes, context.sets.deleting_destructor};
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(owners.nodes, &node);
    while (owners.nodes.count != 0)
    {
        auto* owner = optimization.owner_at(node->value);
        erase_shader_validation_balanced_node_192529(erase_owners, node);
        if (owner != context.constructors.active_owner)
        {
            while (owner->channel_owner != nullptr)
            {
                owner = owner->channel_owner;
            }
            move_shader_validation_readers_to_derived(context, *owner, 1);
            do
            {
                unregister_shader_validation_owner_expression(erase_owners, context.sets.address_of(owner));
                move_shader_validation_readers_to_derived(context, *owner, 1);
                merge_shader_validation_duplicate_channels(*owner, context.sets.operand_at);
                bool has_readers = false;
                for (u32 channel = 0; channel < 4; ++channel)
                {
                    const auto writer = owner->channels[channel];
                    if (writer != 0 || owner->kind == 2 || owner->kind == 3)
                    {
                        if (shader_validation_owner_channel_has_reader(*owner, channel, context.sets.operand_at))
                        {
                            has_readers = true;
                        }
                        else if (writer != 0 && writer != shader_validation_unwritten_channel)
                        {
                            auto& instruction = *reinterpret_cast<ShaderValidationInstruction*>(writer);
                            for (auto* operand : instruction.operands)
                            {
                                if (operand->parent != nullptr)
                                {
                                    enqueue_owner(context, owners, operand->parent);
                                }
                            }
                            remove_shader_validation_destination_channel(instruction, channel, owner);
                            clear_unused_shader_validation_instruction_channels(context.constructors, instruction);
                            if (shader_validation_instruction_destinations_unused(instruction, context.sets.operand_at))
                            {
                                const u32 address = context.sets.address_of(&instruction);
                                unregister_shader_validation_instruction_dependency(erase_instructions, address, context.sets.instruction_at);
                                unregister_shader_validation_instruction_dependency(erase_program, address, context.sets.instruction_at);
                                delete_shader_validation_instruction(optimization.owners.instructions, &instruction, 1);
                            }
                        }
                    }
                }
                auto* derived = owner->derived_owner;
                detach_unused_shader_validation_derived_owner(context, *owner);
                if (!has_readers)
                {
                    if (find_shader_validation_foreign_parent(*owner, context.constructors.active_owner) != nullptr)
                    {
                        enqueue_owner(context, owners, find_shader_validation_foreign_parent(*owner, context.constructors.active_owner));
                    }
                    delete_shader_validation_owner(optimization.owners, owner, 1);
                }
                owner = derived;
            } while (owner != nullptr);
        }
        find_shader_validation_tree_last(owners.nodes, &node);
    }
}

namespace
{
void optimize_worklists_body(ShaderValidationOptimizationContext& outer, ShaderValidationProgramPass& pass,
    ShaderValidationBalancedChildTree& instructions, ShaderValidationBalancedChildTree& owners, i32& state)
{
    auto& dependencies = outer.owners.instructions.dependencies;
    auto& constructors = dependencies.constructors;
    instructions.vtable_address = 0x002037D0;
    instructions.nodes.root = nullptr;
    instructions.nodes.count = 0;
    state = 0;
    owners.vtable_address = 0x0020379C;
    owners.nodes.root = nullptr;
    owners.nodes.count = 0;
    state = 1;
    const u32 instruction_address = dependencies.sets.address_of(&instructions);
    const u32 owner_address = dependencies.sets.address_of(&owners);
    register_shader_validation_value_2037ec(dependencies.instruction_sets, instruction_address);
    register_shader_validation_operand(outer.expressions, owner_address);
    ShaderValidationInstructionDestroyContext instruction_destroy{dependencies,
        [&](u32 address) -> ShaderValidationBalancedChildTree*
        {
            if (address == instruction_address)
            {
                return &instructions;
            }
            return outer.owners.instructions.set_at(address);
        }};
    ShaderValidationOwnerDestroyContext owner_destroy{instruction_destroy, outer.owners.expressions,
        [&](u32 address, ShaderValidationOwner& owner)
        {
            if (address == owner_address)
            {
                ShaderValidationEraseContext erase{owners.nodes, dependencies.sets.deleting_destructor};
                unregister_shader_validation_owner_expression(erase, dependencies.sets.address_of(&owner));
            }
            else
            {
                outer.owners.unregister_owner(address, owner);
            }
        }};
    ShaderValidationOptimizationContext context{owner_destroy, outer.expressions, outer.move_frame, outer.owner_at};
    ShaderValidationInsertContext insert{instructions.nodes, constructors.allocate};
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(pass.instruction_set.nodes, &node);
    while (node != nullptr)
    {
        auto& instruction = *dependencies.sets.instruction_at(node->value);
        register_shader_validation_instruction(insert, node->value, dependencies.sets.instruction_at);
        for (auto* destination : {instruction.output_register, instruction.vector_destination, instruction.scalar_destination})
        {
            if (destination != nullptr)
            {
                enqueue_owner(dependencies, owners, destination);
            }
        }
        advance_shader_validation_arena_iterator(node);
    }
    while (owners.nodes.count != 0 || instructions.nodes.count != 0)
    {
        clean_shader_validation_owner_worklist(context, pass, owners, instructions);
        normalize_shader_validation_instruction_worklist(context, pass, owners, instructions);
    }
    ShaderValidationEraseContext instruction_registry{dependencies.instruction_sets.tree, dependencies.sets.deleting_destructor};
    erase_shader_validation_balanced_node_19224a(instruction_registry,
        find_shader_validation_pass_node(instruction_registry.tree, instruction_address));
    ShaderValidationEraseContext owner_registry{outer.expressions.tree, dependencies.sets.deleting_destructor};
    erase_shader_validation_balanced_node_19204a(owner_registry,
        find_shader_validation_pass_node(owner_registry.tree, owner_address));
    state = 0;
    owners.vtable_address = 0x0020379C;
    destroy_shader_validation_balanced_nodes_18f2d3(owners.nodes.root, dependencies.sets.deleting_destructor);
    state = -1;
    owners.nodes.root = nullptr;
    owners.nodes.count = 0;
    instructions.vtable_address = 0x002037D0;
    destroy_shader_validation_balanced_nodes_18f2aa(instructions.nodes.root, dependencies.sets.deleting_destructor);
}
}

// Xbox 0x00196B22
void optimize_shader_validation_program_worklists(ShaderValidationOptimizationContext& context, ShaderValidationProgramPass& pass)
{
    ShaderValidationBalancedChildTree instructions;
    ShaderValidationBalancedChildTree owners;
    i32 state = -1;
    __try
    {
        optimize_worklists_body(context, pass, instructions, owners, state);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            if (state == 1)
            {
                owners.vtable_address = 0x0020379C;
                clear_shader_validation_child_tree(owners, context.owners.instructions.dependencies.sets.deleting_destructor);
            }
            if (state >= 0)
            {
                instructions.vtable_address = 0x002037D0;
                clear_shader_validation_child_tree(instructions, context.owners.instructions.dependencies.sets.deleting_destructor);
            }
        }
    }
}

// Xbox 0x00197223
void optimize_shader_validation_program(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass, u8 split_scalar_instructions)
{
    auto& dependencies = context.owners.instructions.dependencies;
    if (split_scalar_instructions != 0)
    {
        split_shader_validation_program_scalar_instructions(dependencies.constructors, pass,
            [&](u32 address)
            {
                return const_cast<ShaderValidationInstruction*>(dependencies.sets.instruction_at(address));
            });
    }
    optimize_shader_validation_program_worklists(context, pass);
}
}
