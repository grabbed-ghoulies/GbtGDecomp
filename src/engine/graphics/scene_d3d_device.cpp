#include "scene_d3d_device.h"
#include "d3dx_matrix.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr std::array<float, 16> kIdentity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f,
    1.f};
constexpr u32 kBeginEnd = 0x17FC; // NV097_SET_BEGIN_END.
constexpr u32 kInlineArray = 0x1818; // NV097_INLINE_ARRAY.
constexpr u32 kQuads = 8;
}

void SceneD3DDevice::create_vertex_shader(const i32* declaration, const u32* program, u32& handle, u32 usage)
{
    VertexShader shader;
    std::memcpy(shader.declaration.data(), declaration, sizeof(shader.declaration));
    shader.program.assign(program, program + 1 + (program[0] >> 16) * 4);
    shader.usage = usage;
    handle = next_vertex_shader_++;
    vertex_shaders.emplace(handle, std::move(shader));
}

void SceneD3DDevice::set_vertex_stream(u32 stream, u32 resource, u32 stride)
{
    vertex_streams[stream] = VertexStream{resource, stride};
    render_states_[0x10020 + stream] = stride;
}

SceneD3DDevice::SceneD3DDevice()
{
    dialogue_.simple_state = [this](u32 method, u32 value)
    {
        render_states_[method] = value;
    };
    dialogue_.noise = [this](u32 value)
    {
        render_states_[0x10000] = value;
    };
    dialogue_.cull = [this](u32 value)
    {
        render_states_[0x10001] = value;
    };
    dialogue_.depth = [this](u32 value)
    {
        render_states_[0x10002] = value;
    };
    dialogue_.vertex_shader = [this](u32 handle)
    {
        render_states_[0x10003] = handle;
    };
    dialogue_.pixel_shader = [this](u32 handle)
    {
        render_states_[0x10004] = handle;
    };
    dialogue_.texture = [this](u32 stage, const u8* texture)
    {
        if (stage >= textures_.size())
        {
            throw Error("D3D texture stage outside the device");
        }
        textures_[stage] = texture;
    };
    dialogue_.texture_coordinates = [this](u32 stage, u32 index)
    {
        render_states_[0x10010 + stage] = index;
    };
    dialogue_.stream = [this](u32 stream, const void*, u32 stride)
    {
        render_states_[0x10020 + stream] = stride;
    };
    dialogue_.shader_input = [](u32, u32, const void*)
    {
    };
    dialogue_.set_transform = [this](u32 state, const float* matrix)
    {
        std::array<float, 16> stored;
        std::memcpy(stored.data(), matrix, sizeof(stored));
        transforms_[state] = stored;
    };
    dialogue_.get_transform = [this](u32 state, float* matrix)
    {
        const auto found = transforms_.find(state);
        const auto& stored = found == transforms_.end() ? kIdentity : found->second;
        std::memcpy(matrix, stored.data(), sizeof(stored));
    };
    dialogue_.viewport = [this](std::span<const u32, 6> viewport)
    {
        for (u32 index = 0; index < 6; ++index)
        {
            render_states_[0x10030 + index] = viewport[index];
        }
    };
    dialogue_.constants_not_inline = [](u32, u32)
    {
    };
    dialogue_.constants = [this](u32 reg, const float* data, u32 count)
    {
        // Registers are kept as the dump passes them (Xbox biases them by 96).
        for (u32 index = 0; index < count; ++index)
        {
            const u32 slot = reg + index;
            if (slot >= constants_.size())
            {
                throw Error("D3D vertex constant outside the device");
            }
            std::memcpy(constants_[slot].data(), data + index * 4, 16);
        }
    };
    dialogue_.multiply = [](float* output, const float* first, const float* second)
    {
        multiply_runtime_graphics_matrices(output, first, second);
    };
    dialogue_.transpose = [](float* output, const float* input)
    {
        std::array<float, 16> copy;
        std::memcpy(copy.data(), input, sizeof(copy));
        for (u32 row = 0; row < 4; ++row)
        {
            for (u32 column = 0; column < 4; ++column)
            {
                output[row * 4 + column] = copy[column * 4 + row];
            }
        }
    };
    dialogue_.begin_push = [this](u32 words)
    {
        push_.assign(words, 0);
        return push_.data();
    };
    dialogue_.end_push = [this](u32* end)
    {
        if (end < push_.data() || end > push_.data() + push_.size())
        {
            throw Error("D3D push buffer ends outside its reservation");
        }
        decode_push(push_.data(), end);
    };
    outer_.back_buffer = [this](u32)
    {
        return DialogueSurfaceHandle(1);
    };
    outer_.depth_buffer = [this]()
    {
        return DialogueSurfaceHandle(2);
    };
    outer_.surface = [this](DialogueSurfaceHandle, u32)
    {
        return DialogueSurfaceHandle(0x100 + next_surface_++);
    };
    outer_.render_target = [this](DialogueSurfaceHandle color, DialogueSurfaceHandle depth)
    {
        render_states_[0x10040] = u32(color);
        render_states_[0x10041] = u32(depth);
    };
    outer_.release = [](DialogueSurfaceHandle)
    {
    };
    outer_.clear = [](u32, const void*, u32, u32, float, u32)
    {
    };
    outer_.line_width = [this](float width)
    {
        render_states_[0x10050] = std::bit_cast<u32>(width);
    };
    outer_.pixel_constants = [](u32, const float*, u32)
    {
    };
}

void SceneD3DDevice::decode_push(const u32* begin, const u32* end)
{
    bool open = false;
    std::vector<float> vertices;
    for (const u32* cursor = begin; cursor < end;)
    {
        const u32 header = *cursor++;
        const u32 count = (header >> 18) & 0x7FF;
        const u32 method = header & 0x1FFC;
        if (cursor + count > end)
        {
            throw Error("D3D push buffer method runs past its end");
        }
        if (method == kBeginEnd && count == 1)
        {
            const u32 mode = cursor[0];
            if (mode == kQuads)
            {
                open = true;
                vertices.clear();
            }
            else if (mode == 0 && open)
            {
                if (vertices.size() % 20 != 0)
                {
                    throw Error("D3D quad batch has a partial vertex set");
                }
                for (size_t quad = 0; quad < vertices.size(); quad += 20)
                {
                    SceneD3DQuad output{};
                    for (u32 vertex = 0; vertex < 4; ++vertex)
                    {
                        std::memcpy(output.vertices[vertex].data(), vertices.data() + quad + vertex * 5, 20);
                    }
                    for (u32 row = 0; row < 4; ++row)
                    {
                        std::memcpy(output.transform.data() + row * 4, constants_[0x60 + row].data(), 16);
                    }
                    output.color = constants_[0x64];
                    output.texture = textures_[0];
                    quads.push_back(output);
                }
                open = false;
            }
            else
            {
                throw Error("D3D push buffer uses an unsupported primitive mode");
            }
        }
        else if (method == kInlineArray)
        {
            for (u32 index = 0; index < count; ++index)
            {
                vertices.push_back(std::bit_cast<float>(cursor[index]));
            }
        }
        else
        {
            throw Error("D3D push buffer uses an unsupported method");
        }
        cursor += count;
    }
}

void SceneD3DDevice::set_state(u32 key, u32 value)
{
    render_states_[key] = value;
}

void SceneD3DDevice::set_constant_words(u32 reg, const u32* data, u32 count)
{
    for (u32 index = 0; index < count; ++index)
    {
        const u32 slot = reg + index;
        if (slot >= constants_.size())
        {
            throw Error("D3D vertex constant outside the device");
        }
        std::memcpy(constants_[slot].data(), data + index * 4, 16);
    }
}

void SceneD3DDevice::draw_indexed(u32 primitive, u32 count, u32 indices)
{
    indexed_draws.push_back({primitive, count, indices});
}

std::vector<SceneD3DQuad> SceneD3DDevice::take_quads()
{
    return std::exchange(quads, {});
}
}
