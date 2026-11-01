#include "shader_parameters.h"
#include "../audio/listener_settings.h"
#include "gbtg/model_runtime.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x000F8540, EBX the model: the named shader's texture parameter of that name, or null.
RuntimeShaderTextureParameter* create_shader_parameter(RuntimeModelInstance& model, const char* shader, const char* name)
{
    const RuntimeModelTree* tree = model.source->tree; // *(*(model + 0) + 0x10) + 0x24
    if (tree == nullptr)
    {
        return nullptr;
    }
    RuntimeModelNode* node = nullptr;
    if (!find_runtime_model_named_node(*tree, shader, node) || node == nullptr) // 0x001568E0
    {
        return nullptr;
    }
    const RuntimeShaderCpuEntry* entry = nullptr;
    if (node->shader_cpu == nullptr || !find_runtime_shader_entry(*node->shader_cpu, name, entry)) // 0x0015B160
    {
        return nullptr;
    }
    if (entry->value == -1)
    {
        return nullptr;
    }
    auto* parameter = new RuntimeShaderTextureParameter; // 0x000F72D0(0x18)
    parameter->model = &model;
    parameter->shader = model.tree_storage.shader_nodes.at(node->copy_offset).primary; // model + node +0xC + 0x2A4
    parameter->index = u32(entry->value);
    return parameter;
}

// Xbox 0x000F9B00
u32 runtime_model_texture_address(const RuntimeModelInstance& model, u32 index,
    const std::function<const u8*(u32)>& pointer)
{
    for (const RuntimeModelResourceBlock& block : model.source->blocks)
    {
        if (block.type == 7)
        {
            u32 list;
            std::memcpy(&list, block.data + 4, 4);
            u32 texture;
            std::memcpy(&texture, pointer(list) + size_t(index) * 4, 4);
            return texture;
        }
    }
    throw Error("0x000F9B00 reads the texture list of a model without one (address 4 + 4 index)");
}

// Xbox 0x00040B70
u32 find_actor_shader_texture(const RuntimeModelInstance* model, const char* shader, const char* name,
    const std::function<const u8*(u32)>& pointer)
{
    if (model == nullptr) // Actor +0x204
    {
        return 0;
    }
    const RuntimeModelTree* tree = model->source->tree; // *(*(model + 0) + 0x10) + 0x24
    if (tree == nullptr)
    {
        return 0;
    }
    RuntimeModelNode* node = nullptr;
    if (!find_runtime_model_named_node(*tree, shader, node) || node == nullptr) // 0x001568E0
    {
        return 0;
    }
    const i32 index = runtime_shader_entry_word(*node->shader_cpu, name); // 0x0015B270, the node's +0x20
    if (index == -1)
    {
        return 0;
    }
    return runtime_model_texture_address(*model, u32(index), pointer); // 0x000F9B00
}

// Xbox 0x0015B2B0: the shader instance's texture word for the parameter.
void set_shader_texture(RuntimeShaderInstance& shader, u32 index, u32 texture)
{
    shader.zero_words[index] = texture;
}

// Xbox 0x000F85D0, EAX the parameter.
void release_shader_parameter(RuntimeShaderTextureParameter* parameter)
{
    parameter->texture = 0;
    parameter->mode = 0;
    parameter->time = 0.f;
    set_shader_texture(*parameter->shader, parameter->index, 0); // 0x0015B2B0
    delete parameter; // HeapFree
}

// Xbox 0x000F86F0, EAX the parameter, ECX the texture, EDX the mode.
void set_shader_parameter_texture(RuntimeShaderTextureParameter& parameter, u32 texture, u32 mode,
    const std::function<u8*(u32)>& texture_bytes)
{
    parameter.texture = texture;
    parameter.mode = mode;
    parameter.time = 0.f;
    if (texture != 0)
    {
        u8* bytes = texture_bytes(texture);
        if (bytes[0x11] != 0)
        {
            bytes[0x13] = 0;
        }
        set_shader_texture(*parameter.shader, parameter.index, parameter.texture); // 0x0015B2B0
        return;
    }
    set_shader_texture(*parameter.shader, parameter.index, 0); // 0x0015B2B0
}

// Xbox 0x00053640, ESI the texture
u8 runtime_texture_loop_frame(const u8* texture, float time)
{
    const float rate = float(texture[0x10]);
    if (rate == 0.f) // DAT_0032B100.
    {
        return 0;
    }
    const u32 frames = texture[0x11];
    if (frames == 0)
    {
        throw Error("0x00053640 divides by a texture's zero frame count");
    }
    const u32 ticks = u32(truncate_runtime_float(static_cast<long double>(rate) * time)); // 0x001673D8
    return u8(ticks % frames);
}

// Xbox 0x000F8640, EAX the parameter, XMM0 the step
void advance_shader_parameter(RuntimeShaderTextureParameter& parameter, float step,
    const std::function<u8*(u32)>& texture_bytes)
{
    const float time = step + parameter.time;
    parameter.time = time;
    if (parameter.texture == 0)
    {
        return;
    }
    u8* texture = texture_bytes(parameter.texture);
    const i32 frames = texture[0x11];
    i32 frame;
    if (parameter.mode == 0)
    {
        if (1.f > time) // DAT_0032B0F8.
        {
            frame = runtime_texture_loop_frame(texture, time); // 0x00053640
        }
        else
        {
            frame = frames - 1;
        }
    }
    else if (parameter.mode == 1)
    {
        const float rate = float(texture[0x10]);
        if (rate == 0.f) // DAT_0032B100.
        {
            frame = 0;
        }
        else
        {
            if (frames == 0)
            {
                throw Error("0x000F8640 divides by a texture's zero frame count");
            }
            const u32 ticks = u32(truncate_runtime_float(static_cast<long double>(time) * rate)); // 0x001673D8
            frame = u8(ticks % u32(frames));
        }
    }
    else
    {
        return;
    }
    if (frame == -1)
    {
        return;
    }
    if (frame < frames)
    {
        texture[0x13] = u8(frame);
    }
}
}
