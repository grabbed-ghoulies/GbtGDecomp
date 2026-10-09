#pragma once
#include "shader_validation_insert.h"

namespace gbtg
{
struct ShaderValidationRegisteredObject
{
    u32 vtable_address; // Retail +0x00.
};

void register_shader_validation_object(ShaderValidationInsertContext& context, u32 object_address); // Xbox 0x00192655
void register_shader_validation_diagnostic(ShaderValidationInsertContext& context, u32 object_address); // Xbox 0x00192713
ShaderValidationRegisteredObject* initialize_shader_validation_registered_object(ShaderValidationInsertContext& context, ShaderValidationRegisteredObject& object, u32 object_address); // Xbox 0x001928DE
}
