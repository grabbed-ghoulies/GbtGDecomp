#include "gbtg/model_runtime.h"
#include "gbtg/resource_error.h"
#include "gbtg/model_relocation.h"
#include <new>
#include <bit>
#include <cmath>

namespace gbtg
{

// Xbox 0x000F9550
void bind_runtime_model_pose(RuntimeModelInstance& model, ModelPoseBuffer* pose,
    const RuntimeShaderMatrixBoundaries& boundaries)
{
    if (model.skeleton_node != nullptr)
    {
        model.tree_storage.skeleton_poses[model.skeleton_node->copy_offset + 4] = pose;
        if (pose == nullptr)
        {
            model.render_pose = nullptr;
        }
        else
        {
            model.render_pose = pose;
            if (pose->dirty)
            {
                // Host storage keeps the retail allocation's two interpretations
                // separate. The dirty flag still selects which one is valid.
                pose->packed_matrices.resize(static_cast<size_t>(pose->count));
                i32 remaining = pose->count;
                i32 index = remaining - 1;
                if (remaining > 3)
                {
                    i32 large = remaining - 3;
                    remaining -= large;
                    do
                    {
                        RuntimeShaderMatrix input{};
                        std::memcpy(input.data(), pose->matrices[index].data(), 0x30);
                        // The source's extra four words affect only entries which
                        // the retail code immediately overwrites below.
                        RuntimeShaderMatrix& output = pose->packed_matrices[index];
                        boundaries.transpose(output, input);
                        output[11] = 0.f;
                        output[7] = 0.f;
                        output[3] = 0.f;
                        output[15] = 1.f;
                        --index;
                        --large;
                    }
                    while (large != 0);
                }
                for (; remaining != 0; --remaining)
                {
                    const AnimationMatrix source = pose->matrices[index];
                    pose->packed_matrices[index] =
                    {
                        source[0], source[4], source[8], 0.f,
                        source[1], source[5], source[9], 0.f,
                        source[2], source[6], source[10], 0.f,
                        source[3], source[7], source[11], 1.f
                    };
                    --index;
                }
                pose->dirty = false;
                return;
            }
        }
    }
}

// Xbox 0x00151E64
RuntimeShaderMatrix& multiply_runtime_matrix(RuntimeShaderMatrix& output,
    const RuntimeShaderMatrix& left, const RuntimeShaderMatrix& right)
{
    RuntimeShaderMatrix product;
    for (size_t row = 0; row < 4; ++row)
    {
        for (size_t column = 0; column < 4; ++column)
        {
            const float first = left[row * 4] * right[column];
            const float second = left[row * 4 + 1] * right[column + 4];
            const float third = left[row * 4 + 2] * right[column + 8];
            const float fourth = left[row * 4 + 3] * right[column + 12];
            const float pair = first + second;
            const float triple = pair + third;
            product[row * 4 + column] = triple + fourth;
        }
    }
    output = product;
    return output;
}

// Xbox 0x00156010
void write_runtime_model_matrix(std::span<const u8> node, std::span<u8> instance,
    i32 index, const RuntimeShaderMatrix& matrix)
{
    const u32 offset = read_u32(node, 0xC);
    std::memcpy(instance.data() + offset + 4 + index * 0x40, matrix.data(), 0x40);
    instance[offset + 4 + read_u32(node, 0x30) * 0x40 + index] = 1;
}

// Xbox 0x000F96F0
void set_runtime_model_node_state(RuntimeModelInstance& model, u8 slot, u8 value,
    std::span<u32> shared_states)
{
    const u32 bit = static_cast<u32>(slot) * 2;
    if (shared_states[slot] != static_cast<u32>(value))
    {
        shared_states[slot] = value;
    }
    u8& flags = model.node_flags[bit >> 3];
    flags = static_cast<u8>((~(3u << (bit & 7)) & flags) |
        (static_cast<u32>(value) << (bit & 7)));
}
// Original string pointers at 0x00339A08..0x00339AC8, exported read-only.
const std::array<const char*, 48> runtime_model_auxiliary_names
{
    "MORPH_TARGET_BLINK", "MORPH_TARGET_ANGRY", "MORPH_TARGET_ANGRY1",
    "MORPH_TARGET_HIT", "MORPH_TARGET_HIT1", "MORPH_TARGET_HIT2",
    "MORPH_TARGET_MOUTH_OPEN", "MORPH_TARGET_SMILE", "MORPH_TARGET_HAIR_SPIKE",
    "MORPH_TARGET_SCARED", "MORPH_TARGET_MOUTHCLOSED", "MORPH_TARGET_BROWRAISED",
    "MORPH_TARGET_HAIRSCARED", "MORPH_TARGET_ANGER", "MORPH_TARGET_SAD",
    "MORPH_TARGET_BLINK1", "MORPH_TARGET_RAISE_BROW", "MORPH_TARGET_CHEEK_BLOW",
    "MORPH_TARGET_SACK_EMPTY", "MORPH_TARGET_SACK_FULL", "MORPH_TARGET_SACK_OPEN",
    "MORPH_TARGET_SACK_CLOSED", "blendshape22", "blendshape23", "blendshape24",
    "blendshape25", "blendshape26", "blendshape27", "blendshape28", "blendshape29",
    "blendshape30", "blendshape31", "blendshape32", "blendshape33", "blendshape34",
    "blendshape35", "blendshape36", "blendshape37", "blendshape38", "blendshape39",
    "blendshape40", "blendshape41", "blendshape42", "blendshape43", "blendshape44",
    "blendshape45", "blendshape46", "blendshape47"
};
namespace
{

// The two-byte strcmp sequence in 0x00158820 / 0x001568E0.
int compare_runtime_model_name(const char* first, const char* second)
{
    const auto* a = reinterpret_cast<const u8*>(first);
    const auto* b = reinterpret_cast<const u8*>(second);
    do
    {
        u8 value = *a;
        if (value != *b)
        {
            return value < *b ? -1 : 1;
        }
        if (value == 0)
        {
            break;
        }
        value = a[1];
        if (value != b[1])
        {
            return value < b[1] ? -1 : 1;
        }
        a += 2;
        b += 2;
        if (value == 0)
        {
            break;
        }
    } while (true);
    return 0;
}

} // namespace

// Xbox 0x00158820.
u32 find_runtime_model_type(std::span<const RuntimeModelNodeType> types, const char* name,
    const RuntimeModelNodeType*& result)
{
    u32 index = 0;
    if (types.size() != 0)
    {
        do
        {
            if (compare_runtime_model_name(types[index].name, name) == 0)
            {
                result = &types[index];
                return index;
            }
            ++index;
        } while (index < types.size());
    }
    result = nullptr;
    return 0;
}

// Xbox 0x001588A0.
u32 runtime_model_type_index(std::span<const RuntimeModelNodeType> types, const char* name)
{
    const RuntimeModelNodeType* result;
    return find_runtime_model_type(types, name, result);
}

// Xbox 0x00156280.
RuntimeModelNode* find_runtime_model_node(RuntimeModelNode* first, u16 type)
{
    while (true)
    {
        if (first == nullptr)
        {
            return nullptr;
        }
        if (first->type == type)
        {
            break;
        }
        if (first->child != nullptr)
        {
            RuntimeModelNode* child = find_runtime_model_node(first->child, type);
            if (child != nullptr)
            {
                return child;
            }
        }
        first = first->sibling;
    }
    return first;
}

// Xbox 0x001562C0. The 0x00156DD0 global table is a host parameter.
RuntimeModelNode* find_runtime_model_node_by_name(const RuntimeModelTree& tree, u16 root,
    const char* type, std::span<const RuntimeModelNodeType> types)
{
    const u32 index = runtime_model_type_index(types, type);
    return find_runtime_model_node(tree.roots[root], u16(index));
}

// Xbox 0x001561D0.
u32 runtime_model_instance_size(const RuntimeModelTree& tree)
{
    return tree.instance_size;
}

// Xbox 0x001588E0. This is the original type-specific virtual copy call.
void copy_runtime_model_node(const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
{
    node.descriptor->copy(node, instance);
}

// Xbox 0x00156120.
void copy_runtime_model_nodes(const RuntimeModelNode* first, RuntimeModelInstanceStorage& instance)
{
    for (; first != nullptr; first = first->sibling)
    {
        copy_runtime_model_node(*first, instance);
        if (first->child != nullptr)
        {
            copy_runtime_model_nodes(first->child, instance);
        }
    }
}

// Xbox 0x001561E0.
void copy_runtime_model_tree(const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance)
{
    u32 index = 0;
    if (tree.roots.size() != 0)
    {
        do
        {
            copy_runtime_model_nodes(tree.roots[index], instance);
            ++index;
        } while (index < tree.roots.size());
    }
}

// Xbox 0x001568E0. A miss leaves the output pointer untouched.
bool find_runtime_model_named_node(const RuntimeModelTree& tree, const char* name,
    RuntimeModelNode*& result)
{
    const RuntimeModelNameTable* table = tree.names;
    if (table == nullptr)
    {
        return false;
    }
    u32 index = 0;
    if (table->entries.size() != 0)
    {
        do
        {
            if (compare_runtime_model_name(table->entries[index].name, name) == 0)
            {
                result = table->entries[index].node;
                return true;
            }
            ++index;
        } while (index < table->entries.size());
    }
    return false;
}

// Xbox 0x00156960.
u32 runtime_model_named_count(const RuntimeModelTree& tree)
{
    if (tree.names == nullptr)
    {
        return 0;
    }
    return u32(tree.names->entries.size());
}

// Xbox 0x00156970.
void runtime_model_named_entry(const RuntimeModelTree& tree, int index,
    const char*& name, RuntimeModelNode*& node)
{
    name = tree.names->entries[size_t(index)].name;
    node = tree.names->entries[size_t(index)].node;
}

// Xbox 0x000F9AA0. Instance is the storage at model instance +0x2A0;
// names supplies the 48 pointers at 0x00339A08..0x00339AC8.
void bind_model_auxiliary_targets(std::array<float*, 48>& targets, const RuntimeModelTree& tree,
    std::span<u8> instance, std::span<const char* const, 48> names)
{
    size_t index = 0;
    do
    {
        RuntimeModelNode* node;
        if (find_runtime_model_named_node(tree, names[index], node))
        {
            targets[index] = reinterpret_cast<float*>(instance.data() + node->instance_offset);
        }
        else
        {
            targets[index] = nullptr;
        }
        ++index;
    } while (index < 48);
}

// Xbox 0x000F9AA0, using the original global name table.
void bind_model_auxiliary_targets(std::array<float*, 48>& targets, const RuntimeModelTree& tree,
    std::span<u8> instance)
{
    bind_model_auxiliary_targets(targets, tree, instance, runtime_model_auxiliary_names);
}

// Xbox 0x00158940.
void copy_runtime_node_empty(const RuntimeModelNode& node, std::span<u8> instance)
{
    const u32 zero = 0;
    std::memcpy(instance.data() + node.copy_offset, &zero, 4);
}

// Xbox 0x00158670.
void copy_runtime_node_empty_thunk(const RuntimeModelNode& node, std::span<u8> instance)
{
    copy_runtime_node_empty(node, instance);
}

// Xbox 0x001580C0.
void copy_runtime_node_bg_push_buffer(const RuntimeModelNode& node, std::span<u8> instance)
{
    const u32 unset = 0xFFFFFFFF;
    std::memcpy(instance.data() + node.copy_offset + 4, &unset, 4);
    copy_runtime_node_empty(node, instance);
}

// Xbox 0x00158070.
void copy_runtime_node_visibility(const RuntimeModelNode& node, std::span<u8> instance)
{
    instance[node.copy_offset + 8] = 1;
    const u32 zero = 0;
    std::memcpy(instance.data() + node.copy_offset, &zero, 4);
}

// Xbox 0x00158040.
void copy_runtime_node_transform(const RuntimeModelNode& node, std::span<u8> instance)
{
    const u8* source = node.bytes.data() + 0x24;
    u8* destination = instance.data() + node.copy_offset + 8;
    for (int remaining = 0x10; remaining != 0; --remaining)
    {
        std::memcpy(destination, source, 4);
        source += 4;
        destination += 4;
    }
    instance[node.copy_offset + 4] = 0;
    copy_runtime_node_empty(node, instance);
}

// Xbox 0x00155E20. The count is reloaded at each iteration on Xbox.
void copy_runtime_node_matrix_array(const RuntimeModelNode& node, std::span<u8> instance)
{
    i32 count;
    std::memcpy(&count, node.bytes.data() + 0x30, 4);
    i32 index = 0;
    if (count > 0)
    {
        do
        {
            instance[size_t(count * 0x40 + index + 4 + node.copy_offset)] = 0;
            std::memcpy(&count, node.bytes.data() + 0x30, 4);
            ++index;
        } while (index < count);
    }
    copy_runtime_node_empty(node, instance);
}

// Xbox 0x0015B260.
u32 runtime_shader_instance_size(const RuntimeShaderCpu& shader)
{
    return shader.instance_size;
}

// Xbox 0x0015B040. Typed spans replace the two pointers written into the
// runtime header: zeros at +8 and 0x44-byte parameters after zeros.
void initialize_runtime_shader_instance(const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    std::span<u8> storage)
{
    instance.zero_words = std::span<u32>(reinterpret_cast<u32*>(storage.data() + 8),
        shader.zero_word_count);
    instance.parameters = std::span<RuntimeShaderParameter>(reinterpret_cast<RuntimeShaderParameter*>(
        storage.data() + 8 + shader.zero_word_count * 4), shader.initial_parameters.size());
    u32 index = 0;
    if (shader.zero_word_count != 0)
    {
        do
        {
            instance.zero_words[index] = 0;
            ++index;
        } while (index < shader.zero_word_count);
    }
    index = 0;
    if (shader.initial_parameters.size() != 0)
    {
        do
        {
            instance.parameters[index].bytes[0x40] = 0;
            const u8* source = shader.initial_parameters[index].data();
            u8* destination = instance.parameters[index].bytes.data();
            for (int remaining = 0x10; remaining != 0; --remaining)
            {
                std::memcpy(destination, source, 4);
                source += 4;
                destination += 4;
            }
            ++index;
        } while (index < shader.initial_parameters.size());
    }
}

// Xbox 0x00157320. The typed runtime pointers correspond to the two dwords
// at node instance +4/+8; their backing bytes retain the Xbox layout.
void copy_runtime_node_shader_parameters(const RuntimeModelNode& node,
    RuntimeShaderNodeInstance& output, std::span<u8> instance)
{
    u8* destination = instance.data() + node.copy_offset;
    const u32 primary_size = runtime_shader_instance_size(*node.shader_cpu);
    if (primary_size == 0)
    {
        output.primary = nullptr;
        output.primary_storage = nullptr;
    }
    else
    {
        output.primary_storage = destination + 0xC;
        output.primary = &output.primary_instance;
        initialize_runtime_shader_instance(*node.shader_cpu, output.primary_instance,
            std::span<u8>(output.primary_storage, primary_size));
    }
    if (node.secondary_shader_cpu != nullptr)
    {
        const u32 secondary_size = runtime_shader_instance_size(*node.secondary_shader_cpu);
        if (secondary_size != 0)
        {
            const u32 offset = runtime_shader_instance_size(*node.shader_cpu);
            u8* secondary_storage = reinterpret_cast<u8*>(
                reinterpret_cast<std::uintptr_t>(output.primary_storage) + offset);
            output.secondary = &output.secondary_instance;
            initialize_runtime_shader_instance(*node.secondary_shader_cpu, output.secondary_instance,
                std::span<u8>(secondary_storage, secondary_size));
            return;
        }
        output.secondary = nullptr;
    }
}

// Xbox 0x00155D00. This getter returns the original allocator callback;
// allocation itself remains the caller's original indirect heap boundary.
const RuntimeModelGpuAllocator& runtime_model_gpu_allocator(const RuntimeBlendshapeBoundaries& boundaries)
{
    return boundaries.allocator;
}

// Xbox 0x00157550. Output arrives in EDX. The source resource's six words
// overwrite the temporary allocated-data pointer before D3D registration,
// as in the assembly; the allocation is not substituted into the record.
void copy_runtime_blendshape_buffers(const RuntimeModelNode& node,
    RuntimeBlendshapeNodeInstance& output, const RuntimeBlendshapeBoundaries& boundaries)
{
    u32 index = 0;
    if (node.blendshape_cpu->buffers.size() != 0)
    {
        do
        {
            const auto& source = node.blendshape_cpu->buffers[index];
            const u32 size = source.words[5];
            auto& destination = output.buffers[index];
            if (size == 0)
            {
                destination.words = {0, 0, 0, 0, 0, 0};
                destination.data = nullptr;
            }
            else
            {
                const auto& allocator = runtime_model_gpu_allocator(boundaries);
                u8* data = allocator(size, 0x80, ".\\nodes\\geometry\\blendshape.c", 0x1EA);
                const u8* from = source.data;
                u8* to = data;
                for (u32 remaining = size >> 2; remaining != 0; --remaining)
                {
                    std::memcpy(to, from, 4);
                    to += 4;
                    from += 4;
                }
                for (u32 remaining = size & 3; remaining != 0; --remaining)
                {
                    *to++ = *from++;
                }
                destination.data = data;
                destination.words = source.words;
                destination.data = source.data;
                boundaries.register_resource(destination, destination.data);
            }
            ++index;
        } while (index < node.blendshape_cpu->buffers.size());
    }
}

// Xbox 0x00157AA0.
void copy_runtime_node_blendshape(const RuntimeModelNode& node, RuntimeBlendshapeNodeInstance& output,
    std::span<u8> instance, const RuntimeBlendshapeBoundaries& boundaries)
{
    u8* destination = instance.data() + node.copy_offset;
    if (node.bytes[0x26] != 0)
    {
        copy_runtime_blendshape_buffers(node, output, boundaries);
    }
    output.weights = reinterpret_cast<float*>(destination + 0x1C8);
    if (node.bytes[0x26] != 0)
    {
        u32 offset = 0;
        u32 index = 0;
        if (node.blendshape_cpu->buffers.size() != 0)
        {
            do
            {
                const auto& source = node.blendshape_cpu->buffers[index];
                const u8 type = u8(source.words[0] >> 8);
                if (type == 9 || type == 10)
                {
                    u16 count;
                    std::memcpy(&count, node.bytes.data() + 0x24, 2);
                    output.attribute_targets[index] = destination + 0x1C8 + u32(count) * 4 + offset - 4;
                    offset += node.blendshape_cpu->buffers[index].words[5];
                }
                else
                {
                    output.attribute_targets[index] = nullptr;
                }
                ++index;
            } while (index < node.blendshape_cpu->buffers.size());
        }
    }
    u32 index = 0;
    u16 count;
    std::memcpy(&count, node.bytes.data() + 0x24, 2);
    if (count != 1)
    {
        do
        {
            output.weights[index] = 0;
            ++index;
            std::memcpy(&count, node.bytes.data() + 0x24, 2);
        } while (index < u32(count) - 1);
    }
    copy_runtime_node_empty(node, instance);
}

// Xbox 0x00052F40.
u32 runtime_model_shader_storage_size(std::span<const RuntimeModelResourceBlock> blocks)
{
    u32 index = 0;
    if (blocks.size() != 0)
    {
        do
        {
            if (blocks[index].type == 3)
            {
                const u8* data = blocks[index].data;
                if (data == nullptr)
                {
                    return 0;
                }
                u32 count;
                std::memcpy(&count, data, 4);
                return count * 0x98 + 4;
            }
            ++index;
        } while (index < blocks.size());
    }
    return 0;
}

// Xbox 0x000FDF60. As on Xbox, a full registration table is not guarded.
void register_runtime_model_shaders(u8* shader_storage, RuntimeShaderRegistrationState& state)
{
    if (shader_storage != nullptr)
    {
        i32 remaining;
        std::memcpy(&remaining, shader_storage, 4);
        if (remaining > 0)
        {
            u8* record = shader_storage + 4;
            do
            {
                i32 index = 0;
                do
                {
                    if (state.records[size_t(index)] == nullptr)
                    {
                        break;
                    }
                    ++index;
                } while (index < 40);
                if (index == 40)
                {
                    index = -1;
                }
                record[0x66] |= 1;
                state.records.data()[index] = record;
                ++state.count;
                record += 0x98;
                --remaining;
            } while (remaining != 0);
        }
    }
}

// Xbox 0x00165260. Every case calls the original D3DX boundary; unknown
// axes leave the output alone.
void runtime_shader_rotation(RuntimeShaderMatrix& output, char axis, float radians,
    const RuntimeShaderMatrixBoundaries& boundaries)
{
    switch (axis)
    {
    case 'X':
    case 'x':
        boundaries.rotation_x(output, radians);
        return;
    case 'Y':
    case 'y':
        boundaries.rotation_y(output, radians);
        return;
    case 'Z':
    case 'z':
        boundaries.rotation_z(output, radians);
    }
}

// Xbox 0x001658B0. +0x24 is a float accumulator in otherwise retained
// scratch storage. The original keeps the unrounded accumulator for FSIN.
void tick_runtime_shader_animation(RuntimeShaderAnimation& animation, float step,
    const RuntimeShaderMatrixBoundaries& boundaries)
{
    const long double rotation = static_cast<long double>(step) * animation.rates[2] +
        animation.position[2];
    animation.position[2] = float(rotation);
    const float old_phase = std::bit_cast<float>(animation.retained[0]);
    const long double phase = static_cast<long double>(step) + old_phase;
    animation.retained[0] = std::bit_cast<u32>(float(phase));
    const long double wave = std::sin(phase * static_cast<long double>(0.04f));
    const long double x = static_cast<long double>(step) * animation.rates[0] +
        wave * animation.rates[3] + animation.position[0];
    animation.position[0] = float(x);
    const long double y = static_cast<long double>(step) * animation.rates[1] +
        wave * animation.rates[4] + animation.position[1];
    animation.position[1] = float(y);
    RuntimeShaderMatrix output;
    if (rotation != 0.0L)
    {
        RuntimeShaderMatrix first;
        RuntimeShaderMatrix second;
        RuntimeShaderMatrix third;
        RuntimeShaderMatrix translation;
        boundaries.translation(first, -0.5f, -0.5f, 0.0f);
        runtime_shader_rotation(second, 'z',
            float(static_cast<long double>(animation.position[2]) *
                static_cast<long double>(0.017453292f)), boundaries);
        boundaries.multiply(third, first, second);
        boundaries.translation(first, 0.5f, 0.5f, 0.0f);
        boundaries.multiply(second, third, first);
        boundaries.translation(translation, animation.position[0], animation.position[1], 0.0f);
        boundaries.multiply(output, translation, second);
        boundaries.transpose(output, output);
        std::memcpy(animation.parameter->bytes.data(), output.data(), 64);
        return;
    }
    boundaries.translation(output, float(x), float(y), 0.0f);
    boundaries.transpose(output, output);
    std::memcpy(animation.parameter->bytes.data(), output.data(), 64);
}

// Xbox 0x00165A00.
void tick_runtime_model_shader_animations(RuntimeModelShaderAnimations& animations, float step,
    const RuntimeShaderMatrixBoundaries& boundaries)
{
    i32 list_index = 0;
    if (i32(animations.count) > 0)
    {
        do
        {
            RuntimeShaderAnimationList& list = *animations.entries[size_t(list_index)];
            i32 entry_index = 0;
            if (i32(list.count) > 0)
            {
                do
                {
                    tick_runtime_shader_animation(list.entries[size_t(entry_index)], step, boundaries);
                    ++entry_index;
                } while (entry_index < i32(list.count));
            }
            ++list_index;
        } while (list_index < i32(animations.count));
    }
}

// Xbox 0x0015B0C0.
const RuntimeShaderCpuEntry* runtime_shader_entry(const RuntimeShaderCpu& shader, u32 index)
{
    return &shader.entries[index];
}

// Xbox 0x0015B0E0. A miss does not write the output.
bool find_runtime_shader_entry(const RuntimeShaderCpu& shader, const char* name,
    const RuntimeShaderCpuEntry*& result)
{
    u32 index = 0;
    if (shader.entries.size() != 0)
    {
        do
        {
            if (compare_runtime_model_name(name, shader.entries[index].name) == 0)
            {
                result = &shader.entries[index];
                return true;
            }
            ++index;
        } while (index < shader.entries.size());
    }
    return false;
}

// Xbox 0x0015B1F0.
bool find_runtime_shader_parameter(const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    const char* name, RuntimeShaderParameter*& result)
{
    const RuntimeShaderCpuEntry* entry;
    if (find_runtime_shader_entry(shader, name, entry))
    {
        result = reinterpret_cast<RuntimeShaderParameter*>(
            reinterpret_cast<std::intptr_t>(instance.parameters.data()) +
            std::intptr_t(entry->value) * 0x44);
        return true;
    }
    return false;
}

// Xbox 0x0015B230. The original does not test the lookup result.
void set_runtime_shader_parameter_flag(const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    const char* name, u8 flag)
{
    const RuntimeShaderCpuEntry* entry;
    find_runtime_shader_entry(shader, name, entry);
    instance.parameters[size_t(entry->value)].bytes[0x40] = flag;
}

// Xbox 0x00165300. Prefix arrives in EAX in the original.
void runtime_shader_animation_name(const char* prefix, const char* parameter, char* output)
{
    char* target = output;
    char value;
    do
    {
        value = *prefix++;
        *target++ = value;
    } while (value != '\0');
    target = output;
    while (*target != '\0')
    {
        ++target;
    }
    *target++ = '_';
    *target = '\0';
    do
    {
        value = *parameter++;
        *target++ = value;
    } while (value != '\0');
}

// Xbox 0x00165360. Assembly loads the returned entry pointer before its +8
// signed integer; Ghidra incorrectly treats that stack pointer as an array.
std::array<float, 5> runtime_shader_animation_rates(const RuntimeShaderCpu& shader,
    const char* parameter)
{
    std::array<float, 5> rates{};
    constexpr std::array<const char*, 5> suffixes{"_du", "_dv", "_drotate", "_pu", "_pv"};
    for (size_t index = 0; index < suffixes.size(); ++index)
    {
        char name[100];
        char* target = name;
        const char* source = parameter;
        char value;
        do
        {
            value = *source++;
            *target++ = value;
        } while (value != '\0');
        --target;
        source = suffixes[index];
        do
        {
            value = *source++;
            *target++ = value;
        } while (value != '\0');
        const RuntimeShaderCpuEntry* entry;
        if (find_runtime_shader_entry(shader, name, entry))
        {
            rates[index] = float(entry->value);
        }
    }
    return rates;
}

// Xbox 0x00165570.
std::unique_ptr<RuntimeShaderAnimationList> make_runtime_shader_animation_list(
    const char* prefix, const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    RuntimeShaderAnimationScratch& scratch)
{
    u32 index = 0;
    if (shader.entries.size() != 0)
    {
        u32 count = 0;
        do
        {
            const RuntimeShaderCpuEntry* entry = runtime_shader_entry(shader, index);
            if (std::strncmp(entry->name, "xf", 2) == 0)
            {
                RuntimeShaderParameter* parameter;
                if (find_runtime_shader_parameter(shader, instance, entry->name, parameter))
                {
                    const auto rates = runtime_shader_animation_rates(shader, entry->name);
                    if (rates[2] != 0.0f || rates[0] != 0.0f || rates[1] != 0.0f ||
                        rates[3] != 0.0f || rates[4] != 0.0f)
                    {
                        RuntimeShaderAnimation& animation = scratch.animations[count];
                        animation.parameter = parameter;
                        animation.position = {0.0f, 0.0f, 0.0f};
                        for (size_t rate = 0; rate < rates.size(); ++rate)
                        {
                            animation.rates[rate] = float(static_cast<long double>(rates[rate]) *
                                static_cast<long double>(0.0001f));
                        }
                        char name[100];
                        runtime_shader_animation_name(prefix, entry->name, name);
                        ++count;
                        set_runtime_shader_parameter_flag(shader, instance, entry->name, 1);
                    }
                }
            }
            ++index;
        } while (index < shader.entries.size());
        if (count != 0)
        {
            std::unique_ptr<RuntimeShaderAnimationList> result(new (std::nothrow) RuntimeShaderAnimationList);
            result->count = count;
            result->entries.reset(new (std::nothrow) RuntimeShaderAnimation[count]);
            for (u32 entry = 0; entry < count; ++entry)
            {
                result->entries[entry] = scratch.animations[entry];
            }
            return result;
        }
    }
    return nullptr;
}

// Xbox 0x001657A0. Count is passed in EBX and the tree in ESI.
void collect_runtime_model_shader_animations(const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance,
    RuntimeShaderAnimationScratch& scratch, u32& count)
{
    const i32 total = i32(runtime_model_named_count(tree));
    i32 index = 0;
    if (total > 0)
    {
        do
        {
            const char* name;
            RuntimeModelNode* node;
            runtime_model_named_entry(tree, index, name, node);
            if (std::strncmp(name, "shader_", 7) == 0)
            {
                auto list = make_runtime_shader_animation_list(name + 7, *node->shader_cpu,
                    *instance.shader_nodes.at(node->copy_offset).primary, scratch);
                if (list != nullptr)
                {
                    scratch.lists[count] = list.release();
                    ++count;
                }
            }
            ++index;
        } while (index < total);
    }
}

// Xbox 0x00165830. The recovered callee's EBX count output retains this
// allocation branch, which the uncorrected Ghidra prototype removed.
std::unique_ptr<RuntimeModelShaderAnimations> make_runtime_model_shader_animations(
    const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance,
    RuntimeShaderAnimationScratch& scratch)
{
    u32 count = 0;
    collect_runtime_model_shader_animations(tree, instance, scratch, count);
    if (count != 0)
    {
        std::unique_ptr<RuntimeModelShaderAnimations> result(new (std::nothrow) RuntimeModelShaderAnimations);
        result->count = count;
        result->entries.reset(new (std::nothrow) std::unique_ptr<RuntimeShaderAnimationList>[count]);
        for (u32 index = 0; index < (count & 0x3FFFFFFF); ++index)
        {
            result->entries[index].reset(scratch.lists[index]);
        }
        return result;
    }
    return nullptr;
}

// These are the 17 copy-slot bindings installed by Xbox 0x00155A10, in
// registration order. The type-specific callbacks have no substitute path.
std::array<RuntimeModelNodeType, 17> runtime_model_copy_types(const RuntimeBlendshapeBoundaries& boundaries)
{
    const auto empty = [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
    {
        copy_runtime_node_empty(node, instance.bytes);
    };
    const auto thunk = [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
    {
        copy_runtime_node_empty_thunk(node, instance.bytes);
    };
    const auto transform = [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
    {
        copy_runtime_node_transform(node, instance.bytes);
    };
    return
    {
        RuntimeModelNodeType{"ndGroup", empty},
        RuntimeModelNodeType{"ndPixelShaderParam", empty},
        RuntimeModelNodeType{"ndPixelShader", empty},
        RuntimeModelNodeType{"ndVertexShader", thunk},
        RuntimeModelNodeType{"ndVertexBuffer", empty},
        RuntimeModelNodeType{"ndPushBuffer", thunk},
        RuntimeModelNodeType{"ndBGPushBuffer", [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
            {
                copy_runtime_node_bg_push_buffer(node, instance.bytes);
            }},
        RuntimeModelNodeType{"ndSkeleton", empty},
        RuntimeModelNodeType{"ndVisibility", [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
            {
                copy_runtime_node_visibility(node, instance.bytes);
            }},
        RuntimeModelNodeType{"ndTransform", transform},
        RuntimeModelNodeType{"nd2D", transform},
        RuntimeModelNodeType{"ndShaderParam2", [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
            {
                copy_runtime_node_shader_parameters(node, instance.shader_nodes[node.copy_offset], instance.bytes);
            }},
        RuntimeModelNodeType{"ndShader2", empty},
        RuntimeModelNodeType{"ndMtxArray", [](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
            {
                copy_runtime_node_matrix_array(node, instance.bytes);
            }},
        RuntimeModelNodeType{"ndRigidSkinIdx", empty},
        RuntimeModelNodeType{"ndVtxDesc", thunk},
        RuntimeModelNodeType{"ndBlendShape", [&boundaries](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
            {
                copy_runtime_node_blendshape(node, instance.blendshape_nodes[node.copy_offset],
                    instance.bytes, boundaries);
            }}
    };
}

// Xbox 0x000F9840.
std::unique_ptr<RuntimeModelInstance> make_runtime_model_instance(const RuntimeModelAsset& asset,
    int create_pose, int copy_shaders, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch)
{
    const RuntimeModelTree& tree = *asset.tree;
    find_runtime_model_node_by_name(tree, 0, "ndMtxArray", types);
    std::unique_ptr<RuntimeModelInstance> result(new (std::nothrow) RuntimeModelInstance);
    const u8* source_shaders = nullptr;
    u32 shader_size = 0;
    if (copy_shaders != 0)
    {
        u32 index = 0;
        if (asset.blocks.size() != 0)
        {
            do
            {
                if (asset.blocks[index].type == 3)
                {
                    const u8* data = asset.blocks[index].data;
                    u32 count;
                    if (data != nullptr)
                    {
                        std::memcpy(&count, data, 4);
                        if (count != 0)
                        {
                            source_shaders = data;
                            shader_size = runtime_model_shader_storage_size(asset.blocks);
                        }
                    }
                    break;
                }
                ++index;
            } while (index < asset.blocks.size());
        }
    }
    result->allocation_size = u32(runtime_model_instance_size(tree) + 0x2A0 + shader_size);
    result->allocation.reset(new (std::nothrow) u8[result->allocation_size]);
    if (source_shaders != nullptr)
    {
        result->shader_storage = result->allocation.get() + 0x2A0 + runtime_model_instance_size(tree);
        const u32 size = runtime_model_shader_storage_size(asset.blocks);
        std::memcpy(result->shader_storage, source_shaders, size);
        register_runtime_model_shaders(result->shader_storage, registration);
    }
    else
    {
        result->shader_storage = nullptr;
    }
    result->source = &asset;
    result->skeleton_node = find_runtime_model_node_by_name(tree, 0, "ndSkeleton", types);
    result->node_flags = {0, 0, 0, 0, 0, 0, 0, 0};
    result->shader_indices.fill(0xFFFFFFFF);
    result->tree_storage.bytes = std::span<u8>(result->allocation.get() + 0x2A0,
        runtime_model_instance_size(tree));
    std::memset(result->tree_storage.bytes.data(), 0, tree.instance_size);
    copy_runtime_model_tree(tree, result->tree_storage);
    if (create_pose == 0 || result->skeleton_node == nullptr)
    {
        result->primary = nullptr;
    }
    else
    {
        result->primary_pose.count = int(result->skeleton_node->joints.size());
        result->primary_pose.matrices.resize(size_t(result->primary_pose.count));
        initialize_model_pose(result->primary_pose);
        result->primary = &result->primary_pose;
    }
    result->alternate = nullptr;
    bind_model_auxiliary_targets(result->auxiliary_targets, tree, result->tree_storage.bytes);
    result->state_290 = 0;
    result->state_1c4 = 0xFFFFFFFF;
    result->state_1c0 = 0;
    std::memset(result->allocation.get() + 4, 0, 0x56 * 4);
    const u32 zero = 0;
    std::memcpy(result->allocation.get() + 0x15C, &zero, 4);
    result->shader_animations.reset();
    result->shader_animations = make_runtime_model_shader_animations(tree, result->tree_storage, scratch);
    return result;
}

// Xbox 0x001560A0. The count is reloaded at each iteration on Xbox.
void copy_runtime_matrix_array_flags(const RuntimeModelNode& source_node, std::span<const u8> source,
    const RuntimeModelNode& node, std::span<u8> instance)
{
    i32 count;
    std::memcpy(&count, source_node.bytes.data() + 0x30, 4);
    i32 index = 0;
    if (0 < count)
    {
        size_t offset = 0;
        do
        {
            const u8 joint = source_node.matrix_records[size_t(index) * 8 + 6];
            if ((source[size_t(count) * 0x40 + joint + source_node.copy_offset + 4] & 1) != 0)
            {
                std::memcpy(instance.data() + offset + node.copy_offset + 4,
                    source.data() + offset + source_node.copy_offset + 4, 0x40);
                i32 copies;
                std::memcpy(&copies, node.bytes.data() + 0x30, 4);
                instance[size_t(copies) * 0x40 + size_t(index) + 4 + node.copy_offset] = 1;
            }
            std::memcpy(&count, source_node.bytes.data() + 0x30, 4);
            ++index;
            offset += 0x40;
        } while (index < count);
    }
}

// Xbox 0x000F8B80. The eight node flag bytes at +0x294.
void copy_runtime_model_node_flags(RuntimeModelInstance& model, const RuntimeModelInstance& source)
{
    if (&model != &source)
    {
        model.node_flags = source.node_flags;
        std::memcpy(model.allocation.get() + 0x294, source.allocation.get() + 0x294, 8);
    }
}

// Xbox 0x000F87D0. The header copy carries the source's asset, skeleton node, node flags and state words.
std::unique_ptr<RuntimeModelInstance> clone_runtime_model_instance(const RuntimeModelInstance& source,
    std::span<const RuntimeModelNodeType> types, AnimationResourceRegistry& registry,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch)
{
    const RuntimeModelAsset& asset = *source.source;
    const RuntimeModelTree& tree = *asset.tree;
    bool shaders = false;
    u32 index = 0;
    if (asset.blocks.size() != 0)
    {
        do
        {
            if (asset.blocks[index].type == 3)
            {
                const u8* data = asset.blocks[index].data;
                u32 count = 0;
                if (data != nullptr)
                {
                    std::memcpy(&count, data, 4);
                }
                shaders = data != nullptr && count != 0;
                break;
            }
            ++index;
        } while (index < asset.blocks.size());
    }
    std::unique_ptr<RuntimeModelInstance> result(new (std::nothrow) RuntimeModelInstance);
    const u32 tree_size = runtime_model_instance_size(tree);
    if (shaders)
    {
        const u32 size = runtime_model_shader_storage_size(asset.blocks);
        result->allocation_size = size + tree_size + 0x2A0;
        result->allocation.reset(new (std::nothrow) u8[result->allocation_size]);
        std::memcpy(result->allocation.get(), source.allocation.get(), size + tree_size + 0x2A0);
        result->shader_storage = result->allocation.get() + tree_size + 0x2A0;
        register_runtime_model_shaders(result->shader_storage, registration);
    }
    else
    {
        result->allocation_size = tree_size + 0x2A0;
        result->allocation.reset(new (std::nothrow) u8[result->allocation_size]);
        std::memcpy(result->allocation.get(), source.allocation.get(), tree_size + 0x2A0);
        result->shader_storage = source.shader_storage;
    }
    result->source = source.source;
    result->skeleton_node = source.skeleton_node;
    result->node_flags = source.node_flags;
    result->state_290 = source.state_290;
    result->state_1c4 = source.state_1c4;
    result->state_1c0 = source.state_1c0;
    result->state_1c8 = source.state_1c8;
    result->mirror_pass = source.mirror_pass;
    result->tree_storage.bytes = std::span<u8>(result->allocation.get() + 0x2A0, tree_size);
    std::memset(result->tree_storage.bytes.data(), 0, tree.instance_size);
    copy_runtime_model_tree(tree, result->tree_storage);
    const RuntimeModelNode* matrices = find_runtime_model_node_by_name(tree, 0, "ndMtxArray", types);
    const RuntimeModelNode* copies = find_runtime_model_node_by_name(*result->source->tree, 0, "ndMtxArray",
        types);
    if (matrices != nullptr && copies != nullptr)
    {
        copy_runtime_matrix_array_flags(*matrices, source.tree_storage.bytes, *copies, result->tree_storage.bytes);
    }
    retain_animation_resource(registry, asset.cpu);
    result->shader_indices.fill(0xFFFFFFFF);
    if (source.primary != nullptr)
    {
        result->primary_pose = *source.primary;
        result->primary = &result->primary_pose;
    }
    else
    {
        result->primary = nullptr;
    }
    result->alternate = nullptr;
    bind_model_auxiliary_targets(result->auxiliary_targets, tree, result->tree_storage.bytes);
    copy_runtime_model_node_flags(*result, source);
    std::memset(result->allocation.get() + 4, 0, 0x56 * 4);
    result->shader_animations.reset();
    result->shader_animations = make_runtime_model_shader_animations(tree, result->tree_storage, scratch);
    return result;
}

// Xbox 0x000F8A10, EAX name. The joint loop runs over the source skeleton's count and reads the source tree's named
// entry at each index; the new model's entry of that name gives the joint (its +0 word) the matrix is copied to.
std::unique_ptr<RuntimeModelInstance> make_runtime_model_copy_by_name(const char* name,
    const RuntimeModelInstance& source, AnimationResourceRegistry& registry, ResourceErrorState& error,
    const RuntimeModelResourceViews& views, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch)
{
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    if (prefix != 0x5F646961)
    {
        return nullptr;
    }
    const auto resource = acquire_animation_resource(registry, name); // 0x00035E90
    if (resource.data() == nullptr)
    {
        resource_error(error, 2); // 0x00032700
        return nullptr;
    }
    const RuntimeModelAsset& asset = *views.assets.at(resource.data());
    std::unique_ptr<RuntimeModelInstance> result = make_runtime_model_instance(asset, 1, 1, types, registration,
        scratch); // 0x000F9840
    const RuntimeModelTree* source_tree = source.source->tree;
    i32 index = 0;
    while (true)
    {
        const i32 count = source.skeleton_node == nullptr ? 0
                                                           : i32(source.skeleton_node->joints.size()); // 0x00156A20
        if (count <= index)
        {
            break;
        }
        const char* joint = nullptr;
        if (source_tree != nullptr)
        {
            if (source_tree->names == nullptr || size_t(index) >= source_tree->names->entries.size())
            {
                throw Error("0x00156970 reads a named entry the source model's tree does not have");
            }
            RuntimeModelNode* node;
            runtime_model_named_entry(*source_tree, index, joint, node); // 0x00156970
        }
        RuntimeModelNode* found = nullptr;
        if (asset.tree != nullptr && find_runtime_model_named_node(*asset.tree, joint, found) // 0x001568E0
            && found != nullptr)
        {
            ModelPoseBuffer* to = result->alternate != nullptr ? result->alternate : result->primary;
            const ModelPoseBuffer* from = source.alternate != nullptr ? source.alternate : source.primary;
            if (to == nullptr || from == nullptr)
            {
                throw Error("0x00031530 reads a pose the model copy 0x000F8A10 does not have");
            }
            // 0x00031530 twice: the target's matrix (DAT_004DD798 when the pose is packed), then the source's.
            static AnimationMatrix scratch_matrix;
            AnimationMatrix& target = to->dirty ? to->matrices.at(size_t(found->instance_offset)) : scratch_matrix;
            target = model_pose_matrix(*from, index);
        }
        ++index;
    }
    ModelPoseBuffer* pose = result->alternate != nullptr ? result->alternate : result->primary;
    if (pose != nullptr)
    {
        pose->dirty = true;
    }
    std::memset(result->allocation.get() + 4, 0, 0x56 * 4);
    return result;
}

// Xbox 0x001574B0.
void release_runtime_blendshape_node(const RuntimeModelNode& node,
    RuntimeModelInstanceStorage& instance, const RuntimeBlendshapeBoundaries& boundaries)
{
    if (node.bytes[0x26] != 0)
    {
        u32 index = 0;
        if (node.blendshape_cpu->buffers.size() != 0)
        {
            auto& output = instance.blendshape_nodes.at(node.copy_offset);
            do
            {
                const auto& source = node.blendshape_cpu->buffers[index];
                auto& buffer = output.buffers[index];
                if (source.words[5] != 0 && buffer.data != nullptr && buffer.words[5] != 0)
                {
                    int busy;
                    do
                    {
                        busy = boundaries.is_busy(buffer);
                    } while (busy != 0);
                    boundaries.free_physical(buffer.data, ".\\nodes\\geometry\\blendshape.c", 0x18E);
                    buffer.data = nullptr;
                    buffer.words[4] = 0;
                }
                ++index;
            } while (index < node.blendshape_cpu->buffers.size());
        }
    }
}

// Xbox 0x00156160.
void release_runtime_model_nodes(const RuntimeModelNode* first, RuntimeModelInstanceStorage& instance,
    const RuntimeBlendshapeBoundaries& boundaries)
{
    for (; first != nullptr; first = first->sibling)
    {
        if (std::memcmp(first->type_name, "ndVertexShader", 15) == 0)
        {
            runtime_model_noop();
        }
        else if (std::memcmp(first->type_name, "ndBlendShape", 13) == 0)
        {
            release_runtime_blendshape_node(*first, instance, boundaries);
        }
        if (first->child != nullptr)
        {
            release_runtime_model_nodes(first->child, instance, boundaries);
        }
    }
}

// Xbox 0x00156210.
void release_runtime_model_tree_nodes(const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance,
    const RuntimeBlendshapeBoundaries& boundaries)
{
    u32 index = 0;
    if (tree.roots.size() != 0)
    {
        do
        {
            release_runtime_model_nodes(tree.roots[index], instance, boundaries);
            ++index;
        } while (index < tree.roots.size());
    }
}

// Xbox 0x000662A0. vector storage represents the original heap realloc;
// the used record count and grow word retain the cursor/limit arithmetic.
void grow_runtime_model_deferred_pool(RuntimeModelDeferredPool& pool)
{
    const u32 used = u32(pool.records.size());
    const u32 count = u32(pool.grow) + used;
    pool.records.reserve(count);
    pool.limit = count;
}

// Xbox 0x00165A60.
void release_runtime_model_shader_animations(std::unique_ptr<RuntimeModelShaderAnimations>& animations)
{
    i32 index = 0;
    if (i32(animations->count) > 0)
    {
        do
        {
            auto& list = animations->entries[size_t(index)];
            list->entries.reset();
            list.reset();
            ++index;
        } while (index < i32(animations->count));
    }
    animations->entries.reset();
    animations.reset();
}

// Xbox 0x000FDFC0. heap_free is the original owning-heap HeapFree boundary.
void unregister_runtime_model_shaders(u8* shader_storage, RuntimeShaderRegistrationState& state,
    const std::function<void(u8*)>& heap_free)
{
    if (shader_storage != nullptr)
    {
        i32 remaining;
        std::memcpy(&remaining, shader_storage, 4);
        if (remaining > 0)
        {
            u8* record = shader_storage + 4;
            do
            {
                i32 index = 0;
                do
                {
                    if (record == state.records[size_t(index)])
                    {
                        const std::int8_t slot = std::int8_t(index);
                        if (slot != -1)
                        {
                            u8* registered = state.records[size_t(slot)];
                            if ((registered[0x66] & 0x10) == 0)
                            {
                                heap_free(registered);
                            }
                            state.records[size_t(slot)] = nullptr;
                            --state.count;
                        }
                        break;
                    }
                    ++index;
                } while (index < 40);
                record += 0x98;
                --remaining;
            } while (remaining != 0);
        }
    }
}

// Xbox 0x000F8BF0. This only queues the model allocation; the original
// deferred collector owns the later fence test and final heap release.
const RuntimeModelAsset* defer_runtime_model_release(RuntimeModelInstance& instance,
    RuntimeModelDeferredPool& pool, u32 fence, AnimationResourceRegistry& registry,
    RuntimeShaderRegistrationState& registration, const std::function<void(u8*)>& heap_free)
{
    if (pool.records.size() == pool.limit)
    {
        grow_runtime_model_deferred_pool(pool);
    }
    pool.records.push_back(RuntimeModelDeferredRelease{&instance, fence});
    if (instance.shader_animations != nullptr)
    {
        release_runtime_model_shader_animations(instance.shader_animations);
    }
    unregister_runtime_model_shaders(instance.shader_storage, registration, heap_free);
    retain_animation_resource(registry, instance.source->cpu);
    return instance.source;
}

// Xbox 0x00031530
const AnimationMatrix& model_pose_matrix(const ModelPoseBuffer& pose, int joint)
{
    if (pose.dirty)
    {
        return pose.matrices[size_t(joint)];
    }
    static AnimationMatrix scratch; // DAT_004DD798, shared retail scratch
    const auto& source = pose.packed_matrices[size_t(joint)];
    scratch = {source[0], source[4], source[8], source[12],
        source[1], source[5], source[9], source[13],
        source[2], source[6], source[10], source[14]};
    return scratch;
}

// Xbox 0x000316B0
const AnimationMatrix& model_joint_world_matrix(const ModelPoseBuffer& pose, int index,
    const Joint& joint, const AnimationMatrix& world)
{
    static AnimationMatrix scratch; // DAT_0050D7A0
    scratch = world;
    const auto& matrix = model_pose_matrix(pose, index);
    multiply_affine_matrices(scratch.data(), scratch.data(), matrix.data());
    const AnimationMatrix translation{1.f, 0.f, 0.f, joint.world.x,
        0.f, 1.f, 0.f, joint.world.y, 0.f, 0.f, 1.f, joint.world.z};
    multiply_affine_matrices(scratch.data(), scratch.data(), translation.data());
    return scratch;
}

// Xbox 0x00051E40
void model_bone_world_matrix(const ModelBoneBinding& binding, const ModelPoseBuffer* pose,
    const AnimationMatrix& world, AnimationMatrix& output)
{
    if (binding.joint != -1)
    {
        const auto& matrix = model_pose_matrix(*pose, binding.joint);
        multiply_affine_matrices(output.data(), world.data(), matrix.data());
        multiply_affine_matrices(output.data(), output.data(), binding.matrix.data());
        return;
    }
    output = world;
    multiply_affine_matrices(output.data(), output.data(), binding.matrix.data());
}

// Xbox 0x00051CE0
void model_bone_world_point(const ModelBoneBinding& binding, const ModelPoseBuffer* pose,
    const AnimationMatrix* world, Vec3& output)
{
    output = Vec3{binding.matrix[3], binding.matrix[7], binding.matrix[11]};
    if (binding.joint >= 0)
    {
        const auto& matrix = model_pose_matrix(*pose, binding.joint);
        const float x = output.x;
        const float y = output.y;
        const float z = output.z;
        output.x = matrix[2] * z + matrix[1] * y + matrix[0] * x + matrix[3];
        output.y = matrix[4] * x + matrix[6] * z + matrix[5] * y + matrix[7];
        output.z = matrix[8] * x + matrix[10] * z + matrix[9] * y + matrix[11];
    }
    if (world != nullptr)
    {
        const auto& matrix = *world;
        const float x = output.x;
        const float y = output.y;
        const float z = output.z;
        output.x = matrix[2] * z + matrix[1] * y + matrix[0] * x + matrix[3];
        output.y = matrix[4] * x + matrix[6] * z + matrix[5] * y + matrix[7];
        output.z = matrix[8] * x + matrix[10] * z + matrix[9] * y + matrix[11];
    }
}

// Xbox 0x00031470. The pose allocation is represented by the host's owned vector.
ModelPoseBuffer* release_runtime_model_pose(ModelPoseBuffer* pose)
{
    pose->matrices = std::vector<AnimationMatrix>{};
    pose->packed_matrices = std::vector<std::array<float, 16>>{};
    return nullptr;
}

// Xbox 0x000F8C50.
const RuntimeModelAsset* release_runtime_model_instance(RuntimeModelInstance& instance,
    AnimationResourceRegistry& registry, const RuntimeBlendshapeBoundaries& blendshape,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    const RuntimeModelAsset* source = instance.source;
    release_runtime_model_tree_nodes(*source->tree, instance.tree_storage, blendshape);
    if (instance.primary != nullptr)
    {
        instance.primary = release_runtime_model_pose(instance.primary);
    }
    const u32 address = u32(reinterpret_cast<std::uintptr_t>(instance.allocation.get()));
    release_runtime_model_gpu_cache(RuntimeRelocationImage{
        std::span<u8>(instance.allocation.get(), instance.allocation_size), address}, address, heap, d3d);
    if (instance.shader_animations != nullptr)
    {
        release_runtime_model_shader_animations(instance.shader_animations);
    }
    instance.allocation.reset();
    release_animation_resource(registry, source->cpu);
    return source;
}

// Xbox 0x000F9780. Removing a record replaces it with the last used record,
// then revisits that position; it does not shift the remaining records.
void collect_runtime_model_releases(RuntimeModelDeferredPool& pool, u32& fence,
    AnimationResourceRegistry& registry, const RuntimeBlendshapeBoundaries& blendshape,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    ++fence;
    size_t end = pool.records.size();
    size_t index = 0;
    if (index < end)
    {
        do
        {
            if (u32(fence - pool.records[index].fence) > 1)
            {
                release_runtime_model_instance(*pool.records[index].instance, registry, blendshape, heap, d3d);
                if (index < pool.records.size() - 1)
                {
                    pool.records[index] = std::move(pool.records.back());
                }
                --end;
                pool.records.pop_back();
                --index;
            }
            ++index;
        } while (index < end);
    }
}

// Xbox 0x000F97F0.
void flush_runtime_model_releases(RuntimeModelDeferredPool& pool, u32& fence,
    AnimationResourceRegistry& registry, const RuntimeBlendshapeBoundaries& blendshape,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    fence = 0;
    size_t index = 0;
    const size_t end = pool.records.size();
    if (end <= index)
    {
        pool.records.clear();
        return;
    }
    do
    {
        release_runtime_model_instance(*pool.records[index].instance, registry, blendshape, heap, d3d);
        ++index;
    } while (index < end);
    pool.records.clear();
}

// Xbox 0x0003BFD0
void set_runtime_actor_resource(RuntimeActorResourceReference& reference, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (reference.resource.data() != nullptr)
    {
        release_animation_resource(registry, reference.resource);
        reference.resource = {};
    }
    if (name == nullptr)
    {
        reference.name[0] = '\0';
        return;
    }
    std::span<const u8> resource;
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    if (prefix == 0x5F646961)
    {
        resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
        }
    }
    else
    {
        resource = {};
    }
    reference.resource = resource;
    size_t index = 0;
    char value;
    do
    {
        value = name[index];
        reference.name[index] = value;
        ++index;
    }
    while (value != '\0');
}

// Xbox 0x00040C00. Relocated CPU pointers are represented by their typed
// asset views; the original factory passes that acquired CPU pointer in EAX.
bool set_runtime_actor_model(RuntimeActorModelState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error, const RuntimeModelResourceViews& views,
    RuntimeModelDeferredPool& deferred, u32 fence, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch,
    const std::function<void(u8*)>& heap_free)
{
    actor.name[0] = '\0';
    if (actor.model != nullptr)
    {
        const RuntimeModelAsset* old = defer_runtime_model_release(*actor.model, deferred, fence,
            registry, registration, heap_free);
        release_animation_resource(registry, old->cpu);
        deferred.records.back().owner = std::move(actor.model);
    }
    if (name != nullptr && std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
            return false;
        }
        char* destination = actor.name.data();
        char value;
        do
        {
            value = *name++;
            *destination++ = value;
        } while (value != '\0');
        const RuntimeModelAsset& asset = *views.assets.at(resource.data());
        actor.model = make_runtime_model_instance(asset, 1, 1, types, registration, scratch);
    }
    return true;
}

// Xbox 0x00041F90. Secondary models borrow the main model's selected pose.
void set_runtime_actor_secondary_model(RuntimeActorModelState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error, const RuntimeModelResourceViews& views,
    RuntimeModelDeferredPool& deferred, u32 fence, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch,
    const std::function<void(u8*)>& heap_free)
{
    if (actor.secondary != nullptr)
    {
        const RuntimeModelAsset* old = defer_runtime_model_release(*actor.secondary, deferred, fence,
            registry, registration, heap_free);
        release_animation_resource(registry, old->cpu);
        deferred.records.back().owner = std::move(actor.secondary);
    }
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto cpu = acquire_animation_resource(registry, name);
        if (cpu.data() == nullptr)
        {
            resource_error(error, 2);
            return;
        }
        actor.secondary = make_runtime_model_instance(*views.assets.at(cpu.data()), 0, 0,
            types, registration, scratch);
        ModelPoseBuffer* selected;
        if (actor.model == nullptr)
        {
            selected = nullptr;
        }
        else
        {
            selected = actor.model->alternate;
            if (selected == nullptr)
            {
                selected = actor.model->primary;
            }
        }
        actor.secondary->alternate = selected;
        actor.secondary->state_290 = 1;
    }
}

} // namespace gbtg
