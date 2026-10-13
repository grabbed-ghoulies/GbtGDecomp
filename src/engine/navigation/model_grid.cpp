#include "model_grid.h"
#include "grid_preparation.h"
#include "cell_registration.h"
#include "../entities/entity_collision.h"
#include "../entities/entity_heap.h"
#include "../scene/ambient.h"

#include <bit>
#include <cstring>
#include <new>
#include <xmmintrin.h>

namespace gbtg::entity_detail
{
void NavigationGridDeleter::operator()(NavGrid* grid) const
{
    grid->~NavGrid();
    engine_heap_free(grid);
}

void NavigationBlockedCellsDeleter::operator()(NavigationBlockedCells* cells) const
{
    engine_heap_free(cells);
}

// Xbox 0x0004F1B0
void navigation_model_bounds(const NavigationModelGrid& source, Vec3& low, Vec3& high)
{
    const u32 count = u32(source.width) * u32(source.depth);
    if (count != 0)
    {
        low = {read_f32(source.cells, 0), read_f32(source.cells, 4), read_f32(source.cells, 8)};
        high = low;
        for (u32 index = 1; index < count; ++index)
        {
            const Vec3 point{read_f32(source.cells, index * 16),
                read_f32(source.cells, index * 16 + 4), read_f32(source.cells, index * 16 + 8)};
            if (point.x > high.x)
            {
                high.x = point.x;
            }
            if (point.y > high.y)
            {
                high.y = point.y;
            }
            if (point.z > high.z)
            {
                high.z = point.z;
            }
            if (low.x > point.x)
            {
                low.x = point.x;
            }
            if (low.y > point.y)
            {
                low.y = point.y;
            }
            if (low.z > point.z)
            {
                low.z = point.z;
            }
        }
    }
}

// Xbox 0x0004F460
i32 align_navigation_model_bounds(const NavigationModelGrid& source, NavigationModelBounds& bounds)
{
    navigation_model_bounds(source, bounds.low, bounds.high);
    const i32 cell = source.cell;
    i32 value = _mm_cvttss_si32(_mm_set_ss(bounds.low.x));
    i32 remainder = value % cell;
    if (remainder > 0 || remainder < 0)
    {
        if (remainder < 0)
        {
            remainder = std::bit_cast<i32>(u32(remainder) + u32(cell));
        }
        value = std::bit_cast<i32>(u32(value) - u32(remainder));
    }
    bounds.min_x = value;
    value = _mm_cvttss_si32(_mm_set_ss(bounds.high.x));
    remainder = value % cell;
    if (remainder > 0)
    {
        value = std::bit_cast<i32>(u32(value) + u32(cell) - u32(remainder));
    }
    else if (remainder < 0)
    {
        value = std::bit_cast<i32>(u32(value) - u32(remainder));
    }
    bounds.max_x = value;
    value = _mm_cvttss_si32(_mm_set_ss(bounds.low.z));
    remainder = value % cell;
    if (remainder > 0 || remainder < 0)
    {
        if (remainder < 0)
        {
            remainder = std::bit_cast<i32>(u32(remainder) + u32(cell));
        }
        value = std::bit_cast<i32>(u32(value) - u32(remainder));
    }
    bounds.min_z = value;
    value = _mm_cvttss_si32(_mm_set_ss(bounds.high.z));
    const i32 quotient = value / cell;
    remainder = value % cell;
    if (remainder > 0)
    {
        value = std::bit_cast<i32>(u32(value) + u32(cell) - u32(remainder));
    }
    else if (remainder < 0)
    {
        value = std::bit_cast<i32>(u32(value) - u32(remainder));
    }
    bounds.max_z = value;
    return quotient;
}

// Xbox 0x000362F0
NavigationGridOwner allocate_navigation_grid(i32 width, i32 depth, i32 cell)
{
    const u32 count = u32(width) * u32(depth);
    const u32 bytes = count * 0x12u;
    auto* storage = engine_heap_allocate(std::bit_cast<i32>(bytes + u32(sizeof(NavGrid))));
    NavGrid* grid = nullptr;
    if (storage != nullptr)
    {
        grid = new (storage) NavGrid;
        grid->width = width;
        grid->depth = depth;
        grid->cell = cell;
        grid->cost_scale = 1.f; // DAT_0032B0F8.
        grid->cells.resize(count);
        for (auto& entry : grid->cells)
        {
            entry.value_8 = 0x10;
            entry.connections = 0xFF;
        }
    }
    return NavigationGridOwner{grid};
}

// Xbox 0x00111B80
NavigationBlockedCellsOwner decode_navigation_model_grid(i32 floor,
    const NavigationModelGrid& source, NavigationModelBounds& bounds,
    NavigationGridOwner& owner)
{
    align_navigation_model_bounds(source, bounds);
    g_nav_origin_x = bounds.min_x;
    g_nav_origin_z = bounds.min_z;
    owner = allocate_navigation_grid(source.width, source.depth, source.cell);
    g_nav_grids[floor] = owner.get();
    const u32 count = u32(source.width) * u32(source.depth);
    const u32 byte_count = (count + 7u) >> 3;
    auto* blocked = static_cast<NavigationBlockedCells*>(engine_heap_allocate(
        std::bit_cast<i32>(byte_count + u32(sizeof(NavigationBlockedCells)))));
    if (blocked != nullptr)
    {
        blocked->count = count;
        blocked->bytes = byte_count;
        blocked->bits = reinterpret_cast<u8*>(blocked + 1);
        std::memset(blocked->bits, 0, byte_count);
    }
    NavigationBlockedCellsOwner result{blocked};
    float previous_top = g_amb_color_hi[1]; // DAT_0050EA14.
    for (i32 z = 0; z < source.depth; ++z)
    {
        for (i32 x = 0; x < source.width; ++x)
        {
            const u32 index = u32(g_nav_grids[floor]->width) * u32(z) + u32(x);
            const u32 offset = index * 16;
            const Vec3 position{read_f32(source.cells, offset), read_f32(source.cells, offset + 4),
                read_f32(source.cells, offset + 8)};
            auto& entry = g_nav_grids[floor]->cells[index];
            entry.x = i16(_mm_cvttss_si32(_mm_set_ss(position.x)));
            entry.bottom = i16(_mm_cvttss_si32(_mm_set_ss(position.y)));
            entry.z = i16(_mm_cvttss_si32(_mm_set_ss(position.z)));
            const u8 flags = source.cells[offset + 0xD];
            entry.region ^= u16((read_u16(source.cells, offset + 0xE) ^ entry.region) & 0xFFF);
            entry.region = u16((entry.region & 0x9FFF) | ((flags & 3) << 13));
            entry.region = u16((entry.region & 0x7FFF) | ((flags >> 7) << 15));
            entry.flags_11 = u8((entry.flags_11 & 0xC0) | ((flags >> 6) & 1));
            entry.flags_11 ^= u8(((flags << 1) ^ entry.flags_11) & 0x40);
            entry.flags_11 = u8((((flags << 3) ^ entry.flags_11) & 0x7F) ^ (flags << 3));
            const u8 kind = source.cells[offset + 0xC] & 7;
            switch (kind)
            {
            case 0:
                entry.flags_7 = u8((entry.flags_7 & ~2) | 4);
                entry.flags_7 &= ~8;
                break;
            case 1:
                entry.flags_7 |= 6;
                blocked->bits[index >> 3] |= u8(1u << (index & 7));
                entry.flags_7 &= ~8;
                break;
            case 2:
                entry.flags_7 = u8((entry.flags_7 | 2) & ~4);
                blocked->bits[index >> 3] |= u8(1u << (index & 7));
                entry.flags_7 &= ~8;
                break;
            case 3:
                entry.flags_7 = u8((entry.flags_7 & ~2) | 4);
                blocked->bits[index >> 3] |= u8(1u << (index & 7));
                entry.flags_7 |= 8;
                break;
            }
            entry.value_8 = (entry.value_8 & 0xFF00FFFFu) | (u32(source.cells[offset + 0xC] >> 3) << 16);
            if ((entry.flags_7 & 2) == 0)
            {
                Vec3 candidate = position;
                candidate.y += 25.f; // DAT_0032B170.
                Vec3 kept{position.x, previous_top, position.z};
                const i32 hit = entity_record_sweep_probe(kept, candidate, 0xC, 0);
                if (hit == 1)
                {
                    entry.top = i16(_mm_cvttss_si32(_mm_set_ss(kept.y)));
                }
                else
                {
                    entry.top = i16(_mm_cvttss_si32(_mm_set_ss(previous_top)));
                }
            }
            else
            {
                entry.top = i16(_mm_cvttss_si32(_mm_set_ss(previous_top)));
            }
        }
    }
    return result;
}

void NavigationNodeBufferDeleter::operator()(u8* nodes) const
{
    engine_heap_free(nodes);
}

NavigationSceneStorage::~NavigationSceneStorage()
{
    for (size_t floor = 0; floor < grids.size(); ++floor)
    {
        if (grids[floor] != nullptr && g_nav_grids[floor] == grids[floor].get())
        {
            if (g_nav_floor_connections[floor] != nullptr)
            {
                engine_heap_free(g_nav_floor_connections[floor]);
                g_nav_floor_connections[floor] = nullptr;
            }
            g_nav_grids[floor] = nullptr;
            if (floor == 0)
            {
                g_nav_count = 0;
            }
        }
    }
    if (published_nodes != nullptr && *published_nodes == nodes.get())
    {
        *published_nodes = nullptr;
    }
}

// Xbox 0x00111820
void build_scene_navigation(const Actor* background, std::span<const float> surface_costs,
    NavigationSceneStorage& storage, u8*& nodes)
{
    const auto find_block = [&](u32 kind) -> u32
    {
        if (background == nullptr || background->model == nullptr ||
            background->model->cpu.data() == nullptr)
        {
            return 0;
        }
        const auto cpu = background->model->cpu;
        const u32 count = read_u32(cpu, 4);
        const u32 table = read_u32(cpu, 0);
        for (u32 index = 0; index < count; ++index)
        {
            if (read_u32(cpu, table + index * 8) == kind)
            {
                return read_u32(cpu, table + index * 8 + 4);
            }
        }
        return 0;
    };
    const u32 links[2] = {find_block(0x19), find_block(0x1A)};
    const u32 models[2] = {find_block(0xB), find_block(0x17)};
    g_nav_min_y[0] = 10000.f; // DAT_0032B4D0.
    g_nav_min_y[1] = 10000.f;
    g_nav_max_y[0] = -10000.f; // DAT_0032B4E0.
    g_nav_max_y[1] = -10000.f;
    g_nav_count = 0;
    std::array<NavigationBlockedCellsOwner, 2> blocked;
    if (models[0] != 0)
    {
        const auto cpu = background->model->cpu;
        const auto decode = [&](i32 floor)
        {
            const u32 offset = models[floor];
            const NavigationModelGrid source{std::bit_cast<i32>(read_u32(cpu, offset)),
                std::bit_cast<i32>(read_u32(cpu, offset + 4)), std::bit_cast<i32>(read_u32(cpu, offset + 8)),
                cpu.subspan(read_u32(cpu, offset + 0xC))};
            blocked[floor] = decode_navigation_model_grid(floor, source, storage.bounds, storage.grids[floor]);
            ++g_nav_count;
        };
        decode(0);
        if (models[1] != 0)
        {
            decode(1);
        }
    }
    for (i32 floor = 0; floor < g_nav_count; ++floor)
    {
        NavGrid& grid = *g_nav_grids[floor];
        const i32 count = std::bit_cast<i32>(u32(grid.width) * u32(grid.depth));
        const i32 cell = grid.cell;
        accumulate_navigation_height_bounds(floor, count);
        initialize_navigation_edge_costs(floor, count, surface_costs);
        mark_navigation_isolated_cells(floor, count);
        initialize_navigation_height_connections(floor, count, float(cell) * 0.5f, surface_costs);
        if (links[floor] != 0)
        {
            const auto cpu = background->model->cpu;
            const u16 count = read_u16(cpu, links[floor] + 2);
            const u32 pairs = read_u32(cpu, links[floor] + 4);
            remove_navigation_connections(floor, {reinterpret_cast<const NavigationConnectionPair*>(
                cpu.data() + pairs), count});
        }
        grid.cost_scale = 5.f; // DAT_0032B184.
    }
    for (i32 floor = 0; floor < g_nav_count - 1; ++floor)
    {
        const NavGrid& grid = *g_nav_grids[floor];
        collect_navigation_floor_connections(floor,
            std::bit_cast<i32>(u32(grid.width) * u32(grid.depth)));
    }
    for (i32 floor = 0; floor < g_nav_count; ++floor)
    {
        blocked[floor].reset();
    }
    if (g_nav_count != 0)
    {
        const u32 count = u32(g_nav_grids[0]->width) * u32(g_nav_grids[0]->depth);
        storage.nodes.reset(static_cast<u8*>(engine_heap_allocate(std::bit_cast<i32>(count * 20u + 4u))));
        nodes = storage.nodes.get();
        storage.published_nodes = &nodes;
        std::memset(nodes, 0, count * 20u);
        for (u32 index = 0; index < count; ++index)
        {
            const u16 cell = u16(index);
            std::memcpy(nodes + index * 20 + 10, &cell, 2);
        }
    }
}
}
