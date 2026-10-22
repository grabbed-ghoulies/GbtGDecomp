#pragma once
#include "gbtg/entity.h"

#include <functional>
#include <span>
#include <vector>

namespace gbtg
{
// The first background's light grid (0x00112D70 at scene start, 0x001133B0 at its end). The background model's CPU
// header +0x10 is the lighting header: +0 the grid (+0x10 / +0x14 / +0x18 the cell counts, +0x20..+0x28 the origin,
// +0x38 the cell size, +0x3C the 0x1C-byte cells, each +8 its vertex bytes, +0xC set when drawn and +0x10 its vertex
// buffer) and +0x10 the parts block (byte 1 the part count). The model's type-0x11 block gives each part's triangles.
struct RuntimeSceneLightGrid
{
    // DAT_0050E374: the first background (DAT_00511B20 - 0x934). Host: its model's CPU header (pointers as offsets),
    // that header's retail address, and its +0x950 part rows (stride 0x1E0), empty without them.
    bool background = false;
    std::span<const u8> model;
    u32 model_address = 0;
    std::span<u8> parts;
    std::vector<u8> visible; // DAT_0050E378, one byte per grid cell.
    std::vector<u32> part_buffers; // DAT_0050E37C, each part's vertex buffer.
    std::vector<u32> part_vertices; // DAT_0050E380, each part's vertex count.
    u32 part_count = 0; // DAT_0050E384.
};
struct RuntimeSceneLightGridD3D
{
    std::function<u32(u32 length)> create_vertex_buffer; // D3DDevice_CreateVertexBuffer2 (0x001779C0).
    std::function<std::span<u8>(u32 buffer)> lock; // D3DVertexBuffer_Lock2 (0x00177A10).
    std::function<void(u32 resource)> release; // D3DResource_Release (0x001759F0).
};
// The grid offset (the lighting header's +0) of a background model; 0 without one.
u32 runtime_light_grid_offset(std::span<const u8> model);
void build_runtime_scene_light_grid(RuntimeSceneLightGrid& grid, RuntimeSceneLightGridD3D& d3d); // Xbox 0x00112D70
void release_runtime_scene_light_grid(RuntimeSceneLightGrid& grid, RuntimeSceneLightGridD3D& d3d); // Xbox 0x001133B0
}
