#pragma once

#include "gbtg/types.hpp"
#include "gbtg/texture.hpp"
#include "gbtg/bundle.hpp"

namespace gbtg
{

struct Submesh
{
    u32 first_index = 0;
    u32 index_count = 0;
    i32 texture = -1; // -1 untextured, -2 skip draw (colour0)
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
    std::vector<u16> bones;    // v4.x palette slot. Shader 1 ARLs this, then the slot at bone2.
    std::vector<u16> bone2;    // v4.y. Unused slots stay 0.
    std::vector<float> weight0; // v5.x. Shader 1 scales the first matrix row by this.
    std::vector<float> weight1; // v5.y. Rigid vertices are 0.
    Vec3 skin_delta;           // added to joint bind points to land the skeleton on the vertices
    std::vector<Joint> joints;
    std::vector<u16> mtx_joints; // ndMtxArray record -> skeleton joint (0x155E60)
    std::vector<Vec2> uvs;
    std::vector<Vec2> uv1;
    std::vector<Vec2> uv2;
    std::vector<u16> indices; // triangle list
    std::vector<Submesh> submeshes;
    std::vector<RgbaImage> textures;
};

Mesh parse_model(const BundleEntry& entry);
std::vector<Mesh> extract_models(const Bundle& bundle);

} // namespace gbtg
