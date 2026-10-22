#include "scene_light_grid.h"
#include "../entities/entity.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
// The data offset of the model's first type-0x11 block (blocks at CPU +0, count +4), 0 for none or a null one.
u32 light_block(std::span<const u8> model)
{
    const u32 table = read_u32(model, 0);
    const u32 count = read_u32(model, 4);
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(model, table + index * 8) == 0x11)
        {
            return read_u32(model, table + index * 8 + 4);
        }
    }
    return 0;
}

Vec3 vertex_at(std::span<const u8> model, u32 offset)
{
    return Vec3{read_f32(model, offset), read_f32(model, offset + 4), read_f32(model, offset + 8)};
}

// The inlined face normal: the cross product of the triangle's edges (p2 - p1, p3 - p1), normalized (0x001534C0)
// unless it is zero, packed as 11 bits (x 1023), 10 bits (z 511) and 11 bits (y 1023) through _ftol (0x001673D8).
u32 packed_face_normal(const Vec3& p1, const Vec3& p2, const Vec3& p3)
{
    const float a = (p2.z - p1.z) * (p3.x - p1.x) - (p3.z - p1.z) * (p2.x - p1.x);
    const float c = (p3.y - p1.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p3.x - p1.x);
    const float b = (p3.z - p1.z) * (p2.y - p1.y) - (p3.y - p1.y) * (p2.z - p1.z);
    const float length = (b * b + a * a) + c * c;
    float x = a;
    float y = b;
    double z = c;
    if (length != 0.f) // UCOMISS DAT_0032B100; JNP skips only an ordered zero.
    {
        const float scale = engine_reciprocal_sqrt(length); // 0x001534C0
        x = a * scale;
        y = b * scale;
        z = double(c) * double(scale); // Kept on the x87 stack.
    }
    const u32 first = u32(i32(double(x) * 1023.0)) & 0x7FFu; // DAT_0032B34C.
    const u32 middle = u32(i32(z * 511.0)); // DAT_0032B348.
    const u32 last = u32(i32(double(y) * 1023.0)) & 0x7FFu;
    return ((first | (middle << 11)) << 11) | last;
}
}

u32 runtime_light_grid_offset(std::span<const u8> model)
{
    if (model.empty())
    {
        return 0;
    }
    return read_u32(model, read_u32(model, 0x10));
}

// Xbox 0x00112D70
void build_runtime_scene_light_grid(RuntimeSceneLightGrid& grid, RuntimeSceneLightGridD3D& d3d)
{
    if (!grid.background)
    {
        return;
    }
    const auto model = grid.model;
    const u32 header = read_u32(model, 0x10);
    const u32 cells = read_u32(model, header);
    if (cells != 0)
    {
        const u32 count = read_u32(model, cells + 0x10) * read_u32(model, cells + 0x14) * read_u32(model, cells + 0x18);
        grid.visible.assign(count, 1); // 0x000F72D0, filled with 1.
    }
    const u32 block = light_block(model);
    if (block == 0)
    {
        return;
    }
    const u32 vertices = read_u32(model, block + 8);
    if (vertices == 0 || read_u32(model, block + 0xC) == 0) // 0x00053030 returns the block's +0xC.
    {
        return;
    }
    grid.part_count = model[read_u32(model, header + 0x10) + 1];
    grid.part_buffers.assign(grid.part_count, 0); // 0x000F72D0
    grid.part_vertices.assign(grid.part_count, 0);
    for (u32 part = 0; part < grid.part_count; ++part)
    {
        const u32 again = light_block(model);
        if (again == 0)
        {
            throw Error("0x00112D70 reads a part's triangles from a null light block");
        }
        const u32 triangles = read_u32(model, again + 0xC);
        const i32 count = std::bit_cast<i32>(read_u32(model, triangles + part * 8));
        const u32 length = u32(count) * 0x30;
        std::vector<u8> soup(length); // 0x000F72D0
        const u32 positions = read_u32(model, vertices + 4 + part * 8);
        for (i32 index = 0; index < count; ++index)
        {
            const u32 corners = read_u32(model, triangles + 4 + part * 8) + u32(index) * 0xC;
            const Vec3 p1 = vertex_at(model, positions + read_u32(model, corners) * 0xC);
            const Vec3 p2 = vertex_at(model, positions + read_u32(model, corners + 4) * 0xC);
            const Vec3 p3 = vertex_at(model, positions + read_u32(model, corners + 8) * 0xC);
            u8* out = soup.data() + size_t(index) * 0x30;
            std::memcpy(out, &p1, 12);
            std::memcpy(out + 0x10, &p2, 12);
            std::memcpy(out + 0x20, &p3, 12);
            const u32 normals_block = light_block(model);
            const u32 normals = normals_block == 0 ? 0 : read_u32(model, normals_block + 0x10);
            bool computed = normals == 0;
            if (!computed)
            {
                const u32 row = read_u32(model, normals + part * 4) + u32(index) * 0xC;
                const u32 n0 = read_u32(model, row);
                const u32 n1 = read_u32(model, row + 4);
                const u32 n2 = read_u32(model, row + 8);
                std::memcpy(out + 0xC, &n0, 4);
                std::memcpy(out + 0x1C, &n1, 4);
                std::memcpy(out + 0x2C, &n2, 4);
                // The words are compared as floats.
                computed = std::bit_cast<float>(n2) == 0.f || std::bit_cast<float>(n1) == 0.f
                    || std::bit_cast<float>(n0) == 0.f;
            }
            if (computed)
            {
                const u32 packed = packed_face_normal(p1, p2, p3);
                for (u32 corner = 0; corner < 3; ++corner)
                {
                    std::memcpy(out + 0xC + corner * 0x10, &packed, 4);
                }
            }
        }
        const u32 buffer = d3d.create_vertex_buffer(length); // 0x001779C0
        grid.part_buffers[part] = buffer;
        grid.part_vertices[part] = u32(count) * 3;
        const auto locked = d3d.lock(buffer); // 0x00177A10
        if (locked.size() < length)
        {
            throw Error("0x00112D70 copies a part past its vertex buffer");
        }
        std::memcpy(locked.data(), soup.data(), length);
        // HeapFree (0x0014D360) of the soup.
    }
}

// Xbox 0x001133B0
void release_runtime_scene_light_grid(RuntimeSceneLightGrid& grid, RuntimeSceneLightGridD3D& d3d)
{
    grid.visible.clear(); // 0x000663E0
    if (!grid.part_buffers.empty())
    {
        for (u32 part = 0; part < grid.part_count; ++part)
        {
            if (grid.part_buffers[part] != 0)
            {
                d3d.release(grid.part_buffers[part]); // 0x001759F0
            }
        }
        grid.part_buffers.clear(); // HeapFree
        grid.part_count = 0;
        grid.part_vertices.clear();
    }
    grid.background = false;
    grid.model = {};
    grid.model_address = 0;
    grid.parts = {};
}
}
