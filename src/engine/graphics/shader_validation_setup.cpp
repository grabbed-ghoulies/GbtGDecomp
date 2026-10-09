#include "shader_validation_setup.h"

namespace gbtg
{
// Xbox 0x00190CC1
void release_shader_validation_storage(ShaderValidationCleanupContext& context)
{
    reset_shader_validation_expression_arena(context.expressions);
    reset_shader_validation_instruction_arena(context.instructions);
    ShaderValidationTreeNode* node;
    find_shader_validation_tree_last(context.objects, &node);
    while (context.objects.count != 0)
    {
        auto* object = static_cast<ShaderValidationOwnedObject*>(node->value);
        if (object != nullptr)
        {
            object->vtable->destroy(*object, 1);
        }
        node = *find_shader_validation_tree_last(context.objects, &node);
    }
}
}
