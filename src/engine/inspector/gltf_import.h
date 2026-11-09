#pragma once
// Reads a glTF 2.0 binary (.glb) for the inspector's model import: skinned triangle primitives, their morph targets,
// the skin's joint names and bind points, and the base colour images. Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/texture.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace gbtg
{
struct GltfVertex
{
    Vec3 position{};
    Vec3 normal{};
    Vec2 uv{};
    u32 colour = 0xFFFFFFFF; // D3DCOLOR order (B, G, R, A in memory).
    std::array<u16, 4> joints{}; // Indices into GltfScene::joints.
    std::array<float, 4> weights{1.f, 0.f, 0.f, 0.f};
};

struct GltfPrimitive
{
    std::string mesh; // The mesh's name.
    std::string material; // The material's name, without a ".001"-style suffix.
    std::vector<GltfVertex> vertices;
    std::vector<u32> indices; // Triangles.
    bool has_normals = false;
    bool has_colours = false;
    std::vector<std::string> morph_names;
    std::vector<std::vector<Vec3>> morph_positions; // Per target, a delta per vertex.
    std::vector<std::vector<Vec3>> morph_normals; // Empty per target without normal deltas.
    int image = -1; // Its base colour image in GltfScene::images.
};

struct GltfJoint
{
    std::string name;
    Vec3 bind{}; // The joint's bind point in mesh space: the inverse bind matrix's inverse translation.
};

struct GltfScene
{
    std::vector<GltfPrimitive> primitives;
    std::vector<GltfJoint> joints; // The skin's joints, in its order.
    std::vector<RgbaImage> images;
    std::vector<std::string> image_names;
    std::vector<std::string> notes; // What the reader skipped or converted.
};

GltfScene read_glb(const std::filesystem::path& path);
}
#endif
