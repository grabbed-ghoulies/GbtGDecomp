#include "shader_validation_pass_lifecycle.h"
#include "shader_validation_cleanup.h"
#if defined(_WIN32)
#include <excpt.h>
#else
#error Shader validator constructor unwind requires the approved Windows SEH backend.
#endif

namespace gbtg
{
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
