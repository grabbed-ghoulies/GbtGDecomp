#include "model_shader_parameter_draw.h"
#include "d3dx_matrix.h"
#include "../camera/view_frustum.h"

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
    const u32 name = image.get(image.get(node, shader_node::primary), model_material_field::name);
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
    if (image.get(material, model_material_field::colour_count) != 0)
    {
        do
        {
            const u32 color = image.get(material, model_material_field::colours) + index * 4;
            const std::array<float, 4> constant{float(image.byte(color)) * byte_scale,
                float(image.byte(color, 1)) * byte_scale, float(image.byte(color, 2)) * byte_scale,
                float(image.byte(color, 3)) * byte_scale};
            context.pixel_constants(index, constant.data(), 1);
            ++index;
        } while (index < image.get(material, model_material_field::colour_count));
    }
    index = 0;
    if (image.get(material, model_material_field::stage_count) != 0)
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
            const u32 record = image.get(material, model_material_field::stages) + offset;
            const u32 bit = 1u << (index & 0x1F);
            stage[0] = image.get(record, model_material_stage_field::address_u);
            dirty |= bit;
            stage[1] = image.get(record, model_material_stage_field::address_v);
            dirty |= bit;
            stage[2] = image.get(record, model_material_stage_field::address_w);
            stage[7] = 0;
            stage[4] = 2;
            stage[5] = 2;
            dirty |= bit;
            stage[3] = 2;
            if (image.get(record, model_material_stage_field::unk_0A) == 0)
            {
                dirty |= bit;
                stage[10] = 0;
                if (i16(image.get(record, model_material_stage_field::bump_stage)) >= 0)
                {
                    context.bump_environment(index, 0x16,
                        image.get(record, model_material_stage_field::bump_matrix_00));
                    context.bump_environment(index, 0x17,
                        image.get(record, model_material_stage_field::bump_matrix_01));
                    context.bump_environment(index, 0x19,
                        image.get(record, model_material_stage_field::bump_matrix_11));
                    context.bump_environment(index, 0x18,
                        image.get(record, model_material_stage_field::bump_matrix_10));
                }
            }
            else
            {
                dirty |= bit;
                stage[10] = 0xF0000000u;
            }
            if (image.get(record, model_material_stage_field::keyed) == 0)
            {
                if (image.get(material, model_material_field::shadow_stage) == index &&
                    image.get(material, model_material_field::takes_shadow) != 0 && context.globals.shadow_texture != 0)
                {
                    context.set_texture(index, context.globals.shadow_texture);
                    context.vertex_constants(image.get(material, model_material_field::shadow_register) + 0x60,
                        reinterpret_cast<const float*>(image.pointer(0x00336120)), 1);
                }
                else
                {
                    const u32 texture = image.get(record, model_material_stage_field::texture);
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
                        context.hooks.stage_texture(handle, index);
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
            offset += model_material_stage_field::kSize;
        } while (index < image.get(material, model_material_field::stage_count));
    }
    index = 0;
    if (image.get(material, model_material_field::constant_count) != 0)
    {
        u32 entry = 0;
        do
        {
            const u32 table = image.get(material, model_material_field::constants);
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
                const u32 mode = image.get(table + entry, model_material_constant_field::source);
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
                    source = reinterpret_cast<const float*>(
                        image.pointer(table + entry + model_material_constant_field::values_offset));
                }
            }
            context.vertex_constants(image.get(table + entry, model_material_constant_field::constant_register) + 0x60,
                source, 4);
            ++index;
            entry += model_material_constant_field::kSize;
        } while (index < image.get(material, model_material_field::constant_count));
    }
    if (image.get(material, model_material_field::alpha_reference) == 0)
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
        const u8 reference = image.get(material, model_material_field::alpha_reference);
        context.simple(0x40340, reference);
        context.alpha_reference = reference;
    }
    if (context.hooks.material)
    {
        context.hooks.material(material);
    }
}

// Xbox 0x001572C0
void select_runtime_background_shader_parameters(const RuntimeRelocationImage& image)
{
    const u32 slot = 0x001571B0;
    std::memcpy(image.pointer(0x0032E3EC), &slot, 4);
}

// Xbox 0x001572B0
void select_runtime_model_shader_parameters(const RuntimeRelocationImage& image)
{
    const u32 slot = 0x00157090;
    std::memcpy(image.pointer(0x0032E3EC), &slot, 4);
}

// Xbox 0x00157090
void dispatch_runtime_model_shader_parameters(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelMaterialBind& context)
{
    auto& renderer = context.tree.renderer;
    const auto& image = context.tree.image;
    const u32 copy = image.get(node, model_node_field::instance_offset);
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
    if (image.get(node, shader_node::secondary) != 0)
    {
        set_runtime_model_texture_key(runtime_model_material_texture_key(node, context), context.globals);
    }
    // Instance storage +copy+4 is the node's primary shader instance.
    const auto found = storage.shader_nodes.find(copy);
    if (found == storage.shader_nodes.end())
    {
        throw Error("model shader parameters have no instance copy");
    }
    bind_runtime_model_material(image.get(node, shader_node::primary), found->second.primary, context);
    std::array<float, 8> colors;
    std::memcpy(colors.data(), state.base_color.data(), 0x10);
    std::memcpy(colors.data() + 4, state.ambient_color.data(), 0x10);
    context.pixel_constants(6, colors.data(), 2);
    queue_runtime_model_group(node, context.tree);
}

// Xbox 0x00157140
void classify_runtime_model_bound_nodes(u32 node, const float* frustum, u32& visible, u8& plane,
    const RuntimeRelocationImage& image)
{
    do
    {
        while (image.byte(image.get(node, model_node_field::type_name), 2) != 'B')
        {
            if (image.get(node, model_node_field::child) != 0)
            {
                classify_runtime_model_bound_nodes(image.get(node, model_node_field::child), frustum, visible, plane,
                    image);
            }
            node = image.get(node, model_node_field::next);
            if (node == 0)
            {
                return;
            }
        }
        Vec3 first;
        Vec3 second;
        std::memcpy(&first, image.pointer(node + 0x48), 0xC);
        std::memcpy(&second, image.pointer(node + 0x54), 0xC);
        if (classify_runtime_box_frustum(std::span<const float, 42>(frustum, 42), first, second, plane) == 0)
        {
            image.set(node, push_buffer_node::culled, 1);
        }
        else
        {
            image.set(node, push_buffer_node::culled, 0);
            visible = 1;
        }
        node = image.get(node, model_node_field::next);
    } while (node != 0);
}

// Xbox 0x001571B0
void dispatch_runtime_background_shader_parameters(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelMaterialBind& context, RuntimeModelBoundCulling& culling)
{
    auto& renderer = context.tree.renderer;
    const auto& image = context.tree.image;
    ++culling.tested;
    u32 visible = 0;
    const u32 copy = image.get(node, model_node_field::instance_offset);
    RuntimeModelRenderState& state = runtime_render_state(renderer);
    runtime_render_frame(renderer);
    if (state.frustum == nullptr)
    {
        throw Error("background shader parameters need the render state +0x64 frustum");
    }
    classify_runtime_model_bound_nodes(image.get(node, model_node_field::child), state.frustum, visible, culling.plane,
        image);
    if (visible == 0)
    {
        ++culling.culled;
        queue_runtime_model_sibling(node, context.tree);
        return;
    }
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
    if (image.get(node, shader_node::secondary) != 0)
    {
        set_runtime_model_texture_key(runtime_model_material_texture_key(node, context), context.globals);
    }
    // Instance storage +copy+4 is the node's primary shader instance.
    const auto found = storage.shader_nodes.find(copy);
    if (found == storage.shader_nodes.end())
    {
        throw Error("model shader parameters have no instance copy");
    }
    bind_runtime_model_material(image.get(node, shader_node::primary), found->second.primary, context);
    std::array<float, 8> colors;
    std::memcpy(colors.data(), state.base_color.data(), 0x10);
    std::memcpy(colors.data() + 4, state.ambient_color.data(), 0x10);
    context.pixel_constants(6, colors.data(), 2);
    queue_runtime_model_group(node, context.tree);
}
}
