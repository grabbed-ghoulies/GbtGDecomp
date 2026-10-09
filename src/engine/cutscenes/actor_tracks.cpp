#include "actor_tracks.h"
#include "../entities/actor_pose_updates.h"
#include "../entities/entity.h"
#include "../../gbtg/runtime/object_lifecycle.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0004E610
void set_cutscene_actor_replacement(char* replacement_source, char* replacement_target,
    const char* source, const char* target)
{
    char value;
    do
    {
        value = *source;
        *replacement_source = value;
        ++source;
        ++replacement_source;
    }
    while (value != '\0');
    do
    {
        value = *target;
        *replacement_target = value;
        ++target;
        ++replacement_target;
    }
    while (value != '\0');
}

// Xbox 0x0004E9E0
void release_cutscene_actor_row(CutsceneActorRowView* rows, std::int8_t index,
    CutsceneActorReleaseContext& context)
{
    auto& row = rows[index];
    Actor* actor = row.actor;
    if (actor != nullptr)
    {
        if (actor->link == nullptr)
        {
            if (row.listed != 0)
            {
                unlink_actor_view(context.render.at(actor)->view);
            }
            release_factory_object(context.factory, *context.objects.at(actor));
        }
        else
        {
            activate_actor_bind_tree(*actor);
        }
        row.actor = nullptr;
    }
    auto& resource = context.events[index];
    if (resource.data() != nullptr)
    {
        release_animation_resource(context.resources, resource);
        resource = {};
        row.elapsed = 0;
    }
}

// Xbox 0x0004E950
std::int8_t find_cutscene_actor_track(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, std::int8_t id)
{
    std::int8_t index = 0;
    if (0 < instance.actor_count)
    {
        do
        {
            if (tracks[index].id == id)
            {
                return index;
            }
            index = static_cast<std::int8_t>(static_cast<u8>(index) + 1);
        }
        while (static_cast<i32>(index) < instance.actor_count);
    }
    return -1;
}

// Xbox 0x0004E980
Actor* find_cutscene_actor(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, const CutsceneActorRowView* rows,
    std::int8_t id)
{
    std::int8_t index = 0;
    if (0 < instance.actor_count)
    {
        do
        {
            if (tracks[index].id == id)
            {
                goto found;
            }
            index = static_cast<std::int8_t>(static_cast<u8>(index) + 1);
        }
        while (static_cast<i32>(index) < instance.actor_count);
    }
    index = -1;
found:
    if (index != -1)
    {
        return rows[index].actor;
    }
    return nullptr;
}

// Xbox 0x0004E780
void set_cutscene_actor_track_state(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, CutsceneActorRowView* rows,
    std::int8_t id, u32 state)
{
    std::int8_t index = 0;
    if (0 < instance.actor_count)
    {
        while (tracks[index].id != id)
        {
            index = static_cast<std::int8_t>(static_cast<u8>(index) + 1);
            if (instance.actor_count <= static_cast<i32>(index))
            {
                return;
            }
        }
        if (index != -1)
        {
            rows[index].previous = state;
        }
    }
}

// Xbox 0x0004E6D0
void pause_cutscene_actor(CutsceneActorRowView* rows, std::int8_t index, i32 pause,
    const std::unordered_map<Actor*, ActorAnimationState*>& animations)
{
    auto& row = rows[index];
    if (row.actor != nullptr)
    {
        auto& animation = *animations.at(row.actor);
        if (pause == 0)
        {
            if (row.flags != 0 && animation.controller != nullptr)
            {
                auto& controller = *animation.controller;
                if (controller.slots[1].player != nullptr)
                {
                    --controller.slots[1].player->paused;
                }
                controller.dirty = true;
            }
            if (animation.controller == nullptr)
            {
                return;
            }
            if (animation.controller->slots[0].player != nullptr)
            {
                --animation.controller->slots[0].player->paused;
            }
        }
        else
        {
            if (row.flags != 0 && animation.controller != nullptr)
            {
                auto& controller = *animation.controller;
                if (controller.slots[1].player != nullptr)
                {
                    ++controller.slots[1].player->paused;
                }
                controller.dirty = true;
            }
            if (animation.controller == nullptr)
            {
                return;
            }
            if (animation.controller->slots[0].player != nullptr)
            {
                ++animation.controller->slots[0].player->paused;
                animation.controller->dirty = true;
                return;
            }
        }
        animation.controller->dirty = true;
    }
}

// Xbox 0x0004DC70
void initialize_cutscene_actors(const RuntimeCutsceneInstance& instance,
    const CutsceneActorTrackView* tracks, CutsceneActorRowView* rows,
    i32 suppress_secondary, i32 unlist,
    CutsceneActorInitializationContext& context)
{
    i32 index = 0;
    if (0 < instance.actor_count)
    {
        do
        {
            const auto track_index = static_cast<std::int8_t>(index);
            auto object = create_cutscene_track_actor(tracks, track_index,
                suppress_secondary, context.factory);
            Actor* actor = nullptr;
            if (object.bytes != nullptr)
            {
                auto& binding = *context.bindings.at(object.bytes.get());
                actor = &binding.frame.actor;
                context.objects.push_back(std::move(object));
                if (unlist == 0)
                {
                    rows[index].listed = 1;
                }
                else
                {
                    auto& link = binding.render.view;
                    if (link.previous != nullptr)
                    {
                        auto& world = entity_detail::g_world;
                        if (link.previous == reinterpret_cast<RuntimeActorLink*>(&world.view))
                        {
                            world.view.next = link.next;
                        }
                        else
                        {
                            link.previous->next = link.next;
                        }
                    }
                    if (link.next != nullptr)
                    {
                        link.next->previous = link.previous;
                    }
                    link.previous = nullptr;
                    link.next = nullptr;
                    rows[index].listed = 0;
                }
                rows[index].initialized = 1;
                AnimationMix mix;
                mix.mode = 1;
                auto& animation = binding.frame.animation;
                if (animation.controller == nullptr)
                {
                    animation.controller = make_animation_controller(4);
                    animation.controller->event_callback = [](std::span<const u8> event, u32 slot,
                        float position, void* owner, int phase)
                    {
                        animation_actor_event(event, slot, position,
                            *static_cast<ActorAnimationState*>(owner), phase);
                    };
                    animation.controller->event_owner = &animation;
                }
                start_controller_animation(*animation.controller, 0, tracks[track_index].animation,
                    nullptr, mix, context.animations, context.error, context.random);
                Vec3 scale;
                sample_cutscene_actor_scale(&tracks[track_index].scale, 0, 0.f, scale);
                if (scale.x != actor->scale_x || scale.y != actor->radius_y || scale.z != actor->radius_xz)
                {
                    actor->scale_x = scale.x;
                    actor->radius_y = scale.y;
                    actor->radius_xz = scale.z;
                    actor->xform_valid = 0;
                }
                update_actor_animation_pose(*actor, animation, binding.frame.model, binding.frame.bone,
                    context.animations, context.callbacks, context.cache, context.reset);
                if (animation.controller != nullptr)
                {
                    auto* player = animation.controller->slots[0].player.get();
                    if (player != nullptr)
                    {
                        ++player->paused;
                    }
                    animation.controller->dirty = true;
                }
                binding.render.message(binding.render, 9);
            }
            rows[index].actor = actor;
            rows[index].elapsed = 0;
            rows[index].previous = 0;
            rows[index].flags = 0;
            ++index;
        }
        while (index < instance.actor_count);
    }
}

// Xbox 0x0004DEC0
FactoryObject create_cutscene_track_actor(const CutsceneActorTrackView* tracks,
    std::int8_t index, i32 suppress_secondary,
    CutsceneActorFactoryContext& context)
{
    const auto& track = tracks[index];
    Vec3 position;
    sample_cutscene_actor_position(&track.position, 0, 0.f, position, 0);
    char model[128];
    char auxiliary[128];
    std::strcpy(model, track.model);
    if (std::memcmp(context.replacement_source, "aid_", 4) == 0)
    {
        if (resource_names_match(context.replacement_source, model))
        {
            std::strcpy(model, context.replacement_target);
        }
    }
    if (track.auxiliary != nullptr)
    {
        std::strcpy(auxiliary, track.auxiliary);
    }
    const u32 secondary = static_cast<u32>((suppress_secondary != 0) - 1)
        & track.flags & 1;
    return create_cutscene_actor(context.factory, context.resources,
        "aid_objparams_ghoulies_misc_actor", model, position, static_cast<i32>(secondary));
}
}
