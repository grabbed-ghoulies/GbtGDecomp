#pragma once
#include "scene_model_draw.h"
#include "../camera/runtime_display_setup.h"

namespace gbtg
{
struct RuntimeScreenModelDraw
{
    RuntimeDisplaySetupContext& display;
    SceneModelDrawContext& models;
};
// A model drawn in the 640x480 orthographic space at a screen position. The position arrives in EAX; the
// model in ECX reaches 0x000F9080 with its .cpu address.
void draw_runtime_screen_model(RuntimeModelInstance& model, u32 model_cpu, const float* position,
    const float* rotation, float scale, u32 kind, RuntimeScreenModelDraw& context); // Xbox 0x000F9250
}
