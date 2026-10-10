#pragma once

#include "gbtg/types.h"
#include "gbtg/texture.h"
#include "gbtg/bundle.h"

namespace gbtg
{

struct Submesh
{
    std::string shader_name;
    u32 first_index = 0;
    u32 index_count = 0;
    i32 texture = -1; // -1 untextured, -2 skip draw (colour0)
    i32 colour0_stage = -1; // ndShaderParam2 named texture index, not necessarily stage 0.
    i32 layer1 = -1;  // colour1 (often a light layer on env meshes)
    i32 detail = -1;  // colour2/colour3 detail layer
    i32 lightmap = -1;
    i32 celmap = -1; // RARE_celmap0 ramp (actors)
    std::vector<u8> skin; // ndRigidSkinIdx: shader slot -> ndMtxArray record
    std::vector<u16> mtx; // ancestor ndMtxArray: record -> skeleton joint
    Vec3 color{1.f, 1.f, 1.f}; // 0x15AD20 pixel constant 0, byte/255
    u8 alpha_ref = 0;          // 0x15AD20 byte +24. 0 leaves the alpha test off.
    u8 albedo_uv = 0;
    u8 lightmap_uv = 1;
    std::array<i32, 4> stages{-1, -1, -1, -1};
    std::array<u8, 4> stage_uv{0, 1, 0, 0};
    std::array<u8, 4> address_u{1, 1, 1, 1};
    std::array<u8, 4> address_v{1, 1, 1, 1};
    std::array<u32, 60> pixel_shader{};
    bool has_pixel_shader = false;
    bool direct_uv = false;
    i32 render_mode = -1; // 0 opaque, 1 alpha/read-only depth, 2 alpha/depth write, 3 additive
    std::array<u8, 2> colour_source{255, 255}; // declared direct MOV to oD0/oD1
    u8 skin_weight_mask = 0; // zero: rigid shader does not read the weight stream
    bool uses_skin_palette = false;
    u8 cull_disable = 0; // ndPushBuffer +0x3C; nonzero draws with CullMode(0) (0x001582A0).
    std::int8_t node_state_slot = -1; // ndPushBuffer +0x3D; -1 draws unconditionally.
    u8 node_state_value = 0; // +0x3E, compared with the instance's two-bit state.
};

// Skeleton joint from ndSkeleton. parent is the u16 at record +0 (0xFFFF is the root).
// local is record +4, bind/world is record +16.
struct Joint
{
    Vec3 local;
    Vec3 world;
    int parent = -1;
};

struct Mesh
{
    std::string name;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals; // type 0x0A, same count as positions when the mesh is lit
    std::vector<u32> diffuse; // descriptor type 11, packed D3DCOLOR (ARGB)
    std::vector<u32> specular; // descriptor type 12
    std::vector<u16> bones;    // v4.x palette slot. Shader 1 ARLs this, then the slot at bone2.
    std::vector<u16> bone2;    // v4.y. Unused slots stay 0.
    std::vector<u16> bone3;
    std::vector<u16> bone4;
    std::vector<float> weight0; // v5.x. Shader 1 scales the first matrix row by this.
    std::vector<float> weight1; // v5.y. Rigid vertices are 0.
    std::vector<float> weight2;
    std::vector<float> weight3;
    Vec3 skin_delta;           // added to joint bind points to land the skeleton on the vertices
    std::vector<Joint> joints;
    std::vector<u16> mtx_joints; // ndMtxArray record -> skeleton joint (0x155E60)
    std::vector<Vec2> uvs;
    std::vector<Vec2> uv1;
    std::vector<Vec2> uv2;
    std::vector<Vec2> uv3;
    std::vector<u32> indices; // triangle list; concatenated buffers may exceed 65535 vertices
    std::vector<Submesh> submeshes;
    std::vector<RgbaImage> textures;
    std::vector<std::string> issues; // missing rendering paths; never substitute invented geometry
    std::array<u8, 8> node_flags{}; // instance +0x294, initialized by 0x000F9840.
};

bool model_submesh_visible(const Mesh& mesh, std::int8_t slot, u8 value); // Xbox 0x00158240
Mesh parse_model(const BundleEntry& entry);
std::vector<Mesh> extract_models(const Bundle& bundle);

} // namespace gbtg
