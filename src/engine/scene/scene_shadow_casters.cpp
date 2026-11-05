#include "scene_shadow_casters.h"
#include "../camera/display_clip_limits.h"
#include "../camera/view_frustum.h"
#include "../entities/actor_shape_push.h"
#include "../graphics/light_volume_records.h"
#include "../graphics/vector_normalize.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float next_random_unit(u32& random)
{
    random = random * 0x19660D + 0x3C6EF35F;
    return std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u);
}
}

// Xbox 0x00106B90
void runtime_direction_or_axis(const Vec3& vector, Vec3& output, u8 axis)
{
    const float one = 1.f; // DAT_0032B0F8.
    if (vector.x == 0.f && vector.y == 0.f && vector.z == 0.f)
    {
        switch (axis)
        {
        case 0x58:
        case 0x78:
            output = {one, 0.f, 0.f};
            return;
        case 0x59:
        case 0x79:
            output = {0.f, one, 0.f};
            return;
        case 0x5A:
        case 0x7A:
            output = {0.f, 0.f, one};
            return;
        default:
            output = {0.f, 0.f, 0.f};
            return;
        }
    }
    normalize_runtime_graphics_vector(output, vector); // D3DXVec3Normalize.
}

// Xbox 0x00120FE0
void place_runtime_model_shader_lights(RuntimeModelInstance& model, const Actor& actor)
{
    u8* storage = model.shader_storage;
    if (storage == nullptr)
    {
        return;
    }
    i32 remaining;
    std::memcpy(&remaining, storage, 4);
    if (remaining <= 0)
    {
        return;
    }
    u8* record = storage + 4;
    const float* m = actor.xform; // Actor +0x2AC.
    do
    {
        LightRecord& light = light_record(record);
        if ((light.flags & 8) != 0)
        {
            const Vec3 local = light.local_point;
            const i16 joint = i16(std::int8_t(light.joint));
            Vec3 world;
            if (joint == -1)
            {
                world.x = (m[1] * local.y + m[2] * local.z) + m[0] * local.x + m[3];
                world.y = (m[5] * local.y + m[6] * local.z) + m[4] * local.x + m[7];
                world.z = (m[9] * local.y + m[10] * local.z) + m[8] * local.x + m[11];
            }
            else
            {
                actor_transform_joint_point(actor, u16(joint), local, world);
            }
            light.position = world;
            if (light.kind == 2)
            {
                const Vec3 toward{0.f - world.x, 0.f - world.y, 0.f - world.z};
                Vec3 direction;
                runtime_direction_or_axis(toward, direction, 0x79);
                light.direction = direction;
            }
        }
        record += 0x98;
        --remaining;
    }
    while (remaining != 0);
}

// Xbox 0x00121160
float runtime_actor_view_distance(const Actor& actor, const entity_detail::DisplayView& view)
{
    const Vec3 camera = view.position;
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI actor.
    const float x = camera.x - position.x;
    const float y = camera.y - position.y;
    const float z = camera.z - position.z;
    return (z * z + y * y) + x * x;
}

// Xbox 0x00040B20
ModelPoseBuffer* runtime_actor_pose(const RuntimeModelInstance* model)
{
    if (model == nullptr)
    {
        return nullptr;
    }
    if (model->alternate == nullptr)
    {
        return model->primary;
    }
    return model->alternate;
}

// Xbox 0x000FE4D0
bool runtime_background_light_in_view(const Vec3& position, std::span<const u8> background,
    entity_detail::DisplayView& view)
{
    if (background.data() != nullptr)
    {
        const u32 light = read_u32(background, read_u32(background, 0x10));
        if (light != 0)
        {
            const float radius = read_f32(background, light + 0x38);
            const Vec3 first{position.x - radius, position.y - radius, position.z - radius};
            const Vec3 second{radius + position.x, radius + position.y, radius + position.z};
            return classify_runtime_box_frustum(view.frustum, first, second, view.mark) != 0;
        }
    }
    return false;
}

// Xbox 0x00052270
std::span<const u8> nearest_runtime_light_probe(const Vec3& position, std::span<const u8> table)
{
    const i32 count = i32(read_u32(table, 0));
    std::span<const u8> nearest;
    float best = 0.f;
    for (i32 index = 0; index < count; ++index)
    {
        const auto probe = table.subspan(4 + size_t(index) * 0x28, 0x28);
        const float x = position.x - read_f32(probe, 0);
        const float y = position.y - read_f32(probe, 4);
        const float z = position.z - read_f32(probe, 8);
        const float distance = (x * x + z * z) + y * y;
        const float radius = read_f32(probe, 0x24);
        if ((nearest.data() == nullptr || distance < best) && distance < radius * radius)
        {
            nearest = probe;
            best = distance;
        }
    }
    return nearest;
}

// Xbox 0x001238F0
i32 nearest_runtime_ambient_point(const Vec3& position, float& distance, const RuntimeSceneShadowCasters& context)
{
    if (context.points_enabled == 0)
    {
        return -1;
    }
    i32 nearest = 0;
    while (context.points[size_t(nearest)].active == 0)
    {
        ++nearest;
        if (nearest >= 32)
        {
            // Retail then measures against the dword before DAT_00511B70 and returns -1 with that distance,
            // which its only caller discards.
            return -1;
        }
    }
    const Vec3& first = context.points[size_t(nearest)].position;
    float best = ((position.y - first.y) * (position.y - first.y) + (position.z - first.z) * (position.z - first.z)) +
        (position.x - first.x) * (position.x - first.x);
    i32 index = nearest + 1;
    while (index < 32)
    {
        if (context.points[size_t(index)].active == 0)
        {
            ++index;
            continue;
        }
        const Vec3& point = context.points[size_t(index)].position;
        const float x = position.x - point.x;
        const float y = position.y - point.y;
        const float z = position.z - point.z;
        const float candidate = (z * z + y * y) + x * x;
        if (best > candidate)
        {
            best = candidate;
            nearest = index;
        }
        ++index;
    }
    distance = best;
    return nearest;
}

// Xbox 0x00121E30
void add_runtime_actor_shadow_caster(const RuntimeSceneShadowActor& actor, i32 volume)
{
    if (volume < 0)
    {
        return;
    }
    Vec3 position;
    float height;
    float radius;
    runtime_actor_cylinder_metrics(actor.actor, position, height, radius); // 0x0003DD50
    if (radius > 0.f && actor.mode == 0)
    {
        RuntimeModelInstance* model = actor.shadow != nullptr ? actor.shadow : actor.model;
        add_runtime_light_volume_caster(volume, position, model, runtime_actor_pose(actor.model),
            actor.actor.xform, height, radius);
    }
}

// Xbox 0x001211E0
void register_runtime_actor_shadow(const RuntimeSceneShadowActor& actor, u32 player,
    RuntimeSceneShadowCasters& context)
{
    if (actor.mode == 0)
    {
        return;
    }
    Vec3 center;
    float height;
    float radius;
    runtime_actor_cylinder_metrics(actor.actor, center, height, radius); // 0x0003DD50
    center.y = height * 0.5f + center.y; // DAT_0032B0F4.
    if (height > radius)
    {
        radius = height;
    }
    const float size = radius * 2.f; // DAT_0032B110.
    auto& view = *entity_detail::g_display_view;
    Vec3 light;
    if (context.paused == 0)
    {
        light.x = center.x + context.light_offset.x;
        light.y = (context.light_offset.y + center.y) + size;
        light.z = center.z + context.light_offset.z;
        float distance = 0.f;
        const i32 point = nearest_runtime_ambient_point(center, distance, context);
        if (point >= 0 && 10000.f > distance) // DAT_0032B4D0.
        {
            const Vec3& ambient = context.points[size_t(point)].position;
            light.y = ambient.y + 40.f; // DAT_0032B1B8.
            light.x = ambient.x;
            light.z = ambient.z;
        }
        if (context.background.data() != nullptr)
        {
            const u32 header = read_u32(context.background, 0x10);
            const u32 table = read_u32(context.background, header + 0x1C);
            if (table != 0)
            {
                const auto probe = nearest_runtime_light_probe(center, context.background.subspan(table));
                if (probe.data() != nullptr)
                {
                    const float reach = read_f32(probe, 0x24);
                    const float x = center.x - read_f32(probe, 0);
                    const float y = center.y - read_f32(probe, 4);
                    const float z = center.z - read_f32(probe, 8);
                    if (reach * reach > (z * z + y * y) + x * x)
                    {
                        light.y = read_f32(probe, 4);
                        light.x = read_f32(probe, 0);
                        light.z = read_f32(probe, 8);
                        if (probe[0xC] != 0)
                        {
                            const float one = 1.f; // DAT_0032B0F8.
                            const float spread = 6.f; // DAT_0032B218.
                            const float half = 3.f; // DAT_0032B260.
                            light.x = ((next_random_unit(context.random) - one) * spread + light.x) - half;
                            light.y = ((next_random_unit(context.random) - one) * spread + light.y) - half;
                            light.z = ((next_random_unit(context.random) - one) * spread + light.z) - half;
                        }
                    }
                }
            }
        }
    }
    else
    {
        light.y = view.position.y + 20.f; // DAT_0032B234.
        light.x = view.position.x;
        light.z = view.position.z;
    }
    actor.target = {light.x - center.x, light.y - center.y, light.z - center.z};
    if (actor.flags[0] == 0)
    {
        actor.eased = actor.target;
        actor.flags[0] = 1;
    }
    const Vec3 offset = context.ease == 0 ? actor.target : actor.eased;
    Vec3 aim{center.x + offset.x, offset.y + center.y, center.z + offset.z};
    float dx = aim.x - center.x;
    float dy = aim.y - center.y;
    float dz = aim.z - center.z;
    if (1600.f > (dz * dz + dy * dy) + dx * dx) // DAT_0032B4CC.
    {
        const float length = (dz * dz + dx * dx) + dy * dy;
        if (length != 0.f)
        {
            const float scale = engine_reciprocal_sqrt(length); // 0x001534C0
            dx = float(static_cast<long double>(dx) * scale);
            dy = float(static_cast<long double>(dy) * scale);
            dz = float(static_cast<long double>(scale) * dz);
        }
        aim = {center.x + dx * 40.f, center.y + dy * 40.f, center.z + dz * 40.f}; // DAT_0032B1B8.
    }
    if (0.f < radius && 0.f < size && actor.actor.link != nullptr && actor.actor.link->head != nullptr &&
        122500.f > runtime_actor_view_distance(actor.actor, view) && // DAT_0032B4C8.
        runtime_background_light_in_view(center, context.background, view) && actor.shadow != nullptr)
    {
        const i32 volume = register_runtime_light_volume(center, aim, actor.shadow, runtime_actor_pose(actor.model),
            actor.actor.xform, actor.flags[1], player, radius, size, view.position, context.light_count);
        if (actor.held != nullptr)
        {
            add_runtime_actor_shadow_caster(context.shadow(*actor.held), volume);
        }
        for (ActorLink* child = actor.actor.link->child; child != nullptr; child = child->next)
        {
            if (child->actor == nullptr)
            {
                throw Error("shadow caster child +0x918 node has no native actor");
            }
            add_runtime_actor_shadow_caster(context.shadow(*child->actor), volume);
        }
    }
}

// Xbox 0x00120EF0
void update_runtime_scene_shadow_casters(RuntimeSceneShadowCasters& context)
{
    auto& bin = context.bins[context.view];
    entity_detail::ListHead* node = &bin;
    while (node->next != nullptr)
    {
        node = static_cast<entity_detail::ListHead*>(node->next);
    }
    EntityCamNode* camera = entity_detail::g_cam_active[0]; // DAT_004E16A0.
    if (select_display_camera(camera, 0, context.display)) // 0x000489E0
    {
        rebuild_display_view(context.display); // 0x000F5E50
        extend_display_clip_limits(entity_detail::g_display_view->transform, context.display.scene_bounds,
            camera->blend, camera->clip_limit); // 0x0013C490
        entity_detail::g_display_view->near_clip = camera->clip_limit;
        entity_detail::g_display_view->far_clip = camera->blend;
        rebuild_display_projection(context.display); // 0x000F5FD0
        bind_display_projection(context.display); // 0x000F6170
    }
    for (; node != &bin; node = static_cast<entity_detail::ListHead*>(node->prev))
    {
        Actor& actor = context.bin_actor(*node);
        const auto shadow = context.shadow(actor);
        if (shadow.model != nullptr)
        {
            place_runtime_model_shader_lights(*shadow.model, actor);
            register_runtime_actor_shadow(shadow, &actor == context.player ? 1 : 0, context);
        }
    }
}
}
