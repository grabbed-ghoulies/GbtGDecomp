// Host side of navigation_oracle.py. The heap boundary is plain malloc/realloc here.
#include "navigation/cell_registration.h"
#include <cstring>
#include "entities/entity_heap.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace gbtg;
using namespace gbtg::entity_detail;
namespace gbtg
{
void* engine_heap_allocate(i32 size) { return std::malloc(size_t(size)); }
void* engine_heap_reallocate(void* block, i32 size) { return std::realloc(block, size_t(size)); }
void engine_heap_free(void* block) { std::free(block); }
// navigation.cpp's path query references this; the registration functions never reach it.
const OccupancyRecord* find_occupancy_record(const Vec3&, float, u32, const OccupancyRecord*, i16) { std::abort(); }
namespace entity_detail
{
NavGrid* g_nav_grids[kNavSlots]{};
i32 g_nav_count = 0;
float g_nav_min_y[kNavBands]{};
float g_nav_max_y[kNavBands]{};
i32 g_nav_origin_x = 0;
i32 g_nav_origin_z = 0;
}
}
int main()
{
    int cases;
    std::cin >> cases;
    for (int c = 0; c < cases; ++c)
    {
        std::cin >> g_nav_origin_x >> g_nav_origin_z;
        NavGrid grids[4];
        for (int g = 0; g < 4; ++g)
        {
            int present;
            std::cin >> present;
            g_nav_grids[g] = nullptr;
            if (present)
            {
                auto& grid = grids[g];
                std::cin >> grid.width >> grid.depth >> grid.cell;
                grid.cells.resize(size_t(grid.width * grid.depth));
                for (auto& cell : grid.cells)
                {
                    int x, z, word;
                    std::cin >> x >> z >> word;
                    cell.x = i16(x);
                    cell.z = i16(z);
                    cell.value_6 = u8(word);
                    cell.flags_7 = u8(word >> 8);
                }
                g_nav_grids[g] = &grid;
            }
        }
        int floor, calls;
        std::cin >> floor >> calls;
        NavigationCellRegistration* records = nullptr;
        for (int k = 0; k < calls; ++k)
        {
            u32 bits[4];
            int outer;
            std::cin >> bits[0] >> bits[1] >> bits[2] >> bits[3] >> outer;
            Vec3 center;
            float radius;
            std::memcpy(&center.x, &bits[0], 4);
            std::memcpy(&center.y, &bits[1], 4);
            std::memcpy(&center.z, &bits[2], 4);
            std::memcpy(&radius, &bits[3], 4);
            register_navigation_cells(records, center, floor, radius, outer);
            if (records == nullptr)
            {
                std::printf("null");
            }
            else
            {
                std::printf("%u %u %u |", u32((records->end - records->begin) / 12), u32((records->capacity - records->begin) / 12), records->growth);
                for (u8* row = records->begin; row < records->end; row += 12)
                {
                    u32 index, selector;
                    std::memcpy(&index, row, 4);
                    std::memcpy(&selector, row + 4, 4);
                    std::printf(" %u:%u", index, selector);
                }
            }
            std::printf(" |");
            if (g_nav_grids[floor] != nullptr)
            {
                for (auto& cell : g_nav_grids[floor]->cells)
                {
                    std::printf(" %u", u32(cell.value_6) | (u32(cell.flags_7) << 8));
                }
            }
            std::printf("\n");
        }
        std::free(records);
    }
}
