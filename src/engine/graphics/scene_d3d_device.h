#pragma once

#include "../frontend/dialogue_draw_outer.h"
#include <map>
#include <vector>

namespace gbtg
{
// Host D3D device behind the translated dialogue draw boundaries. It keeps the device state the dump
// reads back (transforms, vertex constants, textures, render targets) and decodes the push-buffer
// quads it receives (SET_BEGIN_END QUADS, inline array of x, y, z, u, v) into recorded draws.
struct SceneD3DQuad
{
    std::array<std::array<float, 5>, 4> vertices; // Vertex shader input, before the c0x60 transform.
    std::array<float, 16> transform; // Vertex constants c0x60..c0x63 at the draw.
    std::array<float, 4> color; // Vertex constant c0x64 at the draw.
    const u8* texture; // Stage 0 texture header.
};
class SceneD3DDevice
{
public:
    SceneD3DDevice();
    DialogueD3DBoundaries& dialogue() { return dialogue_; }
    DialogueOuterD3DBoundaries& outer() { return outer_; }
    std::vector<SceneD3DQuad> quads; // Draws since the last take_quads.
    std::vector<SceneD3DQuad> take_quads();
    // Model renderer boundaries: render and texture-stage states, vertex constants and indexed push-buffer
    // draws (D3D 0x00171050, 0x001733E0, 0x001729C0).
    void set_state(u32 key, u32 value);
    void set_constant_words(u32 reg, const u32* data, u32 count);
    struct IndexedDraw
    {
        u32 primitive;
        u32 count;
        u32 indices;
    };
    std::vector<IndexedDraw> indexed_draws;
    void draw_indexed(u32 primitive, u32 count, u32 indices);
    struct VertexShader
    {
        std::array<i32, 32> declaration;
        std::vector<u32> program;
        u32 usage;
    };
    std::map<u32, VertexShader> vertex_shaders;
    void create_vertex_shader(const i32* declaration, const u32* program, u32& handle, u32 usage);
    struct VertexStream
    {
        u32 resource;
        u32 stride;
    };
    std::map<u32, VertexStream> vertex_streams;
    void set_vertex_stream(u32 stream, u32 resource, u32 stride);

private:
    void decode_push(const u32* begin, const u32* end);
    std::map<u32, std::array<float, 16>> transforms_;
    std::array<std::array<float, 4>, 192> constants_{};
    std::array<const u8*, 4> textures_{};
    std::map<u32, u32> render_states_;
    std::vector<u32> push_;
    u32 next_surface_ = 1;
    u32 next_vertex_shader_ = 1;
    DialogueD3DBoundaries dialogue_;
    DialogueOuterD3DBoundaries outer_;
};
}
