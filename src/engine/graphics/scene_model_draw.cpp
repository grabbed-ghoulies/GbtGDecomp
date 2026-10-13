#include "scene_model_draw.h"
#include "scene_render_helpers.h"

namespace gbtg
{
// Xbox 0x000F9080
void draw_runtime_scene_model(RuntimeModelInstance& model, u32 model_cpu,
    const AnimationMatrix& world, u32 kind, SceneModelDrawContext& c)
{
    const Vec3 position{world[3], world[7], world[11]};
    RuntimeModelRenderContext& renderer = c.lights.environment.upload.renderer;
    if ((~(c.image.word(model_cpu, 8) >> 1) & 1) != 0)
    {
        build_runtime_scene_lights(position, c.lights);
        apply_runtime_model_materials(c.image, model_cpu, renderer);
        draw_runtime_model_instance(model, c.image, model_cpu, world, kind, renderer, c.matrices);
        return;
    }
    set_runtime_scene_color(1.f, 1.f, 1.f, 1.f, c.lights.retained_color, renderer);
    clear_runtime_lights(renderer);
    configure_runtime_scene_environment(position, 0, c.lights.environment);
    apply_runtime_model_materials(c.image, model_cpu, renderer);
    draw_runtime_model_instance(model, c.image, model_cpu, world, kind, renderer, c.matrices);
}
}
