#include "shader_validation_scheduler.h"
#include "shader_validation_cleanup.h"
#include <excpt.h>

namespace gbtg
{
namespace
{
struct ScheduleLocals
{
    ShaderValidationBalancedChildTree writers;
    ShaderValidationBalancedChildTree deferred;
    ShaderValidationList ready;
    ShaderValidationList blocked;
    ShaderValidationBalancedChildTree readers;
    void* allocation;
};

void release_dependency_set(ShaderValidationDependencyContext& context, ShaderValidationInstruction& instruction)
{
    ShaderValidationEraseContext registry{context.instruction_sets.tree, context.sets.deleting_destructor};
    release_shader_validation_instruction_set(instruction, registry, context.sets.address_of,
        [&](ShaderValidationBalancedChildTree& tree, u32 flags)
        {
            delete_shader_validation_balanced_tree(tree, static_cast<u8>(flags), context.sets.deleting_destructor,
                context.constructors.release);
        });
}

void move_list_node(ShaderValidationScheduleContext& context, ShaderValidationList& source,
    ShaderValidationList& destination, ShaderValidationListNode* node)
{
    append_shader_validation_list_value(destination, context.programs.list_node_vtable,
        context.programs.dependencies.constructors.allocate, node->value);
    erase_shader_validation_list_node(source.storage, node);
}

bool schedule_body(ShaderValidationScheduleContext& schedule, ShaderValidationProgramPass& program,
    ScheduleLocals& locals, i32& state)
{
    auto& context = schedule.programs.dependencies;
    auto& constructors = context.constructors;
    auto& pass = program.base;
    locals.writers.vtable_address = 0x002037D0;
    locals.writers.nodes = {nullptr, 0};
    state = 0;
    locals.deferred.vtable_address = 0x002037D0;
    locals.deferred.nodes = {nullptr, 0};
    locals.ready = ShaderValidationList{0x002037C8, {nullptr, nullptr, 0}};
    locals.blocked = ShaderValidationList{0x002037C8, {nullptr, nullptr, 0}};
    locals.readers.vtable_address = 0x002037B8;
    locals.readers.nodes = {nullptr, 0};
    state = 4;
    clear_shader_validation_pass_tree(pass.special.tree, schedule.programs.deleting_destructor);
    clear_shader_validation_pass_tree(pass.registers.tree, schedule.programs.deleting_destructor);
    for (u32 kind = 0; kind < 13; ++kind)
    {
        locals.allocation = constructors.allocate(sizeof(ShaderValidationOwner), 0x24870000);
        state = 5;
        ShaderValidationOwner* owner;
        if (locals.allocation == nullptr)
        {
            owner = nullptr;
        }
        else
        {
            ShaderValidationChannelMask full;
            construct_shader_validation_channel_mask(full, 0xF);
            owner = construct_shader_validation_owner(constructors, *static_cast<ShaderValidationOwner*>(locals.allocation),
                constructors.retail_address(locals.allocation), 1, kind, nullptr, shader_validation_unwritten_channel, full);
        }
        pass.initial_registers[kind] = owner;
        state = 4;
        ShaderValidationPassNode* inserted;
        set_shader_validation_pass_value(pass.registers.tree.nodes, constructors.allocate, &inserted, kind, owner);
        retire_shader_validation_register(pass, static_cast<i32>(kind), locals.readers.nodes,
            schedule.programs.deleting_destructor, constructors.allocate, context.sets);
    }
    pass.initial_registers[12]->kind = 4;
    pass.initial_registers[12]->source = 0x100;
    ShaderValidationPassNode* inserted;
    set_shader_validation_pass_value(pass.special.tree.nodes, constructors.allocate, &inserted, 0x100, pass.initial_registers[12]);
    pass.owner = nullptr;
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(program.instruction_set.nodes, &node);
    while (node != nullptr)
    {
        release_dependency_set(context, *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(node->value)));
        advance_shader_validation_arena_iterator(node);
    }
    find_shader_validation_tree_last(program.instruction_set.nodes, &node);
    while (node != nullptr)
    {
        build_shader_validation_instruction_dependencies(context, *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(node->value)));
        advance_shader_validation_arena_iterator(node);
    }
    merge_shader_validation_program_instructions(schedule.instructions, program);
    ShaderValidationInsertContext deferred_insert{locals.deferred.nodes, constructors.allocate};
    find_shader_validation_tree_last(program.instruction_set.nodes, &node);
    while (node != nullptr)
    {
        auto* instruction = const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(node->value));
        if (shader_validation_instruction_ready(context, pass, *instruction, locals.writers.nodes, locals.readers.nodes)
            && !shader_validation_instruction_reads_unset_channel(*instruction))
        {
            append_shader_validation_list_value(locals.ready, schedule.programs.list_node_vtable, constructors.allocate, instruction);
        }
        else
        {
            register_shader_validation_instruction(deferred_insert, node->value, context.sets.instruction_at);
        }
        advance_shader_validation_arena_iterator(node);
    }
    bool previous_pair = false;
    bool current_pair = false;
    ShaderValidationInstruction* previous = nullptr;
    bool success = true;
    while (locals.ready.storage.count != 0)
    {
        auto* candidate = locals.ready.storage.first;
        while (candidate != nullptr)
        {
            auto* instruction = static_cast<ShaderValidationInstruction*>(candidate->value);
            if (!shader_validation_instruction_registers_available(context, pass, *instruction, locals.readers.nodes, nullptr))
            {
                auto* next = candidate->next;
                move_list_node(schedule, locals.ready, locals.blocked, candidate);
                candidate = next;
            }
            else
            {
                candidate = candidate->next;
            }
        }
        if (locals.ready.storage.count == 0)
        {
            candidate = locals.blocked.storage.first;
            if (candidate == nullptr)
            {
                success = false;
                break;
            }
            do
            {
                auto* instruction = static_cast<ShaderValidationInstruction*>(candidate->value);
                if (shader_validation_instruction_registers_available(context, pass, *instruction, locals.readers.nodes, nullptr))
                {
                    auto* next = candidate->next;
                    move_list_node(schedule, locals.blocked, locals.ready, candidate);
                    candidate = next;
                }
                else
                {
                    candidate = candidate->next;
                }
            } while (candidate != nullptr);
            if (locals.ready.storage.count == 0)
            {
                success = false;
                break;
            }
        }
        candidate = locals.ready.storage.first;
        ShaderValidationInstruction* selected;
        bool available;
        do
        {
            selected = static_cast<ShaderValidationInstruction*>(select_shader_validation_list_priority(context, *candidate, nullptr));
            auto* next = candidate->next;
            if (schedule.conservative == 0)
            {
                convert_shader_validation_vector_move(*selected, 1);
                for (auto* current = next; current != nullptr; current = current->next)
                {
                    convert_shader_validation_vector_move(*static_cast<ShaderValidationInstruction*>(current->value), 1);
                }
                merge_shader_validation_list_candidates(schedule.instructions, *selected, locals.ready.storage, next, pass, locals.readers.nodes);
                next = candidate->next;
                convert_shader_validation_scalar_move(*selected);
                for (auto* current = next; current != nullptr; current = current->next)
                {
                    convert_shader_validation_scalar_move(*static_cast<ShaderValidationInstruction*>(current->value));
                }
                merge_shader_validation_list_candidates(schedule.instructions, *selected, locals.ready.storage, next, pass, locals.readers.nodes);
                next = candidate->next;
                if (convert_shader_validation_vector_move(*selected, 1))
                {
                    merge_shader_validation_list_candidates(schedule.instructions, *selected, locals.ready.storage, next, pass, locals.readers.nodes);
                }
            }
            available = shader_validation_instruction_registers_available(context, pass, *selected, locals.readers.nodes, nullptr);
            if (!available)
            {
                candidate = candidate->next;
            }
            if (candidate == nullptr)
            {
                break;
            }
        } while (!available);
        if (candidate == nullptr)
        {
            while (locals.blocked.storage.count != 0)
            {
                move_list_node(schedule, locals.blocked, locals.ready, locals.blocked.storage.last);
            }
            success = false;
            break;
        }
        erase_shader_validation_list_node(locals.ready.storage, candidate);
        ShaderValidationInsertContext writers_insert{locals.writers.nodes, constructors.allocate};
        register_shader_validation_instruction(writers_insert, context.sets.address_of(selected), context.sets.instruction_at);
        ShaderValidationInsertContext readers_insert{locals.readers.nodes, constructors.allocate};
        for (auto* operand : selected->operands)
        {
            register_shader_validation_diagnostic(readers_insert, context.sets.address_of(operand));
            if (operand->parent != nullptr && operand->parent->operand != nullptr)
            {
                register_shader_validation_diagnostic(readers_insert, context.sets.address_of(operand->parent->operand));
            }
        }
        assign_shader_validation_instruction_registers(context, *selected, pass, locals.readers.nodes, schedule.programs.deleting_destructor);
        append_shader_validation_list_value(pass.instructions, schedule.programs.list_node_vtable, constructors.allocate, selected);
        candidate = locals.blocked.storage.first;
        while (candidate != nullptr)
        {
            auto* instruction = static_cast<ShaderValidationInstruction*>(candidate->value);
            if (locals.writers.nodes.count < 2
                || (previous_pair && shader_validation_instruction_depends_on(*instruction, *previous, constructors.active_owner)))
            {
                candidate = candidate->next;
            }
            else
            {
                auto* next = candidate->next;
                move_list_node(schedule, locals.blocked, locals.ready, candidate);
                candidate = next;
            }
        }
        find_shader_validation_tree_last(locals.deferred.nodes, &node);
        ShaderValidationEraseContext deferred_erase{locals.deferred.nodes, context.sets.deleting_destructor};
        while (node != nullptr)
        {
            auto* instruction = const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(node->value));
            if (!shader_validation_instruction_ready(context, pass, *instruction, locals.writers.nodes, locals.readers.nodes))
            {
                advance_shader_validation_arena_iterator(node);
            }
            else
            {
                bool blocked = shader_validation_instruction_must_follow(*instruction, *selected, constructors.active_owner);
                if (!blocked && current_pair)
                {
                    blocked = shader_validation_instruction_depends_on(*instruction, *selected, constructors.active_owner);
                }
                if (locals.writers.nodes.count < 2)
                {
                    if (!blocked && !shader_validation_instruction_reads_unset_channel(*instruction))
                    {
                        blocked = shader_validation_instruction_depends_on(*instruction, *selected, constructors.active_owner);
                    }
                }
                else if (!blocked && previous_pair)
                {
                    blocked = shader_validation_instruction_depends_on(*instruction, *previous, constructors.active_owner);
                }
                append_shader_validation_list_value(blocked ? locals.blocked : locals.ready,
                    schedule.programs.list_node_vtable, constructors.allocate, instruction);
                node = erase_shader_validation_balanced_node_19234a(deferred_erase, node);
            }
        }
        bool next_pair = false;
        if (locals.ready.storage.count == 0)
        {
            if (locals.writers.nodes.count >= 2)
            {
                candidate = locals.blocked.storage.first;
                next_pair = true;
                while (candidate != nullptr)
                {
                    auto* instruction = static_cast<ShaderValidationInstruction*>(candidate->value);
                    if (shader_validation_instructions_can_pair(*instruction, *selected))
                    {
                        auto* next = candidate->next;
                        move_list_node(schedule, locals.blocked, locals.ready, candidate);
                        candidate = next;
                    }
                    else
                    {
                        candidate = candidate->next;
                    }
                }
            }
            if (locals.ready.storage.count == 0 && locals.blocked.storage.count != 0)
            {
                next_pair = true;
                if (current_pair || locals.writers.nodes.count < 2)
                {
                    auto* nop = create_shader_validation_instruction(constructors);
                    append_shader_validation_list_value(pass.instructions, schedule.programs.list_node_vtable, constructors.allocate, nop);
                    next_pair = false;
                }
                do
                {
                    move_list_node(schedule, locals.blocked, locals.ready, locals.blocked.storage.last);
                } while (locals.blocked.storage.count != 0);
            }
        }
        previous = selected;
        previous_pair = current_pair;
        current_pair = next_pair;
    }
    if (success && locals.deferred.nodes.count != 0)
    {
        // Retail tail-jumps to the noncontinuable C++ exception dispatcher.
        raise_invalid_shader_validation_operand(constructors.exceptions);
#if defined(_MSC_VER)
        __assume(0);
#else
        __builtin_unreachable();
#endif
    }
    if (success)
    {
        find_shader_validation_tree_last(locals.writers.nodes, &node);
        while (node != nullptr)
        {
            release_dependency_set(context, *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(node->value)));
            advance_shader_validation_arena_iterator(node);
        }
    }
    state = 3;
    locals.readers.vtable_address = 0x002037B8;
    clear_shader_validation_child_tree(locals.readers, context.sets.deleting_destructor);
    state = 2;
    destroy_shader_validation_list(locals.blocked);
    state = 1;
    destroy_shader_validation_list(locals.ready);
    state = 0;
    locals.deferred.vtable_address = 0x002037D0;
    destroy_shader_validation_balanced_nodes_18f2aa(locals.deferred.nodes.root, context.sets.deleting_destructor);
    state = -1;
    locals.deferred.nodes = {nullptr, 0};
    locals.writers.vtable_address = 0x002037D0;
    destroy_shader_validation_balanced_nodes_18f2aa(locals.writers.nodes.root, context.sets.deleting_destructor);
    return success;
}
}

// Xbox 0x001973F1
bool schedule_shader_validation_program(ShaderValidationScheduleContext& context, ShaderValidationProgramPass& pass)
{
    ScheduleLocals locals;
    i32 state = -1;
    bool result;
    __try
    {
        result = schedule_body(context, pass, locals, state);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            if (state == 5)
            {
                release_shader_validation_allocation(locals.allocation, context.programs.dependencies.constructors.release);
            }
            if (state >= 4)
            {
                locals.readers.vtable_address = 0x002037B8;
                clear_shader_validation_child_tree(locals.readers, context.programs.dependencies.sets.deleting_destructor);
            }
            if (state >= 3)
            {
                destroy_shader_validation_list(locals.blocked);
            }
            if (state >= 2)
            {
                destroy_shader_validation_list(locals.ready);
            }
            if (state >= 1)
            {
                locals.deferred.vtable_address = 0x002037D0;
                clear_shader_validation_child_tree(locals.deferred, context.programs.dependencies.sets.deleting_destructor);
            }
            if (state >= 0)
            {
                locals.writers.vtable_address = 0x002037D0;
                clear_shader_validation_child_tree(locals.writers, context.programs.dependencies.sets.deleting_destructor);
            }
        }
    }
    return result;
}

// Xbox 0x00197A94
bool schedule_and_serialize_shader_validation_program(ShaderValidationScheduleContext& context,
    ShaderValidationProgramPass& pass, u32* packed_program)
{
    if (!schedule_shader_validation_program(context, pass))
    {
        return false;
    }
    return serialize_shader_validation_program(pass.base.instructions.storage.first, packed_program,
        context.serializer_frame, context.programs.dependencies.constructors.active_owner);
}
}
