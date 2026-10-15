#include "runtime_display_setup.h"
#include "display_setup_math.h"
#include "actor_camera_effects.h"
#include "../entities/actor_sound_pose.h"
#include "gbtg/anim.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
template<class T>
T effect_value(const ActorCameraEffect& effect, size_t offset)
{
    T value;
    std::memcpy(&value, effect.payload + offset - 8, sizeof(value));
    return value;
}
constexpr std::array<float, 4> aspects
{
    std::bit_cast<float>(0x3FAAAAABu),
    std::bit_cast<float>(0x3FE38E39u),
    std::bit_cast<float>(0x40166666u), 0.f
};
}
// Xbox 0x00049560
void apply_display_y_offset(const Vec3& angles, float amount, Vec3& position)
{
    Vec3 offset;
    rotate_euler_vector(angles, Vec3{0.f, amount, 0.f}, offset);
    position.x += offset.x;
    position.y += offset.y;
    position.z += offset.z;
}
// Xbox 0x000495B0
void apply_display_z_offset(const Vec3& angles, float amount, Vec3& position)
{
    Vec3 offset;
    rotate_euler_vector(angles, Vec3{0.f, 0.f, amount}, offset);
    position.x += offset.x;
    position.y += offset.y;
    position.z += offset.z;
}
// Xbox 0x00049600
void apply_display_x_offset(const Vec3& angles, float amount, Vec3& position)
{
    Vec3 offset;
    rotate_euler_vector(angles, Vec3{amount, 0.f, 0.f}, offset);
    position.x += offset.x;
    position.y += offset.y;
    position.z += offset.z;
}
// Xbox 0x00049650
void apply_display_offset(u32 kind, const Vec3& angles, float amount, Vec3& position)
{
    switch (kind)
    {
    case 0:
        apply_display_y_offset(angles, amount, position);
        return;
    case 1:
        apply_display_x_offset(angles, amount, position);
        return;
    case 2:
        apply_display_z_offset(angles, amount, position);
        return;
    case 3:
        position.y += amount;
    }
}
// Xbox 0x00049260
void apply_display_camera_effects(Vec3& position, const Vec3& angles,
    RuntimeDisplaySetupContext& context)
{
    if (context.effects_disabled == 0)
    {
        const Vec3 initial = position;
        auto* effect = g_actor_camera_effects.next;
        while (effect != nullptr)
        {
            float attenuation = 1.f;
            if (effect_value<u8>(*effect, 0x82) != 0)
            {
                const float x = initial.x - effect_value<float>(*effect, 8);
                const float y = initial.y - effect_value<float>(*effect, 12);
                const float z = initial.z - effect_value<float>(*effect, 16);
                const float inner = effect_value<float>(*effect, 0x14);
                const float outer = effect_value<float>(*effect, 0x18);
                if (outer == inner)
                {
                    attenuation = 0.f;
                }
                else
                {
                    const long double squared = (static_cast<long double>(z) * z
                        + static_cast<long double>(y) * y) + static_cast<long double>(x) * x;
                    const long double value = 1.L - (std::sqrt(squared) - inner)
                        / (static_cast<long double>(outer) - inner);
                    if (value < 0.L)
                    {
                        attenuation = 0.f;
                    }
                    else
                    {
                        const float rounded = static_cast<float>(value);
                        if (!(rounded > 1.f))
                        {
                            attenuation = rounded;
                        }
                    }
                }
                const float minimum = effect_value<float>(*effect, 0x1C);
                if (!(attenuation >= minimum))
                {
                    attenuation = minimum;
                }
                if (attenuation == 0.f)
                {
                    effect = effect->next;
                    continue;
                }
            }
            const u8 state = effect_value<u8>(*effect, 0x80);
            float amount;
            if (state == 1)
            {
                amount = effect_value<float>(*effect, 0x30);
            }
            else
            {
                if (state < 2 || state > 3)
                {
                    effect = effect->next;
                    continue;
                }
                amount = effect_value<float>(*effect, 0x34) * effect_value<float>(*effect, 0x30);
            }
            apply_display_offset(effect_value<u8>(*effect, 0x81), angles, amount * attenuation, position);
            effect = effect->next;
        }
    }
}
// Xbox 0x000489E0
bool select_display_camera(EntityCamNode* camera, i32 slot, RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    if (camera != nullptr)
    {
        g_display_view = &g_display_records[slot];
        context.current_display_address = 0x507018u + static_cast<u32>(slot) * 0x1E4u;
        Vec3 position = camera->position;
        Quat rotation;
        if ((camera->flags & 1) == 0)
        {
            normalize_display_quaternion(camera->rotation, camera->rotation);
            Vec3 angles;
            extract_display_quaternion_euler(camera->rotation, 0, angles.x, angles.y, angles.z);
            apply_display_camera_effects(position, angles, context);
            rotation.x = camera->rotation.x;
            rotation.y = camera->rotation.y;
            rotation.z = camera->rotation.z;
            rotation.w = camera->rotation.w;
        }
        else
        {
            const Vec3 angles{camera->rotation.x, camera->rotation.y, camera->rotation.z};
            apply_display_camera_effects(position, angles, context);
            camera_euler_to_quaternion(angles, rotation);
        }
        g_display_view->position.x = position.x;
        g_display_view->position.y = position.y;
        g_display_view->position.z = position.z;
        set_display_rotation(rotation);
        if ((camera->flags & 8) == 0)
        {
            g_display_view->aspect_mode = 3;
        }
        else
        {
            g_display_view->aspect_mode = camera->aspect_slot;
        }
        set_display_focal_length(camera->aspect_slot, camera->focal);
        camera->clip_limit = std::bit_cast<float>(0x4E6E6B28u);
        camera->blend = 0.f;
        i32 width, x, height, y;
        float field_of_view;
        camera_fit_display_view(camera->focal, camera->aspect_slot, width, x, height, y,
            field_of_view, camera->pixel_aspect, camera->focal_pixels);
        return true;
    }
    return false;
}
// Xbox 0x000F6430
void bind_display_device_view(const float* world, RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    expand_display_affine(g_display_view->transform.data(), context.device_view.data());
    context.draw.d3d.set_transform(0, context.device_view.data());
    if (world != nullptr)
    {
        expand_display_affine(world, context.device_world.data());
        context.draw.d3d.set_transform(6, context.device_world.data());
        return;
    }
    expand_display_affine(context.default_world.data(), context.device_world.data());
    context.draw.d3d.set_transform(6, context.device_world.data());
}
// Xbox 0x000F5E50
void rebuild_display_view(RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    auto& view = *g_display_view;
    camera_fit_display_view(view.focal, view.aspect_mode, view.fitted_width,
        view.fitted_x, view.fitted_height, view.fitted_y,
        view.field_of_view, view.pixel_aspect, view.focal_pixels);
    Quat rotation;
    conjugate_display_quaternion(view.rotation, rotation);
    make_display_quaternion_matrix(rotation, view.rotation_transform.data());
    const std::array<float, 12> translation
    {
        1.f, 0.f, 0.f, 0.f - view.position.x,
        0.f, 1.f, 0.f, 0.f - view.position.y,
        0.f, 0.f, 1.f, 0.f - view.position.z
    };
    multiply_affine_matrices(view.transform.data(), view.rotation_transform.data(), translation.data());
    expand_display_affine(view.transform.data(), view.projection.data());
    set_dialogue_render_view(view.projection.data(), context.draw);
    invert_model_matrix(view.inverse_transform.data(), view.transform.data());
    bind_display_device_view(nullptr, context);
}
// Xbox 0x000F6630
void rebuild_display_frustum()
{
    using namespace entity_detail;
    auto& view = *g_display_view;
    const float half_angle = static_cast<float>(static_cast<long double>(view.field_of_view)
        * std::bit_cast<float>(0x3C0EFA35u));
    const float sine = static_cast<float>(retail_sin(half_angle));
    const float cosine = static_cast<float>(retail_cos(half_angle));
    float aspect;
    if (view.aspect_mode == 3)
    {
        aspect = aspects[g_aspect_preset];
    }
    else
    {
        aspect = aspects[view.aspect_mode];
    }
    if (g_aspect_exact == 0)
    {
        aspect *= std::bit_cast<float>(0x3F8CCCCDu);
    }
    const float height = (sine / cosine) * view.far_clip;
    const float width = height * aspect;
    const float negative_far = 0.f - view.far_clip;
    const std::array<float, 18> origins
    {
        0.f - width, height, negative_far,
        width, height, negative_far,
        0.f - width, 0.f - height, negative_far,
        width, 0.f - height, negative_far,
        0.f, 0.f, 0.f,
        0.f, 0.f, 0.f - view.near_clip
    };
    std::array<std::array<float, 4>, 6> normals
    {{
        {negative_far, 0.f, width, 0.f},
        {view.far_clip, 0.f, width, 0.f},
        {0.f, view.far_clip, height, 0.f},
        {0.f, negative_far, height, 0.f},
        {0.f, 0.f, negative_far, 0.f},
        {0.f, 0.f, view.near_clip, 0.f}
    }};
    i32 remaining = 6;
    auto* normal = normals.data();
    do
    {
        const float squared = ((*normal)[2] * (*normal)[2]
            + (*normal)[0] * (*normal)[0]) + (*normal)[1] * (*normal)[1];
        const long double factor = retail_reciprocal_sqrt(squared);
        (*normal)[0] = static_cast<float>(factor * (*normal)[0]);
        (*normal)[1] = static_cast<float>(factor * (*normal)[1]);
        (*normal)[2] = static_cast<float>(factor * (*normal)[2]);
        ++normal;
        --remaining;
    }
    while (remaining != 0);
    const auto& inverse = view.inverse_transform;
    auto transform_normal = [&inverse](const std::array<float, 4>& value, float* output)
    {
        output[0] = (inverse[2] * value[2] + inverse[1] * value[1]) + inverse[0] * value[0];
        output[1] = (inverse[6] * value[2] + inverse[5] * value[1]) + inverse[4] * value[0];
        output[2] = (inverse[10] * value[2] + inverse[9] * value[1]) + inverse[8] * value[0];
    };
    auto* planes = view.frustum.data() + 18;
    transform_normal(normals[0], planes);
    transform_normal(normals[1], planes + 4);
    transform_normal(normals[2], planes + 8);
    transform_normal(normals[3], planes + 12);
    transform_normal(normals[4], planes + 16);
    transform_normal(normals[5], planes + 20);
    transform_display_points(inverse.data(), origins.data(), view.frustum.data(), 6);
    const auto* points = view.frustum.data();
    planes[3] = (planes[2] * points[14] + planes[1] * points[13]) + planes[0] * points[12];
    planes[7] = (planes[6] * points[14] + planes[5] * points[13]) + planes[4] * points[12];
    planes[11] = (planes[10] * points[14] + planes[9] * points[13]) + planes[8] * points[12];
    planes[15] = (planes[14] * points[14] + planes[13] * points[13]) + planes[12] * points[12];
    planes[19] = (points[2] * planes[18] + points[1] * planes[17]) + planes[16] * points[0];
    planes[23] = (points[17] * planes[22] + points[16] * planes[21]) + planes[20] * points[15];
}
// Xbox 0x00156350
void set_display_render_frustum(const float* frustum, RuntimeModelRenderContext& renderer)
{
    runtime_render_state(renderer).frustum = frustum;
}
// Xbox 0x000F5FD0
void rebuild_display_projection(RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    auto& view = *g_display_view;
    if (view.far_clip > 5000.f)
    {
        view.far_clip = 5000.f;
    }
    float aspect;
    if (view.aspect_mode == 3)
    {
        aspect = aspects[g_aspect_preset];
    }
    else
    {
        aspect = aspects[view.aspect_mode];
    }
    if (g_aspect_exact == 0)
    {
        aspect *= std::bit_cast<float>(0x3F8CCCCDu);
    }
    const float half_angle = static_cast<float>((static_cast<long double>(view.field_of_view) * .5L)
        * std::bit_cast<float>(0x3C8EFA35u));
    const long double reciprocal_tangent = 1.L / display_tangent(half_angle);
    const float reciprocal_depth = 1.f / (view.far_clip - view.near_clip);
    auto& m = context.device_projection;
    m[1] = m[2] = m[3] = m[4] = m[6] = m[7] = m[8] = m[9] = 0.f;
    m[10] = 0.f - (view.near_clip + view.far_clip) * reciprocal_depth;
    m[11] = 0.f - ((view.near_clip * view.far_clip) * 2.f) * reciprocal_depth;
    m[12] = m[13] = 0.f;
    m[14] = -1.f;
    m[15] = 0.f;
    m[0] = static_cast<float>(reciprocal_tangent / aspect);
    m[5] = static_cast<float>(reciprocal_tangent);
    rebuild_display_frustum();
    set_display_render_frustum(view.frustum.data(), context.draw.renderer);
}
// Xbox 0x000F6170
void bind_display_projection(RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    context.draw.d3d.transpose(context.projection_transpose.data(), context.device_projection.data());
    context.draw.d3d.set_transform(1, context.projection_transpose.data());
    const std::array<u32, 6> viewport
    {
        static_cast<u32>(g_display_view->fitted_x),
        static_cast<u32>(g_display_view->fitted_y),
        static_cast<u32>(g_display_view->fitted_width),
        static_cast<u32>(g_display_view->fitted_height), 0u, 0x3F800000u
    };
    context.draw.d3d.viewport(viewport);
    set_dialogue_render_projection(context.projection_transpose.data(), context.draw);
    rebuild_display_view(context);
}
// Xbox 0x00048BC0
bool setup_explicit_camera_display(EntityCamNode* camera, i32 slot,
    RuntimeDisplaySetupContext& context)
{
    if (select_display_camera(camera, slot, context))
    {
        rebuild_display_view(context);
        extend_display_clip_limits(entity_detail::g_display_view->transform, context.scene_bounds,
            camera->blend, camera->clip_limit);
        entity_detail::g_display_view->near_clip = camera->clip_limit;
        entity_detail::g_display_view->far_clip = camera->blend;
        rebuild_display_projection(context);
        bind_display_projection(context);
        return true;
    }
    return false;
}
// Xbox 0x00048B60
void setup_active_camera_display(i32 slot, RuntimeDisplaySetupContext& context)
{
    auto* camera = entity_detail::g_cam_active[slot];
    if (select_display_camera(camera, slot, context))
    {
        rebuild_display_view(context);
        extend_display_clip_limits(entity_detail::g_display_view->transform, context.scene_bounds,
            camera->blend, camera->clip_limit);
        entity_detail::g_display_view->near_clip = camera->clip_limit;
        entity_detail::g_display_view->far_clip = camera->blend;
        rebuild_display_projection(context);
        bind_display_projection(context);
    }
}

// Xbox 0x000F6EB0
void set_display_transform(const float* matrix, RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    auto& view = *g_display_view;
    for (u32 index = 0; index < 12; ++index)
    {
        view.transform[index] = matrix[index];
    }
    auto& output = view.projection;
    output[0] = view.transform[0];
    output[1] = view.transform[4];
    output[2] = view.transform[8];
    output[4] = view.transform[1];
    output[5] = view.transform[5];
    output[6] = view.transform[9];
    output[8] = view.transform[2];
    output[9] = view.transform[6];
    output[10] = view.transform[10];
    output[12] = view.transform[3];
    output[13] = view.transform[7];
    output[11] = 0.f;
    output[7] = 0.f;
    output[3] = 0.f;
    output[14] = view.transform[11];
    output[15] = 1.f; // DAT_0032B0F8.
    set_dialogue_render_view(output.data(), context.draw);
}
// Xbox 0x000F5CC0
void set_display_viewport(i32 x, i32 y, i32 width, i32 height)
{
    using namespace entity_detail;
    auto& view = *g_display_view;
    view.origin_x = x;
    view.origin_y = y;
    view.width = width;
    view.height = height;
    camera_fit_display_view(view.focal, view.aspect_mode, view.fitted_width, view.fitted_x, view.fitted_height,
        view.fitted_y, view.field_of_view, view.pixel_aspect, view.focal_pixels);
}
// Xbox 0x000F6E60
void set_display_inverse_transform(const float* matrix, RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    auto& view = *g_display_view;
    for (u32 index = 0; index < 12; ++index)
    {
        view.inverse_transform[index] = matrix[index];
    }
    rebuild_display_frustum();
    set_display_render_frustum(view.frustum.data(), context.draw.renderer);
}
// Xbox 0x000F61F0
void bind_display_projection_only(std::array<float, 16>& transposed, RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    context.draw.d3d.transpose(transposed.data(), context.device_projection.data());
    context.draw.d3d.set_transform(1, transposed.data());
    const std::array<u32, 6> viewport
    {
        static_cast<u32>(g_display_view->fitted_x),
        static_cast<u32>(g_display_view->fitted_y),
        static_cast<u32>(g_display_view->fitted_width),
        static_cast<u32>(g_display_view->fitted_height), 0u, 0x3F800000u
    };
    context.draw.d3d.viewport(viewport);
    set_dialogue_render_projection(transposed.data(), context.draw);
}
// Xbox 0x00033A20
void present_runtime_display_views(const std::function<void(i32)>& draw_view,
    RuntimeDisplaySetupContext& context)
{
    using namespace entity_detail;
    i32 slot = 0;
    do
    {
        EntityCamNode* camera = g_cam_active[slot];
        if (select_display_camera(camera, slot, context))
        {
            rebuild_display_view(context);
            extend_display_clip_limits(g_display_view->transform, context.scene_bounds, camera->blend,
                camera->clip_limit);
            g_display_view->near_clip = camera->clip_limit;
            g_display_view->far_clip = camera->blend;
            rebuild_display_projection(context);
            bind_display_projection(context);
            draw_view(slot); // 0x0011C240
        }
        ++slot;
    } while (slot < 4);
}
}
