#include "entities/trigger_actor.h"
#include "entities/cinema_mind_lifecycle.h"
#include "entities/cinema_mind_requests.h"
#include "entities/cinema_mind_control.h"
#include "entities/body_orientation.h"
#include "entities/mind_target_policy.h"
#include "navigation/mind_escape_position.h"
#include "navigation/mind_navigation_angles.h"
#include "entities/body_damage_motion.h"
#include "entities/cinema_mind_request_update.h"
#include "entities/actor_collision_culling.h"
#include "entities/actor_shape_push.h"
#include "graphics/vector_normalize.h"
#include "scripts/script_conditions.h"
#include "entities/model_sphere_collision.h"
#include "entities/collision_shape_dispatch.h"
#include "entities/physical_mesh_queries.h"
#include "entities/mesh_sphere_geometry.h"
#include <cstring>
#include <iostream>

using namespace gbtg;

int main()
{
    u32 count;
    std::cin >> count;
    for (u32 index = 0; index < count; ++index)
    {
        u32 mode, random, slot_word;
        std::cin >> mode >> random >> slot_word;
        std::array<u8, 0x1000> parameters;
        FactoryObject object;
        object.size = 0xC6C;
        object.bytes = std::make_unique<u8[]>(object.size);
        for (u8& byte : parameters)
        {
            u32 value;
            std::cin >> value;
            byte = u8(value);
        }
        for (u32 offset = 0; offset < object.size; ++offset)
        {
            u32 value;
            std::cin >> value;
            object.bytes[offset] = u8(value);
        }
        i32 slot = i32(slot_word);
        bool result;
        if (mode == 0)
        {
            result = initialize_runtime_area_actor(object, parameters);
        }
        else if (mode == 1)
        {
            Actor actor;
            GhoulyController controller;
            GhoulySpawnRecord* load = nullptr;
            const i32 blocked = -1;
            RuntimeTriggerActorContext context{{object.bytes.get(), object.size}, actor,
                controller, random, 0.f, {}, load, blocked, {}};
            result = initialize_runtime_trigger_actor(context, parameters);
        }
        else if (mode == 2)
        {
            result = find_runtime_trigger_spawn_slot({object.bytes.get(), object.size}, slot);
        }
        else if (mode == 3)
        {
            std::span<const u8> resource;
            AnimationResourceRegistry registry;
            ResourceErrorState error{};
            RuntimeCinemaMindLifecycle context{{object.bytes.get(), object.size}, parameters, resource,
                registry, error, random, [](const void*) -> u32
                {
                    throw Error("cinema-mind fixture unexpectedly acquired a resource");
                }};
            result = initialize_runtime_cinema_mind(context, parameters);
        }
        else if (mode == 4)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.euler = {read_f32(parameters, 0x14), read_f32(parameters, 0x18), read_f32(parameters, 0x1C)};
            const auto target = read_u32(parameters, 0) == 0 ? std::span<const u8>{} :
                std::span<const u8>{parameters}.subspan(0x400);
            result = initialize_runtime_cinema_mind_request(actor, target,
                {object.bytes.get(), object.size}, random) != 0;
        }
        else if (mode == 5)
        {
            refresh_runtime_cinema_mind_action_permission({object.bytes.get(), object.size}, parameters,
                i32(read_u32(parameters, 0)), std::span<const u8>{parameters}.subspan(0x400));
            result = true;
        }
        else if (mode == 6)
        {
            Actor actor;
            actor.animation_rate = read_f32(parameters, 0x92C);
            actor.animation_pause = read_f32(parameters, 0x930);
            const float freeze = read_f32(parameters, 8);
            const float dt = read_f32(parameters, 0xC);
            const float scale = read_f32(parameters, 0x10);
            const u32 zero = 0;
            RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            RuntimeActorSoundQueues sounds{};
            update_runtime_cinema_mind_sound({object.bytes.get(), object.size},
                std::span<const u8>{parameters}.subspan(0x100),
                std::span<const u8>{parameters}.subspan(0x400), actor, clock, random, sounds);
            result = true;
        }
        else if (mode == 7 || mode == 8)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            if (mode == 7)
            {
                const Vec3 point{read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
                face_runtime_body_point({object.bytes.get(), object.size}, actor, point);
                result = true;
            }
            else
            {
                result = face_runtime_cinema_mind_target(std::span<const u8>{parameters}.subspan(0x600),
                    {object.bytes.get(), object.size}, actor, std::span<const u8>{parameters}.subspan(0x400)) != 0;
            }
        }
        else if (mode == 9)
        {
            Actor actor, target_actor;
            RuntimeScriptMindQuery query{};
            RuntimeMindTarget target{nullptr, std::span<const u8>{parameters}.subspan(0x600),
                0x10002600, target_actor, parameters, query, nullptr};
            result = runtime_mind_target_is_eligible({object.bytes.get(), object.size}, parameters,
                actor, 0x10002000, target, read_u32(parameters, 0) != 0);
        }
        else if (mode == 10)
        {
            std::array<RuntimeInputAxisWriteDevice, 4> children;
            for (size_t index = 0; index < children.size(); ++index)
            {
                children[index].record = {object.bytes.get() + 0x100 + index * 0x80, 0x80};
            }
            RuntimeInputAxisWriteDevice device{{object.bytes.get(), 0x80},
                {&children[0], &children[1], &children[2], &children[3]}};
            set_runtime_input_vertical_axes(device,
                {read_f32(parameters, 0), read_f32(parameters, 4)}, {object.bytes.get() + 0x800, 0x80});
            result = true;
        }
        else if (mode == 26)
        {
            std::array<scene_detail::LiveScript, 8> nodes{};
            const u32 count = read_u32(parameters, 4);
            for (u32 row = 0; row < count; ++row)
            {
                nodes[row].type = read_u32(parameters, 0x400 + row * 4);
                nodes[row].next = row + 1 < count ? &nodes[row + 1] : nullptr;
            }
            g_live_scripts = count != 0 ? &nodes[0] : nullptr;
            result = find_runtime_script_event_type(read_u32(parameters, 0)) != nullptr;
            g_live_scripts = nullptr;
        }
        else if (mode == 25)
        {
            const Vec3 first{read_f32(parameters, 0x680), read_f32(parameters, 0x684), read_f32(parameters, 0x688)};
            const Vec3 second{read_f32(parameters, 0x6A0), read_f32(parameters, 0x6A4), read_f32(parameters, 0x6A8)};
            const Vec3 direction{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 output, contact;
            std::memcpy(&output, object.bytes.get() + 0x700, 12);
            std::memcpy(&contact, object.bytes.get() + 0x740, 12);
            switch (read_u32(parameters, 0))
            {
            case 0:
                normalize_runtime_graphics_vector(output, first);
                result = true;
                break;
            case 1:
                result = runtime_graphics_scalars_equal(read_f32(parameters, 0x3C0), read_f32(parameters, 0x3C4));
                break;
            case 2:
                divide_runtime_graphics_vector(output, first, read_f32(parameters, 0x3C0));
                result = true;
                break;
            case 3:
                sweep_runtime_directed_cylinder_push(first, second, read_f32(parameters, 0x3C0),
                    read_f32(parameters, 0x3C4), direction, output, normalize_runtime_graphics_vector);
                result = true;
                break;
            default:
            {
                Actor actor, other;
                actor.position = {read_f32(parameters, 0x520), read_f32(parameters, 0x524), read_f32(parameters, 0x528)};
                other.position = {read_f32(parameters, 0x540), read_f32(parameters, 0x544), read_f32(parameters, 0x548)};
                result = push_runtime_attached_cylinder(actor, other,
                    *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x400),
                    *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x440),
                    read_u32(parameters, 4) != 0 ? &direction : nullptr, first,
                    output, contact, normalize_runtime_graphics_vector);
                break;
            }
            }
            std::memcpy(object.bytes.get() + 0x700, &output, 12);
            std::memcpy(object.bytes.get() + 0x740, &contact, 12);
        }
        else if (mode == 24)
        {
            Actor actor;
            ActorModel model{parameters};
            actor.model = read_u32(parameters, 0x3C4) != 0 ? &model : nullptr;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.radius_y = read_f32(parameters, 0x34);
            actor.radius_xz = read_f32(parameters, 0x38);
            actor.model_size_x = read_f32(parameters, 0x1C4);
            actor.model_size_y = read_f32(parameters, 0x1C8);
            actor.model_size_z = read_f32(parameters, 0x1CC);
            actor.model_mode = i32(read_u32(parameters, 0x118));
            std::memcpy(actor.xform, parameters.data() + 0x500, 48);
            const auto& merged = *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x400);
            Vec3 output, contact;
            std::memcpy(&output, object.bytes.get() + 0x700, 12);
            std::memcpy(&contact, object.bytes.get() + 0x740, 12);
            RuntimeActorShapePush context{actor, reinterpret_cast<const u32*>(parameters.data() + 0x3A0),
                contact, {}, {}};
            result = push_runtime_actor_shapes(nullptr, nullptr, merged,
                reinterpret_cast<EntityCylinderBound*>(parameters.data() + 0x440),
                i32(read_u32(parameters, 0x3C0)), output, context);
            std::memcpy(object.bytes.get() + 0x700, &output, 12);
            std::memcpy(object.bytes.get() + 0x740, &contact, 12);
        }
        else if (mode == 23)
        {
            Actor actor, other;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            other.position = {read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
            actor.radius_y = read_f32(parameters, 0x34);
            actor.radius_xz = read_f32(parameters, 0x38);
            actor.model_size_x = read_f32(parameters, 0x1C4);
            actor.model_size_y = read_f32(parameters, 0x1C8);
            actor.model_size_z = read_f32(parameters, 0x1CC);
            const auto& fixed = *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x400);
            const auto& moving = *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x440);
            const Vec3 offset{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 output;
            std::memcpy(&output, object.bytes.get() + 0x700, 12);
            switch (read_u32(parameters, 0))
            {
            case 0:
            {
                float height, radius;
                runtime_actor_cylinder_metrics(actor, output, height, radius);
                std::memcpy(object.bytes.get() + 0x70C, &height, 4);
                std::memcpy(object.bytes.get() + 0x710, &radius, 4);
                result = true;
                break;
            }
            case 1:
                result = runtime_offset_cylinders_apart(fixed, moving, offset);
                break;
            case 2:
                runtime_actor_planar_delta(actor, other, output);
                result = true;
                break;
            default:
                result = push_runtime_attachment_projection(actor, other, fixed, output);
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &output, 12);
        }
        else if (mode == 22)
        {
            const Vec3* vertices = reinterpret_cast<const Vec3*>(parameters.data() + 0x400);
            const Vec3 point{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            const Vec3 normal{read_f32(parameters, 0x640), read_f32(parameters, 0x644), read_f32(parameters, 0x648)};
            result = read_u32(parameters, 0) == 0
                ? runtime_projected_point_in_triangle(normal, point, vertices[1], vertices[2])
                : runtime_sphere_intersects_triangle(point, read_f32(parameters, 0x3C0), vertices[0], vertices[1], vertices[2]);
        }
        else if (mode == 21)
        {
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const float* inverse = reinterpret_cast<const float*>(parameters.data() + 0x540);
            const Vec3 start{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 end{read_f32({object.bytes.get(), object.size}, 0x700), read_f32({object.bytes.get(), object.size}, 0x704),
                read_f32({object.bytes.get(), object.size}, 0x708)};
            const Vec3 stationary{};
            const std::span<u8> record = read_u32(parameters, 0x3C4) != 0 ? std::span<u8>{object.bytes.get(), 0x54} : std::span<u8>{};
            RuntimeCollisionShapeCall call{parameters.data() + 0x800, 0x10002800, world, inverse, nullptr,
                0x10002400, start, end, read_f32(parameters, 0x3C0), read_u32(parameters, 0x3B0),
                i32(read_u32(parameters, 0x3B4)), record, nullptr, stationary};
            const auto resolve = [&](u32 address, size_t size)
            {
                return std::span<const u8>{parameters}.subspan(address - 0x10002000, size);
            };
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = collide_runtime_physical_mesh_segment(call, std::span<const u8>{parameters}.subspan(0x800, 0x34), resolve);
                break;
            case 1:
                result = collide_runtime_physical_mesh_sweep(call, std::span<const u8>{parameters}.subspan(0x800, 0x34), resolve);
                break;
            case 2:
                result = collect_runtime_physical_mesh_contacts(call, std::span<const u8>{parameters}.subspan(0x800, 0x34), resolve);
                break;
            case 3:
                result = collide_runtime_posed_mesh_segment(call, nullptr, std::span<const u8>{parameters}.subspan(0x800, 0x1C), resolve);
                break;
            case 4:
                result = collide_runtime_posed_mesh_sweep(call, nullptr, std::span<const u8>{parameters}.subspan(0x800, 0x1C), resolve);
                break;
            default:
                result = collect_runtime_posed_mesh_contacts(call, nullptr, std::span<const u8>{parameters}.subspan(0x800, 0x1C), resolve);
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &end, 12);
        }
        else if (mode == 20)
        {
            const Vec3* vertices = reinterpret_cast<const Vec3*>(parameters.data() + 0x400);
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const Vec3 origin{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            const Vec3 movement{read_f32(parameters, 0x620), read_f32(parameters, 0x624), read_f32(parameters, 0x628)};
            const Vec3 normal{read_f32(parameters, 0x640), read_f32(parameters, 0x644), read_f32(parameters, 0x648)};
            switch (read_u32(parameters, 0))
            {
            case 0:
            {
                std::array<i32, 3> lower, upper;
                runtime_mesh_query_cell_bounds(std::span<const u8>{parameters}.subspan(0x800), origin,
                    movement, read_f32(parameters, 0x3C0), lower, upper);
                std::memcpy(object.bytes.get() + 0x700, lower.data(), 12);
                std::memcpy(object.bytes.get() + 0x70C, upper.data(), 12);
                result = true;
                break;
            }
            case 1:
            {
                float fraction = read_f32({object.bytes.get(), object.size}, 0x700);
                result = intersect_runtime_mesh_triangle(movement, vertices[1], vertices[2], vertices[0], origin, fraction);
                std::memcpy(object.bytes.get() + 0x700, &fraction, 4);
                break;
            }
            case 2:
                write_runtime_mesh_triangle_contact({object.bytes.get(), 0x54}, std::span<const u8>{parameters}.subspan(0x800),
                    0x10002800, vertices, normal, origin, read_f32(parameters, 0x3C0), world);
                result = true;
                break;
            default:
                write_runtime_posed_mesh_contact({object.bytes.get(), 0x54}, std::span<const u8>{parameters}.subspan(0x800),
                    0x10002800, vertices, normal, origin, read_f32(parameters, 0x3C0), world,
                    read_u32(parameters, 0x3C4) != 0 ? reinterpret_cast<const float*>(parameters.data() + 0x540) : nullptr);
                result = true;
                break;
            }
        }
        else if (mode == 19)
        {
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const float* inverse = reinterpret_cast<const float*>(parameters.data() + 0x540);
            const Vec3 start{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 end{read_f32({object.bytes.get(), object.size}, 0x700), read_f32({object.bytes.get(), object.size}, 0x704),
                read_f32({object.bytes.get(), object.size}, 0x708)};
            const Vec3 stationary{};
            const std::span<u8> record = read_u32(parameters, 0x3C4) != 0 ? std::span<u8>{object.bytes.get(), 0x54} : std::span<u8>{};
            const std::unordered_map<u32, std::function<u32(RuntimeCollisionShapeCall&)>> physical;
            const u32* masks = reinterpret_cast<const u32*>(parameters.data() + 0x3A4);
            RuntimeCollisionShapeDispatch context{{nullptr, 0, world, inverse, nullptr, 0, start, end,
                read_f32(parameters, 0x3C0), read_u32(parameters, 0x3B0), 0, record, nullptr, stationary},
                masks, std::span<const u8>{parameters}.subspan(0x900),
                [&](const void* pointer)
                {
                    return 0x10002000u + u32(static_cast<const u8*>(pointer) - parameters.data());
                }, physical};
            const auto list = std::span<const u8>{parameters}.subspan(0x800);
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = dispatch_runtime_model_segment_shapes(list, context) != 0;
                break;
            case 1:
                result = dispatch_runtime_model_swept_shapes(list, context) != 0;
                break;
            default:
                result = dispatch_runtime_model_contact_shapes(list, context) != 0;
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &end, 12);
        }
        else if (mode == 18)
        {
            const auto& node = *reinterpret_cast<const EntityShapeNode*>(parameters.data() + 0x400);
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const float* inverse = reinterpret_cast<const float*>(parameters.data() + 0x540);
            const Vec3 start{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            const Vec3 center{read_f32(parameters, 0x620), read_f32(parameters, 0x624), read_f32(parameters, 0x628)};
            Vec3 end{read_f32({object.bytes.get(), object.size}, 0x700), read_f32({object.bytes.get(), object.size}, 0x704),
                read_f32({object.bytes.get(), object.size}, 0x708)};
            const Vec3 stationary{};
            const std::span<u8> record = read_u32(parameters, 0x3C4) != 0 ? std::span<u8>{object.bytes.get(), 0x54} : std::span<u8>{};
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = collide_runtime_model_sphere_segment(node, 0x10002400, world, inverse, nullptr, start, end, record, stationary);
                break;
            case 1:
                result = collide_runtime_model_sphere_sweep(node, 0x10002400, world, inverse, nullptr, start, end,
                    read_f32(parameters, 0x3C0), record, stationary);
                break;
            case 2:
                result = collect_runtime_model_sphere_contact(node, 0x10002400, world, inverse, nullptr, start,
                    read_f32(parameters, 0x3C0), nullptr);
                break;
            default:
                write_runtime_sphere_contact({object.bytes.get(), 0x54}, std::span<const u8>{parameters}.subspan(0x400),
                    0x10002400, start, center, read_f32(parameters, 0x3C0), world);
                result = true;
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &end, 12);
        }
        else if (mode == 17)
        {
            result = runtime_collision_shape_list_has_physical_shape(std::span<const u8>{parameters}.subspan(0x800));
        }
        else if (mode == 16)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            const Vec3 start{read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
            const Vec3 end{read_f32(parameters, 0x440), read_f32(parameters, 0x444), read_f32(parameters, 0x448)};
            const float radius = read_f32(parameters, 0x3C0);
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = runtime_collision_spheres_overlap(actor.position, read_f32(parameters, 0x3C4), start, radius);
                break;
            case 1:
                result = runtime_actor_culls_segment(actor, parameters, start, end);
                break;
            case 2:
                result = runtime_actor_culls_swept_sphere(actor, parameters, start, end, radius);
                break;
            default:
                result = runtime_actor_culls_sphere(actor, parameters, start, radius);
                break;
            }
        }
        else if (mode == 15)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.euler.y = read_f32(parameters, 0x18);
            result = update_runtime_cinema_mind_facing_request(std::span<const u8>{parameters}.subspan(0x600),
                {object.bytes.get(), object.size}, actor, std::span<const u8>{parameters}.subspan(0x400),
                std::span<const u8>{parameters}.subspan(0x800)) != 0;
        }
        else if (mode == 14)
        {
            std::array<float, 2> direction{read_f32(parameters, 0), read_f32(parameters, 4)};
            scale_runtime_planar_direction(direction, read_f32(parameters, 8));
            std::memcpy(object.bytes.get() + 0x70, direction.data(), 8);
            result = true;
        }
        else if (mode == 12)
        {
            const float angle = interpolate_runtime_mind_angle(read_f32(parameters, 0),
                read_f32(parameters, 4), read_f32(parameters, 8));
            std::memcpy(object.bytes.get() + 0x70, &angle, 4);
            result = true;
        }
        else if (mode == 13)
        {
            const Vec3 source{read_f32(parameters, 0), read_f32(parameters, 4), read_f32(parameters, 8)};
            Vec3 output;
            offset_runtime_horizontal_point(output, source, read_f32(parameters, 0xC), read_f32(parameters, 0x10));
            std::memcpy(object.bytes.get() + 0x70, &output, sizeof(output));
            result = true;
        }
        else
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            RuntimeMindEscapePosition context{actor, parameters, std::span<const u8>{parameters}.subspan(0x400),
                parameters.data() + 0x800, nullptr, random, 0.f};
            Vec3 point;
            result = select_runtime_mind_escape_position(context, read_f32(parameters, 0x3C0),
                read_f32(parameters, 0x3C4), read_f32(parameters, 0x3C8), point);
            if (result)
            {
                std::memcpy(object.bytes.get() + 0x70, &point, sizeof(point));
            }
        }
        std::cout << result << ' ' << random << ' ' << u32(slot) << ' ';
        for (u32 offset = 0; offset < object.size; ++offset)
        {
            std::cout << u32(object.bytes[offset]) << ' ';
        }
        std::cout << '\n';
    }
}
