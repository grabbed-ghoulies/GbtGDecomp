#include "shader_validation_program.h"
#include "shader_validation_setup.h"
#include <excpt.h>

namespace gbtg
{
// Xbox 0x00190C93
void reset_shader_validation_tree_storage(ShaderValidationBalancedTree& objects)
{
    objects.root = nullptr;
    objects.count = 0;
    destroy_shader_validation_tree(nullptr);
}

// Xbox 0x00190CC1
void release_shader_validation_storage(ShaderValidationProgramContext& context)
{
    auto& dependencies = context.schedule.programs.dependencies;
    auto& expressions = context.optimization.expressions.tree;
    destroy_shader_validation_child_nodes(expressions.root, dependencies.sets.deleting_destructor);
    expressions.root = nullptr;
    expressions.count = 0;
    auto& instructions = dependencies.instruction_sets.tree;
    destroy_shader_validation_child_nodes(instructions.root, dependencies.sets.deleting_destructor);
    instructions.root = nullptr;
    instructions.count = 0;
    auto& objects = dependencies.constructors.objects.tree;
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(objects, &node);
    while (objects.count != 0)
    {
        const u32 object = node->value;
        if (object != 0)
        {
            context.delete_registered_object(object, 1);
        }
        find_shader_validation_tree_last(objects, &node);
    }
}

namespace
{
ShaderValidationProgramPass* create_program(ShaderValidationProgramContext& context, u32* packed_program,
    i32 owner_state, i32 program_state, i32& state, void*& allocation)
{
    auto& constructors = context.schedule.programs.dependencies.constructors;
    allocation = constructors.allocate(sizeof(ShaderValidationOwner), 0x24870000);
    state = owner_state;
    if (allocation == nullptr)
    {
        constructors.active_owner = nullptr;
    }
    else
    {
        ShaderValidationChannelMask full;
        construct_shader_validation_channel_mask(full, 0xF);
        constructors.active_owner = construct_shader_validation_owner(constructors,
            *static_cast<ShaderValidationOwner*>(allocation), constructors.retail_address(allocation), 0, 0, nullptr, 0, full);
    }
    allocation = constructors.allocate(sizeof(ShaderValidationProgramPass), 0x24870000);
    state = program_state;
    ShaderValidationProgramPass* program;
    if (allocation == nullptr)
    {
        program = nullptr;
    }
    else
    {
        program = construct_shader_validation_program_pass(context.schedule.programs,
            *static_cast<ShaderValidationProgramPass*>(allocation), packed_program);
    }
    state = 0;
    return program;
}

void validate_program_body(ShaderValidationProgramContext& context, u32* packed_program, i32& state, void*& allocation)
{
    auto& dependencies = context.schedule.programs.dependencies;
    auto& constructors = dependencies.constructors;
    reset_shader_validation_tree_storage(constructors.objects.tree);
    context.schedule.conservative = 0;
    context.assigning_registers = 0;
    context.split_scalar = 1;
    dependencies.include_operand_writers = 1;
    auto* program = create_program(context, packed_program, 1, 2, state, allocation);
    optimize_shader_validation_program(context.optimization, *program, context.split_scalar);
    context.assigning_registers = 1;
    dependencies.include_operand_writers = 0;
    if (!schedule_and_serialize_shader_validation_program(context.schedule, *program, packed_program))
    {
        delete_shader_validation_program_pass(context.passes, program, 1, dependencies.sets.deleting_destructor, constructors.release);
        release_shader_validation_storage(context);
        constructors.active_owner = nullptr;
        reset_shader_validation_tree_storage(constructors.objects.tree);
        context.schedule.conservative = 1;
        context.assigning_registers = 0;
        context.split_scalar = 0;
        dependencies.include_operand_writers = 1;
        program = create_program(context, packed_program, 3, 4, state, allocation);
        optimize_shader_validation_program(context.optimization, *program, context.split_scalar);
        context.assigning_registers = 1;
        dependencies.include_operand_writers = 0;
        schedule_and_serialize_shader_validation_program(context.schedule, *program, packed_program);
    }
    delete_shader_validation_program_pass(context.passes, program, 1, dependencies.sets.deleting_destructor, constructors.release);
}
}

// Xbox 0x0019723C
bool validate_shader_validation_program(ShaderValidationProgramContext& context, u32* packed_program)
{
    bool result = true;
    i32 state = 0;
    void* allocation;
    __try
    {
        __try
        {
            validate_program_body(context, packed_program, state, allocation);
        }
        __finally
        {
            if (AbnormalTermination() && state >= 1 && state <= 4)
            {
                release_shader_validation_allocation(allocation, context.schedule.programs.dependencies.constructors.release);
            }
        }
    }
    __except (GetExceptionCode() == 0xE06D7363 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        // FuncInfo 0x0032BE70 catches every C++ exception in states 0..4;
        // the handler at 0x001973C2 clears the result byte and resumes cleanup.
        result = false;
    }
    state = -1;
    release_shader_validation_storage(context);
    context.schedule.programs.dependencies.constructors.active_owner = nullptr;
    return result;
}
}
