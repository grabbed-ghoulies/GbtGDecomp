#pragma once

#include "../frontend/dialogue_draw_outer.h"
#include <map>
#include <variant>
#include <vector>

namespace gbtg
{
// Host D3D device behind the translated dialogue and model draw boundaries. It keeps the device state
// the dump reads back (transforms, vertex constants, textures, render targets) and records each draw,
// clear and target change in issue order with the state it was issued under. Inline arrays with a
// created shader preserve raw vertex words; zero-handle dialogue quads retain their x,y,z,u,v path.
using SceneD3DConstants = std::array<std::array<float, 4>, 192>; // Hardware numbering: register + 96.
struct SceneD3DQuad
{
    std::array<std::array<float, 5>, 4> vertices; // Vertex shader input, before the c0x60 transform.
    std::array<float, 16> transform; // Vertex constants c0x60..c0x63 at the draw.
    std::array<float, 4> color; // Vertex constant c0x64 at the draw.
    const u8* texture; // Stage 0 texture record.
    std::map<u32, u32> states;
};
struct SceneD3DVertexStream
{
    u32 resource;
    u32 stride;
};
struct SceneD3DIndexedDraw
{
    u32 primitive;
    u32 count;
    u32 indices;
    SceneD3DConstants constants;
    std::array<const u8*, 4> textures;
    std::map<u32, u32> states;
    std::map<u32, SceneD3DVertexStream> streams;
    std::array<std::array<float, 4>, 16> pixel_constants;
    u32 pixel_constants_set; // Bit n: SetPixelShaderConstant wrote register n.
    std::array<u32, 4> address_u{1, 1, 1, 1}; // D3DTSS_ADDRESSU / ADDRESSV at the draw.
    std::array<u32, 4> address_v{1, 1, 1, 1};
};
struct SceneD3DClear
{
    std::vector<std::array<i32, 4>> rectangles;
    u32 flags;
    u32 color;
    float depth;
};
struct SceneD3DInlineDraw
{
    SceneD3DIndexedDraw state;
    std::vector<u32> vertices;
};
struct SceneD3DTarget
{
    std::uintptr_t color;
    std::uintptr_t depth;
};
using SceneD3DCommand = std::variant<SceneD3DQuad, SceneD3DIndexedDraw, SceneD3DInlineDraw, SceneD3DClear, SceneD3DTarget>;
struct SceneD3DVertexShader
{
    std::array<i32, 32> declaration;
    std::vector<u32> program;
    u32 usage;
};

class SceneD3DDevice
{
public:
    // Render-state keys outside the NV2A method space.
    static constexpr u32 kCullMode = 0x10060; // D3DDevice_SetRenderState_CullMode.
    static constexpr u32 kZEnable = 0x10002; // D3DDevice_SetRenderState_ZEnable.
    static constexpr u32 kVertexShader = 0x10003;
    static constexpr u32 kPixelShader = 0x10004;
    static constexpr u32 kViewport = 0x10030; // Six words: X, Y, Width, Height, MinZ, MaxZ.
    static constexpr u32 kBackBuffer = 1;
    static constexpr u32 kDepthBuffer = 2;
    // D3D's screen-space scale and offset (vertex constants -38 and -37) for the viewport.
    static constexpr float kDepthScale = 16777215.f; // D24S8.

    SceneD3DDevice();
    DialogueD3DBoundaries& dialogue() { return dialogue_; }
    DialogueOuterD3DBoundaries& outer() { return outer_; }
    std::vector<SceneD3DCommand> commands; // Since the last take_commands.
    std::vector<SceneD3DCommand> take_commands();
    // Model renderer boundaries: render and texture-stage states, vertex constants and indexed push-buffer
    // draws (D3D 0x00171050, 0x001733E0, 0x001729C0).
    void set_state(u32 key, u32 value);
    void set_constant_words(u32 reg, const u32* data, u32 count);
    SceneD3DIndexedDraw& draw_indexed(u32 primitive, u32 count, u32 indices);
    std::map<u32, SceneD3DVertexShader> vertex_shaders;
    void create_vertex_shader(const i32* declaration, const u32* program, u32& handle, u32 usage);
    void set_vertex_stream(u32 stream, u32 resource, u32 stride);

private:
    void decode_push(const u32* begin, const u32* end);
    std::map<u32, std::array<float, 16>> transforms_;
    SceneD3DConstants constants_{};
    std::array<const u8*, 4> textures_{};
    std::map<u32, u32> render_states_;
    std::map<u32, SceneD3DVertexStream> vertex_streams_;
    std::array<std::array<float, 4>, 16> pixel_constants_{};
    u32 pixel_constants_set_ = 0;
    std::vector<u32> push_;
    u32 next_surface_ = 1;
    u32 next_vertex_shader_ = 1;
    DialogueD3DBoundaries dialogue_;
    DialogueOuterD3DBoundaries outer_;
};
}
