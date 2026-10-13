#include "screen_model_draw.h"
#include "../camera/display_view.h"
#include "gbtg/anim.h"

namespace gbtg
{
// Xbox 0x000F9250
void draw_runtime_screen_model(RuntimeModelInstance& model, u32 model_cpu, const float* position,
    const float* rotation, float scale, u32 kind, RuntimeScreenModelDraw& context)
{
    auto& draw = context.display.draw;
    initialize_dialogue_orthographic(draw);
    const std::array<float, 12> saved = entity_detail::g_display_view->transform;
    // DAT_0032B0F8 is 1.0 and DAT_0032B304 is -50.0.
    const std::array<float, 12> identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    set_display_transform(identity.data(), context.display);
    float* affine = draw.affine.data();
    draw.current_affine = affine;
    for (u32 index = 0; index < 12; ++index)
    {
        affine[index] = identity[index];
    }
    float* translation = affine + 12;
    if (position == nullptr)
    {
        translation[3] = 0.f;
        translation[7] = 0.f;
    }
    else
    {
        translation[3] = position[0];
        translation[7] = position[1];
    }
    translation[10] = 1.f;
    translation[5] = 1.f;
    translation[0] = 1.f;
    translation[11] = -50.f;
    translation[9] = 0.f;
    translation[8] = 0.f;
    translation[6] = 0.f;
    translation[4] = 0.f;
    translation[2] = 0.f;
    translation[1] = 0.f;
    multiply_affine_matrices(affine, affine, translation);
    if (rotation != nullptr)
    {
        rotate_dialogue_affine('y', rotation[1], draw);
        rotate_dialogue_affine('x', rotation[0], draw);
        rotate_dialogue_affine('z', rotation[2], draw);
    }
    float* current = draw.current_affine;
    float* scaling = current + 12;
    scaling[0] = scale;
    scaling[1] = 0.f;
    scaling[2] = 0.f;
    scaling[3] = 0.f;
    scaling[4] = 0.f;
    scaling[5] = 0.f - scale;
    scaling[6] = 0.f;
    scaling[7] = 0.f;
    scaling[8] = 0.f;
    scaling[9] = 0.f;
    scaling[10] = scale;
    scaling[11] = 0.f;
    multiply_affine_matrices(draw.current_affine, draw.current_affine, draw.current_affine + 12);
    ++draw.override_count;
    draw.fog_disabled = 1;
    set_runtime_render_fog_enabled(draw.renderer, 0);
    // 0x000F9080 receives DAT_0050DD70 itself in EAX.
    draw_runtime_scene_model(model, model_cpu, *reinterpret_cast<const AnimationMatrix*>(draw.current_affine), kind,
        context.models);
    --draw.override_count;
    if (draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
    bind_display_projection(context.display);
    set_display_transform(saved.data(), context.display);
}
}
