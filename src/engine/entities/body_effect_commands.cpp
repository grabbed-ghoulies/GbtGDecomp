#include "body_effect_commands.h"
#include "actor_model_anchors.h"
#include "../navigation/navigation.h"
#include "../frontend/dialogue_resources.h"
#include "../cutscenes/event_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0001A3C0
void command_runtime_body_projectile(std::span<const u8> body, std::span<const u8> command,
    RuntimeBodyProjectileCreation& context)
{
    const u8* record = find_actor_model_binding(context.actor, i16(read_u16(command, 0x90)));
    const ModelPoseBuffer* pose;
    if (context.actor.model == nullptr)
    {
        pose = nullptr;
    }
    else
    {
        pose = context.actor.model->runtime->alternate;
        if (pose == nullptr)
        {
            pose = context.actor.model->runtime->primary;
        }
    }
    const std::span<const u8> binding{record, 0x34};
    float x = read_f32(binding, 0x10);
    float y = read_f32(binding, 0x20);
    float z = read_f32(binding, 0x30);
    const i16 joint = i16(read_u16(binding, 2));
    if (joint >= 0)
    {
        const auto& matrix = model_pose_matrix(*pose, joint);
        const float transformed_x = ((matrix[2] * z + matrix[1] * y) + matrix[0] * x) + matrix[3];
        const float transformed_y = ((matrix[6] * z + matrix[4] * x) + matrix[5] * y) + matrix[7];
        const float transformed_z = ((matrix[10] * z + matrix[8] * x) + matrix[9] * y) + matrix[11];
        x = transformed_x;
        y = transformed_y;
        z = transformed_z;
    }
    const float* world = context.actor.xform;
    Vec3 position;
    position.x = ((world[2] * z + world[1] * y) + world[0] * x) + world[3];
    position.y = ((world[6] * z + world[4] * x) + world[5] * y) + world[7];
    position.z = ((world[10] * z + world[8] * x) + world[9] * y) + world[11];
    Vec3 euler{get(body, body_field::euler).x, get(body, body_field::euler).y, 0.f};
    auto object = create_runtime_actor_at(reinterpret_cast<const char*>(command.data() + 0x10),
        &position, &euler, 1.f, context.resources, context.factory, context.default_position, context.default_euler);
    if (object.bytes != nullptr)
    {
        const u8* bytes = object.bytes.get();
        context.objects.push_back(std::move(object));
        Vec3 direction = position;
        // The retail operands are degrees per radian and pi / 180, read from the image.
        add_sound_direction(direction, (euler.x * std::bit_cast<float>(u32(0x42652EE0))) * std::bit_cast<float>(u32(0x3C8EFA35)),
            (euler.y * std::bit_cast<float>(u32(0x42652EE0))) * std::bit_cast<float>(u32(0x3C8EFA35)), 30.f);
        auto& actor = context.attached(bytes);
        actor.host = &context.host;
        std::memcpy(actor.bytes.data() + 0xD8C, &context.host.address, 4);
        if (actor.actor != nullptr)
        {
            actor.actor->spawn_owner = context.host.actor; // Native alias of the +0xD8C word.
        }
        const u32 message[]{0xA5, std::bit_cast<u32>(direction.x), std::bit_cast<u32>(direction.y), std::bit_cast<u32>(direction.z)};
        actor.message(message);
        const u32 notice[]{0x5F, actor.address};
        context.host.message(notice);
    }
}

// Xbox 0x0013DEF0
bool format_runtime_body_surface_effect(const char* tag, u8 surface,
    const char* suffix, const char* side, char* output, RuntimeBodyEffectCommands& context)
{
    auto& resources = context.effects.models.registry;
    format_dialogue_resource_name(output, "aid_fxemitter_%s_kickedup%s_%s_%s%s",
        context.scene_name, side, context.surfaces[surface], tag, suffix);
    if (find_animation_resource(resources.records, output) == nullptr)
    {
        if (!cutscene_resource_exists(output, resources))
        {
            format_dialogue_resource_name(output, "aid_fxemitter_%s_kickedup%s_%s_default",
                context.scene_name, side, context.surfaces[surface]);
            if (find_animation_resource(resources.records, output) == nullptr)
            {
                if (!cutscene_resource_exists(output, resources))
                {
                    output[0] = 0;
                    return false;
                }
            }
        }
    }
    return true;
}

// Xbox 0x0001A130
bool select_runtime_body_surface_effect(std::span<const u8> body, const Vec3& position,
    i32 alternate, const char* side, char* output, RuntimeBodyEffectCommands& context)
{
    u8 surface;
    if (get(body, body_field::surface_override_frames) == 0)
    {
        const auto* grid = entity_detail::g_nav_grids[context.floor];
        if (grid == nullptr)
        {
            surface = 0;
        }
        else
        {
            const i32 index = entity_detail::navigation_cell_index(context.floor, position);
            if (index == -1)
            {
                surface = 0;
            }
            else
            {
                const auto& cell = grid->cells[index];
                surface = u8(cell.value_8 >> 24);
                if (surface == 0)
                {
                    surface = u8(cell.value_8 >> 16);
                }
            }
        }
    }
    else
    {
        surface = get(body, body_field::surface_override);
    }
    const u16 tag = read_u16(body, 0x87C);
    output[0] = 0;
    if (surface == 0)
    {
        return false;
    }
    const u8 kind = body[alternate != 0 ? 0x2D4 : 0x2D3];
    const char* suffix = output;
    if (kind == 0)
    {
        suffix = context.suffix_zero;
    }
    else if (kind == 1)
    {
        suffix = context.suffix_one;
    }
    const char* name = context.tags[tag];
    size_t last = 0;
    size_t index = 0;
    while (name[index] != 0)
    {
        if (name[index] == '_')
        {
            last = index;
        }
        ++index;
    }
    return format_runtime_body_surface_effect(name + last + 1, surface, suffix, side, output, context);
}

// Xbox 0x0001A220
void create_runtime_body_surface_effect(std::span<const u8> body, std::span<const u8> command,
    const Vec3& position, const char* side, const RuntimeActorCreationOptions* options,
    RuntimeBodyEffectCommands& context)
{
    char name[128];
    if (select_runtime_body_surface_effect(body, position, i32(read_u32(command, 0x10)), side, name, context))
    {
        if (load_actor_effect_stream(name, options, context.effects) != 0)
        {
            const i16 index = i16(g_last_actor_effect);
            g_last_actor_effect = -1;
            if (index != -1)
            {
                g_actor_effect_nodes[index]->bytes[0x18] |= 8;
            }
        }
    }
}

// Xbox 0x00019C60
void command_runtime_body_surface_effect(std::span<const u8> body, std::span<const u8> command,
    RuntimeBodyEffectCommands& context)
{
    auto& random = context.effects.random_state;
    random = random * 0x19660D + 0x3C6EF35F;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000) - 1.f;
    if (read_f32(command, 0x1C) >= fraction)
    {
        const u8* record = find_actor_model_binding(context.actor, i16(read_u16(command, 0x14)));
        if (record != nullptr)
        {
            g_actor_creation_options.flags = 0;
            ModelBoneBinding binding;
            std::memcpy(&binding.kind, record, 2);
            std::memcpy(&binding.joint, record + 2, 2);
            std::memcpy(binding.matrix.data(), record + 4, 48);
            Vec3 position;
            Vec3 euler;
            const ModelPoseBuffer* pose;
            if (context.actor.model == nullptr)
            {
                pose = nullptr;
            }
            else
            {
                pose = context.actor.model->runtime->alternate;
                if (pose == nullptr)
                {
                    pose = context.actor.model->runtime->primary;
                }
            }
            float x = binding.matrix[3];
            float y = binding.matrix[7];
            float z = binding.matrix[11];
            if (binding.joint >= 0)
            {
                const auto& matrix = model_pose_matrix(*pose, binding.joint);
                const float transformed_x = ((matrix[2] * z + matrix[1] * y) + matrix[0] * x) + matrix[3];
                const float transformed_y = ((matrix[4] * x + matrix[6] * z) + matrix[5] * y) + matrix[7];
                const float transformed_z = ((matrix[8] * x + matrix[10] * z) + matrix[9] * y) + matrix[11];
                x = transformed_x;
                y = transformed_y;
                z = transformed_z;
            }
            const float* world = context.actor.xform;
            position.x = ((world[2] * z + world[1] * y) + x * world[0]) + world[3];
            position.y = ((world[4] * x + world[6] * z) + world[5] * y) + world[7];
            position.z = ((world[8] * x + world[10] * z) + world[9] * y) + world[11];
            actor_binding_world_euler(context.actor, binding, euler);
            if (read_u32(command, 0x20) == 0)
            {
                g_actor_creation_options.scale = context.actor.radius_xz;
            }
            else
            {
                g_actor_creation_options.scale = context.actor.scale_x * context.actor.scale_200;
            }
            g_actor_creation_options.position = position;
            g_actor_creation_options.euler = euler;
            g_actor_creation_options.flags |= 0x45;
            const u32 side = read_u32(command, 0x18);
            if (side == 0)
            {
                create_runtime_body_surface_effect(body, command, position, context.side_zero, &g_actor_creation_options, context);
            }
            else if (side == 1)
            {
                create_runtime_body_surface_effect(body, command, position, context.side_one, &g_actor_creation_options, context);
                return;
            }
            else if (side == 2)
            {
                create_runtime_body_surface_effect(body, command, position, context.side_zero, &g_actor_creation_options, context);
                create_runtime_body_surface_effect(body, command, position, context.side_one, &g_actor_creation_options, context);
                return;
            }
        }
    }
}
}
