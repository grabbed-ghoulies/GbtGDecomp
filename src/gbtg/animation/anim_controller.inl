// Xbox 0x00035C00.
void release_animation_resource(AnimationResourceRegistry& registry, std::span<const u8> resource)
{
    AnimationResourceRecord* found = nullptr;
    for (AnimationResourceRecord& record : registry.records)
    {
        if (record.resource_slot != 0 && registry.loaded[record.resource_slot - 1].cpu.data() == resource.data())
        {
            found = &record;
            break;
        }
    }
    // As in the dump, releasing an unregistered owned resource is invalid.
    --found->references;
}

// Xbox 0x00035BA0.
void retain_animation_resource(AnimationResourceRegistry& registry, std::span<const u8> resource)
{
    AnimationResourceRecord* found = nullptr;
    for (AnimationResourceRecord& record : registry.records)
    {
        if (record.resource_slot != 0 && registry.loaded[record.resource_slot - 1].cpu.data() == resource.data())
        {
            found = &record;
            break;
        }
    }
    ++found->references;
}

// Xbox 0x0003B3C0.
void animation_actor_event(std::span<const u8> event, u32 slot, float position,
    ActorAnimationState& actor, int phase)
{
    const ActorAnimationMessage message{7, position, slot, event, phase};
    actor.message_handler(message);
}

// Xbox 0x0003ADA0. The message handler is the original actor virtual call;
// the specific actor's handler must be translated before constructing it.
void start_actor_animation_mix(ActorAnimationState& actor, u32 slot, const AnimationMix& mix,
    const char* name, AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random_state)
{
    if (actor.controller == nullptr)
    {
        actor.controller = make_animation_controller(4);
        actor.controller->event_callback = [](std::span<const u8> event, u32 index,
                                               float position, void* owner, int phase)
        {
            animation_actor_event(event, index, position, *static_cast<ActorAnimationState*>(owner), phase);
        };
        actor.controller->event_owner = &actor;
    }
    if (mix.borrowed_animation.data() != nullptr)
    {
        start_controller_animation(*actor.controller, slot, mix.borrowed_animation, nullptr, mix,
                                     registry, error, random_state);
        return;
    }
    start_controller_animation(*actor.controller, slot, {}, name, mix, registry, error, random_state);
}

// Xbox 0x0003A9E0. ECX supplies the record; actor +0x84 supplies the
// retain-position argument to 0x000322A0 and is cleared after startup.
void start_actor_animation(ActorAnimationState& actor, const AnimationRecord& record,
    AnimationResourceRegistry& registry, ResourceErrorState& error, u32& random_state)
{
    const AnimationMix mix = animation_mix(record, actor.retain_position != 0);
    start_actor_animation_mix(actor, u32(int(record.slot)), mix, record.animation.data(),
                                registry, error, random_state);
    actor.retain_position = 0;
}

// Xbox 0x0003B2E0.
void set_actor_animation_position(ActorAnimationState& actor, u32 slot, float position)
{
    if (actor.controller != nullptr)
    {
        MinaPlayer* player = actor.controller->slots[slot].player.get();
        if (player != nullptr)
        {
            player->position = position;
            player->previous = position;
        }
        actor.controller->dirty = true;
    }
}

// Xbox 0x0003B290.
bool actor_animation_completed(const ActorAnimationState& actor, u32 slot)
{
    if (actor.controller == nullptr || actor.controller->slots[slot].player == nullptr)
    {
        return true;
    }
    return mina_playback_completed(actor.controller->slots[slot].player.get());
}

// Xbox 0x0002F570
void remove_animation_callback(AnimationCallbackPool& pool, u16 id)
{
    auto cursor = std::make_unique<u32>(0);
    u32 index = 0;
    while (true)
    {
        const auto& ids = *pool.arrays[0];
        const u32 offset = u32(ids.stride) * *cursor;
        const u8* record = ids.bytes.data() + offset;
        if (offset == ids.used)
        {
            break;
        }
        ++*cursor;
        if (record == nullptr)
        {
            break;
        }
        u16 value;
        std::memcpy(&value, record, sizeof(value));
        if (id == value)
        {
            break;
        }
        ++index;
    }
    cursor.reset();
    --pool.count;
    for (size_t array = 0; array < 3; ++array)
    {
        auto& storage = *pool.arrays[array];
        const u32 offset = u32(storage.stride) * index;
        const u32 last = storage.used - storage.stride;
        if (offset < last)
        {
            std::memmove(storage.bytes.data() + offset, storage.bytes.data() + offset + storage.stride,
                last - offset);
        }
        storage.used -= storage.stride;
    }
}

// Xbox 0x0002F2D0
void release_animation_callback_pool(std::unique_ptr<AnimationCallbackPool>& pool)
{
    for (size_t index = 0; index < 3; ++index)
    {
        if (pool->arrays[index] != nullptr)
        {
            pool->arrays[index].reset();
        }
    }
    pool.reset();
}

// Xbox 0x0003AC20
void clear_actor_animation(ActorAnimationState& actor, ModelAnimationState* model,
    AnimationResourceRegistry& registry)
{
    if (actor.controller != nullptr)
    {
        release_animation_controller(actor.controller, false, registry);
        actor.controller.reset();
    }
    if (actor.pose_callbacks != nullptr)
    {
        if ((actor.callback_flags & 1) != 0)
        {
            remove_animation_callback(*actor.callback_owner, actor.callback_id);
        }
        release_animation_callback_pool(actor.pose_callbacks);
        actor.pose_callbacks.reset();
    }
    ModelPoseBuffer* selected;
    if (model == nullptr)
    {
        selected = nullptr;
    }
    else
    {
        selected = model->alternate;
        if (selected == nullptr)
        {
            selected = model->primary;
        }
    }
    if (selected != nullptr)
    {
        initialize_model_pose(*selected);
        return;
    }
}

// Xbox 0x0003AA30
void set_actor_animation_table(ActorAnimationState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const AnimationTableResourceViews& views)
{
    if (actor.table != nullptr)
    {
        if (actor.table_owned != 0)
        {
            release_animation_resource(registry, actor.table_resource);
        }
        actor.table = nullptr;
        actor.table_resource = {};
    }
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    if (prefix != 0x5F646961)
    {
        actor.table = nullptr;
        actor.table_resource = {};
        actor.table_owned = 1;
        return;
    }
    const auto resource = acquire_animation_resource(registry, name);
    if (resource.data() == nullptr)
    {
        resource_error(error, 2);
    }
    actor.table_resource = resource;
    // The host pointer view represents the acquired CPU pointer, including null.
    actor.table = views.tables.at(resource.data());
    actor.table_owned = 1;
}

// Xbox 0x0003ACE0. The final call's EAX survives the RET.
bool start_actor_initial_animation(ActorAnimationState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error, u32& random_state)
{
    AnimationMix mix;
    mix.duration = 0.f;
    mix.weight = -10.f;
    mix.blend_mode = 0x21;
    mix.fade_duration = 0.2f;
    mix.position = 0.f;
    mix.mode = 0;
    mix.priority = 0x31;
    mix.old_pose_duration = 0.2f;
    mix.events = {};
    mix.borrowed_animation = {};
    mix.borrowed_events = {};
    mix.record = nullptr;
    if (actor.controller == nullptr)
    {
        actor.controller = make_animation_controller(4);
        actor.controller->event_callback = [](std::span<const u8> event, u32 index,
            float position, void* owner, int phase)
        {
            animation_actor_event(event, index, position, *static_cast<ActorAnimationState*>(owner), phase);
        };
        actor.controller->event_owner = &actor;
    }
    return start_controller_animation(*actor.controller, 0, {}, name, mix, registry, error, random_state);
}

// Xbox 0x000307C0
void release_animation_controller(std::unique_ptr<AnimationController>& controller,
    bool send_end, AnimationResourceRegistry& registry)
{
    u32 index = 0;
    if (controller->slot_count != 0)
    {
        do
        {
            clear_animation_controller_slot(*controller, index, send_end, registry);
            ++index;
        }
        while (index < controller->slot_count);
    }
    if (controller->combined_pose != nullptr)
    {
        controller->combined_pose.reset();
    }
    controller->combined_pose.reset();
    controller.reset();
}

// Xbox 0x0002D850.
void release_mina_player(std::unique_ptr<MinaPlayer>& player, AnimationResourceRegistry& registry)
{
    if (player->old_pose != nullptr)
    {
        player->old_pose.reset();
    }
    if (player->pose != nullptr)
    {
        player->pose.reset();
    }
    if (player->mina.data() != nullptr && player->owned_resource)
    {
        release_animation_resource(registry, player->mina);
        player->mina = {};
    }
    player.reset();
}

namespace
{

AnimationEventCallback controller_event_callback(AnimationController& controller, u32 slot, MinaPlayer& player)
{
    if (!controller.event_callback)
    {
        return {};
    }
    return [&controller, slot, &player](std::span<const u8> event, int phase)
    {
        controller.event_callback(event, slot, player.position, controller.event_owner, phase);
    };
}

// Xbox 0x0002DCD0.
bool player_at_endpoint(const MinaPlayer& player)
{
    if (player.mode == 1)
    {
        if (player.speed <= 0.f && player.speed != 0.f)
        {
            if (player.position <= 0.f)
            {
                return true;
            }
        }
        else if (1.f <= player.position)
        {
            return true;
        }
    }
    return false;
}

} // namespace

// Xbox 0x00030880.
bool start_controller_animation(AnimationController& controller, u32 index,
    std::span<const u8> resource, const char* name, const AnimationMix& mix,
    AnimationResourceRegistry& registry, ResourceErrorState& error, u32& random_state)
{
    const bool reverse = mix.duration <= 0.f && mix.duration != 0.f;
    float position = mix.position;
    auto& slot = controller.slots[index];
    if (slot.events.data() != nullptr)
    {
        if (slot.player->ticks != 0)
        {
            end_animation_events(slot.events, *slot.player,
                                   controller_event_callback(controller, index, *slot.player));
        }
        if (!slot.borrowed_events)
        {
            release_animation_resource(registry, slot.events);
        }
        slot.events = {};
        slot.borrowed_events = false;
    }
    if (slot.player == nullptr)
    {
        slot.player = make_mina_player();
        slot.fade_fraction = 0.f;
    }
    slot.fade_mode = 1;
    if (position == -1.f)
    {
        position = slot.player->position;
    }
    else if (position == -2.f)
    {
        random_state = random_state * 0x19660du + 0x3c6ef35fu;
        position = std::bit_cast<float>((random_state & 0x7fffffu) | 0x3f800000u) - 1.f;
    }
    slot.player->blend_duration = mix.old_pose_duration;
    slot.player->resource_registry = &registry;
    bool started;
    if (name == nullptr)
    {
        started = start_mina_player(*slot.player, resource, position, animation_absolute(mix.duration),
                                     u8(mix.mode), i16(reverse));
    }
    else
    {
        started = start_named_mina_player(*slot.player, registry, name, position,
                                           animation_absolute(mix.duration), u8(mix.mode),
                                           i16(reverse), error);
    }
    if (!started)
    {
        if (index != 0)
        {
            clear_animation_controller_slot(controller, index, false, registry);
        }
        return false;
    }
    if (mix.weight == -10.f)
    {
        float weight = 1.f;
        if (mix.blend_mode == 0x21)
        {
            weight = 0.5f;
        }
        slot.weight = weight;
    }
    if (mix.fade_duration == 0.f)
    {
        slot.fade_mode = 0;
        slot.fade_fraction = 1.f;
    }
    slot.fade_duration = mix.fade_duration;
    slot.blend_mode = mix.blend_mode;
    slot.priority = mix.priority;
    slot.record = mix.record;
    auto events = mix.borrowed_events;
    if (events.data() == nullptr)
    {
        if (mix.events.starts_with("aid_"))
        {
            events = acquire_named_animation_resource(registry, mix.events.data(), error);
            slot.events = events;
        }
    }
    else
    {
        slot.borrowed_events = true;
        slot.events = events;
    }
    controller.dirty = true;
    return started;
}

// Xbox 0x00031240.
void clear_animation_controller_slot(AnimationController& controller, u32 slot, bool send_end,
                                       AnimationResourceRegistry& registry)
{
    AnimationControllerSlot& record = controller.slots[slot];
    if (record.events.data() != nullptr)
    {
        if (send_end && record.player->ticks != 0)
        {
            end_animation_events(record.events, *record.player,
                                   controller_event_callback(controller, slot, *record.player));
        }
        if (!record.borrowed_events)
        {
            release_animation_resource(registry, record.events);
        }
        record.events = {};
        record.borrowed_events = false;
    }
    if (record.player != nullptr)
    {
        release_mina_player(record.player, registry);
    }
    record.fade_fraction = 0.f;
    record.fade_mode = 0;
    controller.dirty = true;
}

// Xbox 0x00030E30.
bool tick_animation_controller_slot(AnimationController& controller, u32 slot, float dt, bool skip_pose,
                                      AnimationResourceRegistry& registry)
{
    AnimationControllerSlot& record = controller.slots[slot];
    MinaPlayer* player = record.player.get();
    if (player == nullptr)
    {
        return false;
    }
    const int fade_mode = record.fade_mode;
    if (fade_mode == 1)
    {
        record.fade_fraction = (1.f / record.fade_duration) * dt + record.fade_fraction;
        if (1.f <= record.fade_fraction)
        {
            record.fade_fraction = 1.f;
            record.fade_mode = 0;
        }
    }
    else if (1 < fade_mode && fade_mode < 4)
    {
        float fraction = record.fade_fraction - (1.f / record.fade_duration) * dt;
        record.fade_fraction = fraction;
        if (fraction <= 0.f)
        {
            fraction = 0.f;
        }
        record.fade_fraction = fraction;
        if (fade_mode == 3 && fraction <= 0.f)
        {
            clear_animation_controller_slot(controller, slot, true, registry);
            return false;
        }
    }
    if (record.priority == 0x32 && slot != 0 && record.fade_mode != 3 && player_at_endpoint(*player))
    {
        record.fade_mode = 3;
    }
    if (record.events.data() != nullptr && player->ticks == 0)
    {
        begin_animation_events(record.events, *player, controller_event_callback(controller, slot, *player));
    }
    tick_mina_player(*player, dt, skip_pose);
    if (record.events.data() != nullptr && 0.f < dt)
    {
        dispatch_animation_events(record.events, *player, controller_event_callback(controller, slot, *player));
    }
    if (0.f < player->step)
    {
        controller.dirty = true;
    }
    return true;
}

// Xbox 0x00030BA0.
bool tick_animation_controller(AnimationController& controller, float dt, bool skip_pose,
                                 AnimationResourceRegistry& registry)
{
    controller.combined_root.translation.z = 0.f;
    controller.combined_root.translation.y = 0.f;
    controller.combined_root.translation.x = 0.f;
    controller.combined_root.rotation = Quat{0.f, 0.f, 0.f, 1.f};
    u32 slot = 0;
    bool active = false;
    if (controller.slot_count != 0)
    {
        do
        {
            const MinaPlayer* player = controller.slots[slot].player.get();
            if (player != nullptr && !player->paused)
            {
                active = true;
                break;
            }
            ++slot;
        } while (slot < controller.slot_count);
    }
    if (!active && !controller.dirty)
    {
        controller.idle = true;
        return false;
    }
    int ticked = 0;
    slot = 0;
    controller.idle = false;
    if (controller.slot_count != 0)
    {
        do
        {
            ticked += tick_animation_controller_slot(controller, slot, dt, skip_pose, registry);
            ++slot;
        } while (slot < controller.slot_count);
        if (ticked != 0 && !skip_pose)
        {
            combine_animation_controller(controller);
        }
    }
    return true;
}
