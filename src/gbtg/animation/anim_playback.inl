namespace
{

// Xbox 0x00031D00.
void sample_playback_root(const MinaPlayback& player, float previous, u32 cycles,
                          AnimationRoot& output, AnimationRoot* cached, bool sampled)
{
    const i32 clamp = 0;
    JointPose current;
    sample_mina_pose(&current, player, nullptr, &clamp, true, player.weight); // 0x0002E8A0 into DAT_004DD7C8
    Vec3 displacement{};
    if (u16(cycles) != 0)
    {
        displacement = mina_root_cycle_translation(player); // 0x0002E750
    }
    output.rotation = current.rotation;
    const float count = float(u16(cycles));
    if (player.rate_factor >= 0.f)
    {
        output.translation.x = count * displacement.x + current.translation.x;
        output.translation.y = count * displacement.y + current.translation.y;
        output.translation.z = count * displacement.z + current.translation.z;
    }
    else
    {
        output.translation.x = current.translation.x - count * displacement.x;
        output.translation.y = current.translation.y - count * displacement.y;
        output.translation.z = current.translation.z - count * displacement.z;
    }
    if (cached != nullptr && sampled)
    {
        output.translation.x -= cached->translation.x;
        output.translation.y -= cached->translation.y;
        output.translation.z -= cached->translation.z;
        cached->translation = current.translation;
        return;
    }
    const float at = float(i32(read_u16(player.mina, 0x1A)) - 1) * previous;
    JointPose old;
    sample_mina_pose(&old, player, &at, &clamp, true, player.weight); // 0x0002E8A0

    if (cached == nullptr)
    {
        output.translation.x -= old.translation.x;
        output.translation.y -= old.translation.y;
        output.translation.z -= old.translation.z;
        return;
    }
    output.translation.x -= old.translation.x;
    if (player.position < previous)
    {
        output.translation.z -= old.translation.z;
        output.translation.y -= old.translation.y;
        cached->translation = Vec3{};
        return;
    }
    output.translation.y -= old.translation.y;
    output.translation.z -= old.translation.z;
    cached->translation = old.translation;
}

} // namespace

// Xbox 0x000312C0.
void blend_animation_root(AnimationRoot& output, const AnimationRoot* first,
                          const AnimationRoot* second, float fraction)
{
    const float remainder = 1.f - fraction;
    const Quat a = first == nullptr ? Quat{0.f, 0.f, 0.f, 1.f} : first->rotation;
    const Quat b = second == nullptr ? Quat{0.f, 0.f, 0.f, 1.f} : second->rotation;
    output.rotation = slerp(a, b, fraction);
    if (first != nullptr)
    {
        if (second != nullptr)
        {
            output.translation.x = second->translation.x * fraction + remainder * first->translation.x;
            output.translation.y = first->translation.y * remainder + second->translation.y * fraction;
            output.translation.z = first->translation.z * remainder + second->translation.z * fraction;
            return;
        }
        output.translation.x = first->translation.x * remainder;
        output.translation.y = first->translation.y * remainder;
        output.translation.z = first->translation.z * remainder;
        return;
    }
    if (second != nullptr)
    {
        output.translation.x = second->translation.x * fraction;
        output.translation.y = second->translation.y * fraction;
        output.translation.z = second->translation.z * fraction;
    }
}

// Xbox 0x0002CE70.
void blend_animation_joints(std::span<JointPose> output, std::span<const JointPose> first,
                            std::span<const JointPose> second, float fraction)
{
    const float remainder = 1.f - fraction;
    for (size_t i = 0; i < first.size(); ++i)
    {
        const JointPose a = first[i];
        const JointPose b = second[i];
        JointPose& result = output[i];
        result.rotation = slerp(a.rotation, b.rotation, fraction);
        result.scale.x = b.scale.x * fraction + a.scale.x * remainder;
        result.scale.y = a.scale.y * remainder + b.scale.y * fraction;
        result.scale.z = a.scale.z * remainder + b.scale.z * fraction;
        result.translation.x = b.translation.x * fraction + a.translation.x * remainder;
        result.translation.y = b.translation.y * fraction + a.translation.y * remainder;
        result.translation.z = b.translation.z * fraction + a.translation.z * remainder;
    }
}

// Xbox 0x0002D450.
void blend_animation_pose(AnimationPose& output, const AnimationPose& first,
                          const AnimationPose& second, float fraction)
{
    const float remainder = 1.f - fraction;
    blend_animation_joints(output.joints, first.joints, second.joints, fraction);
    for (size_t group = 0; group < 6; ++group)
    {
        const size_t index = group * 8;
        output.auxiliary[index] = second.auxiliary[index] * fraction + first.auxiliary[index] * remainder;
        output.auxiliary[index + 1] = first.auxiliary[index + 1] * remainder + second.auxiliary[index + 1] * fraction;
        output.auxiliary[index + 2] = first.auxiliary[index + 2] * remainder + second.auxiliary[index + 2] * fraction;
        for (size_t field = 3; field < 8; ++field)
        {
            output.auxiliary[index + field] = second.auxiliary[index + field] * fraction + first.auxiliary[index + field] * remainder;
        }
    }
}

// Xbox 0x00031760.
void sample_mina_auxiliary(const MinaPlayback& player, std::array<float, 64>& output)
{
    const u32 directory = read_u32(player.mina, 0x20);
    for (size_t channel = 0; channel < 48; ++channel)
    {
        if (directory == 0 || read_u32(player.mina, directory + channel * 4) == 0)
        {
            output[channel] = 0.f;
        }
        else
        {
            const u32 samples = read_u32(player.mina, directory + channel * 4);
            u32 count = read_u32(player.mina, 0x24);
            u32 before;
            u32 current;
            u32 after;
            u32 following;
            if (player.mode != 0)
            {
                --count;
                const u32 frame = u32(int(float(count) * player.position));
                current = frame & 0xffff;
                before = frame - (i16(frame) != 0);
                after = frame + (current != count);
                following = after + ((after & 0xffff) != count);
            }
            else
            {
                const u32 frame = u32(int(float(count) * player.position));
                current = (frame & 0xffff) % count;
                after = (current + 1) % count;
                following = (current + 2) % count;
                before = u32(int((current - 1) + count) % int(count));
            }
            const float a = read_f32(player.mina, samples + (before & 0xffff) * 4);
            const float b = read_f32(player.mina, samples + current * 4);
            const float c = read_f32(player.mina, samples + (after & 0xffff) * 4);
            const float d = read_f32(player.mina, samples + (following & 0xffff) * 4);
            const float t = float(count) * player.position - float(current);
            const float difference = c - b;
            output[channel] = (((difference * 2.f + a - (d + b) * 0.5f) +
                                (((d - a) - difference) * 0.5f - difference) * t) * t +
                               (c - a) * 0.5f) * t + b;
        }
    }
    if (directory != 0 && read_u32(player.mina, directory + 0xC0) != 0)
    {
        const u32 samples = read_u32(player.mina, directory + 0xC0);
        const int frame = int(float(read_u32(player.mina, 0x24) - 1) * player.position);
        output[48] = read_f32(player.mina, samples + frame * 4);
        return;
    }
    output[48] = 0.f;
}

// Xbox 0x0002D370.
void initialize_animation_pose(AnimationPose& pose)
{
    for (JointPose& joint : pose.joints)
    {
        joint.rotation = Quat{0.f, 0.f, 0.f, 1.f};
        joint.scale = Vec3{1.f, 1.f, 1.f};
        joint.translation = Vec3{};
    }
    pose.auxiliary.fill(0.f);
}

// Xbox 0x0002D780.
std::unique_ptr<MinaPlayer> make_mina_player()
{
    auto player = std::make_unique<MinaPlayer>();
    player->rate_factor = 0.f;
    player->weight = 0.f;
    player->reference_root.rotation = Quat{0.f, 0.f, 0.f, 0.f};
    return player;
}

// Xbox 0x00031A30.
void initialize_mina_playback(MinaPlayback& player, std::span<const u8> mina,
                              float position, float rate_factor, u8 mode, u8 sampling_mode)
{
    player.mina = mina;
    player.duration = read_f32(mina, 8);
    player.weight = 1.f;
    player.speed = 0.f;
    player.sampling_mode = sampling_mode;
    player.mode = mode;
    player.rate_factor = rate_factor;
    player.speed = animation_absolute(rate_factor / player.duration);
    player.position = position;
}

// Xbox 0x00031AC0.
void initialize_mina_playback(MinaPlayback& player, std::span<const u8> mina,
                              float position, u8 mode, u8 sampling_mode)
{
    player.mina = mina;
    player.duration = read_f32(mina, 8);
    player.weight = 1.f;
    player.speed = 0.f;
    player.sampling_mode = sampling_mode;
    player.mode = mode;
    player.rate_factor = 1.f;
    player.speed = animation_absolute(1.f / player.duration);
    player.position = position;
}

// Xbox 0x0002D8E0.
bool start_mina_player(MinaPlayer& player, std::span<const u8> mina, float position,
                       float duration, u8 mode, i16 direction, const char* name)
{
    if (player.mina.data() == nullptr || player.blend_duration <= 0.f)
    {
        player.blend_duration = 0.f;
        player.blend_remaining = 0.f;
    }
    else
    {
        if (player.sampled)
        {
            if (player.old_pose != nullptr)
            {
                player.old_pose.reset();
            }
            player.old_pose = std::move(player.pose);
        }
        player.blend_remaining = player.blend_duration;
    }
    if (player.pose != nullptr)
    {
        player.pose.reset();
    }
    if (player.mina.data() != nullptr && player.owned_resource)
    {
        release_animation_resource(*player.resource_registry, player.mina);
        player.mina = {};
    }
    const char* resource_name = name;
    if (name == nullptr)
    {
        resource_name = "";
    }
    player.resource_name = resource_name;
    player.owned_resource = name != nullptr;
    initialize_mina_playback(player, mina, position, mode, 0);
    if (duration <= 0.f)
    {
        duration = read_f32(mina, 8);
    }
    const float reciprocal = 1.f / duration;
    player.duration = duration;
    if (0.f < player.speed || player.speed == 0.f)
    {
        player.speed = animation_absolute(reciprocal);
    }
    else
    {
        player.speed = -animation_absolute(reciprocal);
    }
    if (direction == 0)
    {
        player.speed = animation_absolute(player.speed);
    }
    else if (direction == 1)
    {
        player.speed = -animation_absolute(player.speed);
    }
    else if (direction == 2)
    {
        player.speed = 0.f - player.speed;
    }
    player.pose = std::make_unique<AnimationPose>();
    player.pose->joints.resize(read_u16(mina, 0x14));
    initialize_animation_pose(*player.pose);
    player.previous = position;
    player.ticks = 0;
    player.age = 0.f;
    player.sampled = false;
    player.cached_root = AnimationRoot{};
    player.reference_root = AnimationRoot{};
    if (read_u16(mina, 0x1A) != 0)
    {
        const float saved = player.position;
        player.position = 1.f;
        sample_playback_root(player, 0.f, 0, player.reference_root, nullptr, false); // ESI = player +0xF0.
        player.position = saved;
    }
    return true;
}

// Xbox 0x0002DB50. The assembly rechecks the loaded pointer after the fatal
// routine returns on its suppression branch.
bool start_named_mina_player(MinaPlayer& player, AnimationResourceRegistry& registry,
    const char* name, float position, float duration, u8 mode, i16 direction,
    ResourceErrorState& error)
{
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
        }
        if (resource.data() != nullptr)
        {
            return start_mina_player(player, resource, position, duration, mode, direction, name);
        }
    }
    return false;
}

// Xbox 0x00031B50.
void advance_mina_playback(MinaPlayback& player, float dt, AnimationRoot* root,
                           AnimationRoot* cached_root, bool sampled)
{
    const float previous = player.position;
    const float position = player.speed * dt + player.position;
    player.cycles = 0;
    player.position = position;
    u32 cycles = 0;
    if (position < 1.f)
    {
        if (position < 0.f)
        {
            const float shifted = position - 1.f;
            if (player.mode == 0)
            {
                player.cycles = i16(int(shifted));
                cycles = u32(-int(shifted));
                player.position = float(cycles & 0xffff) + position;
            }
            else if (player.mode == 1)
            {
                player.cycles = i16(int(shifted));
                player.position = 0.f;
            }
            else if (player.mode == 2)
            {
                player.cycles = i16(int(shifted));
                if ((player.cycles % 2) != 0)
                {
                    player.position = 1.f - (position - float(player.cycles));
                }
                else
                {
                    player.position = position - float(player.cycles);
                }
                player.speed = -player.speed;
            }
        }
    }
    else
    {
        if (player.mode == 0)
        {
            cycles = u32(int(position));
            player.position = position - float(cycles & 0xffff);
            player.cycles = i16(cycles);
        }
        else if (player.mode == 1)
        {
            player.cycles = i16(int(position));
            player.position = 1.f;
        }
        else if (player.mode == 2)
        {
            player.cycles = i16(int(position));
            if ((player.cycles % 2) == 0)
            {
                player.position = position - float(player.cycles);
            }
            else
            {
                player.position = 1.f - (position - float(player.cycles));
            }
            player.speed = -player.speed;
        }
    }
    if (root != nullptr)
    {
        if (read_u16(player.mina, 0x1A) != 0)
        {
            sample_playback_root(player, previous, cycles, *root, cached_root, sampled);
            return;
        }
        root->translation = Vec3{};
        root->rotation = Quat{0.f, 0.f, 0.f, 1.f};
    }
}

// Xbox 0x00030DB0. The caller selects a slot from controller +0x58.
void retime_mina_playback(MinaPlayback* player, float duration, bool& dirty)
{
    if (player != nullptr)
    {
        float reciprocal;
        if (0.f < duration)
        {
            reciprocal = 1.f / duration;
            player->duration = duration;
        }
        else
        {
            reciprocal = 1.f / read_f32(player->mina, 8);
            player->duration = read_f32(player->mina, 8);
        }
        const float absolute = animation_absolute(reciprocal);
        if (player->speed < 0.f)
        {
            player->speed = -absolute;
            dirty = true;
            return;
        }
        player->speed = absolute;
    }
    dirty = true;
}

// Xbox 0x0002DD00, ESI player
void set_mina_playback_direction(MinaPlayback& player, u16 direction)
{
    switch (direction)
    {
    case 0:
        player.speed = animation_absolute(player.speed);
        return;
    case 1:
        player.speed = -animation_absolute(player.speed);
        return;
    case 2:
        player.speed = 0.f - player.speed;
        return;
    default:
        return;
    }
}

// Xbox 0x0002DBB0.
void tick_mina_player(MinaPlayer& player, float dt, bool skip_pose)
{
    if (player.mina.data() != nullptr)
    {
        player.step = 0.f;
        if (!player.paused)
        {
            player.step = dt;
        }
        player.previous = player.position;
        advance_mina_playback(player, player.step, &player.root, &player.cached_root, player.sampled);
        if (!skip_pose)
        {
            player.pose->joints.resize(read_u16(player.mina, 0x14)); // The pose buffer holds one per bone.
            sample_mina_pose(player.pose->joints.data(), player, nullptr, nullptr, false, player.weight); // 0x0002E8A0
            sample_mina_auxiliary(player, player.pose->auxiliary);
            player.sampled = true;
        }
        if (0.f < player.blend_remaining)
        {
            player.blend_remaining -= player.step;
            if (player.blend_remaining <= 0.f || skip_pose || player.old_pose == nullptr)
            {
                if (player.old_pose != nullptr)
                {
                    player.old_pose.reset();
                }
                player.blend_remaining = 0.f;
            }
            else
            {
                blend_animation_pose(*player.pose, *player.pose, *player.old_pose,
                                     player.blend_remaining / player.blend_duration);
            }
        }
        ++player.ticks;
        player.age += player.step;
    }
}

// Xbox 0x0003B290. A missing controller/slot supplies a null player.
bool mina_playback_completed(const MinaPlayback* player)
{
    if (player == nullptr)
    {
        return true;
    }
    if (player->mode == 1)
    {
        bool incomplete;
        if (player->speed < 0.f)
        {
            incomplete = 0.f < player->position;
        }
        else
        {
            incomplete = player->position < 1.f;
        }
        if (!incomplete)
        {
            return true;
        }
    }
    return false;
}
