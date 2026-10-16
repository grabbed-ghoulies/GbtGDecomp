#pragma once
#if defined(GBTG_INSPECTOR_APP)

#include "gbtg/entity.h"
#include "gbtg/texture.h"
#include "../camera/display_view.h"
#include "../graphics/scene_d3d_device.h"

namespace gbtg
{
// Host inspection inputs and copied diagnostics. None of these are retail state.
struct InspectionView
{
    Vec3 position{};
    Quat rotation{0.f, 0.f, 0.f, 1.f};
    float fov = 60.f; // Vertical degrees, using the native display's aspect convention.
    float near_clip = 0.1f;
    float far_clip = 5000.f;
};

struct InspectionDraw
{
    std::string kind;
    u32 owner = 0;
    std::string model_resource;
    u32 primitive = 0;
    u32 count = 0;
    std::array<u32, 4> textures{};
    std::array<const RgbaImage*, 4> texture_images{}; // Borrowed from the active runtime session.
    std::vector<std::pair<u32, SceneD3DVertexStream>> streams;
    u32 index_address = 0;
    std::vector<std::pair<u32, u32>> states;
};

struct InspectionFrame
{
    entity_detail::DisplayView display;
    std::array<float, 16> projection{};
    std::vector<InspectionDraw> draws;
};
}
#endif
