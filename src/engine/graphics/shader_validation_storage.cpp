#include "shader_validation_storage.h"

namespace gbtg
{
// Xbox 0x0018E46F
void decode_shader_validation_channel_mask(ShaderValidationChannelMask& mask, u32 bits)
{
    i32 index = 0;
    do
    {
        mask.channels[index] = static_cast<u8>((bits & (8u >> index)) != 0);
        ++index;
    } while (index < 4);
}

// Xbox 0x0018F2FC
ShaderValidationChannelMask* construct_shader_validation_channel_mask(ShaderValidationChannelMask& mask, u32 bits)
{
    mask.vtable_address = 0x00203790;
    decode_shader_validation_channel_mask(mask, bits);
    return &mask;
}

// Xbox 0x0018F258
void destroy_shader_validation_tree(ShaderValidationTreeNode* node)
{
    if (node != nullptr)
    {
        node->retained = 0;
        destroy_shader_validation_tree(node->first);
        destroy_shader_validation_tree(node->second);
        node->vtable->destroy(*node, 1);
    }
}

// Xbox 0x00190C93
void reset_shader_validation_tree_storage(ShaderValidationTreeStorage& storage)
{
    storage.root = nullptr;
    storage.count = 0;
    destroy_shader_validation_tree(nullptr);
}

// Xbox 0x001900DE
ShaderValidationTreeNode** find_shader_validation_tree_last(const ShaderValidationTreeStorage& storage, ShaderValidationTreeNode** result)
{
    ShaderValidationTreeNode* node = storage.root;
    ShaderValidationTreeNode* last = storage.root;
    while (node != nullptr)
    {
        last = node;
        node = node->first;
    }
    *result = last;
    return result;
}
}
