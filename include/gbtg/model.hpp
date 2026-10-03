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
    std::vector<u8> skin; // ndRigidSkinIdx: palette slot -> skeleton joint
    u8 albedo_uv = 0;
    u8 lightmap_uv = 1;
};

// Skeleton joint. local/world are the bind translations. parent is the u16 link at record +4.
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
    std::vector<u16> bones;    // rigid joint index, parallel to positions. (shader slot - 24) / 3
    std::vector<Joint> joints;
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
