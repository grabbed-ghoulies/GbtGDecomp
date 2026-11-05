#include "body_construction.h"
#include <cstddef>
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00024C50
bool initialize_runtime_parent_body(std::span<u8> body, std::span<const u8> parameters,
    RuntimeParentBodyConstruction& context)
{
    auto& owner = context.owner;
    auto& actor = owner.attachment.actor;
    owner.attachment.actor_address = read_u32(parameters, 0x1B0);
    std::memcpy(body.data() + 8, parameters.data() + 0x1B0, 4);
    const float perturbation = read_f32(parameters, 0x18C);
    if (perturbation != 0.f)
    {
        i32 index = 0;
        while (index < context.scale_exclusion_count && context.scale_exclusions[index] != actor.class_key)
        {
            ++index;
        }
        if (index >= context.scale_exclusion_count)
        {
            const float range = perturbation / actor.scale_200;
            auto& random = context.selection.random;
            random = random * 1664525u + 1013904223u;
            const float low = 0.f - range;
            const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            const float factor = (range - low) * fraction + low + 1.f;
            const float x = factor * actor.scale_x;
            const float y = actor.radius_y * factor;
            const float z = actor.radius_xz * factor;
            if (x != actor.scale_x || y != actor.radius_y || z != actor.radius_xz)
            {
                actor.scale_x = x;
                actor.radius_y = y;
                actor.radius_xz = z;
                actor.xform_valid = 0;
            }
        }
    }
    const auto put = [&](size_t offset, u32 value)
    {
        std::memcpy(body.data() + offset, &value, 4);
    };
    const auto copy = [&](size_t target, size_t source, size_t size)
    {
        std::memcpy(body.data() + target, parameters.data() + source, size);
    };
    copy(0x884, 0x190, 4);
    copy(0x888, 0x194, 4);
    copy(0x88C, 0x1B8, 4);
    put(0x890, 2);
    initialize_runtime_body_health(body, parameters, owner.health);
    put(0x738, parameters[0x1A9]);
    owner.attachment.flags = allocate_runtime_body_bits(0x41);
    owner.attachment.flags->bytes[3] |= 0x20;
    initialize_runtime_body_motion(body);
    body_record(body).unk_38 = 0;
    body_record(body).unk_39 = 0;
    const Vec3 euler = actor_world_euler(actor);
    std::memcpy(body.data() + offsetof(BodyRecord, euler.x), &euler, sizeof(euler));
    const Vec3 position = actor_world_position(actor);
    std::memcpy(body.data() + offsetof(BodyRecord, position.x), &position, sizeof(position));
    if (read_u32(parameters, 0x88) == 0x5F646961)
    {
        reload_runtime_body_actions(body, reinterpret_cast<const char*>(parameters.data() + 0x88),
            owner.first_action, owner.second_action, owner.actions, owner.factory,
            owner.animation.registry, owner.animation.error);
        context.selection.row = nullptr;
        context.selection.table = context.action_tables.at(owner.actions.data());
    }
    copy(0x74A, 0x188, 2);
    initialize_runtime_body_animation_settings(body, parameters, context.actor_bytes,
        context.animation_classes, context.animation_class_count, owner.animation);
    put(0x58, count_runtime_body_model_kind(context.nodes, 4));
    initialize_runtime_body_friction(body, parameters, context.actor_bytes, context.friction_curve, context.tags);
    actor.shape_mode = parameters[0x1AA];
    acquire_runtime_body_attribute_table(body, parameters, owner.attributes,
        owner.animation.registry, owner.animation.error);
    owner.collision = create_runtime_body_collision_state(context.nodes);
    body_record(body).unk_92 = 1;
    initialize_runtime_body_model_channels(body, context.model_channels);
    acquire_runtime_body_resource_hashes(parameters, owner.resources, context.resource_counts,
        owner.hashes, owner.hash_count, owner.animation.registry, context.loaded_sizes, owner.animation.error);
    body_record(body).unk_2D0 = parameters[0x1BC];
    body_record(body).unk_2D1 = parameters[0x1BD];
    body_record(body).unk_2D3 = parameters[0x1BE];
    body_record(body).unk_2D4 = parameters[0x1BF];
    if (body_record(body).unk_2D0 != 0)
    {
        body_record(body).unk_2D2 = 0;
    }
    put(0x44, 0);
    put(0x40, 0);
    put(0x3C, 0);
    put(0x718, 0);
    put(0x71C, 0x3F0EFA35);
    put(0x720, 0x41490FDB);
    put(0x724, 0x40C00000);
    put(0x728, 1);
    body_record(body).surface = 0;
    body_record(body).surface_override = 0;
    copy(0x878, 0x1B4, 4);
    copy(0x87C, 0x1AC, 2);
    collect_runtime_body_special_shapes(body, context.nodes, owner.shapes);
    body_record(body).collision_kind = 0;
    initialize_runtime_body_action(body, parameters, context.action);
    const u16 key = body_record(body).default_action_key;
    if (key != 0)
    {
        select_runtime_body_action(body, key, context.selection);
    }
    return true;
}
}
