// Xbox 0x0002F720.
void subtract_skeleton_bind_points(std::span<AnimationMatrix> output,
                                    std::span<const Joint> joints, int count)
{
    int index = 0;
    if (3 < count)
    {
        int groups = ((u32(count) - 4u) >> 2) + 1;
        index = groups * 4;
        size_t first = 0;
        do
        {
            auto& a = output[first];
            const Vec3 ab = joints[first].world;
            a[3] -= a[2] * ab.z + ab.y * a[1] + ab.x * a[0];
            a[7] -= a[5] * ab.y + a[4] * ab.x + a[6] * ab.z;
            a[11] -= a[9] * ab.y + a[8] * ab.x + a[10] * ab.z;
            auto& b = output[first + 1];
            const Vec3 bb = joints[first + 1].world;
            b[3] -= b[1] * bb.y + b[0] * bb.x + b[2] * bb.z;
            b[7] -= b[5] * bb.y + b[4] * bb.x + b[6] * bb.z;
            b[11] -= b[9] * bb.y + b[8] * bb.x + b[10] * bb.z;
            auto& c = output[first + 2];
            const Vec3 cb = joints[first + 2].world;
            c[3] -= c[2] * cb.z + c[1] * cb.y + c[0] * cb.x;
            c[7] -= c[6] * cb.z + c[5] * cb.y + c[4] * cb.x;
            c[11] -= c[10] * cb.z + c[9] * cb.y + c[8] * cb.x;
            auto& d = output[first + 3];
            const Vec3 db = joints[first + 3].world;
            d[3] -= d[2] * db.z + d[1] * db.y + d[0] * db.x;
            d[7] -= d[6] * db.z + d[5] * db.y + d[4] * db.x;
            d[11] -= d[10] * db.z + d[9] * db.y + d[8] * db.x;
            first += 4;
            --groups;
        } while (groups != 0);
    }
    if (index < count)
    {
        int remaining = count - index;
        do
        {
            auto& matrix = output[size_t(index)];
            const Vec3 bind = joints[size_t(index)].world;
            matrix[3] -= matrix[2] * bind.z + matrix[0] * bind.x + matrix[1] * bind.y;
            matrix[7] -= matrix[6] * bind.z + matrix[4] * bind.x + matrix[5] * bind.y;
            matrix[11] -= matrix[10] * bind.z + matrix[8] * bind.x + matrix[9] * bind.y;
            ++index;
            --remaining;
        } while (remaining != 0);
    }
}

// Xbox 0x0002FAC0.
void evaluate_mina_skeleton(const MinaPlayback& player, const AnimationPose& pose,
                             std::span<const Joint> joints, std::span<AnimationMatrix> output,
                             std::span<const SkeletonCallbackRecord> callbacks,
                             void* first_context, void* second_context)
{
    const AnimationMatrix identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const u16 animated_count = read_u16(player.mina, 0x14);
    u16 joint_index = 0;
    size_t callback_index = 0;
    if (animated_count != 0)
    {
        do
        {
            const JointPose& source = pose.joints[joint_index];
            const Quat q = source.rotation;
            AnimationMatrix local;
            if (q.w == 1.f)
            {
                local = identity;
            }
            else
            {
                const float factor = 2.f / (q.z * q.z + q.y * q.y + q.x * q.x + q.w * q.w);
                const float z = q.z * factor;
                const float y = q.y * factor;
                const float xw = q.x * factor * q.w;
                const float xx = q.x * q.x * factor;
                local[0] = 1.f - (q.z * z + q.y * y);
                local[1] = y * q.x - z * q.w;
                local[4] = y * q.x + z * q.w;
                local[8] = z * q.x - y * q.w;
                local[2] = z * q.x + y * q.w;
                local[9] = q.y * z + xw;
                local[6] = q.y * z - xw;
                local[5] = 1.f - (q.z * z + xx);
                local[10] = 1.f - (q.y * y + xx);
            }
            const Joint& joint = joints[joint_index];
            local[3] = joint.local.x + source.translation.x;
            local[7] = joint.local.y + source.translation.y;
            local[11] = joint.local.z + source.translation.z;
            if (source.scale.x != 1.f)
            {
                local[0] *= source.scale.x;
                local[4] *= source.scale.x;
                local[8] *= source.scale.x;
            }
            if (source.scale.y != 1.f)
            {
                local[5] *= source.scale.y;
                local[1] *= source.scale.y;
                local[9] *= source.scale.y;
            }
            if (source.scale.z != 1.f)
            {
                local[2] *= source.scale.z;
                local[6] *= source.scale.z;
                local[10] *= source.scale.z;
            }
            const AnimationMatrix* parent;
            const u16 parent_index = u16(joint.parent);
            if (parent_index == 0xffff)
            {
                parent = &identity;
            }
            else
            {
                const u32 mask = read_u32(player.mina, 0x28);
                if (mask != 0 && ((player.mina[mask + (joint_index >> 3)] >> (joint_index & 7)) & 1) != 0)
                {
                    const Vec3 parent_scale = pose.joints[parent_index].scale;
                    float x = parent_scale.x;
                    float y = parent_scale.y;
                    float z = parent_scale.z;
                    if (x == 0.f)
                    {
                        x = std::bit_cast<float>(0x38d1b717u);
                    }
                    if (y == 0.f)
                    {
                        y = std::bit_cast<float>(0x38d1b717u);
                    }
                    if (z == 0.f)
                    {
                        z = std::bit_cast<float>(0x38d1b717u);
                    }
                    x = 1.f / x;
                    local[0] = x * local[0];
                    local[1] = x * local[1];
                    y = 1.f / y;
                    local[4] = y * local[4];
                    local[5] = y * local[5];
                    z = 1.f / z;
                    local[8] = z * local[8];
                    local[9] = z * local[9];
                    local[10] = z * local[10];
                    local[2] = x * local[2];
                    local[6] = y * local[6];
                }
                parent = &output[parent_index];
            }
            if (callbacks[callback_index].joint == 0xffff || callbacks[callback_index].joint != joint_index)
            {
                fn_1041b0(output[joint_index].data(), parent->data(), local.data());
            }
            else
            {
                callbacks[callback_index].callback(*parent, local, output[joint_index], callback_index,
                                                     first_context, second_context);
                ++callback_index;
            }
            ++joint_index;
        } while (joint_index < animated_count);
    }
    u32 index = joint_index;
    if (int(index) < int(joints.size()))
    {
        do
        {
            const Joint& joint = joints[index];
            AnimationMatrix local = identity;
            local[3] = joint.local.x;
            local[7] = joint.local.y;
            local[11] = joint.local.z;
            const AnimationMatrix* parent;
            if (u16(joint.parent) == 0xffff)
            {
                parent = &identity;
            }
            else
            {
                parent = &output[u16(joint.parent)];
            }
            if (callbacks[callback_index].joint == 0xffff || callbacks[callback_index].joint != joint_index)
            {
                fn_1041b0(output[joint_index].data(), parent->data(), local.data());
            }
            else
            {
                callbacks[callback_index].callback(*parent, local, output[joint_index], callback_index,
                                                     first_context, second_context);
                ++callback_index;
            }
            ++joint_index;
            index = joint_index;
        } while (int(index) < int(joints.size()));
    }
    subtract_skeleton_bind_points(output, joints, int(joints.size()));
}
// Xbox 0x00030100. The assembly retains parent-scale compensation and the
// unanimated child translation stores that Ghidra's C drops as dead locals.
void evaluate_mina_skeleton_plain(const MinaPlayback& player, AnimationPose& pose,
                                   std::span<const Joint> joints,
                                   std::span<AnimationMatrix> output)
{
    const AnimationMatrix identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const u16 animated_count = read_u16(player.mina, 0x14);
    u32 index = 0;
    if (animated_count != 0)
    {
        do
        {
            const JointPose& source = pose.joints[index];
            const Quat q = source.rotation;
            AnimationMatrix local;
            if (q.w == 1.f)
            {
                local = identity;
            }
            else
            {
                const float factor = 2.f / (((q.w * q.w + q.x * q.x) + q.z * q.z) + q.y * q.y);
                const float z = q.z * factor;
                const float y = q.y * factor;
                const float x = q.x * factor;
                const float wx = q.w * x;
                const float wy = q.w * y;
                const float wz = q.w * z;
                const float xx = q.x * x;
                const float xy = q.x * y;
                const float xz = q.x * z;
                const float yy = q.y * y;
                const float yz = q.y * z;
                const float zz = q.z * z;
                local[0] = 1.f - (zz + yy);
                local[1] = xy - wz;
                local[2] = xz + wy;
                local[4] = xy + wz;
                local[5] = 1.f - (zz + xx);
                local[6] = yz - wx;
                local[8] = xz - wy;
                local[9] = yz + wx;
                local[10] = 1.f - (yy + xx);
            }
            const Joint& joint = joints[index];
            local[3] = source.translation.x + joint.local.x;
            local[7] = source.translation.y + joint.local.y;
            local[11] = source.translation.z + joint.local.z;
            if (source.scale.x != 1.f)
            {
                local[0] = source.scale.x * local[0];
                local[4] = source.scale.x * local[4];
                local[8] = source.scale.x * local[8];
            }
            if (source.scale.y != 1.f)
            {
                local[1] = source.scale.y * local[1];
                local[5] = source.scale.y * local[5];
                local[9] = source.scale.y * local[9];
            }
            if (source.scale.z != 1.f)
            {
                local[2] = source.scale.z * local[2];
                local[6] = source.scale.z * local[6];
                local[10] = source.scale.z * local[10];
            }
            const u16 parent = u16(joint.parent);
            if (parent == 0xffff)
            {
                output[index] = local;
            }
            else
            {
                const u32 mask = read_u32(player.mina, 0x28);
                if (mask != 0 && ((player.mina[mask + (index >> 3)] >> (index & 7)) & 1) != 0)
                {
                    Vec3& scale = pose.joints[parent].scale;
                    if (scale.x == 0.f)
                    {
                        scale.x = std::bit_cast<float>(0x38d1b717u);
                    }
                    if (scale.y == 0.f)
                    {
                        scale.y = std::bit_cast<float>(0x38d1b717u);
                    }
                    if (scale.z == 0.f)
                    {
                        scale.z = std::bit_cast<float>(0x38d1b717u);
                    }
                    if (scale.x != 1.f)
                    {
                        const float inverse = 1.f / scale.x;
                        local[0] = inverse * local[0];
                        local[1] = inverse * local[1];
                        local[2] = inverse * local[2];
                    }
                    if (scale.y != 1.f)
                    {
                        const float inverse = 1.f / scale.y;
                        local[4] = inverse * local[4];
                        local[5] = inverse * local[5];
                        local[6] = inverse * local[6];
                    }
                    if (scale.z != 1.f)
                    {
                        const float inverse = 1.f / scale.z;
                        local[8] = inverse * local[8];
                        local[9] = inverse * local[9];
                        local[10] = inverse * local[10];
                    }
                }
                fn_1041b0(output[index].data(), output[parent].data(), local.data());
            }
            ++index;
        } while (index < animated_count);
    }
    while (int(index) < int(joints.size()))
    {
        const Joint& joint = joints[index];
        AnimationMatrix local = identity;
        local[3] = joint.local.x;
        local[7] = joint.local.y;
        local[11] = joint.local.z;
        if (u16(joint.parent) == 0xffff)
        {
            output[index] = local;
        }
        else
        {
            fn_1041b0(output[index].data(), output[u16(joint.parent)].data(), local.data());
        }
        index = (index + 1) & 0xffffu;
    }
    subtract_skeleton_bind_points(output, joints, int(joints.size()));
}

// Xbox 0x00156A20 / 0x000F9760.
int model_animation_joint_count(const ModelAnimationState& model)
{
    if (model.skeleton.data() != nullptr)
    {
        return model.joint_count;
    }
    return 0;
}

// Xbox 0x000F96F0.
void set_model_node_animation(ModelAnimationState& model, u8 node, u8 value,
                                ModelNodeFlagCache& cache)
{
    const u32 bit = u32(node) * 2;
    const u32 shift = bit & 7;
    if (cache[node] != u32(value))
    {
        cache[node] = value;
    }
    model.node_flags[bit >> 3] = u8((~(3u << shift) & model.node_flags[bit >> 3]) |
                                    (u32(value) << shift));
}

// Xbox 0x000320F0. The mask is looked up on each of the sixteen iterations.
void apply_model_node_animation(ModelAnimationState& model, u32 values, ModelNodeFlagCache& cache)
{
    u32 node = 0;
    do
    {
        u32 mask = 0;
        size_t block = 0;
        if (model.resource_blocks.size() != 0)
        {
            do
            {
                if (model.resource_blocks[block].type == 1)
                {
                    mask = model.resource_blocks[block].value;
                    break;
                }
                ++block;
            } while (block < model.resource_blocks.size());
        }
        if ((mask & (1u << (node & 0x1f))) != 0)
        {
            set_model_node_animation(model, u8(node), u8((values >> (node * 2)) & 3), cache);
        }
        ++node;
    } while (node < 16);
}

// Xbox 0x00030FF0.
void dispatch_mina_skeleton(const SkeletonDispatchContext* context, const MinaPlayback& player,
    AnimationPose& pose, std::span<const Joint> joints, std::span<AnimationMatrix> output,
    void* second_context)
{
    if (context != nullptr)
    {
        const SkeletonCallbackRecord* callbacks = nullptr;
        void* first_context = nullptr;
        if (context->enabled)
        {
            callbacks = context->callbacks.data();
            first_context = context->first_context;
        }
        if (callbacks != nullptr)
        {
            evaluate_mina_skeleton(player, pose, joints, output, context->callbacks,
                                     first_context, second_context);
            return;
        }
    }
    evaluate_mina_skeleton_plain(player, pose, joints, output);
}

// Xbox 0x00030C70.
void apply_controller_to_model(AnimationController& controller, ModelAnimationState& model,
    bool skip_pose, const SkeletonDispatchContext* context, ModelNodeFlagCache& cache,
    const AnimationMatrix& reset_matrix)
{
    if (!skip_pose && !controller.idle)
    {
        if (controller.slots[0].player == nullptr)
        {
            controller.combined_root.translation = Vec3{};
            controller.combined_root.rotation = Quat{0.f, 0.f, 0.f, 1.f};
            reset_model_pose(model, reset_matrix);
            clear_model_auxiliary(model);
        }
        else
        {
            ModelPoseBuffer* selected = model.alternate;
            if (selected == nullptr)
            {
                selected = model.primary;
            }
            if (selected != nullptr)
            {
                const int count = model_animation_joint_count(model);
                const u32 step = std::bit_cast<u32>(controller.slots[0].player->step);
                dispatch_mina_skeleton(context, *controller.selected_player, *controller.selected_pose,
                    model.skeleton.first(size_t(count)), selected->matrices,
                    reinterpret_cast<void*>(std::uintptr_t(step)));
                selected->dirty = true;
            }
            write_model_auxiliary(model, std::span<const float, 48>(
                                    controller.selected_pose->auxiliary.data(), 48));
            const auto resource = controller.selected_player->mina;
            const u32 directory = read_u32(resource, 0x20);
            if (directory != 0 && read_u32(resource, directory + 0xc0) != 0)
            {
                apply_model_node_animation(model,
                    std::bit_cast<u32>(controller.selected_pose->auxiliary[48]), cache);
                controller.dirty = false;
                return;
            }
        }
        controller.dirty = false;
    }
}

// Xbox 0x000314C0.
void initialize_model_pose(ModelPoseBuffer& output)
{
    output.dirty = true;
    int index = 0;
    if (0 < output.count)
    {
        do
        {
            output.matrices[size_t(index)] = AnimationMatrix
            {
                1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f
            };
            ++index;
        } while (index < output.count);
    }
}

// Xbox 0x00030F90.
void reset_model_pose(ModelAnimationState& output, const AnimationMatrix& matrix)
{
    ModelPoseBuffer* selected = output.alternate;
    if (selected == nullptr)
    {
        selected = output.primary;
    }
    int index = 0;
    if (0 < output.joint_count)
    {
        do
        {
            selected->matrices[size_t(index)] = matrix;
            ++index;
        } while (index < output.joint_count);
    }
    selected->dirty = true;
}

// Xbox 0x0002CD80.
void write_model_auxiliary(ModelAnimationState& output, std::span<const float, 48> values)
{
    size_t index = 0;
    int groups = 6;
    do
    {
        if (output.auxiliary_targets[index + 0] != nullptr)
        {
            *output.auxiliary_targets[index + 0] = values[index + 0];
        }
        if (output.auxiliary_targets[index + 1] != nullptr)
        {
            *output.auxiliary_targets[index + 1] = values[index + 1];
        }
        if (output.auxiliary_targets[index + 2] != nullptr)
        {
            *output.auxiliary_targets[index + 2] = values[index + 2];
        }
        if (output.auxiliary_targets[index + 3] != nullptr)
        {
            *output.auxiliary_targets[index + 3] = values[index + 3];
        }
        if (output.auxiliary_targets[index + 4] != nullptr)
        {
            *output.auxiliary_targets[index + 4] = values[index + 4];
        }
        if (output.auxiliary_targets[index + 5] != nullptr)
        {
            *output.auxiliary_targets[index + 5] = values[index + 5];
        }
        if (output.auxiliary_targets[index + 6] != nullptr)
        {
            *output.auxiliary_targets[index + 6] = values[index + 6];
        }
        if (output.auxiliary_targets[index + 7] != nullptr)
        {
            *output.auxiliary_targets[index + 7] = values[index + 7];
        }
        index += 8;
        --groups;
    } while (groups != 0);
}

// Xbox 0x0002CE00.
void clear_model_auxiliary(ModelAnimationState& output)
{
    size_t index = 0;
    int groups = 6;
    do
    {
        if (output.auxiliary_targets[index + 0] != nullptr)
        {
            *output.auxiliary_targets[index + 0] = 0.f;
        }
        if (output.auxiliary_targets[index + 1] != nullptr)
        {
            *output.auxiliary_targets[index + 1] = 0.f;
        }
        if (output.auxiliary_targets[index + 2] != nullptr)
        {
            *output.auxiliary_targets[index + 2] = 0.f;
        }
        if (output.auxiliary_targets[index + 3] != nullptr)
        {
            *output.auxiliary_targets[index + 3] = 0.f;
        }
        if (output.auxiliary_targets[index + 4] != nullptr)
        {
            *output.auxiliary_targets[index + 4] = 0.f;
        }
        if (output.auxiliary_targets[index + 5] != nullptr)
        {
            *output.auxiliary_targets[index + 5] = 0.f;
        }
        if (output.auxiliary_targets[index + 6] != nullptr)
        {
            *output.auxiliary_targets[index + 6] = 0.f;
        }
        if (output.auxiliary_targets[index + 7] != nullptr)
        {
            *output.auxiliary_targets[index + 7] = 0.f;
        }
        index += 8;
        --groups;
    } while (groups != 0);
}
