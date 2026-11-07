#include "animated_light.h"
#include "actor_model_anchors.h"
#include "actor_transform.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
AnimatedLightState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<AnimatedLightState>(bytes);
}

const AnimatedLightState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<AnimatedLightState>(bytes);
}

namespace parameter = animated_light_parameter;

constexpr u32 kColor = 0x40A6FFFFu; // DAT_002FF980.
constexpr u32 kSecondColor = 0xFF00FFFFu; // DAT_002FF98C.

// The affine 3x4 product the three slots inline, in their summation order.
Vec3 transform_point(const float* m, const Vec3& p)
{
    return Vec3{((m[2] * p.z + m[1] * p.y) + m[0] * p.x) + m[3], ((m[6] * p.z + m[4] * p.x) + m[5] * p.y) + m[7],
        ((m[10] * p.z + m[8] * p.x) + m[9] * p.y) + m[11]};
}

// The binding's position (+0x10, +0x20, +0x30) through its joint (+2, when not negative) on the model's selected pose
// (0x00031530 over +0x168, else +0x164), then through the actor transform at +0x2AC.
Vec3 binding_position(const RuntimeAnimatedLight& context)
{
    const u8* binding = context.binding;
    Vec3 local{};
    std::memcpy(&local.x, binding + 0x10, 4);
    std::memcpy(&local.y, binding + 0x20, 4);
    std::memcpy(&local.z, binding + 0x30, 4);
    i16 joint;
    std::memcpy(&joint, binding + 2, 2);
    if (joint >= 0)
    {
        const RuntimeModelInstance* model = context.actor.model == nullptr ? nullptr : context.actor.model->runtime;
        const ModelPoseBuffer* pose = model == nullptr ? nullptr
            : model->alternate != nullptr ? model->alternate : model->primary;
        if (pose == nullptr)
        {
            throw Error("0x00031530 reads a joint of an animated light without a model pose");
        }
        local = transform_point(model_pose_matrix(*pose, joint).data(), local); // 0x00031530
    }
    return transform_point(context.actor.xform, local); // Actor +0x2AC.
}

void write_light(RuntimeAnimatedLight& context, const Vec3& position)
{
    const i32 index = fields_of(context.bytes).light;
    if (index < 0)
    {
        return;
    }
    auto& point = context.lights.points.at(size_t(index));
    point.position = position;
    point.unk_14 = 0.9f; // DAT_0032B174.
    point.unk_18 = 0.9f;
    point.color = kColor;
    point.unk_1C = std::bit_cast<float>(0x428B6666u); // DAT_0032B614.
    point.second_color = kSecondColor;
    point.unk_20 = std::bit_cast<float>(0x429F3333u); // DAT_0032B610.
}
}

// Xbox 0x0006C240
void animated_light_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(AnimatedLightState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0006C2B0
bool initialize_runtime_animated_light(RuntimeAnimatedLight& context, std::span<const u8> parameters)
{
    context.binding = find_actor_model_binding(context.actor, 0); // 0x000408D0, EDI 0
    const Vec3 position = context.binding == nullptr ? actor_world_position(context.actor) // 0x000412A0
                                                     : binding_position(context);
    fields_of(context.bytes).light = add_runtime_scene_point_light(position, kColor, kSecondColor, 0.9f, 0.9f,
        std::bit_cast<float>(0x428B6666u), std::bit_cast<float>(0x429F3333u), 1, context.lights); // 0x00123720
    const auto name = get(parameters, parameter::glow);
    u32 prefix;
    std::memcpy(&prefix, name.data(), 4);
    context.glow = {};
    if (prefix == 0x5F646961)
    {
        context.glow = acquire_animation_resource(context.registry, name.data()); // 0x00035E90
        if (context.glow.data() == nullptr)
        {
            resource_error(context.error, 2); // 0x00032700
        }
    }
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float start = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr)
    {
        MinaPlayer* player = controller->slots.empty() ? nullptr : controller->slots[0].player.get(); // +0x58.
        if (player != nullptr)
        {
            player->position = start; // +0x14.
            player->previous = start; // +0x10C.
        }
        controller->dirty = true; // +0x5C.
    }
    return true;
}

// Xbox 0x0006C540
void destroy_runtime_animated_light(RuntimeAnimatedLight& context)
{
    release_runtime_scene_point_light(fields_of(context.bytes).light, context.lights); // The inlined release.
    fields_of(context.bytes).light = -1;
    release_animation_resource(context.registry, context.glow); // 0x00035C00
    context.glow = {};
}

// Xbox 0x0006C5A0
void update_runtime_animated_light(RuntimeAnimatedLight& context)
{
    if (context.binding == nullptr)
    {
        return;
    }
    write_light(context, binding_position(context));
}

// Xbox 0x0006C7C0
void queue_runtime_animated_light_glow(RuntimeAnimatedLight& context)
{
    if (context.binding == nullptr)
    {
        return;
    }
    const Vec3 position = binding_position(context);
    const std::array<float, 2> size{5.f, 5.f}; // DAT_0032B184.
    queue_billboard(context.billboards, context.glow.data(), kColor, 0, position, size.data(), 0.f); // 0x0005C1A0
}
}
