#pragma once
#include "scene_environment_setup.h"

namespace gbtg
{
struct SceneModelDrawContext
{
    const RuntimeRelocationImage& image;
    SceneLightConstructionContext& lights;
    const RuntimeShaderMatrixBoundaries& matrices;
};
void draw_runtime_scene_model(RuntimeModelInstance& model, u32 model_cpu,
    const AnimationMatrix& world, u32 kind, SceneModelDrawContext& context); // Xbox 0x000F9080
}
