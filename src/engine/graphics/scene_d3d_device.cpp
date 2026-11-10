#include "scene_d3d_device.h"
#include <utility>
#include "render_profile.h"
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
    SceneD3DVertexShader shader;
    std::memcpy(shader.declaration.data(), declaration, sizeof(shader.declaration));
    shader.program.assign(program, program + 1 + (program[0] >> 16) * 4);
    shader.usage = usage;
    // D3D hands out the shader address with bit 0 set, so SetVertexShader tells a created shader from an
    // even fixed-function format.
    handle = (next_vertex_shader_++ << 1) | 1u;
    vertex_shaders.emplace(handle, std::move(shader));
}

void SceneD3DDevice::set_vertex_shader(u32 handle)
{
    put_state(kVertexShader, handle);
    // A created shader with a program (all the host creates) carries flag 0x10 and is loaded and selected without
    // touching the vertex colours. A fixed-function format (0x00172DA0) has flag 0x400 with D3DFVF_DIFFUSE and
    // 0x800 with D3DFVF_SPECULAR, and each colour it lacks is set (NV097_SET_VERTEX_DATA4UB): diffuse white,
    // specular zero, back diffuse white, back specular zero.
    if ((handle & 1) != 0)
    {
        return;
    }
    if ((handle & 0x40) == 0)
    {
        put_state(kVertexDataColor + 3, 0xFFFFFFFFu); // 0x194C
    }
    if ((handle & 0x80) == 0)
    {
        put_state(kVertexDataColor + 4, 0); // 0x1950
    }
    put_state(kVertexDataColor + 7, 0xFFFFFFFFu); // 0x195C
    put_state(kVertexDataColor + 8, 0); // 0x1960
}

void SceneD3DDevice::set_vertex_stream(u32 stream, u32 resource, u32 stride)
{
    vertex_streams_[stream] = SceneD3DVertexStream{resource, stride};
    put_state(0x10020 + stream, stride);
}

SceneD3DDevice::SceneD3DDevice()
{
    dialogue_.simple_state = [this](u32 method, u32 value)
    {
        put_state(method, value);
    };
    dialogue_.noise = [this](u32 value)
    {
        put_state(0x10000, value);
    };
    dialogue_.cull = [this](u32 value)
    {
        put_state(kCullMode, value);
    };
    dialogue_.depth = [this](u32 value)
    {
        put_state(kZEnable, value);
    };
    dialogue_.vertex_shader = [this](u32 handle)
    {
        set_vertex_shader(handle);
    };
    dialogue_.pixel_shader = [this](u32 handle)
    {
        put_state(kPixelShader, handle);
        erase_state(kPSTextureModes); // The shader's own texture modes apply again.
        for (const u32 method : kPixelShaderRegisters)
        {
            erase_state(method); // The upload replaces any raw combiner writes.
        }
        // D3DDevice_SetPixelShader (0x00175370) writes the definition's stage constants (0x800A60) and keeps none
        // set before it, so the definition's own constants stand until the next SetPixelShaderConstant.
        pixel_constants_set_ = 0;
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
        put_state(0x10010 + stage, index);
    };
    dialogue_.stream = [this](u32 stream, const void*, u32 stride)
    {
        put_state(0x10020 + stream, stride);
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
    dialogue_.projection_viewport = [this](float* matrix)
    {
        // 0x00171210: x by half the viewport width, y by minus half its height, z by the depth range in buffer
        // units, then offset by the half width, half height and MinZ (the viewport's X and Y are not added). The
        // device has no supersampling (+0x958 / +0x95C 1) and no w-buffer (DAT_00181DAC 2).
        const auto word = [this](u32 index)
        {
            const auto found = render_states_.find(kViewport + index);
            return found == render_states_.end() ? 0u : found->second;
        };
        const float width = float(word(2)) * 0.5f; // DAT_0032B0F4
        const float height = float(word(3)) * -0.5f; // DAT_0032B1C4
        const float near_z = std::bit_cast<float>(word(4));
        const float far_z = std::bit_cast<float>(word(5));
        std::array<float, 16> scale{};
        scale[0] = width;
        scale[5] = height;
        scale[10] = (far_z - near_z) * kDepthScale;
        scale[12] = width;
        scale[13] = -height;
        scale[14] = kDepthScale * near_z;
        scale[15] = 1.f; // DAT_0032B0F8
        const auto found = transforms_.find(1);
        const auto& projection = found == transforms_.end() ? kIdentity : found->second;
        multiply_runtime_graphics_matrices(matrix, projection.data(), scale.data()); // 0x00177AF0
    };
    dialogue_.viewport = [this](std::span<const u32, 6> viewport)
    {
        for (u32 index = 0; index < 6; ++index)
        {
            put_state(kViewport + index, viewport[index]);
        }
        // SetViewport also loads the screen-space transform vertex programs end with: c-38 scales and
        // c-37 offsets the projected position into viewport pixels and depth-buffer units.
        const float width = float(viewport[2]) * 0.5f;
        const float height = float(viewport[3]) * 0.5f;
        const float near_z = std::bit_cast<float>(viewport[4]);
        const float far_z = std::bit_cast<float>(viewport[5]);
        constants_[96 - 38] = {width, -height, kDepthScale * (far_z - near_z), 0.f};
        constants_[96 - 37] = {float(viewport[0]) + width, float(viewport[1]) + height, kDepthScale * near_z, 0.f};
        forget_constants(); // GBTG_OPTIMIZE step 21
    };
    dialogue_.constants_not_inline = [this](u32 reg, const u32* data, u32 count)
    {
        // Xbox D3D 0x001735D0 copies the dwords from the register's first component on.
        if (reg * 4 + count > constants_.size() * 4)
        {
            throw Error("D3D vertex constant outside the device");
        }
        std::memcpy(constants_[reg].data(), data, count * 4);
        forget_constants(); // GBTG_OPTIMIZE step 21
    };
    dialogue_.constants = [this](u32 reg, const float* data, u32 count)
    {
        // Registers are kept as the dump passes them (Xbox biases them by 96).
        for (u32 index = 0; index < count; ++index)
        {
            const u32 slot = reg + index;
            if (slot >= constants_.size())
            {
                throw Error("D3D vertex constant outside the device: register " + std::to_string(reg) + ", count "
                    + std::to_string(count));
            }
            std::memcpy(constants_[slot].data(), data + index * 4, 16);
            forget_constants(); // GBTG_OPTIMIZE step 21
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
    // The NV2A words D3D writes for it: the begin, the vertices as one inline array, the end.
    dialogue_.draw_vertices_up = [this](u32 primitive, u32 count, const void* vertices, u32 stride)
    {
        const u32 words = count * stride / 4;
        if ((count * stride) % 4 != 0 || words > 0x7FF)
        {
            throw Error("D3D DrawVerticesUP of a size one inline array cannot carry");
        }
        std::vector<u32> push{0x417FCu, primitive, 0x40001818u | (words << 18)};
        const auto* source = static_cast<const u32*>(vertices);
        push.insert(push.end(), source, source + words);
        push.insert(push.end(), {0x417FCu, 0u});
        decode_push(push.data(), push.data() + push.size());
    };
    dialogue_.multisample_mode = [this](u32 mode)
    {
        put_state(kMultiSampleMode, mode);
    };
    dialogue_.stencil_enable = [this](u32 enabled)
    {
        put_state(kStencilEnable, enabled);
    };
    dialogue_.vertex_data_color = [this](u32 reg, u32 color)
    {
        put_state(kVertexDataColor + reg, color);
    };
    // Immediate-mode vertices go out as the push buffer's begin, inline vertices of the bound fixed-function
    // format (the position, then the current diffuse for D3DFVF_DIFFUSE) and end.
    dialogue_.begin = [this](u32 primitive)
    {
        immediate_.assign({0x417FCu, primitive});
        immediate_textured_ = false;
        immediate_second_textured_ = false;
    };
    // Register 9 (D3DVSDE_TEXCOORD0) holds the current texture coordinate; each later position carries it, and
    // the block draws as D3DFVF_XYZ | D3DFVF_TEX1. Register 10 (D3DVSDE_TEXCOORD1) is the second set, which only a
    // D3DFVF_XYZ | D3DFVF_TEX2 (0x202) block reads; under the other formats it is dropped.
    dialogue_.vertex_data2f = [this](i32 reg, float a, float b)
    {
        if (reg == 10 && !immediate_.empty())
        {
            immediate_second_texcoord_ = {a, b};
            immediate_second_textured_ = true;
            return;
        }
        if (reg != 9 || immediate_.empty())
        {
            throw Error("D3D immediate 2f data other than texture coordinate 0, or outside Begin/End");
        }
        if (!immediate_textured_ && immediate_.size() > 2)
        {
            throw Error("D3D immediate texture coordinate after untextured vertices");
        }
        immediate_texcoord_ = {a, b};
        immediate_textured_ = true;
    };
    // Register -1 (D3DVSDE_VERTEX) and 0 (D3DVSDE_POSITION) both write the position, which emits the vertex.
    dialogue_.vertex_data4f = [this](i32 reg, float x, float y, float z, float w)
    {
        if ((reg != -1 && reg != 0) || immediate_.empty())
        {
            throw Error("D3D immediate vertex data other than a position, or outside Begin/End");
        }
        const auto bound = render_states_.find(kVertexShader);
        const u32 format = bound == render_states_.end() ? 0u : bound->second;
        // D3DFVF_XYZ | D3DFVF_TEX1 (0x102) bound takes the textured block too (the point-light composite, 0x00063320),
        // and D3DFVF_XYZ | D3DFVF_TEX2 (0x202) its frame-copy composite with both coordinate sets.
        if (format != 0x2 && format != 0x42 && format != 0x44 && format != 0x102 && format != 0x202)
        {
            throw Error("D3D immediate vertices under vertex format " + std::to_string(format));
        }
        if ((immediate_textured_ && format != 0x2 && format != 0x102 && format != 0x202)
            || ((format == 0x102 || format == 0x202) && !immediate_textured_)
            || (format == 0x202 && !immediate_second_textured_))
        {
            throw Error("D3D immediate texture coordinates under vertex format " + std::to_string(format));
        }
        if (format == 0x202)
        {
            immediate_.push_back(0x1818u | (7u << 18));
            immediate_.push_back(std::bit_cast<u32>(x));
            immediate_.push_back(std::bit_cast<u32>(y));
            immediate_.push_back(std::bit_cast<u32>(z));
            immediate_.push_back(std::bit_cast<u32>(immediate_texcoord_[0]));
            immediate_.push_back(std::bit_cast<u32>(immediate_texcoord_[1]));
            immediate_.push_back(std::bit_cast<u32>(immediate_second_texcoord_[0]));
            immediate_.push_back(std::bit_cast<u32>(immediate_second_texcoord_[1]));
            return;
        }
        const u32 words = immediate_textured_ ? 5 : format == 0x44 ? 5 : format == 0x42 ? 4 : 3;
        immediate_.push_back(0x1818u | (words << 18));
        immediate_.push_back(std::bit_cast<u32>(x));
        immediate_.push_back(std::bit_cast<u32>(y));
        immediate_.push_back(std::bit_cast<u32>(z));
        if (immediate_textured_)
        {
            immediate_.push_back(std::bit_cast<u32>(immediate_texcoord_[0]));
            immediate_.push_back(std::bit_cast<u32>(immediate_texcoord_[1]));
            return;
        }
        if (format == 0x44)
        {
            immediate_.push_back(std::bit_cast<u32>(w)); // D3DFVF_XYZRHW's reciprocal w.
        }
        if (format == 0x42 || format == 0x44)
        {
            const auto diffuse = render_states_.find(kVertexDataColor + 3);
            immediate_.push_back(diffuse == render_states_.end() ? 0xFFFFFFFFu : diffuse->second);
        }
    };
    dialogue_.end = [this]()
    {
        immediate_.push_back(0x417FCu);
        immediate_.push_back(0);
        const auto bound = render_states_.find(kVertexShader);
        const bool dual = bound != render_states_.end() && bound->second == 0x202u;
        immediate_format_ = dual ? 0x202u : immediate_textured_ ? 0x102u : 0u;
        decode_push(immediate_.data(), immediate_.data() + immediate_.size());
        immediate_format_ = 0;
        immediate_textured_ = false;
        immediate_second_textured_ = false;
        immediate_.clear();
    };
    outer_.back_buffer = [](u32)
    {
        return DialogueSurfaceHandle(kBackBuffer);
    };
    outer_.depth_buffer = []()
    {
        return DialogueSurfaceHandle(kDepthBuffer);
    };
    outer_.surface = [this](DialogueSurfaceHandle texture, u32)
    {
        const DialogueSurfaceHandle surface(0x100 + next_surface_++);
        surface_textures[surface] = texture;
        return surface;
    };
    outer_.render_target = [this](DialogueSurfaceHandle color, DialogueSurfaceHandle depth)
    {
        commands.push_back(SceneD3DTarget{color, depth});
        // As in D3D8, a new render target resets the viewport to its whole surface (and with it c-38 / c-37).
        u32 width = 640;
        u32 height = 480;
        if (color != kBackBuffer)
        {
            const auto texture = surface_textures.find(color);
            if (texture == surface_textures.end() || !texture_size)
            {
                throw Error("render target surface has no texture size");
            }
            const auto size = texture_size(texture->second);
            width = size[0];
            height = size[1];
        }
        const std::array<u32, 6> viewport{0, 0, width, height, 0, std::bit_cast<u32>(1.f)};
        dialogue_.viewport(viewport);
    };
    outer_.release = [](DialogueSurfaceHandle)
    {
    };
    outer_.copy_rects = [this](DialogueSurfaceHandle source, DialogueSurfaceHandle target)
    {
        const auto texture = surface_textures.find(target);
        if (source != kBackBuffer || texture == surface_textures.end())
        {
            throw Error("D3D CopyRects other than the back buffer into a texture surface");
        }
        commands.push_back(SceneD3DCopy{u32(texture->second)});
    };
    outer_.clear = [this](u32 count, const void* rectangles, u32 flags, u32 color, float depth, u32)
    {
        // D3DRECT: x1, y1, x2, y2.
        SceneD3DClear clear{std::vector<std::array<i32, 4>>(count), flags, color, depth};
        if (count != 0)
        {
            std::memcpy(clear.rectangles.data(), rectangles, count * sizeof(std::array<i32, 4>));
        }
        commands.push_back(std::move(clear));
    };
    outer_.line_width = [this](float width)
    {
        put_state(0x10050, std::bit_cast<u32>(width));
    };
    outer_.pixel_constants = [this](u32 reg, const float* data, u32 count)
    {
        for (u32 index = 0; index < count; ++index)
        {
            if (reg + index >= pixel_constants_.size())
            {
                throw Error("D3D pixel constant outside the device");
            }
            std::memcpy(pixel_constants_[reg + index].data(), data + index * 4, 16);
            pixel_constants_set_ |= 1u << (reg + index);
        }
    };
}

void SceneD3DDevice::decode_push(const u32* begin, const u32* end)
{
    render_profile::Scope measurement(render_profile::Recording);
    bool open = false;
    u32 primitive = 0;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 1: the inline vertices reuse the device's scratch capacity.
    std::vector<float>& vertices = push_vertices_;
    vertices.clear();
#else
    std::vector<float> vertices;
#endif
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
            if (mode != 0)
            {
                open = true;
                primitive = mode;
                vertices.clear();
            }
            else if (open)
            {
                open = false;
                const auto bound = render_states_.find(kVertexShader);
                const u32 shader = immediate_format_ != 0 ? immediate_format_
                    : bound == render_states_.end() ? 0u : bound->second;
                if (shader != 0 && (shader & 1u) == 0)
                {
                    record_fixed(primitive, shader, vertices);
                    cursor += count;
                    continue;
                }
                if (shader != 0)
                {
                    flush_deferred_states();
#if GBTG_OPTIMIZE
                    SceneD3DInlineDraw output{SceneD3DIndexedDraw{primitive, 0, 0, constant_snapshot(), textures_,
                        snapshot(), vertex_streams_, pixel_constants_, pixel_constants_set_, address_u_,
                        address_v_}, {}};
#else
                    SceneD3DInlineDraw output{SceneD3DIndexedDraw{primitive, 0, 0, constants_, textures_,
                        snapshot(), vertex_streams_, pixel_constants_, pixel_constants_set_, address_u_,
                        address_v_}, {}};
#endif
                    output.vertices.resize(vertices.size());
                    std::memcpy(output.vertices.data(), vertices.data(), vertices.size() * sizeof(u32));
                    commands.push_back(std::move(output));
                    cursor += count;
                    continue;
                }
                if (primitive != kQuads)
                {
                    throw Error("D3D push buffer draws a non-quad primitive without a vertex shader");
                }
                if (vertices.size() % 20 != 0)
                {
                    throw Error("D3D quad batch has a partial vertex set");
                }
                flush_deferred_states();
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
                    output.states = snapshot();
                    output.address_u = address_u_[0];
                    output.address_v = address_v_[0];
                    commands.push_back(std::move(output));
                }
            }
            else
            {
                throw Error("D3D push buffer ends a primitive it did not begin");
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

void SceneD3DDevice::record_fixed(u32 primitive, u32 format, const std::vector<float>& vertices)
{
    const auto transform = [this](u32 state)
    {
        const auto found = transforms_.find(state);
        return found == transforms_.end() ? kIdentity : found->second;
    };
    flush_deferred_states();
    SceneD3DFixedDraw output{primitive, format, std::vector<u32>(vertices.size()), transform(6), transform(0),
        transform(1), constants_[96 - 38], constants_[96 - 37], 0xFFFFFFFFu, snapshot(), textures_[0],
        address_u_[0], address_v_[0], textures_, address_u_, address_v_, pixel_constants_, pixel_constants_set_};
    std::memcpy(output.vertices.data(), vertices.data(), vertices.size() * sizeof(u32));
    const auto diffuse = render_states_.find(kVertexDataColor + 3);
    if (diffuse != render_states_.end())
    {
        output.diffuse = diffuse->second;
    }
    commands.push_back(std::move(output));
}

void SceneD3DDevice::set_state(u32 key, u32 value)
{
    put_state(key, value);
}

void SceneD3DDevice::put_state(u32 key, u32 value)
{
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 1: only a changed value retires the shared draw snapshot.
    const auto [it, inserted] = render_states_.try_emplace(key, value);
    if (inserted || it->second != value)
    {
        it->second = value;
        snapshot_.reset();
    }
#else
    render_states_[key] = value;
#endif
}

void SceneD3DDevice::erase_state(u32 key)
{
#if GBTG_OPTIMIZE
    if (render_states_.erase(key) != 0)
    {
        snapshot_.reset();
    }
#else
    render_states_.erase(key);
#endif
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 21: draws recorded while no constant is written share one copy of the constants.
SceneD3DConstantSnapshot SceneD3DDevice::constant_snapshot()
{
    if (constants_snapshot_.identity() == nullptr)
    {
        constants_snapshot_ = SceneD3DConstantSnapshot(constants_);
    }
    return constants_snapshot_;
}
#endif

void SceneD3DDevice::forget_constants()
{
#if GBTG_OPTIMIZE
    constants_snapshot_ = {};
#endif
}

SceneD3DStateSnapshot SceneD3DDevice::snapshot()
{
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 1: draws under unchanged states share the last snapshot.
    if (!snapshot_)
    {
        snapshot_ = std::make_shared<const SceneD3DStateSnapshot::Values>(render_states_.begin(), render_states_.end());
    }
    return SceneD3DStateSnapshot(snapshot_);
#else
    return SceneD3DStateSnapshot(render_states_);
#endif
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
        forget_constants(); // GBTG_OPTIMIZE step 21
    }
}

void SceneD3DDevice::flush_deferred_states()
{
    if (fog_enable != nullptr)
    {
        put_state(kFogEnable, *fog_enable);
    }
    if (deferred_texture_states != nullptr)
    {
        for (u32 stage = 0; stage < 4; ++stage)
        {
            const u32 u = deferred_texture_states[stage * 0x20];
            const u32 v = deferred_texture_states[stage * 0x20 + 1];
            address_u_[stage] = u == 0 ? 1 : u;
            address_v_[stage] = v == 0 ? 1 : v;
        }
    }
}

SceneD3DIndexedDraw& SceneD3DDevice::draw_indexed(u32 primitive, u32 count, u32 indices)
{
    render_profile::Scope measurement(render_profile::Recording);
    flush_deferred_states();
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 16: the draw is built in the command list, not in a temporary that is then copied there.
    return std::get<SceneD3DIndexedDraw>(commands.emplace_back(std::in_place_type<SceneD3DIndexedDraw>, primitive,
        count, indices, constant_snapshot(), textures_, snapshot(), vertex_streams_, pixel_constants_, pixel_constants_set_,
        address_u_, address_v_));
#else
    commands.push_back(SceneD3DIndexedDraw{primitive, count, indices, constants_, textures_, snapshot(),
        vertex_streams_, pixel_constants_, pixel_constants_set_, address_u_, address_v_});
    return std::get<SceneD3DIndexedDraw>(commands.back());
#endif
}

SceneD3DIndexedDraw& SceneD3DDevice::draw_vertices(u32 primitive, u32 first, u32 count)
{
    auto& draw = draw_indexed(primitive, count, 0);
    draw.sequential = true;
    draw.first = first;
    return draw;
}

std::vector<SceneD3DCommand> SceneD3DDevice::take_commands()
{
    return std::exchange(commands, {});
}
}
