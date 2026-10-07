namespace
{

// Host pointer mapping; a null model remains a null Xbox actor +0x204.
void bind_actor_model_view(RuntimeActorStartupState& state, const RuntimeActorStartupContext& context)
{
    if (state.models.model == nullptr)
    {
        state.actor.model = nullptr;
        return;
    }
    state.model_view = context.actor_model_views.at(state.models.model->source->cpu.data());
    state.model_view.runtime = state.models.model.get();
    state.actor.model = &state.model_view;
}

} // namespace

// Xbox 0x0003E940
bool initialize_runtime_actor(RuntimeActorStartupState& state, std::span<const u8> parameters,
    RuntimeActorStartupContext& context)
{
    Actor& actor = state.actor;
    actor.position = Vec3{read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
    actor.xform_valid = 0;
    const Vec3 euler{read_f32(parameters, 0x14), read_f32(parameters, 0x18), read_f32(parameters, 0x1C)};
    fn_105aa0(actor.euler, euler);
    actor.xform_valid = 0;
    const float scale = read_f32(parameters, 0x20);
    if (actor.scale_x != scale || actor.radius_y != scale || actor.radius_xz != scale)
    {
        actor.scale_x = scale;
        actor.radius_y = scale;
        actor.radius_xz = scale;
        actor.xform_valid = 0;
    }
    actor.quat = Quat{0.f, 0.f, 0.f, 1.f}; // DAT_002038F0 / DAT_002038F4
    actor.animation_rate = 1.f;
    actor.xform_valid = 0;
    actor.scale_200 = read_f32(parameters, 0x24);
    if (read_u32(parameters, 0x28) == 0x5F646961)
    {
        set_runtime_actor_model(state.models, reinterpret_cast<const char*>(parameters.data() + 0x28),
            context.registry, context.error, context.models, context.deferred, context.fence,
            context.types, context.shaders, context.scratch, context.heap_free);
        bind_actor_model_view(state, context);
    }
    state.word_288 = 0;
    state.word_28c = 0;
    state.value_29c = 0.f;
    state.word_2a0 = 0;
    fn_3d5f0(actor);
    if (read_u32(parameters, 0x128) == 0x5F646961)
    {
        set_actor_animation_table(state.animation, reinterpret_cast<const char*>(parameters.data() + 0x128),
            context.registry, context.error, context.tables);
    }
    if (read_u32(parameters, 0xA8) == 0x5F646961)
    {
        if (!start_actor_initial_animation(state.animation, reinterpret_cast<const char*>(parameters.data() + 0xA8),
            context.registry, context.error, context.random_state))
        {
            ModelAnimationState model;
            // Mapping the existing model pointer to its pose fields is a host adapter.
            if (actor.model != nullptr)
            {
                model.primary = actor.model->runtime->primary;
                model.alternate = actor.model->runtime->alternate;
            }
            clear_actor_animation(state.animation, actor.model != nullptr ? &model : nullptr, context.registry);
        }
    }
    fn_3e6d0(actor);
    fn_41be0(actor);
    fn_3c680(actor);
    state.secondary_mode = i32(read_u32(parameters, 0x2B0));
    state.secondary_flags[0] = 0;
    state.secondary_flags[1] = 0;
    if (state.secondary_mode != 0)
    {
        set_runtime_actor_secondary_model(state.models, reinterpret_cast<const char*>(parameters.data() + 0x2B4),
            context.registry, context.error, context.models, context.deferred, context.fence,
            context.types, context.shaders, context.scratch, context.heap_free);
    }
    state.color = 0xFFFFFFFF; // DAT_00314F48
    state.flags = {0, 0, 0, 0xFF};
    state.bone = 0xFFFF;
    set_runtime_actor_resource(state.resource, reinterpret_cast<const char*>(parameters.data() + 0x1A8),
        context.registry, context.error);
    state.setting_1fc = read_u16(parameters, 0x2AA);
    state.setting_1fe = read_u16(parameters, 0x2AC);
    if (read_u32(parameters, 0x228) == 0x5F646961)
    {
        const u16 bone = read_u16(parameters, 0x2A8);
        ActorAttachmentNode* node = fn_3d1b0(state.attachments,
            reinterpret_cast<const char*>(parameters.data() + 0x228), 0, 3);
        if (node != nullptr)
        {
            std::memcpy(node->bytes.data() + 0xC, &bone, 2);
        }
    }
    RuntimeActorLink* tail = &context.live_actors;
    if (context.live_actors.next != nullptr)
    {
        do
        {
            tail = tail->next;
        }
        while (tail->next != nullptr);
    }
    tail->next = &state.live;
    state.live.previous = tail;
    state.live.next = nullptr;
    fn_40de0(actor);
    return true;
}
// Xbox 0x0012A260
void queue_actor_sound(RuntimeActorSound& sound, i32 group, Actor* owner,
    u16 request_id, const Vec3* position, const Vec3* direction, float volume,
    const char* name, RuntimeActorSoundQueues& queues)
{
    release_actor_sound(sound, queues);
    char* destination = sound.name.data();
    do
    {
        *destination++ = *name;
    }
    while (*name++ != '\0');
    if (position == nullptr)
    {
        if (owner == nullptr)
        {
            sound.position = {};
        }
        else
        {
            sound.position = actor_world_position(*owner);
        }
    }
    else
    {
        sound.position = *position;
    }
    if (direction == nullptr)
    {
        if (owner == nullptr)
        {
            sound.direction = {0.f, 1.f, 0.f};
        }
        else
        {
            const Vec3 euler = actor_world_euler(*owner);
            sound.direction = {};
            add_sound_direction(sound.direction, euler.x, euler.y, 1.f);
        }
    }
    else
    {
        sound.direction = *direction;
    }
    sound.request_id = request_id;
    sound.elapsed = 0.f;
    sound.owner = owner;
    sound.previous_bone = 0xFFFF;
    sound.volume = volume;
    RuntimeSoundLink* root = &queues.groups[group].pending;
    while (root->previous != nullptr)
    {
        root = root->previous;
    }
    RuntimeSoundLink* next = root->next;
    root->next = &sound;
    sound.previous = root;
    sound.next = next;
    if (next != nullptr)
    {
        next->previous = &sound;
    }
}

// Xbox 0x0003A830
void request_actor_event_sound(std::span<const u8> event, Actor& actor,
    u32& random_state, RuntimeActorSoundQueues& queues)
{
    random_state = random_state * 0x19660D + 0x3C6EF35F;
    const float random = std::bit_cast<float>((random_state & 0x7FFFFF) | 0x3F800000) - 1.f;
    if (random <= read_f32(event, 0x94) && event[0x14] != 0)
    {
        const i32 index = static_cast<i32>(read_u32(event, 0x10));
        queue_actor_sound(actor.sound_slots[index], actor_sound_groups[index], &actor,
            static_cast<u16>(index), nullptr, nullptr, 1.f,
            reinterpret_cast<const char*>(event.data() + 0x14), queues);
    }
}

// Xbox 0x00041E10
void stop_actor_sound(Actor& actor, i32 slot, RuntimeActorSoundQueues& queues)
{
    release_actor_sound(actor.sound_slots[slot], queues);
}

// Xbox 0x00041C80
bool actor_sound_is_playing(const Actor& actor, i32 slot, const RuntimeActorSoundQueues& queues)
{
    if (!actor_sound_is_active(actor.sound_slots[slot], queues))
    {
        return false;
    }
    return static_cast<i16>(actor.sound_slots[slot].previous_bone) != -1;
}

// Xbox 0x00041D00
bool request_actor_sound(Actor& actor, i32 slot, const char* name, RuntimeActorSoundQueues& queues)
{
    if (*name != '\0')
    {
        queue_actor_sound(actor.sound_slots[slot], actor_sound_groups[slot], &actor,
            static_cast<u16>(slot), nullptr, nullptr, 1.f, name, queues);
        return true;
    }
    return false;
}

// Xbox 0x000426B0
void clear_actor_shader_bits(Actor& actor, i16 slot, u32 mask)
{
    if (actor.model != nullptr)
    {
        actor.model->runtime->shader_indices[slot] &= ~mask;
    }
}

// Xbox 0x000426D0
void set_actor_shader_bits(Actor& actor, i16 slot, u32 mask)
{
    if (actor.model != nullptr)
    {
        actor.model->runtime->shader_indices[slot] |= mask;
    }
}

// Xbox 0x00040D90
i32 actor_shader_state(const Actor& actor, u8 slot)
{
    if (actor.model == nullptr)
    {
        return 0; // host typed return represents the retail AL result
    }
    const u32 bit = static_cast<u32>(slot) * 2;
    return static_cast<i32>(actor.model->runtime->node_flags[bit >> 3] != 0) >> (bit & 7);
}

// Xbox 0x00040D50
void set_actor_node_state(RuntimeActorStartupState& state, u8 slot, u8 value,
    std::span<u32> shared_states)
{
    if (state.actor.model != nullptr)
    {
        set_runtime_model_node_state(*state.actor.model->runtime, slot, value, shared_states);
    }
    if (state.models.secondary != nullptr)
    {
        set_runtime_model_node_state(*state.models.secondary, slot, value, shared_states);
    }
}

// Xbox 0x0003A8B0
RuntimeActorCreationOptions& actor_creation_options(const Actor& actor, void* user,
    i32 scaled, i32 include_euler)
{
    static RuntimeActorCreationOptions options; // DAT_004E1C5C shared scratch
    options.flags = 0;
    options.position = actor_world_position(actor);
    options.user = user;
    options.flags |= 9;
    if (include_euler != 0)
    {
        options.euler = actor_world_euler(actor);
        options.flags |= 4;
    }
    if (scaled == 0)
    {
        options.scale = actor.scale_x;
    }
    else
    {
        options.scale = actor.scale_200 * actor.scale_x;
    }
    options.flags |= 0x40;
    return options;
}

// Xbox 0x00040200
void refresh_actor_model_matrices(const Actor& actor, RuntimeActorMatrixState& state,
    i32 force, const RuntimeShaderMatrixBoundaries& boundaries)
{
    if (state.count != 0)
    {
        AnimationMatrix inverse;
        std::memcpy(inverse.data(), actor.xform_inv, sizeof(actor.xform_inv));
        for (i32 index = 0; index < state.count; ++index)
        {
            const auto record = state.records.subspan(index * 0x48, 0x48);
            RuntimeShaderMatrix result;
            if (read_u32(record, 0x34) == 0)
            {
                if (read_u32(record, 0x44) != 0 || force != 0)
                {
                    RuntimeShaderMatrix center, translation, x, y, z;
                    boundaries.translation(center, read_f32(record, 0x28), read_f32(record, 0x2C), read_f32(record, 0x30));
                    boundaries.translation(translation, read_f32(record, 4), read_f32(record, 8), read_f32(record, 0xC));
                    runtime_shader_rotation(x, 'x', read_f32(record, 0x10), boundaries);
                    runtime_shader_rotation(y, 'y', read_f32(record, 0x14), boundaries);
                    runtime_shader_rotation(z, 'z', read_f32(record, 0x18), boundaries);
                    boundaries.scaling(result, read_f32(record, 0x1C), read_f32(record, 0x20), read_f32(record, 0x24));
                    multiply_runtime_matrix(y, y, translation);
                    multiply_runtime_matrix(x, x, y);
                    multiply_runtime_matrix(x, z, x);
                    multiply_runtime_matrix(result, result, x);
                    multiply_runtime_matrix(result, center, result);
                    write_runtime_model_matrix(state.node, state.instance, index, result);
                }
            }
            else if (read_u32(record, 0x44) != 0 || force != 0)
            {
                RuntimeShaderMatrix center, translation, x, y, z, second_x, second_y, second_z;
                boundaries.translation(center, read_f32(record, 0x28), read_f32(record, 0x2C), read_f32(record, 0x30));
                boundaries.translation(translation, read_f32(record, 4), read_f32(record, 8), read_f32(record, 0xC));
                runtime_shader_rotation(x, 'x', read_f32(record, 0x10), boundaries);
                runtime_shader_rotation(y, 'y', read_f32(record, 0x14), boundaries);
                runtime_shader_rotation(z, 'z', read_f32(record, 0x18), boundaries);
                runtime_shader_rotation(second_x, 'x', read_f32(record, 0x38), boundaries);
                runtime_shader_rotation(second_y, 'y', read_f32(record, 0x3C), boundaries);
                runtime_shader_rotation(second_z, 'z', read_f32(record, 0x40), boundaries);
                boundaries.scaling(result, read_f32(record, 0x1C), read_f32(record, 0x20), read_f32(record, 0x24));
                multiply_runtime_matrix(y, y, translation);
                multiply_runtime_matrix(x, x, y);
                multiply_runtime_matrix(z, z, x);
                multiply_runtime_matrix(center, center, z);
                multiply_runtime_matrix(second_y, second_y, center);
                multiply_runtime_matrix(second_x, second_x, second_y);
                multiply_runtime_matrix(second_z, second_z, second_x);
                multiply_runtime_matrix(result, result, second_z);
                write_runtime_model_matrix(state.node, state.instance, index, result);
            }
        }
    }
}
