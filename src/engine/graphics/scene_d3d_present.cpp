#include "scene_d3d_present.h"
#include "render_profile.h"
#include "xbox_vertex_program.h"
#include "xbox_vertex_hlsl.h"
#include "d3dx_matrix.h"
#include "gbtg/gfx.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

namespace gbtg
{
namespace
{
// NV2A method headers (count 1) of the render states the draws read.
constexpr u32 kAlphaTestEnable = 0x40300;
constexpr u32 kBlendEnable = 0x40304;
constexpr u32 kAlphaFunction = 0x4033C;
constexpr u32 kAlphaReference = 0x40340;
constexpr u32 kSourceBlend = 0x40344;
constexpr u32 kDestinationBlend = 0x40348;
constexpr u32 kBlendEquation = 0x40350;
constexpr u32 kDepthFunction = 0x40354;
constexpr u32 kColorMask = 0x40358;
constexpr u32 kDepthWrite = 0x4035C;
constexpr u32 kStencilMask = 0x40360;
constexpr u32 kStencilFunction = 0x40364;
constexpr u32 kStencilReference = 0x40368;
constexpr u32 kStencilFunctionMask = 0x4036C;
constexpr u32 kStencilFail = 0x40370;
constexpr u32 kStencilDepthFail = 0x40374;
constexpr u32 kStencilPass = 0x40378;

u32 state(const SceneD3DStateSnapshot& states, u32 key, u32 fallback)
{
    const auto found = states.find(key);
    return found == states.end() ? fallback : found->second;
}

// D3D's device defaults stand for any state the scene has not written.
#if GBTG_OPTIMIZE
void apply_snapshot_states(const SceneD3DStateSnapshot& states, GfxNativeDraw& draw)
#else
void apply_states(const SceneD3DStateSnapshot& states, GfxNativeDraw& draw)
#endif
{
    draw.cull = state(states, SceneD3DDevice::kCullMode, 0);
    draw.depth_test = state(states, SceneD3DDevice::kZEnable, 1) != 0;
    draw.depth_write = state(states, kDepthWrite, 1) != 0;
    draw.depth_function = state(states, kDepthFunction, 0x203);
    draw.blend = state(states, kBlendEnable, 0) != 0;
    draw.source_blend = state(states, kSourceBlend, 1);
    draw.destination_blend = state(states, kDestinationBlend, 0);
    draw.blend_equation = state(states, kBlendEquation, 0x8006);
    draw.alpha_test = state(states, kAlphaTestEnable, 0) != 0;
    draw.alpha_function = state(states, kAlphaFunction, 0x207);
    draw.alpha_reference = state(states, kAlphaReference, 0);
    draw.color_mask = state(states, kColorMask, 0x01010101);
    draw.stencil_test = state(states, SceneD3DDevice::kStencilEnable, 0) != 0;
    draw.stencil_function = state(states, kStencilFunction, 0x207);
    draw.stencil_reference = state(states, kStencilReference, 0);
    draw.stencil_read_mask = state(states, kStencilFunctionMask, 0xFF);
    draw.stencil_write_mask = state(states, kStencilMask, 0xFF);
    draw.stencil_fail = state(states, kStencilFail, 0x1E00);
    draw.stencil_depth_fail = state(states, kStencilDepthFail, 0x1E00);
    draw.stencil_pass = state(states, kStencilPass, 0x1E00);
    draw.fog = state(states, SceneD3DDevice::kFogEnable, 0) != 0;
    draw.fog_color = state(states, SceneD3DDevice::kFogColor, 0);
    const u32 x = state(states, SceneD3DDevice::kViewport, 0);
    const u32 y = state(states, SceneD3DDevice::kViewport + 1, 0);
    draw.scissor = {i32(x), i32(y), i32(x + state(states, SceneD3DDevice::kViewport + 2, 640)),
        i32(y + state(states, SceneD3DDevice::kViewport + 3, 480))};
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 3: draws recorded under one shared snapshot reuse its decoded states.
void apply_states(const SceneD3DStateSnapshot& states, GfxNativeDraw& draw)
{
    thread_local std::shared_ptr<const SceneD3DStateSnapshot::Values> last;
    thread_local GfxNativeDraw applied;
    if (states.shared() == nullptr || states.shared() != last)
    {
        apply_snapshot_states(states, applied);
        last = states.shared();
    }
    draw.cull = applied.cull;
    draw.depth_test = applied.depth_test;
    draw.depth_write = applied.depth_write;
    draw.depth_function = applied.depth_function;
    draw.blend = applied.blend;
    draw.source_blend = applied.source_blend;
    draw.destination_blend = applied.destination_blend;
    draw.blend_equation = applied.blend_equation;
    draw.alpha_test = applied.alpha_test;
    draw.alpha_function = applied.alpha_function;
    draw.alpha_reference = applied.alpha_reference;
    draw.color_mask = applied.color_mask;
    draw.stencil_test = applied.stencil_test;
    draw.stencil_function = applied.stencil_function;
    draw.stencil_reference = applied.stencil_reference;
    draw.stencil_read_mask = applied.stencil_read_mask;
    draw.stencil_write_mask = applied.stencil_write_mask;
    draw.stencil_fail = applied.stencil_fail;
    draw.stencil_depth_fail = applied.stencil_depth_fail;
    draw.stencil_pass = applied.stencil_pass;
    draw.fog = applied.fog;
    draw.fog_color = applied.fog_color;
    draw.scissor = applied.scissor;
}
#endif

float signed_normal(i32 value, i32 range)
{
    return std::max(float(value) / float(range), -1.f);
}

// One declared input register (D3DVSD_REG) and the stream bytes it reads.
XboxVector fetch(u32 type, const u8* data)
{
    XboxVector value{0.f, 0.f, 0.f, 1.f};
    const auto f32 = [data](u32 index)
    {
        float result;
        std::memcpy(&result, data + index * 4, 4);
        return result;
    };
    const auto s16 = [data](u32 index)
    {
        std::int16_t result;
        std::memcpy(&result, data + index * 2, 2);
        return i32(result);
    };
    const u32 components = type >> 4;
    switch (type & 0xF)
    {
    case 0x2:
        if (type == 0x72)
        {
            // FLOAT2H: x, y, w.
            value = {f32(0), f32(1), 0.f, f32(2)};
            break;
        }
        for (u32 index = 0; index < components; ++index)
        {
            value[index] = f32(index);
        }
        break;
    case 0x0:
        if (type != 0x40)
        {
            throw Error("vertex declaration uses type " + std::to_string(type));
        }
        // D3DCOLOR is stored B, G, R, A and reads as (R, G, B, A).
        value = {float(data[2]) / 255.f, float(data[1]) / 255.f, float(data[0]) / 255.f, float(data[3]) / 255.f};
        break;
    case 0x1:
        for (u32 index = 0; index < components; ++index)
        {
            value[index] = signed_normal(s16(index), 32767);
        }
        break;
    case 0x5:
        for (u32 index = 0; index < components; ++index)
        {
            value[index] = float(s16(index));
        }
        break;
    case 0x4:
        for (u32 index = 0; index < components; ++index)
        {
            value[index] = float(data[index]) / 255.f;
        }
        break;
    case 0x6:
    {
        if (type != 0x16)
        {
            throw Error("vertex declaration uses type " + std::to_string(type));
        }
        u32 packed;
        std::memcpy(&packed, data, 4);
        const i32 x = i32(packed << 21) >> 21;
        const i32 y = i32(packed << 10) >> 21;
        const i32 z = i32(packed) >> 22;
        value = {signed_normal(x, 1023), signed_normal(y, 1023), signed_normal(z, 511), 1.f};
        break;
    }
    default:
        throw Error("vertex declaration uses type " + std::to_string(type));
    }
    return value;
}

u32 type_size(u32 type)
{
    switch (type)
    {
    case 0x02:
        return 0;
    case 0x72:
        return 12;
    case 0x40:
    case 0x16:
        return 4;
    default:
        break;
    }
    const u32 components = type >> 4;
    switch (type & 0xF)
    {
    case 0x2:
        return components * 4;
    case 0x1:
    case 0x5:
        return components * 2;
    case 0x4:
        return components;
    default:
        throw Error("vertex declaration uses type " + std::to_string(type));
    }
}

struct DeclaredInput
{
    u32 reg;
    u32 stream;
    u32 offset;
    u32 type;
};

std::vector<DeclaredInput> declared_inputs(const std::array<i32, 32>& declaration)
{
    render_profile::Scope measurement(render_profile::Declaration);
    std::vector<DeclaredInput> inputs;
    u32 stream = 0;
    u32 offset = 0;
    for (const i32 signed_token : declaration)
    {
        const u32 token = u32(signed_token);
        if (token == 0xFFFFFFFFu)
        {
            return inputs;
        }
        switch (token >> 29)
        {
        case 0:
            break; // D3DVSD_NOP.
        case 1:
            stream = token & 0xF;
            offset = 0;
            break;
        case 2:
            if ((token & 0x10000000u) != 0)
            {
                offset += ((token >> 16) & 0xF) * 4; // D3DVSD_SKIP, in dwords.
            }
            else
            {
                const u32 type = (token >> 16) & 0xFF;
                inputs.push_back({token & 0x1F, stream, offset, type});
                offset += type_size(type);
            }
            break;
        default:
            throw Error("vertex declaration uses token " + std::to_string(token));
        }
    }
    throw Error("vertex declaration has no end token");
}

u32 pack_color(const std::array<float, 4>& color)
{
    const auto byte = [](float value)
    {
        return u32(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f);
    };
    return (byte(color[3]) << 24) | (byte(color[0]) << 16) | (byte(color[1]) << 8) | byte(color[2]);
}

// SetPixelShaderConstant writes each combiner stage constant mapped to the register (PSC0Mapping /
// PSC1Mapping nibbles) and the final combiner constants (PSFinalCombinerConstants).
std::array<u32, 60> pixel_shader_words(const u8* definition, const SceneD3DIndexedDraw& draw)
{
    std::array<u32, 60> words;
    std::memcpy(words.data(), definition, sizeof(words));
    for (u32 stage = 0; stage < 8; ++stage)
    {
        const u32 first = (words[57] >> (stage * 4)) & 0xF;
        const u32 second = (words[58] >> (stage * 4)) & 0xF;
        if ((draw.pixel_constants_set & (1u << first)) != 0)
        {
            words[10 + stage] = pack_color(draw.pixel_constants[first]);
        }
        if ((draw.pixel_constants_set & (1u << second)) != 0)
        {
            words[18 + stage] = pack_color(draw.pixel_constants[second]);
        }
    }
    // D3DDevice_SetRenderState_PSTextureModes after the shader replaces its texture modes.
    const auto modes = draw.states.find(SceneD3DDevice::kPSTextureModes);
    if (modes != draw.states.end())
    {
        words[54] = modes->second;
    }
    const u32 final_first = words[59] & 0xF;
    const u32 final_second = (words[59] >> 4) & 0xF;
    if ((draw.pixel_constants_set & (1u << final_first)) != 0)
    {
        words[43] = pack_color(draw.pixel_constants[final_first]);
    }
    if ((draw.pixel_constants_set & (1u << final_second)) != 0)
    {
        words[44] = pack_color(draw.pixel_constants[final_second]);
    }
    // Raw combiner register writes since the shader was bound (the model shader overrides) replace their words.
    for (size_t word = 0; word < SceneD3DDevice::kPixelShaderRegisters.size(); ++word)
    {
        if (word == 54 && modes != draw.states.end())
        {
            continue;
        }
        const auto raw = draw.states.find(SceneD3DDevice::kPixelShaderRegisters[word]);
        if (raw != draw.states.end())
        {
            words[word] = raw->second;
        }
    }
    return words;
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 3: consecutive draws mostly share a declaration, so compare against the
// last one instead of searching a content-keyed map.
const std::vector<DeclaredInput>& last_declared_inputs(const std::array<i32, 32>& declaration)
{
    thread_local std::array<i32, 32> last{};
    thread_local std::vector<DeclaredInput> inputs;
    thread_local bool cached = false;
    if (!cached || last != declaration)
    {
        inputs = declared_inputs(declaration);
        last = declaration;
        cached = true;
        if (render_profile::enabled)
        {
            ++render_profile::frame.declaration_misses;
        }
    }
    return inputs;
}

// GBTG_OPTIMIZE step 3: pixel_shader_words for the previous draw, reused while the definition,
// pixel constants and shared state snapshot are unchanged.
const std::array<u32, 60>& last_pixel_shader_words(const u8* definition, const SceneD3DIndexedDraw& draw)
{
    thread_local std::array<u8, 240> last_definition{};
    thread_local u32 last_set = 0;
    thread_local std::array<std::array<float, 4>, 16> last_constants{};
    thread_local std::shared_ptr<const SceneD3DStateSnapshot::Values> last_states;
    thread_local std::array<u32, 60> words{};
    if (last_states == nullptr || last_states != draw.states.shared() || last_set != draw.pixel_constants_set
        || std::memcmp(last_definition.data(), definition, last_definition.size()) != 0
        || std::memcmp(last_constants.data(), draw.pixel_constants.data(), sizeof(last_constants)) != 0)
    {
        words = pixel_shader_words(definition, draw);
        std::memcpy(last_definition.data(), definition, last_definition.size());
        last_set = draw.pixel_constants_set;
        last_constants = draw.pixel_constants;
        last_states = draw.states.shared();
    }
    return words;
}
#endif

// D3DPT 2 line list, 3 line loop and 4 line strip are drawn as line lists.
bool line_primitive(u32 primitive)
{
    return primitive >= 2 && primitive <= 4;
}

// Triangle-list (or, for the line primitives, line-list) vertex order for an Xbox primitive (D3DPT 2 line list,
// 3 line loop, 4 line strip, 5 list, 6 strip, 7 fan, 8 quad list).
const std::vector<u32>& triangle_order(u32 primitive, u32 count)
{
    render_profile::Scope measurement(render_profile::Topology);
    static std::map<std::pair<u32, u32>, std::vector<u32>> cache;
    const auto key = std::pair{primitive, count};
    const auto found = cache.find(key);
    if (found != cache.end())
    {
        return found->second;
    }
    if (cache.size() >= 4096)
    {
        cache.clear();
    }
    std::vector<u32> order;
    switch (primitive)
    {
    case 2:
        for (u32 index = 0; index + 1 < count; index += 2)
        {
            order.insert(order.end(), {index, index + 1});
        }
        break;
    case 3:
        for (u32 index = 0; index + 1 < count; ++index)
        {
            order.insert(order.end(), {index, index + 1});
        }
        if (count > 1)
        {
            order.insert(order.end(), {count - 1, 0});
        }
        break;
    case 4:
        for (u32 index = 0; index + 1 < count; ++index)
        {
            order.insert(order.end(), {index, index + 1});
        }
        break;
    case 5:
        for (u32 index = 0; index + 2 < count; index += 3)
        {
            order.insert(order.end(), {index, index + 1, index + 2});
        }
        break;
    case 6:
        for (u32 index = 0; index + 2 < count; ++index)
        {
            if ((index & 1) == 0)
            {
                order.insert(order.end(), {index, index + 1, index + 2});
            }
            else
            {
                order.insert(order.end(), {index + 1, index, index + 2});
            }
        }
        break;
    case 7:
        for (u32 index = 1; index + 1 < count; ++index)
        {
            order.insert(order.end(), {0, index, index + 1});
        }
        break;
    case 8:
        for (u32 index = 0; index + 3 < count; index += 4)
        {
            order.insert(order.end(), {index, index + 1, index + 2, index, index + 2, index + 3});
        }
        break;
    default:
        throw Error("indexed draw uses primitive " + std::to_string(primitive));
    }
    return cache.emplace(key, std::move(order)).first->second;
}

// The program's oPos is in back-buffer pixels and depth units after its c-38 / c-37 transform, with the
// clip w kept. Undo the 640x480 mapping and restore the homogeneous position.
std::array<float, 4> clip_position(const XboxVector& screen)
{
    const float w = screen[3];
    return {(screen[0] / 320.f - 1.f) * w, (1.f - screen[1] / 240.f) * w,
        screen[2] / SceneD3DDevice::kDepthScale * w, w};
}

void present_indexed(const SceneD3DIndexedDraw& draw, const SceneD3DPresentContext& context)
{
    const auto shader = context.vertex_shaders.find(state(draw.states, SceneD3DDevice::kVertexShader, 0));
    if (shader == context.vertex_shaders.end())
    {
        throw Error("indexed draw has no created vertex shader bound");
    }
#if GBTG_OPTIMIZE
    const auto& inputs = last_declared_inputs(shader->second.declaration);
#else
    // Content key avoids stale declarations when shader handles are replaced or reused.
    static std::map<std::array<i32, 32>, std::vector<DeclaredInput>> declarations;
    auto declaration = declarations.find(shader->second.declaration);
    if (declaration == declarations.end())
    {
        if (declarations.size() >= 128)
        {
            declarations.clear();
        }
        declaration = declarations.emplace(shader->second.declaration, declared_inputs(shader->second.declaration)).first;
        if (render_profile::enabled)
        {
            ++render_profile::frame.declaration_misses;
        }
    }
    const auto& inputs = declaration->second;
#endif
    struct Stream
    {
        const u8* data;
        size_t size;
        u32 stride;
    };
    std::array<Stream, 16> streams{};
    for (const auto& input : inputs)
    {
        auto& stream = streams[input.stream];
        if (stream.data != nullptr)
        {
            continue;
        }
        const auto binding = draw.streams.find(input.stream);
        if (binding == draw.streams.end())
        {
            throw Error("vertex declaration reads an unbound stream");
        }
        const auto resource = context.resources.resources.find(binding->second.resource);
        if (resource == context.resources.resources.end() || resource->second.data == 0)
        {
            throw Error("vertex stream has no registered data");
        }
        const auto bytes = context.memory.tail(resource->second.data);
        stream = {bytes.data(), bytes.size(), binding->second.stride};
    }
    const auto indices = draw.sequential ? std::span<const u8>{}
                                         : context.memory.resolve(draw.indices, size_t(draw.count) * 2);
    const bool gpu_vertex = gfx_native_vertex_program_supported(shader->second.program);
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 3: the address checks depend only on the program's words.
    thread_local std::vector<u32> validated_program;
    thread_local std::vector<XboxVertexAddressValidation> program_validation;
    thread_local const std::vector<XboxVertexAddressValidation> no_validation;
    if (gpu_vertex && validated_program != shader->second.program)
    {
        program_validation = xbox_vertex_address_validation(shader->second.program);
        validated_program = shader->second.program;
    }
    const auto& address_validation = gpu_vertex ? program_validation : no_validation;
#else
    const auto address_validation = gpu_vertex ? xbox_vertex_address_validation(shader->second.program) : std::vector<XboxVertexAddressValidation>{};
#endif
    if (render_profile::enabled)
    {
        if (gpu_vertex)
        {
            ++render_profile::frame.gpu_vertex_draws;
        }
        else
        {
            ++render_profile::frame.cpu_vertex_draws;
        }
    }
    thread_local std::vector<GfxNativeInputs> gpu_inputs;
    gpu_inputs.clear();
    XboxVertexProgramState program{};
    program.constants = draw.constants;
    // Indices are u16. Scratch is bounded and reset per draw; constants and resource bytes
    // are always reread, so mutation, scene unload and address reuse cannot leave stale vertices.
    thread_local std::array<i32, 65536> slots;
    thread_local std::array<u32, 65536> stamps{};
    thread_local u32 generation = 0;
    if (++generation == 0)
    {
        stamps.fill(0);
        generation = 1;
    }
    thread_local std::vector<GfxNativeVertex> shaded;
    shaded.clear();
    const auto vertex = [&](u32 index) -> const GfxNativeVertex&
    {
        if (stamps[index] == generation)
        {
            return shaded[size_t(slots[index])];
        }
        program.inputs.fill({0.f, 0.f, 0.f, 1.f});
        for (const auto& input : inputs)
        {
            const auto& stream = streams[input.stream];
            const size_t offset = size_t(index) * stream.stride + input.offset;
            if (offset + type_size(input.type) > stream.size)
            {
                throw Error("vertex index reads past its stream");
            }
            program.inputs[input.reg] = fetch(input.type, stream.data + offset);
        }
        if (gpu_vertex)
        {
            validate_xbox_vertex_address(address_validation, program.inputs, draw.constants);
            stamps[index] = generation;
            slots[index] = i32(shaded.size());
            gpu_inputs.push_back(program.inputs);
            shaded.push_back({});
            if (render_profile::enabled)
            {
                ++render_profile::frame.vertices;
                render_profile::frame.shader_instructions += shader->second.program[0] >> 16;
            }
            return shaded.back();
        }
        program.outputs.fill({0.f, 0.f, 0.f, 1.f});
        program.outputs[3] = {1.f, 1.f, 1.f, 1.f};
        program.outputs[4] = {0.f, 0.f, 0.f, 0.f};
        {
            render_profile::Scope measurement(render_profile::Vertex);
            run_xbox_vertex_program(shader->second.program, program);
        }
        if (render_profile::enabled)
        {
            ++render_profile::frame.vertices;
            render_profile::frame.shader_instructions += shader->second.program[0] >> 16;
        }
        GfxNativeVertex output{};
        output.position = clip_position(program.outputs[0]);
        for (u32 component = 0; component < 4; ++component)
        {
            output.diffuse[component] = std::clamp(program.outputs[3][component], 0.f, 1.f);
            output.specular[component] = std::clamp(program.outputs[4][component], 0.f, 1.f);
        }
        for (u32 stage = 0; stage < 4; ++stage)
        {
            output.uv[stage] = {program.outputs[9 + stage][0], program.outputs[9 + stage][1]};
        }
        output.fog = std::clamp(program.outputs[5][0], 0.f, 1.f); // oFog.x, the fog factor.
        stamps[index] = generation;
        slots[index] = i32(shaded.size());
        shaded.push_back(output);
        return shaded.back();
    };
    thread_local std::vector<GfxNativeVertex> vertices;
    thread_local std::vector<u32> retained_indices;
    vertices.clear();
    retained_indices.clear();
    for (const u32 position : triangle_order(draw.primitive, draw.count))
    {
        u16 index;
        if (draw.sequential)
        {
            index = u16(draw.first + position);
        }
        else
        {
            std::memcpy(&index, indices.data() + position * 2, 2);
        }
        const auto& value = vertex(index);
        if (render_profile::reference)
        {
            vertices.push_back(value);
        }
        else
        {
            retained_indices.push_back(u32(slots[index]));
        }
    }
    GfxNativeDraw output;
    output.lines = line_primitive(draw.primitive);
    output.vertices = render_profile::reference ? std::span<const GfxNativeVertex>(vertices) : std::span<const GfxNativeVertex>(shaded);
    output.indices = retained_indices;
    if (gpu_vertex)
    {
        output.vertices = {};
        output.program_inputs = gpu_inputs;
        output.vertex_program = shader->second.program;
        output.vertex_constants = &draw.constants;
    }
    for (u32 stage = 0; stage < 4; ++stage)
    {
        if (draw.textures[stage] != nullptr)
        {
            output.textures[stage] = &context.texture(draw.textures[stage]);
        }
    }
    output.address_u = draw.address_u;
    output.address_v = draw.address_v;
    const u32 pixel_shader = state(draw.states, SceneD3DDevice::kPixelShader, 0);
    std::array<u32, 60> words{};
    if (pixel_shader != 0)
    {
        const auto found = context.resources.pixel_shaders.find(pixel_shader);
        if (found == context.resources.pixel_shaders.end())
        {
            throw Error("indexed draw binds a pixel shader the device did not create");
        }
#if GBTG_OPTIMIZE
        words = last_pixel_shader_words(context.memory.resolve(found->second, sizeof(words)).data(), draw);
#else
        words = pixel_shader_words(context.memory.resolve(found->second, sizeof(words)).data(), draw);
#endif
        output.pixel_shader = words.data();
    }
    apply_states(draw.states, output);
    gfx_draw_native(output);
}

// Inline arrays use the bound declaration, including packed D3DCOLOR, and the same vertex interpreter
// and pixel-shader definition as indexed model draws.
void present_inline(const SceneD3DInlineDraw& inline_draw, const SceneD3DPresentContext& context)
{
    if (render_profile::enabled)
    {
        ++render_profile::frame.cpu_vertex_draws;
    }
    const auto& draw = inline_draw.state;
    const auto shader = context.vertex_shaders.find(state(draw.states, SceneD3DDevice::kVertexShader, 0));
    if (shader == context.vertex_shaders.end())
    {
        throw Error("inline draw has no created vertex shader bound");
    }
#if GBTG_OPTIMIZE
    const auto& inputs = last_declared_inputs(shader->second.declaration);
#else
    // Content key avoids stale declarations when shader handles are replaced or reused.
    static std::map<std::array<i32, 32>, std::vector<DeclaredInput>> declarations;
    auto declaration = declarations.find(shader->second.declaration);
    if (declaration == declarations.end())
    {
        if (declarations.size() >= 128)
        {
            declarations.clear();
        }
        declaration = declarations.emplace(shader->second.declaration, declared_inputs(shader->second.declaration)).first;
        if (render_profile::enabled)
        {
            ++render_profile::frame.declaration_misses;
        }
    }
    const auto& inputs = declaration->second;
#endif
    u32 stride = 0;
    for (const auto& input : inputs)
    {
        if (input.stream != 0)
        {
            throw Error("inline draw declares more than one stream");
        }
        stride = std::max(stride, input.offset + type_size(input.type));
    }
    const size_t size = inline_draw.vertices.size() * sizeof(u32);
    if (stride == 0 || size % stride != 0)
    {
        throw Error("inline draw has a partial vertex");
    }
    const auto* data = reinterpret_cast<const u8*>(inline_draw.vertices.data());
    XboxVertexProgramState program{};
    program.constants = draw.constants;
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 4: per-thread scratch keeps its capacity between draws.
    thread_local std::vector<GfxNativeVertex> shaded;
    thread_local std::vector<GfxNativeVertex> vertices;
    shaded.clear();
    vertices.clear();
#else
    std::vector<GfxNativeVertex> shaded;
#endif
    for (size_t offset = 0; offset < size; offset += stride)
    {
        program.inputs.fill({0.f, 0.f, 0.f, 1.f});
        for (const auto& input : inputs)
        {
            program.inputs[input.reg] = fetch(input.type, data + offset + input.offset);
        }
        program.outputs.fill({0.f, 0.f, 0.f, 1.f});
        program.outputs[3] = {1.f, 1.f, 1.f, 1.f};
        program.outputs[4] = {0.f, 0.f, 0.f, 0.f};
        {
            render_profile::Scope measurement(render_profile::Vertex);
            run_xbox_vertex_program(shader->second.program, program);
        }
        if (render_profile::enabled)
        {
            ++render_profile::frame.vertices;
            render_profile::frame.shader_instructions += shader->second.program[0] >> 16;
        }
        GfxNativeVertex vertex{};
        vertex.position = clip_position(program.outputs[0]);
        vertex.diffuse = program.outputs[3];
        vertex.specular = program.outputs[4];
        for (u32 stage = 0; stage < 4; ++stage)
        {
            vertex.uv[stage] = {program.outputs[9 + stage][0], program.outputs[9 + stage][1]};
        }
        vertex.fog = std::clamp(program.outputs[5][0], 0.f, 1.f); // oFog.x, the fog factor.
        shaded.push_back(vertex);
    }
#if !GBTG_OPTIMIZE
    std::vector<GfxNativeVertex> vertices;
#endif
    for (u32 index : triangle_order(draw.primitive, u32(shaded.size())))
    {
        vertices.push_back(shaded[index]);
    }
    GfxNativeDraw output;
    output.lines = line_primitive(draw.primitive);
    output.vertices = vertices;
    for (u32 stage = 0; stage < 4; ++stage)
    {
        if (draw.textures[stage] != nullptr)
        {
            output.textures[stage] = &context.texture(draw.textures[stage]);
        }
    }
    output.address_u = draw.address_u;
    output.address_v = draw.address_v;
    std::array<u32, 60> words{};
    const u32 pixel = state(draw.states, SceneD3DDevice::kPixelShader, 0);
    if (pixel != 0)
    {
        const auto found = context.resources.pixel_shaders.find(pixel);
        if (found == context.resources.pixel_shaders.end())
        {
            throw Error("inline draw binds a pixel shader the device did not create");
        }
#if GBTG_OPTIMIZE
        words = last_pixel_shader_words(context.memory.resolve(found->second, sizeof(words)).data(), draw);
#else
        words = pixel_shader_words(context.memory.resolve(found->second, sizeof(words)).data(), draw);
#endif
        output.pixel_shader = words.data();
    }
    apply_states(draw.states, output);
    gfx_draw_native(output);
}

// Fixed-function formats: clip = v * world * view * projection (row vectors), then the viewport's screen
// mapping (c-38 / c-37) the vertex programs also end with, so depth matches the model draws.
void present_fixed(const SceneD3DFixedDraw& draw, const SceneD3DPresentContext& context)
{
    u32 stride = 0;
    bool has_diffuse = false;
    bool pretransformed = false;
    bool textured = false;
    switch (draw.format)
    {
    case 0x002: // D3DFVF_XYZ.
        stride = 12;
        break;
    case 0x042: // D3DFVF_XYZ | D3DFVF_DIFFUSE.
        stride = 16;
        has_diffuse = true;
        break;
    case 0x044: // D3DFVF_XYZRHW | D3DFVF_DIFFUSE: already in screen space.
        stride = 20;
        has_diffuse = true;
        pretransformed = true;
        break;
    case 0x102: // D3DFVF_XYZ | D3DFVF_TEX1: textured, under the current diffuse.
        stride = 20;
        textured = true;
        break;
    default:
        throw Error("fixed-function draw uses vertex format " + std::to_string(draw.format));
    }
    if (textured && draw.texture == nullptr)
    {
        throw Error("textured fixed-function draw has no stage-0 texture");
    }
    const size_t size = draw.vertices.size() * sizeof(u32);
    if (size % stride != 0)
    {
        throw Error("fixed-function draw has a partial vertex");
    }
    std::array<float, 16> world_view;
    std::array<float, 16> composite;
    multiply_runtime_graphics_matrices(world_view.data(), draw.world.data(), draw.view.data());
    multiply_runtime_graphics_matrices(composite.data(), world_view.data(), draw.projection.data());
    const auto color = [](u32 packed)
    {
        // D3DCOLOR: A8R8G8B8.
        return std::array<float, 4>{float((packed >> 16) & 0xFF) / 255.f, float((packed >> 8) & 0xFF) / 255.f,
            float(packed & 0xFF) / 255.f, float(packed >> 24) / 255.f};
    };
    const auto* data = reinterpret_cast<const u8*>(draw.vertices.data());
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 4: per-thread scratch keeps its capacity between draws.
    thread_local std::vector<GfxNativeVertex> shaded;
    thread_local std::vector<XboxVector> screens;
    thread_local std::vector<GfxNativeVertex> vertices;
    shaded.clear();
    screens.clear();
    vertices.clear();
#else
    std::vector<GfxNativeVertex> shaded;
    std::vector<XboxVector> screens;
#endif
    for (size_t offset = 0; offset < size; offset += stride)
    {
        float position[4]{};
        std::memcpy(position, data + offset, pretransformed ? 16 : 12);
        XboxVector screen{};
        if (pretransformed)
        {
            screen = {position[0], position[1], position[2], 1.f / position[3]};
        }
        else
        {
            std::array<float, 4> clip{};
            for (u32 column = 0; column < 4; ++column)
            {
                clip[column] = position[0] * composite[column] + position[1] * composite[4 + column]
                    + position[2] * composite[8 + column] + composite[12 + column];
            }
            const float reciprocal = 1.f / clip[3];
            screen = {clip[0] * reciprocal * draw.screen_scale[0] + draw.screen_offset[0],
                clip[1] * reciprocal * draw.screen_scale[1] + draw.screen_offset[1],
                clip[2] * reciprocal * draw.screen_scale[2] + draw.screen_offset[2], clip[3]};
        }
        if (line_primitive(draw.primitive))
        {
            // The NV2A samples pixel centres on whole coordinates and D3D11 half a pixel in, so a line drawn
            // along a pixel row moves half a pixel to land on that row instead of on its edge.
            screen[0] += 0.5f;
            screen[1] += 0.5f;
        }
        GfxNativeVertex vertex{};
        vertex.position = clip_position(screen);
        u32 packed = draw.diffuse;
        if (has_diffuse)
        {
            std::memcpy(&packed, data + offset + (pretransformed ? 16 : 12), 4);
        }
        vertex.diffuse = color(packed);
        if (textured)
        {
            std::memcpy(vertex.uv[0].data(), data + offset + 12, 8);
        }
        shaded.push_back(vertex);
        screens.push_back(screen);
    }
#if !GBTG_OPTIMIZE
    std::vector<GfxNativeVertex> vertices;
#endif
    const auto& order = triangle_order(draw.primitive, u32(shaded.size()));
    // D3DRS_LINEWIDTH is in retail framebuffer pixels (the 640x480 screen space above), and D3D11 lines are one
    // output pixel, so every line becomes a quad across each segment: at a larger output a one-pixel line still
    // covers its whole retail row (the game select panel's caps, 0x00061360, are one line per row).
    const float width = std::bit_cast<float>(state(draw.states, 0x10050, std::bit_cast<u32>(1.f)));
    const bool wide = line_primitive(draw.primitive) && width > 0.f;
    if (wide)
    {
        for (size_t index = 0; index + 1 < order.size(); index += 2)
        {
            const XboxVector& a = screens[order[index]];
            const XboxVector& b = screens[order[index + 1]];
            const float dx = b[0] - a[0];
            const float dy = b[1] - a[1];
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length == 0.f)
            {
                continue;
            }
            const float px = -dy / length * width * 0.5f;
            const float py = dx / length * width * 0.5f;
            const auto corner = [&](const XboxVector& end, u32 source, float sign)
            {
                GfxNativeVertex vertex = shaded[source];
                vertex.position = clip_position({end[0] + px * sign, end[1] + py * sign, end[2], end[3]});
                return vertex;
            };
            const GfxNativeVertex a0 = corner(a, order[index], 1.f);
            const GfxNativeVertex a1 = corner(a, order[index], -1.f);
            const GfxNativeVertex b0 = corner(b, order[index + 1], 1.f);
            const GfxNativeVertex b1 = corner(b, order[index + 1], -1.f);
            vertices.insert(vertices.end(), {a0, b0, b1, a0, b1, a1});
        }
    }
    else
    {
        for (u32 index : order)
        {
            vertices.push_back(shaded[index]);
        }
    }
    GfxNativeDraw output;
    output.lines = line_primitive(draw.primitive) && !wide;
    output.vertices = vertices;
    if (textured)
    {
        output.textures[0] = &context.texture(draw.texture);
        output.address_u[0] = draw.address_u;
        output.address_v[0] = draw.address_v;
    }
    apply_states(draw.states, output);
    gfx_draw_native(output);
}

// Constants c0x60..c0x63 are the transposed transform: clip = row . (x, y, z, 1).
void present_quad(const SceneD3DQuad& quad, const SceneD3DPresentContext& context)
{
    if (quad.texture == nullptr)
    {
        return;
    }
    std::array<GfxNativeVertex, 4> corners{};
    for (u32 corner = 0; corner < 4; ++corner)
    {
        const auto& input = quad.vertices[corner];
        for (u32 row = 0; row < 4; ++row)
        {
            const float* m = quad.transform.data() + row * 4;
            corners[corner].position[row] = m[0] * input[0] + m[1] * input[1] + m[2] * input[2] + m[3];
        }
        corners[corner].diffuse = quad.color;
        corners[corner].uv[0] = {input[3], input[4]};
    }
    const std::array<GfxNativeVertex, 6> vertices{corners[0], corners[1], corners[2], corners[0], corners[2],
        corners[3]};
    GfxNativeDraw output;
    output.vertices = vertices;
    output.textures[0] = &context.texture(quad.texture);
    output.address_u[0] = quad.address_u;
    output.address_v[0] = quad.address_v;
    apply_states(quad.states, output);
    gfx_draw_native(output);
}

#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 4: a run of adjacent quads under one snapshot, texture and address mode
// (dialogue glyphs) goes out as one triangle list. Triangles rasterize in list order, so blending
// matches drawing each quad with present_quad.
bool same_quad_draw(const SceneD3DQuad& first, const SceneD3DQuad& next)
{
    return next.texture == first.texture && next.address_u == first.address_u && next.address_v == first.address_v
        && next.states.identity() != nullptr && next.states.identity() == first.states.identity();
}

void present_quads(std::span<const SceneD3DCommand> run, const SceneD3DPresentContext& context)
{
    const auto& first = std::get<SceneD3DQuad>(run.front());
    if (first.texture == nullptr)
    {
        return;
    }
    thread_local std::vector<GfxNativeVertex> vertices;
    vertices.clear();
    for (const auto& command : run)
    {
        const auto& quad = std::get<SceneD3DQuad>(command);
        std::array<GfxNativeVertex, 4> corners{};
        for (u32 corner = 0; corner < 4; ++corner)
        {
            const auto& input = quad.vertices[corner];
            for (u32 row = 0; row < 4; ++row)
            {
                const float* m = quad.transform.data() + row * 4;
                corners[corner].position[row] = m[0] * input[0] + m[1] * input[1] + m[2] * input[2] + m[3];
            }
            corners[corner].diffuse = quad.color;
            corners[corner].uv[0] = {input[3], input[4]};
        }
        vertices.insert(vertices.end(), {corners[0], corners[1], corners[2], corners[0], corners[2], corners[3]});
    }
    GfxNativeDraw output;
    output.vertices = vertices;
    output.textures[0] = &context.texture(first.texture);
    output.address_u[0] = first.address_u;
    output.address_v[0] = first.address_v;
    apply_states(first.states, output);
    gfx_draw_native(output);
}
#endif
}

void present_scene_d3d_commands(std::span<const SceneD3DCommand> commands, const SceneD3DPresentContext& context)
{
    if (render_profile::capture_replay)
    {
        const std::vector<SceneD3DCommand> retained(commands.begin(), commands.end());
        const SceneD3DPresentContext owner = context;
        auto previous = std::move(render_profile::replay);
        render_profile::replay = [previous = std::move(previous), retained, owner]()
        {
            if (previous)
            {
                previous();
            }
            present_scene_d3d_commands(retained, owner);
        };
    }
    render_profile::Scope measurement(render_profile::Presenter);
    struct Batch
    {
        Batch()
        {
            gfx_begin_native_batch();
        }
        ~Batch()
        {
            gfx_end_native_batch();
        }
    } batch;
    for (size_t index = 0; index < commands.size(); ++index)
    {
        const auto& command = commands[index];
        if (const auto* draw = std::get_if<SceneD3DIndexedDraw>(&command))
        {
            present_indexed(*draw, context);
        }
        else if (const auto* quad = std::get_if<SceneD3DQuad>(&command))
        {
#if GBTG_OPTIMIZE
            // GBTG_OPTIMIZE step 4: merge the run of quads that draw alike.
            size_t end = index + 1;
            while (end < commands.size())
            {
                const auto* next = std::get_if<SceneD3DQuad>(&commands[end]);
                if (next == nullptr || !same_quad_draw(*quad, *next))
                {
                    break;
                }
                ++end;
            }
            present_quads(commands.subspan(index, end - index), context);
            index = end - 1;
#else
            present_quad(*quad, context);
#endif
        }
        else if (const auto* draw = std::get_if<SceneD3DInlineDraw>(&command))
        {
            present_inline(*draw, context);
        }
        else if (const auto* draw = std::get_if<SceneD3DFixedDraw>(&command))
        {
            present_fixed(*draw, context);
        }
        else if (const auto* clear = std::get_if<SceneD3DClear>(&command))
        {
            gfx_end_native_batch();
            gfx_clear_native(clear->rectangles, clear->flags, clear->color, clear->depth);
            gfx_begin_native_batch();
        }
        else if (const auto* copy = std::get_if<SceneD3DCopy>(&command))
        {
            gfx_end_native_batch();
            if (!context.copy_back_buffer)
            {
                throw Error("back-buffer copy presentation is not bound");
            }
            context.copy_back_buffer(copy->texture);
            gfx_begin_native_batch();
        }
        else
        {
            gfx_end_native_batch();
            gfx_begin_native_batch();
            const auto& target = std::get<SceneD3DTarget>(command);
            if (!context.render_target)
            {
                if (target.color != SceneD3DDevice::kBackBuffer)
                {
                    throw Error("dialogue render target presentation is not bound");
                }
            }
            else
            {
                context.render_target(target.color, target.depth);
            }
        }
    }
}
}
