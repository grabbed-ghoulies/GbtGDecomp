#include "model_shader_parameter_draw.h"
#include "d3dx_matrix.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0015B6B0
void set_runtime_model_texture_table(u32 table, RuntimeModelMaterialGlobals& globals)
{
    globals.texture_table = table;
}

// Xbox 0x0015B6C0
void set_runtime_model_material_camera(const u32* camera, RuntimeModelMaterialGlobals& globals)
{
    for (u32 index = 0; index < 0x10; ++index)
    {
        globals.camera[index] = camera[index];
    }
}

// Xbox 0x0015B6E0
void set_runtime_model_normalized_combined(const float* combined, RuntimeModelMaterialGlobals& globals)
{
    std::array<float, 3> lengths;
    runtime_graphics_matrix_column_lengths(combined, lengths.data());
    scale_runtime_graphics_matrix_rows(combined, globals.normalized_combined.data(), lengths[0], lengths[1],
        lengths[2]);
}

// Xbox 0x0015B690
void set_runtime_model_material_vector(const float* vector, RuntimeModelMaterialGlobals& globals)
{
    copy_runtime_graphics_vector(vector, globals.material_vector.data());
}

// Xbox 0x0015B720
void set_runtime_model_texture_key(u32 key, RuntimeModelMaterialGlobals& globals)
{
    globals.texture_key = key;
}

// Xbox 0x00158BB0
void hash_runtime_model_name(const u8* text, i32 count, u32& hash)
{
    i32 index = 0;
    if (0 < count)
    {
        do
        {
            const u32 value = hash * 0x10 + u32(i32(std::bit_cast<std::int8_t>(text[index])));
            const u32 high = value & 0xF0000000u;
            hash = value;
            if (high != 0)
            {
                hash = (high >> 0x18 | high) ^ value;
            }
            ++index;
        } while (index < count);
    }
}

// Xbox 0x00157030
u32 runtime_model_material_texture_key(u32 node, RuntimeModelMaterialBind& context)
{
    const RuntimeModelRenderState& state = runtime_render_state(context.tree.renderer);
    const auto& image = context.tree.image;
    u32 hash = 0;
    const u32 name = image.word(image.word(node, 0x20), 0x20);
    if (name != 0)
    {
        hash_runtime_model_name(image.pointer(name + 1), 1, hash);
    }
    std::array<u8, 2> key;
    std::memcpy(key.data(), &state.texture_key, 2);
    hash_runtime_model_name(key.data(), 2, hash);
    return hash;
}

// Xbox 0x0015ABB0
void resolve_runtime_model_texture_record(u32 key, u32& texture, RuntimeModelMaterialBind& context)
{
    const auto& image = context.tree.image;
    u32 record = context.records.first;
    while (record != 0)
    {
        if (image.word(record, 0xC4) == key)
        {
            const u32 used = 1;
            std::memcpy(image.pointer(record + 0x68), &used, 4);
            texture = record + 200;
            return;
        }
        record = image.word(record, 4);
    }
    if (context.records.lookup)
    {
        // The call's result replaces only the key; the output pointer stays on the stack.
        const u32 found = context.records.lookup(key, texture);
        resolve_runtime_model_texture_record(found, texture, context);
        return;
    }
    texture = 0;
}

// Xbox 0x0015AD20
void bind_runtime_model_material(u32 material, const RuntimeShaderInstance* instance,
    RuntimeModelMaterialBind& context)
{
    const auto& image = context.tree.image;
    // DAT_0032B154 is 1/255.
    const float byte_scale = std::bit_cast<float>(0x3B808081u);
    u32 index = 0;
    if (image.word(material, 0x14) != 0)
    {
        do
        {
            const u32 color = image.word(material) + index * 4;
            const std::array<float, 4> constant{float(image.byte(color)) * byte_scale,
                float(image.byte(color, 1)) * byte_scale, float(image.byte(color, 2)) * byte_scale,
                float(image.byte(color, 3)) * byte_scale};
            context.pixel_constants(index, constant.data(), 1);
            ++index;
        } while (index < image.word(material, 0x14));
    }
    index = 0;
    if (image.word(material, 0xC) != 0)
    {
        u32 offset = 0;
        u32& dirty = context.deferred[0];
        do
        {
            if (2 + (index + 1) * 0x20 > context.deferred.size())
            {
                throw Error("model material stage lies outside the deferred texture states");
            }
            u32* stage = context.deferred.data() + 2 + index * 0x20;
            const u32 bit = 1u << (index & 0x1F);
            stage[0] = image.byte(image.word(material, 8), offset + 4);
            dirty |= bit;
            stage[1] = image.byte(image.word(material, 8), offset + 5);
            dirty |= bit;
            stage[2] = image.byte(image.word(material, 8), offset + 6);
            stage[7] = 0;
            stage[4] = 2;
            stage[5] = 2;
            dirty |= bit;
            stage[3] = 2;
            if (image.half(image.word(material, 8), offset + 0xA) == 0)
            {
                dirty |= bit;
                stage[10] = 0;
                if (i16(image.half(image.word(material, 8), offset + 8)) >= 0)
                {
                    context.bump_environment(index, 0x16, image.word(image.word(material, 8), offset + 0xC));
                    context.bump_environment(index, 0x17, image.word(image.word(material, 8), offset + 0x10));
                    context.bump_environment(index, 0x19, image.word(image.word(material, 8), offset + 0x14));
                    context.bump_environment(index, 0x18, image.word(image.word(material, 8), offset + 0x18));
                }
            }
            else
            {
                dirty |= bit;
                stage[10] = 0xF0000000u;
            }
            if (image.byte(image.word(material, 8), offset + 7) == 0)
            {
                if (image.word(material, 0x40) == index && image.byte(material, 0x1B) != 0 &&
                    context.globals.shadow_texture != 0)
                {
                    context.set_texture(index, context.globals.shadow_texture);
                    context.vertex_constants(image.word(material, 0x34) + 0x60,
                        reinterpret_cast<const float*>(image.pointer(0x00336120)), 1);
                }
                else
                {
                    const u32 texture = image.word(image.word(material, 8), offset);
                    if (texture != 0xFFFFFFFFu)
                    {
                        if (instance == nullptr || index >= instance->zero_words.size())
                        {
                            throw Error("model material stage has no instance texture word");
                        }
                        u32 handle = instance->zero_words[index];
                        if (handle == 0)
                        {
                            handle = image.word(context.globals.texture_table, texture * 4);
                        }
                        context.stage_texture(handle, index);
                    }
                }
            }
            else
            {
                u32 texture = 0;
                resolve_runtime_model_texture_record(context.globals.texture_key, texture, context);
                context.set_texture(index, texture);
            }
            ++index;
            offset += 0x1C;
        } while (index < image.word(material, 0xC));
    }
    index = 0;
    if (image.word(material, 0x10) != 0)
    {
        u32 entry = 0;
        do
        {
            const u32 table = image.word(material, 4);
            const float* source = nullptr;
            if (instance != nullptr)
            {
                if (index >= instance->parameters.size())
                {
                    throw Error("model material constant has no instance parameter");
                }
                if (instance->parameters[index].bytes[0x40] != 0)
                {
                    source = reinterpret_cast<const float*>(instance->parameters[index].bytes.data());
                }
            }
            if (source == nullptr)
            {
                const u32 mode = image.word(table, entry + 0x44);
                if (mode == 1)
                {
                    source = reinterpret_cast<const float*>(context.environment_primary.data());
                }
                else if (mode == 2)
                {
                    source = reinterpret_cast<const float*>(context.environment_secondary.data());
                }
                else
                {
                    source = reinterpret_cast<const float*>(image.pointer(table + entry));
                }
            }
            context.vertex_constants(image.word(table, entry + 0x40) + 0x60, source, 4);
            ++index;
            entry += 0x48;
        } while (index < image.word(material, 0x10));
    }
    if (image.byte(material, 0x18) == 0)
    {
        context.simple(0x40300, 0);
        context.alpha_test = 0;
    }
    else
    {
        context.simple(0x40300, 1);
        context.alpha_test = 1;
        context.simple(0x4033C, 0x204);
        context.alpha_function = 0x204;
        const u8 reference = image.byte(material, 0x18);
        context.simple(0x40340, reference);
        context.alpha_reference = reference;
    }
    if (context.material)
    {
        context.material(material);
    }
}

// Xbox 0x00157090
void dispatch_runtime_model_shader_parameters(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelMaterialBind& context)
{
    auto& renderer = context.tree.renderer;
    const auto& image = context.tree.image;
    const u32 copy = image.word(node, 0xC);
    RuntimeModelRenderState& state = runtime_render_state(renderer);
    runtime_render_frame(renderer);
    if (state.material != 0)
    {
        set_runtime_model_texture_table(state.material_values, context.globals);
    }
    const RuntimeShaderMatrix* camera = context.tree.camera[size_t(renderer.frame_index)];
    if (camera == nullptr)
    {
        throw Error("model shader parameters need the frame camera");
    }
    set_runtime_model_material_camera(reinterpret_cast<const u32*>(camera->data()), context.globals);
    set_runtime_model_normalized_combined(context.tree.combined[size_t(renderer.frame_index)].data(), context.globals);
    set_runtime_model_material_vector(state.material_vector.data(), context.globals);
    if (image.word(node, 0x24) != 0)
    {
        set_runtime_model_texture_key(runtime_model_material_texture_key(node, context), context.globals);
    }
    // Instance storage +copy+4 is the node's primary shader instance.
    const auto found = storage.shader_nodes.find(copy);
    if (found == storage.shader_nodes.end())
    {
        throw Error("model shader parameters have no instance copy");
    }
    bind_runtime_model_material(image.word(node, 0x20), found->second.primary, context);
    std::array<float, 8> colors;
    std::memcpy(colors.data(), state.base_color.data(), 0x10);
    std::memcpy(colors.data() + 4, state.ambient_color.data(), 0x10);
    context.pixel_constants(6, colors.data(), 2);
    queue_runtime_model_group(node, context.tree);
}
}
