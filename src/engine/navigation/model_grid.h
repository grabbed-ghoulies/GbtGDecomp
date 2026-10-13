#pragma once

#include "navigation.h"
#include "gbtg/entity.h"
#include <memory>
#include <span>

namespace gbtg::entity_detail
{
struct NavigationModelGrid
{
    i32 width;
    i32 depth;
    i32 cell;
    std::span<const u8> cells; // Model +0xC, stride 0x10.
};
struct NavigationGridDeleter
{
    void operator()(NavGrid* grid) const;
};
using NavigationGridOwner = std::unique_ptr<NavGrid, NavigationGridDeleter>;
struct NavigationBlockedCells
{
    u32 count;
    u32 bytes;
    u8* bits;
};
struct NavigationBlockedCellsDeleter
{
    void operator()(NavigationBlockedCells* cells) const;
};
using NavigationBlockedCellsOwner = std::unique_ptr<NavigationBlockedCells, NavigationBlockedCellsDeleter>;
struct NavigationModelBounds
{
    Vec3 low;
    Vec3 high;
    i32 min_x;
    i32 min_z;
    i32 max_x;
    i32 max_z;
};
void navigation_model_bounds(const NavigationModelGrid& source,
    Vec3& low, Vec3& high); // Xbox 0x0004F1B0
i32 align_navigation_model_bounds(const NavigationModelGrid& source,
    NavigationModelBounds& bounds); // Xbox 0x0004F460
NavigationGridOwner allocate_navigation_grid(i32 width, i32 depth,
    i32 cell); // Xbox 0x000362F0
NavigationBlockedCellsOwner decode_navigation_model_grid(i32 floor,
    const NavigationModelGrid& source, NavigationModelBounds& bounds,
    NavigationGridOwner& owner); // Xbox 0x00111B80
struct NavigationNodeBufferDeleter
{
    void operator()(u8* nodes) const;
};
struct NavigationSceneStorage
{
    ~NavigationSceneStorage();
    std::array<NavigationGridOwner, 2> grids;
    std::unique_ptr<u8, NavigationNodeBufferDeleter> nodes;
    NavigationModelBounds bounds;
    u8** published_nodes = nullptr;
};
void build_scene_navigation(const Actor* background, std::span<const float> surface_costs,
    NavigationSceneStorage& storage, u8*& nodes); // Xbox 0x00111820
}
