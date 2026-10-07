// Xbox 0x0002D6E0.
long double mix_auxiliary_linear(bool, bool, float first, float second, float fraction)
{
    return static_cast<long double>(second) * fraction +
           (1.L - static_cast<long double>(fraction)) * first;
}

// Xbox 0x0002D700.
long double mix_auxiliary_additive(bool first_present, bool second_present, float first,
                                   float second, float fraction)
{
    if (!first_present)
    {
        if (second_present)
        {
            return second;
        }
        return 0.L;
    }
    if (second_present)
    {
        return static_cast<long double>(second) * fraction + first;
    }
    return first;
}

// Xbox 0x0002D740.
long double mix_auxiliary_masked(bool first_present, bool second_present, float first,
                                 float second, float fraction)
{
    if (!first_present)
    {
        if (second_present)
        {
            return second;
        }
        return 0.L;
    }
    if (second_present)
    {
        return static_cast<long double>(second) * fraction +
               (1.L - static_cast<long double>(fraction)) * first;
    }
    return first;
}

// Xbox 0x00031950.
void mix_animation_auxiliary(AnimationPose& output, const MinaPlayback& first_player,
                              const AnimationPose& first, const MinaPlayback& second_player,
                              const AnimationPose& second, float fraction,
                              AnimationAuxiliaryCallback callback)
{
    const u32 first_directory = read_u32(first_player.mina, 0x20);
    const u32 second_directory = read_u32(second_player.mina, 0x20);
    if (first_directory == 0 && second_directory == 0)
    {
        for (size_t channel = 0; channel < 48; ++channel)
        {
            output.auxiliary[channel] = 0.f;
        }
        return;
    }
    for (size_t channel = 0; channel < 48; ++channel)
    {
        bool first_present;
        if (first_directory == 0)
        {
            first_present = false;
        }
        else
        {
            first_present = read_u32(first_player.mina, first_directory + channel * 4) != 0;
        }
        bool second_present = false;
        if (second_directory != 0)
        {
            second_present = read_u32(second_player.mina, second_directory + channel * 4) != 0;
        }
        output.auxiliary[channel] = float(callback(first_present, second_present,
                                                  first.auxiliary[channel], second.auxiliary[channel], fraction));
    }
}

// Xbox 0x00031FC0.
void mix_animation_linear(AnimationPose& output, AnimationRoot* output_root,
                           const MinaPlayback& first_player, const AnimationPose& first,
                           const AnimationRoot* first_root, const MinaPlayback& second_player,
                           const AnimationPose& second, const AnimationRoot* second_root,
                           float fraction)
{
    const u16 count = read_u16(first_player.mina, 0x14);
    blend_animation_joints(std::span(output.joints).first(count), std::span(first.joints).first(count),
                           std::span(second.joints).first(count), fraction);
    if (output_root != nullptr)
    {
        blend_animation_root(*output_root, first_root, second_root, fraction);
    }
    mix_animation_auxiliary(output, first_player, first, second_player, second, fraction,
                             mix_auxiliary_linear);
}
// Xbox 0x00030740. Typed storage replaces the single zeroed heap allocation.
std::unique_ptr<AnimationController> make_animation_controller(u32 slot_count)
{
    auto controller = std::make_unique<AnimationController>();
    controller->slots.resize(slot_count);
    controller->combined_player.rate_factor = 0.f;
    controller->combined_player.weight = 0.f;
    controller->selected_root = &controller->combined_root;
    controller->slot_count = u16(slot_count);
    controller->dirty = true;
    controller->combined_root.translation.z = 0.f;
    controller->combined_root.translation.y = 0.f;
    controller->combined_root.translation.x = 0.f;
    controller->combined_root.rotation = Quat{0.f, 0.f, 0.f, 1.f};
    return controller;
}
namespace
{

// Flag consumption in Xbox 0x0002CFB0 / 0x0002D1A0.
u32 next_joint_flags(std::span<const u8> flags, size_t& cursor)
{
    const u8 rotation = flags[cursor++];
    u8 translation;
    if ((rotation & 2) == 0)
    {
        translation = 0;
    }
    else
    {
        translation = flags[cursor++];
    }
    u8 scale;
    if ((rotation & 1) == 0)
    {
        scale = 0;
    }
    else
    {
        scale = flags[cursor++];
    }
    return u32(rotation) | (u32(translation) << 8) | (u32(scale) << 16);
}

// Xbox 0x00153DC0. Value inputs preserve its alias-safe evaluation order.
Quat animation_quaternion_product(Quat first, Quat second)
{
    return Quat
    {
        (first.x * second.w + second.z * first.y + second.x * first.w) - first.z * second.y,
        (first.z * second.x + second.w * first.y + second.y * first.w) - first.x * second.z,
        (first.x * second.y + first.z * second.w + second.z * first.w) - second.x * first.y,
        ((second.w * first.w - first.x * second.x) - second.y * first.y) - first.z * second.z
    };
}

} // namespace

// Xbox 0x0002D1A0.
void blend_animation_masked(std::span<JointPose> output, std::span<const JointPose> first,
                             std::span<const JointPose> second, std::span<const u8> second_flags,
                             float fraction)
{
    const float remainder = 1.f - fraction;
    size_t cursor = 0;
    if (!first.empty())
    {
        size_t joint = 0;
        do
        {
            const u32 flags = next_joint_flags(second_flags, cursor);
            const JointPose a = first[joint];
            if ((flags & 0xe0e0e0u) == 0)
            {
                output[joint] = a;
            }
            else
            {
                const JointPose b = second[joint];
                JointPose& result = output[joint];
                result.rotation = slerp(a.rotation, b.rotation, fraction);
                result.translation.x = b.translation.x * fraction + a.translation.x * remainder;
                result.translation.y = a.translation.y * remainder + b.translation.y * fraction;
                result.translation.z = a.translation.z * remainder + b.translation.z * fraction;
                result.scale.x = b.scale.x * fraction + a.scale.x * remainder;
                result.scale.y = b.scale.y * fraction + a.scale.y * remainder;
                result.scale.z = b.scale.z * fraction + remainder * a.scale.z;
            }
            ++joint;
        } while (joint != first.size());
    }
}

// Xbox 0x0002CFB0. The assembly's TEST CL/AL distinguishes the two quaternion
// flag bytes; the decompiler aliases their names in its second comparison.
void blend_animation_additive(std::span<JointPose> output, std::span<const JointPose> first,
                               std::span<const JointPose> second, std::span<const u8> first_flags,
                               std::span<const u8> second_flags, float fraction)
{
    constexpr std::array<u32, 6> masks{0x8000, 0x4000, 0x2000, 0x800000, 0x400000, 0x200000};
    size_t first_cursor = 0;
    size_t second_cursor = 0;
    if (!first.empty())
    {
        size_t joint = 0;
        do
        {
            const u32 a_flags = next_joint_flags(first_flags, first_cursor);
            const u32 b_flags = next_joint_flags(second_flags, second_cursor);
            const JointPose a = first[joint];
            const JointPose b = second[joint];
            JointPose& result = output[joint];
            if ((a_flags & 0xe0) == 0)
            {
                result.rotation = b.rotation;
            }
            else if ((b_flags & 0xe0) == 0)
            {
                result.rotation = a.rotation;
            }
            else
            {
                const Quat weighted = slerp(Quat{0.f, 0.f, 0.f, 1.f}, b.rotation, fraction);
                result.rotation = animation_quaternion_product(a.rotation, weighted);
            }
            const std::array<float, 6> a_values
            {
                a.translation.x, a.translation.y, a.translation.z, a.scale.x, a.scale.y, a.scale.z
            };
            const std::array<float, 6> b_values
            {
                b.translation.x, b.translation.y, b.translation.z, b.scale.x, b.scale.y, b.scale.z
            };
            std::array<float, 6> values;
            for (size_t field = 0; field < 6; ++field)
            {
                if ((a_flags & masks[field]) == 0)
                {
                    values[field] = b_values[field];
                }
                else if ((b_flags & masks[field]) == 0)
                {
                    values[field] = a_values[field];
                }
                else
                {
                    values[field] = fraction * b_values[field] + a_values[field];
                }
            }
            result.translation = Vec3{values[0], values[1], values[2]};
            result.scale = Vec3{values[3], values[4], values[5]};
            ++joint;
        } while (joint != first.size());
    }
}
// Xbox 0x00032020.
void mix_animation_additive(AnimationPose& output, AnimationRoot* output_root,
                             const MinaPlayback& first_player, const AnimationPose& first,
                             const AnimationRoot* first_root, const MinaPlayback& second_player,
                             const AnimationPose& second, const AnimationRoot* second_root, float fraction)
{
    const u16 count = read_u16(first_player.mina, 0x14);
    blend_animation_additive(std::span(output.joints).first(count), std::span(first.joints).first(count),
                               std::span(second.joints).first(count), first_player.mina.subspan(0x4c),
                               second_player.mina.subspan(0x4c), fraction);
    if (output_root != nullptr)
    {
        output_root->rotation = slerp(first_root->rotation, second_root->rotation, fraction);
        const float remainder = 1.f - fraction;
        output_root->translation.x = second_root->translation.x * fraction + first_root->translation.x * remainder;
        output_root->translation.y = first_root->translation.y * remainder + second_root->translation.y * fraction;
        output_root->translation.z = first_root->translation.z * remainder + second_root->translation.z * fraction;
    }
    mix_animation_auxiliary(output, first_player, first, second_player, second, fraction,
                             mix_auxiliary_additive);
}

// Xbox 0x00031040. The mode-0x23 skeleton metadata load is an unused value;
// the pose operation consumes the second resource's directory flags.
void mix_controller_poses(int mode, AnimationPose& output, AnimationRoot* output_root,
                           const MinaPlayback& first_player, const AnimationPose& first,
                           const AnimationRoot* first_root, const MinaPlayback& second_player,
                           const AnimationPose& second, const AnimationRoot* second_root, float fraction)
{
    if (mode == 0x21)
    {
        mix_animation_linear(output, output_root, first_player, first, first_root,
                               second_player, second, second_root, fraction);
    }
    else
    {
        if (mode == 0x22)
        {
            mix_animation_additive(output, output_root, first_player, first, first_root,
                                     second_player, second, second_root, fraction);
            return;
        }
        if (mode == 0x23)
        {
            const u16 count = read_u16(first_player.mina, 0x14);
            blend_animation_masked(std::span(output.joints).first(count), std::span(first.joints).first(count),
                                     std::span(second.joints).first(count), second_player.mina.subspan(0x4c), fraction);
            mix_animation_auxiliary(output, first_player, first, second_player, second, fraction,
                                     mix_auxiliary_masked);
            return;
        }
    }
}

// Xbox 0x000310F0.
void combine_animation_controller(AnimationController& controller)
{
    MinaPlayer* first = controller.slots[0].player.get();
    if (first != nullptr)
    {
        float direction = 1.f;
        if (first->speed <= 0.f && first->speed != 0.f)
        {
            direction = -1.f;
        }
        initialize_mina_playback(controller.combined_player, first->mina, 1.f, direction, first->mode, 0);
        u16 count = read_u16(first->mina, 0x14);
        if (1 < controller.slot_count)
        {
            size_t slot = 1;
            int remaining = controller.slot_count - 1;
            do
            {
                const MinaPlayer* player = controller.slots[slot].player.get();
                if (player != nullptr && count < read_u16(player->mina, 0x14))
                {
                    count = read_u16(player->mina, 0x14);
                }
                ++slot;
                --remaining;
            } while (remaining != 0);
        }
        if (controller.joint_capacity < count)
        {
            if (controller.combined_pose == nullptr)
            {
                controller.combined_pose = std::make_unique<AnimationPose>();
            }
            controller.combined_pose->joints.resize(count);
            initialize_animation_pose(*controller.combined_pose);
            controller.joint_capacity = count;
        }
        controller.selected_player = first;
        controller.selected_pose = first->pose.get();
        controller.selected_root = &first->root;
        if (1 < controller.slot_count)
        {
            size_t slot = 1;
            do
            {
                AnimationControllerSlot& record = controller.slots[slot];
                MinaPlayer* player = record.player.get();
                if (player != nullptr)
                {
                    mix_controller_poses(record.blend_mode, *controller.combined_pose, &controller.combined_root,
                                           *controller.selected_player, *controller.selected_pose,
                                           controller.selected_root, *player, *player->pose, &player->root,
                                           record.weight * record.fade_fraction);
                    controller.selected_player = &controller.combined_player;
                    controller.selected_pose = controller.combined_pose.get();
                    controller.selected_root = &controller.combined_root;
                }
                ++slot;
            } while (slot < controller.slot_count);
        }
    }
}
