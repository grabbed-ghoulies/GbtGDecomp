#include "event_runtime.h"
#include "event_binding.h"
#include "event_timing.h"
#include "../frontend/dialogue_activation.h"
#include "../camera/actor_camera_effects.h"
#include "../entities/actor_sound_pose.h"
#include "../entities/actor_transform.h"
#include "../entities/ghouly_requests.h"
#include "../navigation/navigation.h"
#include "../runtime/interpolation.h"
#include "../../gbtg/text/localtext_resources.h"
#include <cstring>

namespace gbtg
{
namespace
{
// The context's dialogue (0x000582A0 and the rest) and movie (0x00106D50) owners. An owner that is not bound
// raises when an event reaches it.
RuntimeDialogueActivationContext& cutscene_event_dialogue(CutsceneRuntimeContext& context)
{
    if (context.dialogue == nullptr)
    {
        throw Error("cutscene dialogue event: the scene dialogue (0x000582A0) is not bound");
    }
    return *context.dialogue;
}
RuntimeMovieStartupContext& cutscene_event_movies(CutsceneRuntimeContext& context)
{
    if (context.movies == nullptr)
    {
        throw Error("cutscene movie event: the movie startup context (0x00106D50) is not bound");
    }
    return *context.movies;
}
}
namespace
{
template<class T>
T event_value(const u8* data, size_t offset)
{
    T value;
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}
const char* event_name(const u8* data, size_t offset)
{
    return reinterpret_cast<const char*>(data + offset);
}
CutsceneEventPauseContext event_pause_context(RuntimeCutsceneInstance& instance,
    CutsceneRuntimeContext& context)
{
    return {context.instances.at(&instance).actors.data(), context.animations,
        context.audio.voices, context.resume_callbacks};
}
void enqueue_event_caption(const u8* event, size_t key_offset, u8 flag,
    CutsceneRuntimeContext& context)
{
    auto& dialogue = cutscene_event_dialogue(context);
    request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs",
        event_name(event, key_offset), dialogue.measurement.wide_scratch,
        dialogue.measurement.localtext);
    enqueue_runtime_dialogue(dialogue.measurement.wide_scratch, flag, dialogue);
}
}

// Xbox 0x000356D0
bool cutscene_resource_exists(const char* name, AnimationResourceRegistry& resources)
{
    AnimationResourceRecord record;
    record.name = name;
    const void* resolved;
    size_t index = 0;
    do
    {
        auto& provider = resources.providers[index];
        if (provider.handle != 0)
        {
            if (provider.find(provider.handle, record, resolved) != 0)
            {
                return true;
            }
        }
        ++index;
    }
    while (index < 16);
    return false;
}

// Xbox 0x0004C850
void dispatch_runtime_cutscene_event(RuntimeCutsceneInstance* instance,
    std::span<u8> event_state, u8* event, Actor* actor, u32 phase,
    CutsceneRuntimeContext& context)
{
    const u32 kind = static_cast<u32>(event_value<u16>(event, 0xC)) - 1;
    switch (kind)
    {
    case 0:
    case 1:
        request_spatial_audio_event_voice(event_name(event, 0x10), 1, 0.f, context.audio.voices);
        return;
    case 2:
    case 4:
    case 5:
    case 6:
    case 7:
    case 0xE:
        break;
    case 3:
        load_actor_effect_stream(event_name(event, 0x10), nullptr, context.effects);
        return;
    case 8:
        begin_cutscene_camera_impulse(context.impulse, context.random);
        return;
    case 9:
        replace_cutscene_stream(event_state, event_name(event, 0x10), context.audio);
        return;
    case 10:
        release_cutscene_stream(event_state, event_name(event, 0x10), context.audio);
        return;
    case 0xB:
        dispatch_cutscene_voice_event(event, event_state, phase, context.audio.voices);
        return;
    case 0xC:
        if (phase == 0)
        {
            set_cutscene_actor_track_state(*instance, context.views.at(instance->resource).tracks.data(),
                context.instances.at(instance).actors.data(), static_cast<std::int8_t>(event[0x10]), 1);
            return;
        }
        if (phase == 1)
        {
            set_cutscene_actor_track_state(*instance, context.views.at(instance->resource).tracks.data(),
                context.instances.at(instance).actors.data(), static_cast<std::int8_t>(event[0x10]), 0);
            return;
        }
        break;
    case 0xD:
        if (phase == 0)
        {
            load_runtime_movie_event(event_name(event, 0x10), event[0x90], 0, 1,
                context.movie_slots, cutscene_event_movies(context));
            context.movie_flag_3c = 0;
            return;
        }
        if (phase == 1)
        {
            start_runtime_movie_event(event[0x90], context.movie_mode, context.movie_slots);
            return;
        }
        break;
    case 0xF:
        if (phase == 0)
        {
            const auto index = find_cutscene_actor_track(*instance,
                context.views.at(instance->resource).tracks.data(), static_cast<std::int8_t>(event[0x10]));
            if (index != -1)
            {
                auto* track_actor = context.instances.at(instance).actors[index].actor;
                context.event_objects.push_back(create_resource_object_with_data(context.factory,
                    context.executable, event_name(event, 0x11), context.actor_addresses.at(track_actor)));
                return;
            }
        }
        break;
    case 0x10:
        switch (phase)
        {
        case 0:
            if (event[0x91] == 0)
            {
                replace_cutscene_bank(event_state, event_name(event, 0xA0), context.audio);
                const u32 value = event_value<u32>(event, 0x98);
                const u8 flag = event[0x90];
                event[0x91] = 1;
                std::memcpy(event + 0x9C, &value, sizeof(value));
                event[0x92] = 0;
                if (static_cast<i32>(cutscene_event_dialogue(context).state.count + 1) < 9)
                {
                    enqueue_event_caption(event, 0x22, flag, context);
                    return;
                }
            }
            break;
        case 1:
            if (event[0x92] == 0)
            {
                seek_runtime_cutscene(*instance, event_value<float>(event, 4) / instance->duration,
                    event_value<float>(event, 0x94), 0, context);
                event[0x93] = 1;
                return;
            }
            break;
        case 2:
            if (cutscene_event_dialogue(context).state.mode == 0)
            {
                release_cutscene_bank(event_state, event_name(event, 0xA0), context.audio);
                event[0x91] = 0;
                close_runtime_dialogue(cutscene_event_dialogue(context));
                seek_runtime_cutscene(*instance, event_value<float>(event, 8) / instance->duration,
                    event_value<float>(event, 0x94), event[0x93] == 0, context);
                event[0x92] = 1;
                if (event[0x120] != 0)
                {
                    replace_cutscene_stream(event_state, event_name(event, 0x120), context.audio);
                    return;
                }
            }
            break;
        case 3:
            if (event[0x91] != 0)
            {
                release_cutscene_bank(event_state, event_name(event, 0xA0), context.audio);
                event[0x91] = 0;
                return;
            }
            break;
        }
        break;
    case 0x11:
    {
        const auto index = find_cutscene_actor_track(*instance,
            context.views.at(instance->resource).tracks.data(), static_cast<std::int8_t>(event[0x10]));
        if (index != -1)
        {
            auto* track_actor = context.instances.at(instance).actors[index].actor;
            if (track_actor != nullptr)
            {
                request_actor_scare(*track_actor, context.actor_addresses.at(track_actor), 1.f, 1.f,
                    1, 0, context.scare);
                return;
            }
        }
        break;
    }
    case 0x12:
        request_ghoulybox(event_name(event, 0x10), event_value<u32>(event, 0x110), event_name(event, 0x90));
        return;
    case 0x13:
        if (instance->pending[0] != 0)
        {
            context.event_callbacks.at(instance->pending[0])(*instance,
                instance->pending[1], event_value<u32>(event, 0x10));
            return;
        }
        break;
    case 0x14:
        if (static_cast<i32>(cutscene_event_dialogue(context).state.count + 1) < 9)
        {
            enqueue_event_caption(event, 0x10, event[0x150], context);
        }
        if (!equal_sound_names(event_name(event, 0x50), "XACT_SOUNDBANK_GMISC_NOSOUND"))
        {
            replace_cutscene_bank(event_state, event_name(event, 0x50), context.audio);
        }
        instance->pending[4] = 0x0004C4F0;
        context.dialogue_events[instance] = event;
        context.resume_callbacks[0x0004C4F0] = [&context](RuntimeCutsceneInstance& resumed, u32, i32)
        {
            complete_cutscene_dialogue_event(resumed, context.dialogue_events.at(&resumed), context.audio);
        };
        instance->pending[3] = 1;
        {
            auto pause = event_pause_context(*instance, context);
            pause_cutscene_events(*instance, 1, pause);
        }
        return;
    case 0x15:
        load_cutscene_camera_event_asset(event_name(event, 0x10), context.camera_asset,
            context.resources, context.error);
        return;
    case 0x16:
    case 0x17:
    case 0x18:
        if (phase == 0)
        {
            const size_t chance_offset = kind == 0x16 ? 0x98 : 0x9C;
            if (actor_event_chance(context.random, event_value<float>(event, chance_offset)))
            {
                auto* track_actor = find_cutscene_actor(*instance,
                    context.views.at(instance->resource).tracks.data(), context.instances.at(instance).actors.data(),
                    static_cast<std::int8_t>(event[0x10]));
                if (track_actor != nullptr)
                {
                    g_actor_effect_binding.mode = kind == 0x16 ? 1 : kind == 0x17 ? 0 : 2;
                    g_actor_effect_binding.actor = track_actor;
                    if (kind == 0x16)
                    {
                        const u32 old = static_cast<u32>(g_actor_effect_binding.index);
                        g_actor_effect_binding.index = static_cast<i32>((old & 0xFFFF0000u)
                            | event_value<u16>(event, 0x92));
                    }
                    else
                    {
                        g_actor_effect_binding.index = event_value<i32>(event, 0x94);
                    }
                    auto& options = cutscene_actor_effect_options(*track_actor, &g_actor_effect_binding);
                    if (load_actor_effect_stream(event_name(event, 0x11), &options, context.effects) != 0)
                    {
                        const size_t duration_offset = kind == 0x16 ? 0x94 : 0x98;
                        const float duration = event_value<u32>(event, duration_offset) != 0
                            ? event_value<float>(event, 8) - event_value<float>(event, 4) : -1.f;
                        retain_last_actor_effect(duration);
                    }
                    const size_t sound_offset = kind == 0x16 ? 0xA0 : 0xA4;
                    if (event[sound_offset] != 0)
                    {
                        request_actor_sound(*track_actor, event_value<i32>(event, sound_offset - 4),
                            event_name(event, sound_offset), context.effects.sounds);
                        return;
                    }
                }
            }
        }
        break;
    case 0x19:
        if (phase == 0 && event_value<u32>(event, 0x11) == 0x5F646961u)
        {
            const auto index = find_cutscene_actor_track(*instance,
                context.views.at(instance->resource).tracks.data(), static_cast<std::int8_t>(event[0x10]));
            Vec3 position;
            if (index != -1)
            {
                position = actor_world_position(*context.instances.at(instance).actors[index].actor);
            }
            else
            {
                position = context.listener_position;
            }
            load_actor_camera_shake(event_name(event, 0x11), position, 1.f, context.resources, context.error);
            return;
        }
        break;
    case 0x1A:
        if (event_value<float>(event, 0x18) == 0.f)
        {
            context.fog.enabled = 0;
            return;
        }
        context.fog.enabled = 1;
        configure_runtime_scene_fog(context.renderer, context.fog);
        std::memcpy(context.fog.color.data(), event + 0x10, 4);
        context.fog.start = event_value<float>(event, 0x14);
        context.fog.end = event_value<float>(event, 0x18);
        return;
    case 0x1B:
        std::memcpy(context.fog.color.data(), event + 0x10, 4);
        if (phase == 0)
        {
            context.fog.enabled = 1;
            configure_runtime_scene_fog(context.renderer, context.fog);
            context.fog.start = event_value<float>(event, 0x14);
            context.fog.end = event_value<float>(event, 0x18);
            return;
        }
        if (phase == 1 && event_value<u32>(event, 0x24) != 0)
        {
            context.fog.enabled = 0;
            return;
        }
        context.fog.start = interpolate_clamped_range(event_value<float>(event, 8), instance->time,
            event_value<float>(event, 4), event_value<float>(event, 0x14), event_value<float>(event, 0x1C));
        context.fog.end = interpolate_clamped_range(event_value<float>(event, 8), instance->time,
            event_value<float>(event, 4), event_value<float>(event, 0x18), event_value<float>(event, 0x20));
        return;
    case 0x1C:
    {
        const Vec3 position = context.listener_position;
        const i32 floor = entity_detail::navigation_floor_index(position, -1, 0);
        const u8 surface = entity_detail::navigation_surface_kind(floor, position);
        if (surface != 0)
        {
            char name[128];
            format_footstep_sound_name(name, surface, "", context.footsteps[event_value<i32>(event, 0x10)],
                context.footstep_names);
            request_spatial_audio_event_voice(name, 1, 0.f, context.audio.voices);
            return;
        }
        break;
    }
    case 0x1D:
        request_audio_event_voice(event_name(event, 0x10), 1, context.audio.voices);
        return;
    case 0x1E:
        if (phase == 0)
        {
            char names[5][128];
            i32 count = 0;
            i32 index = 0;
            do
            {
                const u8* name = event + 0x10 + index * 0x80;
                if (event_value<u32>(name, 0) == 0x5F646961u)
                {
                    std::strcpy(names[count], reinterpret_cast<const char*>(name));
                    ++count;
                }
                ++index;
            }
            while (index < 5);
            const i32 chosen = select_cutscene_random_index(context.random, count, 0);
            load_runtime_movie_event(names[chosen], event[0x290], 0, 1,
                context.movie_slots, cutscene_event_movies(context));
            context.movie_flag_38 = 0;
            return;
        }
        if (phase == 1)
        {
            start_runtime_movie_event(event[0x290], context.movie_mode, context.movie_slots);
            return;
        }
        break;
    case 0x1F:
        if (phase == 0)
        {
            char name[128];
            if (event_value<u32>(event, 0x10) == 0x5F646961u)
            {
                std::strcpy(name, event_name(event, 0x10));
                const char* format;
                switch (context.language)
                {
                case 0: format = "%s"; break;
                case 1: format = "%sfrench"; break;
                case 2: format = "%sgerman"; break;
                case 3: format = "%sspanish"; break;
                case 4: format = "%sitalian"; break;
                case 6: format = "%skorean"; break;
                case 7: format = "%schinese"; break;
                case 8: format = "%sjapanese"; break;
                default: goto localized_movie_ready;
                }
                format_dialogue_resource_name(name, format, name);
            }
localized_movie_ready:
            load_runtime_movie_event(name, event[0x90], 0, 1, context.movie_slots, cutscene_event_movies(context));
            break;
        }
        if (phase == 1)
        {
            start_runtime_movie_event(event[0x90], context.movie_mode, context.movie_slots);
            return;
        }
        break;
    }
}

// Xbox 0x0004C560
void prepare_runtime_cutscene_events(u8* events, CutsceneRuntimeContext& context)
{
    if (events != nullptr)
    {
        while (event_value<u16>(events, 0xC) != 0)
        {
            if (event_value<u16>(events, 0xC) == 0x11)
            {
                events[0x91] = 0;
                events[0x92] = 0;
                events[0x93] = 0;
                const u32 zero = 0;
                std::memcpy(events + 0x9C, &zero, sizeof(zero));
                const char* name = context.dialogue_resources;
                i32 remaining = 6;
                do
                {
                    std::span<const u8> resource;
                    if (event_value<u32>(reinterpret_cast<const u8*>(name), 0) == 0x5F646961u)
                    {
                        resource = acquire_animation_resource(context.resources, name);
                        if (resource.data() == nullptr)
                        {
                            resource_error(context.error, 2);
                        }
                    }
                    release_animation_resource(context.resources, resource);
                    name += 0x80;
                    --remaining;
                }
                while (remaining != 0);
            }
            else if (event_value<u16>(events, 0xC) == 0x1B && event_value<float>(events, 4) == 0.f)
            {
                dispatch_runtime_cutscene_event(nullptr, {}, events, nullptr, 0, context);
            }
            events += context.event_sizes[event_value<u16>(events, 0xC)];
        }
    }
}

// Xbox 0x0004C640
const u8* load_runtime_cutscene_events(const char* name, std::span<u8> event_state,
    CutsceneRuntimeContext& context)
{
    event_state[6] = 0;
    event_state[0x86] = 0;
    if (find_animation_resource(context.resources.records, name) == nullptr)
    {
        if (!cutscene_resource_exists(name, context.resources))
        {
            return nullptr;
        }
    }
    if (event_value<u32>(reinterpret_cast<const u8*>(name), 0) != 0x5F646961u)
    {
        prepare_runtime_cutscene_events(nullptr, context);
        return nullptr;
    }
    const auto resource = acquire_animation_resource(context.resources, name);
    if (resource.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    prepare_runtime_cutscene_events(const_cast<u8*>(resource.data()), context);
    return resource.data();
}

// Xbox 0x0004E020
void load_runtime_cutscene_camera_events(RuntimeCutsceneInstance& instance,
    const char* name, CutsceneRuntimeContext& context)
{
    size_t index = 4;
    while (name[index] != '_')
    {
        ++index;
    }
    char events[128];
    format_dialogue_resource_name(events, "aid_cutsceneevents%s_camera", name + index);
    instance.events = load_runtime_cutscene_events(events, instance.event_state, context);
}

// Xbox 0x0004D370
void advance_runtime_cutscene_events(RuntimeCutsceneInstance& instance,
    std::span<u8> event_state, u8* events, Actor* actor, float current,
    float previous, i32 ending, i32 restricted, CutsceneRuntimeContext& context)
{
    while (event_value<u16>(events, 0xC) != 0)
    {
        u32 phase;
        if (select_cutscene_event_phase(events, ending, current, previous, restricted, phase))
        {
            dispatch_runtime_cutscene_event(&instance, event_state, events, actor, phase, context);
        }
        events += context.event_sizes[event_value<u16>(events, 0xC)];
    }
}

// Xbox 0x0004D3E0
void end_runtime_cutscene_events(RuntimeCutsceneInstance& instance,
    std::span<u8> event_state, u8* events, Actor* actor, float current,
    float previous, CutsceneRuntimeContext& context)
{
    advance_runtime_cutscene_events(instance, event_state, events, actor, current,
        previous, 1, 0, context);
    auto* bank = reinterpret_cast<char*>(event_state.data() + 6);
    if (bank[0] != 0)
    {
        if (equal_sound_names(bank, bank) && bank[0] != 0)
        {
            release_tracked_audio_bank(bank, context.audio);
            bank[0] = 0;
        }
    }
    auto* stream = reinterpret_cast<char*>(event_state.data() + 0x86);
    if (stream[0] != 0)
    {
        if (equal_sound_names(stream, stream))
        {
            release_audio_stream(stream, context.audio);
            stream[0] = 0;
        }
    }
}

// Xbox 0x0004E070
void end_all_runtime_cutscene_events(RuntimeCutsceneInstance& instance,
    CutsceneRuntimeContext& context)
{
    if (instance.events != nullptr)
    {
        end_runtime_cutscene_events(instance, instance.event_state, const_cast<u8*>(instance.events),
            nullptr, instance.time, instance.previous_time, context);
    }
    i32 index = 0;
    if (0 < instance.actor_count)
    {
        auto& rows = context.instances.at(&instance);
        do
        {
            if (rows.events[index].data() != nullptr)
            {
                end_runtime_cutscene_events(instance, instance.event_state, const_cast<u8*>(rows.events[index].data()),
                    rows.actors[index].actor, instance.time, instance.previous_time, context);
            }
            ++index;
        }
        while (index < instance.actor_count);
    }
}

// Xbox 0x0004D680
RuntimeCutsceneInstance* load_runtime_cutscene(const char* name, u32 flags,
    EntityCamNode* camera, CutsceneRuntimeContext& context)
{
    std::span<const u8> resource;
    if (event_value<u32>(reinterpret_cast<const u8*>(name), 0) == 0x5F646961u)
    {
        resource = acquire_animation_resource(context.resources, name);
        if (resource.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
    }
    const float duration = event_value<float>(resource.data(), 4);
    auto* instance = allocate_cutscene_instance(resource.data(), resource.data()[3], duration, flags);
    auto& rows = context.instances[instance];
    rows.actors.resize(instance->actor_count);
    rows.events.resize(instance->actor_count);
    rows.blend_positions.resize(instance->actor_count);
    rows.blend_scales.resize(instance->actor_count);
    const auto& view = context.views.at(resource.data());
    initialize_cutscene_actors(*instance, view.tracks.data(), rows.actors.data(),
        flags & 2, flags & 4, context.initialization);
    load_runtime_cutscene_camera_events(*instance, name, context);
    if (resource.data()[2] != 0 && camera != nullptr)
    {
        update_cutscene_camera_view(*instance, view.cameras.data(), *camera);
    }
    context.replacement_source[0] = 0;
    context.replacement_target[0] = 0;
    return instance;
}

// Xbox 0x0004D720
void release_runtime_cutscene(RuntimeCutsceneInstance* instance, CutsceneRuntimeContext& context)
{
    end_all_runtime_cutscene_events(*instance, context);
    u32 index = 0;
    if (static_cast<u32>(instance->actor_count) != 0)
    {
        auto& rows = context.instances.at(instance);
        CutsceneActorReleaseContext release{context.factory, context.objects, context.render,
            rows.events, context.resources};
        do
        {
            release_cutscene_actor_row(rows.actors.data(), static_cast<std::int8_t>(index), release);
            ++index;
        }
        while (index < static_cast<u32>(instance->actor_count));
    }
    instance->previous_time = 0.f;
    instance->time = 0.f;
    instance->actor_count = 0;
    if (instance->resource != nullptr)
    {
        release_animation_resource(context.resources, {instance->resource, 0});
        instance->resource = nullptr;
    }
    if (instance->events != nullptr)
    {
        release_animation_resource(context.resources, {instance->events, 0});
    }
    instance->events = nullptr;
    stop_cutscene_event_voices(instance->event_state, context.audio.voices);
    context.instances.erase(instance);
    context.dialogue_events.erase(instance);
    instance->~RuntimeCutsceneInstance();
    context.free_instance(instance);
}

// Xbox 0x0004E4A0
void seek_runtime_cutscene(RuntimeCutsceneInstance& instance, float position,
    float duration, i32 events, CutsceneRuntimeContext& context)
{
    const auto& view = context.views.at(instance.resource);
    if (0.f < duration)
    {
        transform_cutscene_camera_pose(instance, view.cameras.data(), &instance.blend_position,
            &instance.blend_rotation, instance.time);
    }
    instance.blend_duration = duration;
    instance.blend_time = duration;
    u32 index = 0;
    if (static_cast<u32>(instance.actor_count) != 0)
    {
        auto& rows = context.instances.at(&instance);
        do
        {
            if (rows.actors[index].actor != nullptr)
            {
                auto* controller = context.animations.at(rows.actors[index].actor)->controller.get();
                if (!(duration > 0.f))
                {
                    if (controller != nullptr)
                    {
                        auto* player = controller->slots[0].player.get();
                        if (player != nullptr)
                        {
                            player->position = position;
                            player->previous = position;
                        }
                        controller->dirty = true;
                    }
                }
                else
                {
                    auto* player = controller->slots[0].player.get();
                    sample_cutscene_actor_position(&view.tracks[static_cast<std::int8_t>(index)].position, 0,
                        instance.time, rows.blend_positions[index], 0);
                    sample_cutscene_actor_scale(&view.tracks[static_cast<std::int8_t>(index)].scale, 0,
                        instance.time, rows.blend_scales[index]);
                    player->blend_duration = duration;
                    start_mina_player(*player, player->mina, position, -1.f, 1, 0);
                }
            }
            ++index;
        }
        while (index < static_cast<u32>(instance.actor_count));
    }
    instance.previous_time = instance.time;
    instance.time = instance.duration * position;
    instance.untouched_38 = 0;
    if (events != 0 && instance.events != nullptr)
    {
        advance_runtime_cutscene_events(instance, instance.event_state, const_cast<u8*>(instance.events),
            nullptr, instance.time, instance.previous_time, 0, 0, context);
    }
}

// Xbox 0x0004E1E0
void update_runtime_cutscene_actors(RuntimeCutsceneInstance& instance,
    float blend, float position, CutsceneRuntimeContext& context)
{
    i32 index = 0;
    if (0 < instance.actor_count)
    {
        const auto& view = context.views.at(instance.resource);
        auto& rows = context.instances.at(&instance);
        do
        {
            auto& row = rows.actors[index];
            auto* actor = row.actor;
            if (actor != nullptr)
            {
                const auto events = rows.events[index];
                if (row.flags == 0)
                {
                    Vec3 point;
                    sample_cutscene_actor_position(&view.tracks[static_cast<std::int8_t>(index)].position, 0,
                        instance.time, point, (instance.flags >> 6) & 1);
                    if (0.f < instance.blend_time)
                    {
                        const auto& start = rows.blend_positions[index];
                        point.x = (point.x - start.x) * blend + start.x;
                        point.y = (point.y - start.y) * blend + start.y;
                        point.z = (point.z - start.z) * blend + start.z;
                    }
                    rotate_euler_vector(instance.euler, point, point);
                    point.x += instance.position.x;
                    point.y += instance.position.y;
                    point.z += instance.position.z;
                    actor_set_world_position(*actor, point);
                    normalize_euler_angles(actor->euler, instance.euler);
                    actor->xform_valid = 0;
                    Vec3 scale;
                    sample_cutscene_actor_scale(&view.tracks[static_cast<std::int8_t>(index)].scale, 0, instance.time, scale);
                    if (0.f < instance.blend_time)
                    {
                        const auto& start = rows.blend_scales[index];
                        scale.x = (scale.x - start.x) * blend + start.x;
                        scale.y = (scale.y - start.y) * blend + start.y;
                        scale.z = (scale.z - start.z) * blend + start.z;
                    }
                    scale.x *= instance.scale.x;
                    scale.y *= instance.scale.y;
                    scale.z *= instance.scale.z;
                    if (scale.x != actor->scale_x || scale.y != actor->radius_y || scale.z != actor->radius_xz)
                    {
                        actor->scale_x = scale.x;
                        actor->radius_y = scale.y;
                        actor->radius_xz = scale.z;
                        actor->xform_valid = 0;
                    }
                    auto* controller = context.animations.at(actor)->controller.get();
                    if (controller != nullptr)
                    {
                        auto* player = controller->slots[0].player.get();
                        if (player != nullptr)
                        {
                            player->position = position;
                            player->previous = position;
                        }
                        controller->dirty = true;
                    }
                }
                if (events.data() != nullptr)
                {
                    advance_runtime_cutscene_events(instance, instance.event_state, const_cast<u8*>(events.data()),
                        actor, instance.time, instance.previous_time, 0, 0, context);
                }
                if (row.listed == 0)
                {
                    context.updates.at(actor)(*actor);
                    actor_refresh_transform(*actor);
                }
            }
            ++index;
        }
        while (index < instance.actor_count);
    }
}

// Xbox 0x0004DA50
bool tick_runtime_cutscene(RuntimeCutsceneInstance& instance, float dt,
    EntityCamNode* camera, CutsceneRuntimeContext& context)
{
    if (instance.pending[2] != 0)
    {
        if (instance.pending[3] != 0 && cutscene_event_dialogue(context).state.mode == 0)
        {
            auto pause = event_pause_context(instance, context);
            pause_cutscene_events(instance, 0, pause);
        }
    }
    if (instance.pending[2] == 0)
    {
        const float next = instance.time + dt;
        instance.previous_time = instance.time;
        instance.time = next;
        std::memcpy(&instance.untouched_38, &dt, sizeof(dt));
        if (next > instance.duration)
        {
            instance.time = instance.duration;
        }
        if (0.f > instance.time)
        {
            instance.time = 0.f;
        }
    }
    float blend;
    if (0.f < instance.blend_time)
    {
        const float duration = instance.blend_duration;
        instance.blend_time -= dt;
        if (duration != 0.f)
        {
            blend = (instance.blend_time - duration) / (0.f - duration);
            if (blend > 1.f)
            {
                blend = 1.f;
            }
            else if (0.f > blend)
            {
                blend = 0.f;
            }
        }
        else
        {
            blend = 1.f;
        }
    }
    else
    {
        blend = -1.f; // DAT_0032B120.
    }
    if (instance.resource[2] != 0 && camera != nullptr)
    {
        update_cutscene_camera_view(instance, context.views.at(instance.resource).cameras.data(), *camera);
    }
    if (instance.events != nullptr && instance.pending[2] == 0 && dt != 0.f)
    {
        advance_runtime_cutscene_events(instance, instance.event_state, const_cast<u8*>(instance.events),
            nullptr, instance.time, instance.previous_time, 0, 0, context);
    }
    update_runtime_cutscene_actors(instance, blend, instance.time / instance.duration, context);
    return instance.time >= instance.duration;
}

// Xbox 0x0004DBD0
void draw_runtime_cutscene(RuntimeCutsceneInstance& instance, i32 view,
    CutsceneRuntimeContext& context)
{
    i32 index = 0;
    if (0 < instance.actor_count)
    {
        auto& rows = context.instances.at(&instance).actors;
        do
        {
            auto& row = rows[index];
            if (row.listed == 0 && row.initialized != 0 && row.actor != nullptr)
            {
                context.draws(*row.actor, view, 1);
                context.draws(*row.actor, view, 2);
            }
            ++index;
        }
        while (index < instance.actor_count);
    }
}
}

