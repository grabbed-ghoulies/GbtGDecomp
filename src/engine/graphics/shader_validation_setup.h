#pragma once
#include "shader_validation_arena.h"
#include "shader_validation_storage.h"

namespace gbtg
{
struct ShaderValidationOwnedObject;
struct ShaderValidationOwnedObjectVtable
{
    void (*destroy)(ShaderValidationOwnedObject&, u32);
};

struct ShaderValidationOwnedObject
{
    const ShaderValidationOwnedObjectVtable* vtable; // Retail +0x00.
};

struct ShaderValidationCleanupContext
{
    ShaderValidationTreeStorage& objects; // DAT_001a4d38 storage.
    ShaderValidationArenaStorage& expressions; // DAT_001a4d48 storage.
    ShaderValidationArenaStorage& instructions; // DAT_001a4d58 storage.
};

void release_shader_validation_storage(ShaderValidationCleanupContext& context); // Xbox 0x00190CC1
}
