#pragma once
#include "gbtg/types.h"
#include <array>

namespace gbtg
{
struct ShaderValidationChannelMask
{
    u32 vtable_address; // Retail +0x00.
    std::array<u8, 4> channels; // Retail +0x04.
};

struct ShaderValidationTreeNode;
struct ShaderValidationTreeVtable
{
    void (*destroy)(ShaderValidationTreeNode&, u32);
};

struct ShaderValidationTreeNode
{
    const ShaderValidationTreeVtable* vtable; // Retail +0x00.
    void* value; // Retail +0x04.
    u32 retained; // Retail +0x08.
    ShaderValidationTreeNode* first; // Retail +0x0c.
    ShaderValidationTreeNode* second; // Retail +0x10.
};

struct ShaderValidationTreeStorage
{
    ShaderValidationTreeNode* root; // Retail storage +0x08, DAT_001a4d40.
    u32 count; // Retail storage +0x0c, DAT_001a4d44.
};

void decode_shader_validation_channel_mask(ShaderValidationChannelMask& mask, u32 bits); // Xbox 0x0018E46F
ShaderValidationChannelMask* construct_shader_validation_channel_mask(ShaderValidationChannelMask& mask, u32 bits); // Xbox 0x0018F2FC
void destroy_shader_validation_tree(ShaderValidationTreeNode* node); // Xbox 0x0018F258
void reset_shader_validation_tree_storage(ShaderValidationTreeStorage& storage); // Xbox 0x00190C93
ShaderValidationTreeNode** find_shader_validation_tree_last(const ShaderValidationTreeStorage& storage, ShaderValidationTreeNode** result); // Xbox 0x001900DE
}
