#include "shader_validation_registration.h"
#include "shader_validation_node_init.h"

namespace gbtg
{
// Xbox 0x00192655
void register_shader_validation_object(ShaderValidationInsertContext& context, u32 object_address)
{
    if (context.tree.root == nullptr)
    {
        context.tree.count = 1;
        auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
        ShaderValidationBalancedNode* node;
        if (allocation == nullptr)
        {
            node = nullptr;
        }
        else
        {
            node = initialize_shader_validation_object_node(*allocation, object_address, nullptr);
        }
        context.tree.root = node;
    }
    else
    {
        insert_shader_validation_object_node(context, object_address, context.tree.root);
    }
}

// Xbox 0x00192713
void register_shader_validation_diagnostic(ShaderValidationInsertContext& context, u32 object_address)
{
    if (context.tree.root == nullptr)
    {
        context.tree.count = 1;
        auto* allocation = static_cast<ShaderValidationBalancedNode*>(context.allocate(sizeof(ShaderValidationBalancedNode), 0x24870000));
        ShaderValidationBalancedNode* node;
        if (allocation == nullptr)
        {
            node = nullptr;
        }
        else
        {
            node = initialize_shader_validation_diagnostic_node(*allocation, object_address, nullptr);
        }
        context.tree.root = node;
    }
    else
    {
        insert_shader_validation_diagnostic_node(context, object_address, context.tree.root);
    }
}

// Xbox 0x001928DE
ShaderValidationRegisteredObject* initialize_shader_validation_registered_object(ShaderValidationInsertContext& context, ShaderValidationRegisteredObject& object, u32 object_address)
{
    object.vtable_address = 0x00203808;
    register_shader_validation_object(context, object_address);
    return &object;
}
}
