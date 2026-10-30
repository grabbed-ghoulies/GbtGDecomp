#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>

namespace gbtg
{
// Xbox 0x00052F80
bool actor_model_record_bit(std::span<const u8> cpu, u32 bit)
{
    u32 value = 0;
    const u32 count = read_u32(cpu, 4);
    const u32 table = read_u32(cpu, 0);
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(cpu, table + index * 8) == 1)
        {
            value = read_u32(cpu, table + index * 8 + 4);
            break;
        }
    }
    return ((1u << (bit & 0x1F)) & value) != 0;
}

// Xbox 0x00052FC0
u32 actor_lookup_joint_index(std::span<const u8> cpu, i16 index)
{
    u32 cursor = 0;
    const u32 count = read_u32(cpu, 4);
    if (count != 0)
    {
        const u32 table = read_u32(cpu, 0);
        do
        {
            if (read_u32(cpu, table + cursor * 8) == 9)
            {
                const u32 block = read_u32(cpu, table + cursor * 8 + 4);
                cursor = 0;
                if (block != 0)
                {
                    return (block & 0xFFFFFF00) | cpu[u32(block + i32(index))];
                }
                break;
            }
            ++cursor;
        }
        while (cursor < count);
    }
    return (cursor & 0xFFFFFF00) | 0xFF;
}

namespace
{

// Host matrix view for the original shared DAT_0050D7A0 scratch.
const AnimationMatrix& actor_joint_matrix(const Actor& actor, u16 index)
{
    const RuntimeModelInstance& model = *actor.model->runtime;
    const ModelPoseBuffer& pose = *(model.alternate != nullptr ? model.alternate : model.primary);
    const Joint* joint = nullptr;
    if (model.skeleton_node != nullptr && model.skeleton_node->joints.data() != nullptr)
    {
        joint = model.skeleton_node->joints.data() + index;
    }
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, sizeof(actor.xform));
    return model_joint_world_matrix(pose, index, *joint, world);
}

} // namespace

// Xbox 0x0003AEB0
void actor_transform_joint_point(const Actor& actor, u16 joint, const Vec3& point, Vec3& output)
{
    const auto& matrix = actor_joint_matrix(actor, joint);
    const float x = point.x;
    const float y = point.y;
    const float z = point.z;
    output.x = matrix[2] * z + matrix[1] * y + x * matrix[0] + matrix[3];
    output.y = matrix[6] * z + matrix[4] * x + matrix[5] * y + matrix[7];
    output.z = matrix[10] * z + matrix[8] * x + matrix[9] * y + matrix[11];
}

// Xbox 0x0003AE60
bool actor_marker_world_position(const Actor& actor, i32 index, Vec3& output)
{
    if (index != -1 && actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(actor.model->cpu, i16(index)) & 0xFF;
        if (joint != 0xFF && joint != 0xFFFFFFFF)
        {
            actor_transform_joint_point(actor, u16(joint), actor_transform_origin, output);
            return true;
        }
    }
    return false;
}

// Xbox 0x0003B050
void actor_joint_world_euler(const Actor& actor, u16 joint, Vec3& output)
{
    const auto& matrix = actor_joint_matrix(actor, joint);
    output = actor_matrix_euler(matrix.data());
}

// Xbox 0x0003B160
bool actor_marker_world_euler(const Actor& actor, i16 index, Vec3& output)
{
    if (index != -1 && actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(actor.model->cpu, index) & 0xFF;
        if (joint != 0xFF && joint != 0xFFFFFFFF)
        {
            actor_joint_world_euler(actor, u16(joint), output);
            return true;
        }
    }
    return false;
}

// Xbox 0x00040A60
void actor_binding_world_position(const Actor& actor, const ModelBoneBinding& binding, Vec3& output)
{
    const ModelPoseBuffer* pose;
    if (actor.model == nullptr)
    {
        pose = nullptr;
    }
    else
    {
        pose = actor.model->runtime->alternate;
        if (pose == nullptr)
        {
            pose = actor.model->runtime->primary;
        }
    }
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, sizeof(actor.xform));
    model_bone_world_point(binding, pose, &world, output);
}

// Xbox 0x00040AA0
void actor_binding_world_euler(const Actor& actor, const ModelBoneBinding& binding, Vec3& output)
{
    const ModelPoseBuffer* pose;
    if (actor.model != nullptr)
    {
        pose = actor.model->runtime->alternate;
        if (pose == nullptr)
        {
            pose = actor.model->runtime->primary;
        }
    }
    else
    {
        pose = nullptr;
    }
    AnimationMatrix world;
    AnimationMatrix matrix;
    std::memcpy(world.data(), actor.xform, sizeof(actor.xform));
    model_bone_world_matrix(binding, pose, world, matrix);
    output = actor_matrix_euler(matrix.data());
}

// Xbox 0x000415F0
void actor_transform_point(const Actor& actor, i16 joint, const Vec3& point, Vec3& output)
{
    if (joint == -1)
    {
        const float x = point.x;
        const float y = point.y;
        const float z = point.z;
        output.x = actor.xform[2] * z + actor.xform[1] * y + actor.xform[0] * x + actor.xform[3];
        output.y = actor.xform[6] * z + actor.xform[4] * x + actor.xform[5] * y + actor.xform[7];
        output.z = actor.xform[10] * z + actor.xform[8] * x + actor.xform[9] * y + actor.xform[11];
        return;
    }
    actor_transform_joint_point(actor, u16(joint), point, output);
}

// Xbox 0x0003D510
void actor_attachment_pose(Actor& actor, const ActorAttachmentNode& node, Vec3& position, Vec3& euler)
{
    const i32 mode = i32(read_u32(node.bytes, 0x1C));
    if (mode == 0)
    {
        const i16 joint = i16(read_u16(node.bytes, 0xC));
        if (joint != -1)
        {
            actor_marker_world_position(actor, joint, position);
            if ((node.bytes[0x20] & 4) != 0)
            {
                actor_marker_world_euler(actor, joint, euler);
                return;
            }
            euler = Vec3{0.f, 0.f, 0.f};
            return;
        }
    }
    else if (mode == 1)
    {
        const ModelPoseBuffer* pose;
        if (actor.model != nullptr)
        {
            pose = actor.model->runtime->alternate;
            if (pose == nullptr)
            {
                pose = actor.model->runtime->primary;
            }
        }
        else
        {
            pose = nullptr;
        }
        AnimationMatrix world;
        std::memcpy(world.data(), actor.xform, sizeof(actor.xform));
        model_bone_world_point(*node.binding, pose, &world, position);
        if ((node.bytes[0x20] & 4) != 0)
        {
            actor_binding_world_euler(actor, *node.binding, euler);
            return;
        }
        euler = Vec3{0.f, 0.f, 0.f};
        return;
    }
    position = actor_world_position(actor);
    if ((node.bytes[0x20] & 4) != 0)
    {
        euler = actor_world_euler(actor);
        return;
    }
    euler = Vec3{0.f, 0.f, 0.f};
}

// Xbox 0x0003D5F0.
void actor_initialize_model_size(Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 table = read_u32(cpu, 0);
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            while (read_u32(cpu, table + index * 8) != 5)
            {
                ++index;
                if (count <= index)
                {
                    actor.model_factor = 1.f;
                    return;
                }
            }
            const u32 block = read_u32(cpu, table + index * 8 + 4);
            if (block != 0)
            {
                actor.model_size_z = 0.f;
                actor.model_size_y = read_f32(cpu, block + 0x20);
                actor.model_size_x = read_f32(cpu, block + 0x24);
            }
        }
    }
    actor.model_factor = 1.f;
}

// Xbox 0x0003D9A0.
float actor_model_radius(const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 table = read_u32(cpu, 0);
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            do
            {
                if (read_u32(cpu, table + index * 8) == 5)
                {
                    const u32 block = read_u32(cpu, table + index * 8 + 4);
                    if (block != 0)
                    {
                        const float extent = actor.scale_200 * read_f32(cpu, block + 0x10) * actor.radius_xz;
                        if (0.f <= extent)
                        {
                            return extent;
                        }
                        return 0.f - extent;
                    }
                    break;
                }
                ++index;
            } while (index < count);
        }
    }
    return 0.f;
}

// Xbox 0x0003C680.
void actor_initialize_shape_mode(Actor& actor)
{
    actor.shape_mode = 2;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 table = read_u32(cpu, 0);
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            while (read_u32(cpu, table + index * 8) != 4)
            {
                ++index;
                if (count <= index)
                {
                    return;
                }
            }
            const u32 block = read_u32(cpu, table + index * 8 + 4);
            if (block != 0)
            {
                auto shape = cpu.subspan(block + 8);
                int remaining = i32(read_u32(cpu, block));
                u32 flags = 0;
                if (0 < remaining)
                {
                    do
                    {
                        flags |= read_u32(shape, 4);
                        if (shape[1] == 0)
                        {
                            shape = {};
                        }
                        else
                        {
                            shape = shape.subspan(shape[1]);
                        }
                        --remaining;
                    } while (remaining != 0);
                }
                if ((flags & 4) != 0)
                {
                    actor.model_mode = 4;
                }
            }
        }
    }
}

// Xbox 0x000559F0.
bool model_has_special_physical_shape(std::span<const u8> shapes)
{
    auto shape = shapes.subspan(16);
    int index = 0;
    const int count = i32(read_u32(shapes, 0));
    if (0 < count)
    {
        do
        {
            if (read_u32(shape, 0xC) == 0x1235 && shape[0] < 3)
            {
                return true;
            }
            ++index;
            shape = shape.subspan(read_u32(shape, 8));
        } while (index < count);
    }
    return false;
}

// Xbox 0x00041BE0. Defaults are the eight 0x002FD1C0 records.
void actor_initialize_sound_slots(Actor& actor)
{
    struct SoundDefault
    {
        i32 index;
        u8 category;
        u8 mode;
    };
    constexpr std::array<SoundDefault, 8> defaults
    {
        SoundDefault{-1, 0, 0}, SoundDefault{4, 1, 1},
        SoundDefault{-1, 0, 0}, SoundDefault{-1, 0, 0},
        SoundDefault{-1, 1, 1}, SoundDefault{-1, 0, 0},
        SoundDefault{4, 1, 1}, SoundDefault{-1, 1, 1}
    };
    for (size_t slot = 0; slot < defaults.size(); ++slot)
    {
        ActorSoundSlot& sound = actor.sound_slots[slot];
        const SoundDefault& setting = defaults[slot];
        sound.enabled = 1;
        u16 bone = 0xffff;
        sound.previous_bone = 0xffff;
        sound.category = setting.category;
        sound.mode = setting.mode;
        if (setting.index != -1)
        {
            if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
            {
                const auto cpu = actor.model->cpu;
                const u32 table = read_u32(cpu, 0);
                const u32 count = read_u32(cpu, 4);
                bool found = false;
                u32 index = 0;
                if (count != 0)
                {
                    do
                    {
                        if (read_u32(cpu, table + index * 8) == 9)
                        {
                            const u32 block = read_u32(cpu, table + index * 8 + 4);
                            if (block != 0)
                            {
                                bone = cpu[block + i16(setting.index)];
                                found = true;
                            }
                            break;
                        }
                        ++index;
                    } while (index < count);
                }
                if (!found)
                {
                    bone = 0xff;
                }
                if (bone != 0xff)
                {
                    sound.bone = bone;
                    continue;
                }
            }
            bone = 0xffff;
        }
        sound.bone = bone;
    }
}

// Xbox 0x0003E6D0.
void actor_collect_model_flags(Actor& actor)
{
    std::span<const u8> physical;
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        physical = {};
    }
    else
    {
        const auto candidate = actor.model->physics_shapes;
        if (candidate.data() == nullptr)
        {
            physical = {};
        }
        else
        {
            physical = model_has_special_physical_shape(candidate) ? candidate : std::span<const u8>{};
        }
    }
    std::span<const u8> graphical;
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        graphical = {};
    }
    else
    {
        const auto cpu = actor.model->cpu;
        const u32 table = read_u32(cpu, 0);
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            do
            {
                if (read_u32(cpu, table + index * 8) == 4)
                {
                    const u32 block = read_u32(cpu, table + index * 8 + 4);
                    if (block != 0)
                    {
                        graphical = cpu.subspan(block);
                    }
                    break;
                }
                ++index;
            } while (index < count);
        }
    }
    if (physical.data() != nullptr)
    {
        int remaining = i32(read_u32(physical, 0));
        u32 flags = 0;
        auto shape = physical.subspan(16);
        if (0 < remaining)
        {
            do
            {
                flags |= read_u32(shape, 4);
                shape = shape.subspan(read_u32(shape, 8));
                --remaining;
            } while (remaining != 0);
        }
        actor.model_flags |= flags;
    }
    if (graphical.data() != nullptr)
    {
        int remaining = i32(read_u32(graphical, 0));
        u32 flags = 0;
        auto shape = graphical.subspan(8);
        if (0 < remaining)
        {
            do
            {
                flags |= read_u32(shape, 4);
                if (shape[1] == 0)
                {
                    shape = {};
                }
                else
                {
                    shape = shape.subspan(shape[1]);
                }
                --remaining;
            } while (remaining != 0);
        }
        actor.model_flags |= flags;
    }
}

// Xbox 0x00105AA0.
void normalize_euler_angles(Vec3& output, const Vec3& input)
{
    const float circle = std::bit_cast<float>(0x40c90fdbu);
    const float positive = std::bit_cast<float>(0x40490fdbu);
    const float negative = std::bit_cast<float>(0xc0490fdbu);
    long double remainder = animation_remainder(input.x, circle);
    float value = float(remainder);
    if (remainder < static_cast<long double>(negative))
    {
        value = value + circle;
    }
    if (positive <= value)
    {
        value = value - circle;
    }
    output.x = value;
    remainder = animation_remainder(input.y, circle);
    value = float(remainder);
    if (remainder < static_cast<long double>(negative))
    {
        value = value + circle;
    }
    if (positive <= value)
    {
        value = value - circle;
    }
    output.y = value;
    remainder = animation_remainder(input.z, circle);
    value = float(remainder);
    if (remainder < static_cast<long double>(negative))
    {
        value = value + circle;
    }
    if (positive <= value)
    {
        value = value - circle;
    }
    output.z = value;
}

// Xbox 0x0006D4F0.
void button_bat_initialize(ButtonBatScreen& bat, ButtonBatPoint initial, float duration,
    ActorAnimationState& animation, AnimationResourceRegistry& registry,
    ResourceErrorState& error, u32& random_state)
{
    bat.from = initial;
    bat.current = initial;
    bat.target = initial;
    bat.flight_duration = duration;
    if (animation.table != nullptr)
    {
        const AnimationRecord* record = find_actor_animation(animation.table, "stand", nullptr, random_state);
        if (record != nullptr)
        {
            start_actor_animation(animation, *record, registry, error, random_state);
        }
    }
    bat.idle_timer = 4.5f;
    bat.current = bat.target;
    bat.from = bat.target;
    bat.state = 1;
}

// Xbox 0x0006D8D0.
void button_bat_set_target(ButtonBatScreen& bat, int state, ActorAnimationState& animation,
    AnimationResourceRegistry& registry, ResourceErrorState& error, u32& random_state)
{
    switch (state)
    {
    case 1:
        if (animation.table != nullptr)
        {
            const AnimationRecord* record = find_actor_animation(animation.table, "stand", nullptr, random_state);
            if (record != nullptr)
            {
                start_actor_animation(animation, *record, registry, error, random_state);
            }
        }
        bat.idle_timer = 4.5f;
        bat.current = bat.target;
        bat.from = bat.target;
        bat.state = state;
        return;
    case 2:
        if (animation.table != nullptr)
        {
            const AnimationRecord* record = find_actor_animation(animation.table, "flybetween", nullptr, random_state);
            if (record != nullptr)
            {
                start_actor_animation(animation, *record, registry, error, random_state);
            }
        }
        if (animation.controller != nullptr)
        {
            retime_mina_playback(animation.controller->slots[0].player.get(), bat.flight_duration,
                                   animation.controller->dirty);
        }
        if (bat.state == 2)
        {
            set_actor_animation_position(animation, 0, 0.2f);
            bat.state = state;
            return;
        }
        break;
    case 3:
        if (animation.table != nullptr)
        {
            const AnimationRecord* record = find_actor_animation(animation.table, "hop", nullptr, random_state);
            if (record != nullptr)
            {
                start_actor_animation(animation, *record, registry, error, random_state);
            }
        }
        break;
    case 4:
        if (animation.table != nullptr)
        {
            const AnimationRecord* record = find_actor_animation(animation.table, "doh", nullptr, random_state);
            if (record != nullptr)
            {
                start_actor_animation(animation, *record, registry, error, random_state);
            }
        }
        break;
    default:
        break;
    }
    bat.state = state;
}

// Xbox 0x00042360. The timer is passed in EAX and the actor in ECX;
// freeze/dt/scale are the original global clock values.
// Xbox 0x00042360
bool actor_tick_timer(const Actor& actor, float& timer, float freeze, float dt, float scene_scale)
{
    float step;
    if (freeze <= 0.0f)
    {
        step = dt * scene_scale;
    }
    else
    {
        step = 0.0f;
    }
    float rate = 0.0f;
    if (actor.animation_pause <= 0.0f)
    {
        rate = actor.animation_rate;
    }
    if (timer > 0.0f)
    {
        step = timer - rate * step;
        timer = step;
        if (step <= 0.0f)
        {
            timer = 0.0f;
            return true;
        }
    }
    return false;
}

// Xbox 0x0006DA00.
void button_bat_tick(ButtonBatScreen& bat, ButtonBatPoint target, float scale,
    ActorAnimationState& animation, AnimationResourceRegistry& registry,
    ResourceErrorState& error, u32& random_state)
{
    bat.from = bat.current;
    if (animation.table != nullptr)
    {
        const AnimationRecord* record = find_actor_animation(animation.table, "flybetween", nullptr, random_state);
        if (record != nullptr)
        {
            start_actor_animation(animation, *record, registry, error, random_state);
        }
    }
    if (animation.controller != nullptr)
    {
        retime_mina_playback(animation.controller->slots[0].player.get(), bat.flight_duration,
                               animation.controller->dirty);
    }
    if (bat.state == 2)
    {
        set_actor_animation_position(animation, 0, 0.2f);
    }
    bat.state = 2;
    bat.target = target;
    const float horizontal = scale * 2.f;
    bat.target.y = bat.target.y - scale * 21.f;
    bat.target.x = bat.target.x - horizontal;
}

} // namespace gbtg
