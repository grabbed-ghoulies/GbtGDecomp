#pragma once
// A serialized (unrelocated) model resource as the loader (0x00052910) receives it: CPU offsets from its first byte,
// the node tree's own pointers from the tree, vertex and texture data at offsets across its GPU chunks. Read here for
// the inspector's model export and import (model_io.h). Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/bundle.h"
#include "gbtg/retail_field.h"
#include "gbtg/texture.h"

#include <array>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace gbtg
{
struct SerializedImage
{
    std::span<const u8> cpu;

    u32 word(u32 at, u32 offset = 0) const
    {
        return read_u32(cpu, at + offset);
    }
    u16 half(u32 at, u32 offset = 0) const
    {
        return read_u16(cpu, at + offset);
    }
    u8 byte(u32 at, u32 offset = 0) const
    {
        if (at + offset >= cpu.size())
        {
            throw Error("serialized model read past its CPU block");
        }
        return cpu[at + offset];
    }
    template <class T, u32 Offset>
    T get(u32 at, RetailField<T, Offset>) const
    {
        T value;
        if (at + Offset + sizeof(T) > cpu.size())
        {
            throw Error("serialized model field past its CPU block");
        }
        std::memcpy(&value, cpu.data() + at + Offset, sizeof(T));
        return value;
    }
    std::string text(u32 at) const
    {
        std::string out;
        while (at < cpu.size() && cpu[at] != 0 && out.size() < 256)
        {
            out += char(cpu[at++]);
        }
        return out;
    }
};

// The GPU chunks as one offset space, in order (0x00051F20).
struct GpuSpace
{
    const BundleEntry* entry = nullptr;
    std::span<const u8> at(u32 offset, size_t size) const;
};

struct ModelJoint
{
    std::string name;
    int parent = -1;
    Vec3 world{}; // The joint's bind point (record +0x10).
};

struct ModelVertex
{
    Vec3 position{};
    Vec3 normal{};
    Vec2 uv{};
    std::array<u16, 2> joints{};
    std::array<float, 2> weights{1.f, 0.f};
    u32 colour = 0xFFFFFFFF; // The kind 0xB stream (D3DCOLOR, B G R A in memory).
};

// One material's triangles in the body or the blend-shape (head) branch.
struct ModelPrimitive
{
    std::string material; // The shader name ("shader_jeans"), else "material_<node>".
    u32 material_node = 0; // The ndShaderParam2 node, tree-relative.
    bool head = false;
    std::vector<ModelVertex> vertices;
    std::vector<u32> indices; // Triangles.
    std::vector<std::vector<Vec3>> morphs; // Head only: per blend-shape target after the base, a delta per vertex.
};

struct ModelTextureInfo
{
    u32 index = 0; // In the texture table (directory kind 7).
    u32 format = 0;
    u16 width = 0;
    u16 height = 0;
    u32 data = 0; // GPU offset.
    u32 frame_size = 0;
    RgbaImage image;
};

struct ModelMaterialInfo
{
    std::string name;
    u32 node = 0;
    i32 colour_texture = -1; // The texture its 'colour0' stage samples, or -1.
    i32 celmap_texture = -1;
    i32 lightmap_texture = -1;
};

struct DecodedModel
{
    std::string aid;
    std::vector<ModelJoint> joints;
    std::vector<ModelPrimitive> primitives;
    std::vector<std::string> morph_names; // One per blend-shape target after the base.
    std::vector<ModelTextureInfo> textures;
    std::vector<ModelMaterialInfo> materials;
    bool winding_flipped = false; // Strips are emitted reversed so faces agree with their normals.
};

DecodedModel decode_serialized_model(const BundleEntry& entry);
}
#endif
